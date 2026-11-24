// SPDX-License-Identifier: GPL-3.0-or-later
// Scene files (benchmarks/*.json), camera rays and per-ray reports, shared by
// the CPU tracer (gravastar_trace) and the GPU driver. Moved unchanged from
// src/experiments/trace.cpp.
#pragma once
#include "gravastar/optics/background.hpp"
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace gravastar::optics {
struct Scene {
  geometry::Parameters geometry;
  Settings settings;
  Vector position;
  std::optional<double> omega;
  DiskMotion disk_motion{DiskMotion::zamo};
  std::optional<StarCatalogue> background; // separate channel when present
  double fov;
  int width, height;
  bool angular{};
};
inline Scene parse_scene(const tools::Json &j) {
  if (j.at("schema_version") != 1)
    throw std::invalid_argument("unsupported optics schema");
  Scene s;
  const auto &p = j.at("geometry");
  auto f = geometry::parse_family(p.at("family").get<std::string>());
  if (auto *error = std::get_if<std::string>(&f))
    throw std::invalid_argument(*error);
  s.geometry = {std::get<geometry::Family>(f), p.at("M"),    p.at("a"),
                p.at("Lambda"),         p.at("r_in"), p.at("r_out")};
  const auto &d = j.at("domain");
  auto &c = s.settings;
  c.inner_radius = d.at("inner_radius");
  c.outer_radius = d.at("outer_radius");
  c.polar_margin = d.at("polar_margin");
  if (!d.at("capture_radius").is_null())
    c.capture_radius = d.at("capture_radius").get<double>();
  const auto &i = j.at("integrator");
  const auto coordinates =
      i.value("coordinates", std::string("boyer_lindquist"));
  if (coordinates != "boyer_lindquist" && coordinates != "angular")
    throw std::invalid_argument("unknown integration coordinates");
  s.angular = coordinates == "angular";
  c.angular_switch_q = i.value("angular_switch_q", 1.5);
  c.continue_core = i.value("continue_core", false);
  c.core_entry_radius = i.value("core_entry_radius", .75);
  c.core_exit_radius = i.value("core_exit_radius", 1.0);
  c.absolute_tolerance = i.at("absolute_tolerance");
  c.relative_tolerance = i.at("relative_tolerance");
  c.initial_step = i.at("initial_step");
  c.minimum_step = i.at("minimum_step");
  c.maximum_step = i.at("maximum_step");
  c.event_tolerance = i.at("event_tolerance");
  c.null_tolerance = i.at("null_tolerance");
  c.maximum_affine = i.at("maximum_affine");
  c.tangency_factor = i.value("tangency_factor", 1000.0);
  if (!i.at("maximum_attempts").is_number_integer() ||
      i.at("maximum_attempts") < 1 || i.at("maximum_attempts") > 10000000)
    throw std::invalid_argument(
        "maximum_attempts must be an integer in [1,10000000]");
  c.maximum_attempts = i.at("maximum_attempts");
  const auto &disk = j.at("disk");
  const auto motion = disk.at("motion").get<std::string>();
  if ((motion != "ZAMO" && motion != "keplerian_prograde") ||
      disk.at("emissivity") != "bolometric_r_inverse_cubed")
    throw std::invalid_argument("unsupported emission prescription");
  s.disk_motion = motion == "ZAMO" ? DiskMotion::zamo
                                   : DiskMotion::keplerian_prograde;
  c.disk = disk.at("enabled");
  c.disk_inner = disk.at("inner_radius");
  c.disk_outer = disk.at("outer_radius");
  // Prescribed orbits must stay in their supported timelike, stable domain.
  if (c.disk && s.disk_motion == DiskMotion::keplerian_prograde &&
      !keplerian_annulus_supported(s.geometry, c.disk_inner, c.disk_outer))
    throw std::invalid_argument(
        "Keplerian disk annulus leaves the stable timelike circular orbits");
  if (j.contains("background") && !j.at("background").is_null()) {
    const auto &b = j.at("background");
    if (b.at("model") != "static_star_sphere_v1")
      throw std::invalid_argument("unsupported background model");
    StarField f;
    f.seed = b.at("seed").get<std::uint64_t>();
    f.count = b.at("stars").get<int>();
    f.width = b.at("width").get<double>();
    f.range = b.at("range").get<double>();
    if (f.count < 1 || f.count > 100000 || !(f.width > 0 && f.width < 1) ||
        !(f.range >= 1 && f.range <= 1e6))
      throw std::invalid_argument("invalid star field parameters");
    s.background = make_catalogue(f);
  }
  const auto &camera = j.at("camera");
  s.position = camera.at("position").get<Vector>();
  if (!camera.at("omega").is_null())
    s.omega = camera.at("omega").get<double>();
  s.fov = camera.at("vertical_field_of_view_degrees").get<double>() *
          std::numbers::pi / 180;
  for (const auto *axis : {"width", "height"})
    if (!j.at("image").at(axis).is_number_integer() ||
        j.at("image").at(axis) < 1 || j.at("image").at(axis) > 128)
      throw std::invalid_argument(
          "image dimensions must be integers in [1,128]");
  s.width = j.at("image").at("width");
  s.height = j.at("image").at("height");
  if (!std::isfinite(s.fov) || s.fov <= 0 ||
      s.fov >= 170 * std::numbers::pi / 180 || s.width < 1 || s.height < 1 ||
      s.width > 128 || s.height > 128)
    throw std::invalid_argument(
        "invalid camera field of view or image size (maximum 128x128)");
  return s;
}
// One integration tolerance for preview and recording paths: absolute and
// relative tolerance, the event tolerance ten times looser, and a null-drift
// safety net that sits above what that tolerance allows.
inline Scene with_tolerance(Scene s, double tolerance) {
  if (!(tolerance > 0 && tolerance <= 1e-3))
    throw std::invalid_argument("tolerance must be in (0, 1e-3]");
  s.settings.absolute_tolerance = s.settings.relative_tolerance = tolerance;
  s.settings.event_tolerance = 10 * tolerance;
  s.settings.null_tolerance = std::max(s.settings.null_tolerance, 1e3 * tolerance);
  return s;
}
inline Observer scene_observer(const Scene &s) {
  return observer(s.geometry, s.position, s.omega);
}
// With the camera's observer computed once per image (same result as below).
inline State scene_pixel(const Scene &s, const Observer &camera, int x, int y) {
  double tangent = std::tan(s.fov / 2);
  double right = (2 * (x + .5) / s.width - 1) * tangent * s.width / s.height;
  double up = (1 - 2 * (y + .5) / s.height) * tangent;
  return launch(camera, {-1, -up, right});
}
inline State scene_pixel(const Scene &s, int x, int y) {
  double tangent = std::tan(s.fov / 2);
  double right = (2 * (x + .5) / s.width - 1) * tangent * s.width / s.height;
  double up = (1 - 2 * (y + .5) / s.height) * tangent;
  return launch(s.geometry, s.position, {-1, -up, right}, s.omega);
}
inline tools::Json point_report(const Point &p) {
  return {{"affine", p.affine},
          {"state", p.state},
          {"chart", chart_name(p.chart)},
          {"null_residual", p.null_error}};
}
// Radiance data of one traced ray, without the JSON report around it: what
// the image needs. Fields are empty where the outcome has no such value.
struct RaySample {
  std::optional<double> intensity, redshift, alpha;
  std::optional<double> background, background_redshift;
  std::optional<std::array<double, 3>> background_direction;
};
// `camera` is the frame at the ray's start (scene_observer(s).frame for
// camera rays); the overload below computes it from `initial`.
inline RaySample ray_sample(const Scene &s, const Frame &camera, const State &initial,
                            const Trace &trace) {
  RaySample out;
  if (trace.outcome == Outcome::emitted) {
    const auto &frame = camera;
    // Disk hits are at q=1, where conversion to the original emission chart
    // is regular. The camera and physical ZAMO emission prescription are
    // shared.
    auto hit = trace.final.chart == Chart::boyer_lindquist
                   ? trace.final.state
                   : transform_state(trace.final.state, trace.final.chart,
                                        Chart::boyer_lindquist);
    auto emission =
        disk_emission(s.geometry, hit, frequency(initial, frame.basis[0]),
                         s.settings.disk_inner, s.disk_motion);
    out.intensity = emission.linear_intensity;
    out.redshift = emission.redshift;
    out.alpha = 1;
  } else if (trace.outcome == Outcome::escaped) {
    out.intensity = 0;
    out.alpha = 0;
    if (s.background) {
      const auto &frame = camera;
      const auto b = background_emission(
          s.geometry, trace.final, frequency(initial, frame.basis[0]),
          *s.background);
      out.background = b.linear_intensity;
      out.background_redshift = b.redshift;
      out.background_direction = b.direction;
    }
  } else if (trace.outcome == Outcome::capture_surface) {
    out.intensity = 0;
    out.alpha = 1;
  }
  return out;
}
inline RaySample ray_sample(const Scene &s, const State &initial, const Trace &trace) {
  if (trace.outcome != Outcome::emitted &&
      !(trace.outcome == Outcome::escaped && s.background))
    return ray_sample(s, Frame{}, initial, trace); // the frame is not used
  return ray_sample(s, circular_frame(metric(s.geometry, initial), s.omega), initial, trace);
}
inline tools::Json ray_report(const Scene &s, const State &initial,
                              const Trace &trace) {
  tools::Json result{{"outcome", outcome_name(trace.outcome)},
           {"reason", trace.detail},
           {"initial_state", initial},
           {"initial_chart", "boyer_lindquist"},
           {"final", point_report(trace.final)},
           {"accepted_steps", trace.accepted},
           {"rejected_steps", trace.rejected},
           {"rhs_calls", trace.rhs_calls},
           {"chart_switches", trace.chart_switches},
           {"maximum_null_residual", trace.maximum_null_error},
           {"energy_error", trace.energy_error},
           {"angular_momentum_error", trace.angular_momentum_error},
           {"linear_intensity", nullptr},
           {"redshift", nullptr},
           {"alpha", nullptr},
           {"background_intensity", nullptr},
           {"background_redshift", nullptr},
           {"background_direction", nullptr}};
  const auto sample = ray_sample(s, initial, trace);
  auto set = [&](const char *key, const auto &value) {
    if (value)
      result[key] = *value;
  };
  set("linear_intensity", sample.intensity);
  set("redshift", sample.redshift);
  set("alpha", sample.alpha);
  set("background_intensity", sample.background);
  set("background_redshift", sample.background_redshift);
  set("background_direction", sample.background_direction);
  result["crossings"] = tools::Json::array();
  for (const auto &c : trace.crossings)
    result["crossings"].push_back(
        {{"surface", c.surface}, {"point", point_report(c.point)}});
  // Surface approaches closer than the trace resolves. "terminal": the
  // outcome itself may differ; otherwise only the crossing list may.
  result["near_tangencies"] = tools::Json::array();
  for (const auto &n : trace.tangencies)
    result["near_tangencies"].push_back({{"surface", n.surface},
                                         {"point", point_report(n.point)},
                                         {"surface_value", n.value},
                                         {"margin", n.margin},
                                         {"terminal", n.terminal}});
  if (s.settings.save_trajectory) {
    result["trajectory"] = tools::Json::array();
    for (const auto &p : trace.trajectory)
      result["trajectory"].push_back(point_report(p));
  }
  return result;
}
// Diagnostic display colour (gamma 1/2.2 after I/(1+I)); not linear data.
inline std::array<unsigned char, 3> sample_color(Outcome outcome, const RaySample &s) {
  if (outcome == Outcome::emitted) {
    const double intensity = *s.intensity;
    auto v = static_cast<unsigned char>(
        std::lround(255 * std::pow(intensity / (1 + intensity), 1 / 2.2)));
    return {v, v, v};
  }
  if (outcome == Outcome::escaped) {
    if (!s.background)
      return {16, 24, 48};
    const double b = *s.background;
    const double v = std::pow(b / (1 + b), 1 / 2.2);
    return {static_cast<unsigned char>(std::lround(16 + 200 * v)),
            static_cast<unsigned char>(std::lround(24 + 210 * v)),
            static_cast<unsigned char>(std::lround(48 + 207 * v))};
  }
  if (outcome == Outcome::capture_surface)
    return {0, 0, 0};
  if (outcome == Outcome::domain_limit)
    return {255, 0, 255};
  if (outcome == Outcome::affine_limit || outcome == Outcome::step_limit)
    return {255, 192, 0};
  return {255, 0, 0};
}
inline std::array<unsigned char, 3> diagnostic_color(const tools::Json &p) {
  const auto name = p.at("outcome").get<std::string>();
  Outcome outcome = Outcome::numerical_failure;
  for (auto o : {Outcome::escaped, Outcome::emitted, Outcome::capture_surface,
                 Outcome::domain_limit, Outcome::affine_limit, Outcome::step_limit,
                 Outcome::invalid_input, Outcome::numerical_failure})
    if (outcome_name(o) == name)
      outcome = o;
  RaySample s;
  auto get = [&](const char *key, std::optional<double> &value) {
    if (p.contains(key) && !p.at(key).is_null())
      value = p.at(key).get<double>();
  };
  get("linear_intensity", s.intensity);
  get("background_intensity", s.background);
  return sample_color(outcome, s);
}
} // namespace gravastar::optics

