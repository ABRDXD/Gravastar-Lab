// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
u::Json checks = u::Json::array();
void check(const std::string &name, double error, double ceiling) {
  const bool pass = std::isfinite(error) && error <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", error},
                    {"threshold", ceiling},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << error << " > " << ceiling << '\n';
}
void require(const std::string &name, bool pass) {
  check(name, pass ? 0 : 1, 0);
}
double difference(const o::State &a, const o::State &b) {
  double e = 0;
  for (int i = 0; i < 8; ++i)
    e = std::max(e, std::abs(a[i] - b[i]) / (1 + std::abs(b[i])));
  return e;
}
void transformations() {
  double roundtrip = 0, frame_error = 0, pairing = 0, killing = 0, null = 0,
         flow = 0;
  g::Parameters p;
  for (auto family : {g::Family::flat, g::Family::kerr, g::Family::de_sitter,
                      g::Family::toy}) {
    p.family = family;
    for (double radius : {1.65, 2.0})
      for (double theta : {1.0, 1.5, 2.0})
        for (double phi : {-.7, .3, 2.1}) {
          auto bl = o::launch(p, {0, radius, theta, phi}, {-.6, .4, .2});
          auto frame = o::circular_frame(o::metric(p, bl));
          const auto derivative = o::rhs(p, bl);
          for (auto patch : {o::Chart::north, o::Chart::south}) {
            const auto other =
                patch == o::Chart::north ? o::Chart::south : o::Chart::north;
            auto a = o::transform_state(bl, o::Chart::boyer_lindquist, patch);
            auto b = o::transform_state(a, patch, other);
            roundtrip = std::max(
                {roundtrip,
                 difference(bl, o::transform_state(a, patch,
                                                   o::Chart::boyer_lindquist)),
                 difference(a, o::transform_state(b, other, patch))});
            killing = std::max(killing,
                               std::abs(o::axial_momentum(a, patch) - bl[7]));
            auto f =
                o::transform_frame(frame, bl, o::Chart::boyer_lindquist, patch);
            auto f2 = o::transform_frame(f, a, patch, other);
            auto m = o::angular_metric(p, b);
            for (int i = 0; i < 4; ++i) {
              pairing =
                  std::max(pairing, std::abs(o::frequency(b, f2.basis[i]) -
                                             o::frequency(bl, frame.basis[i])));
              for (int j = 0; j < 4; ++j) {
                double dot = 0;
                for (int k = 0; k < 4; ++k)
                  for (int l = 0; l < 4; ++l)
                    dot += m.metric[k][l] * f2.basis[i][k] * f2.basis[j][l];
                frame_error =
                    std::max(frame_error,
                             std::abs(dot - (i == j ? (i == 0 ? -1 : 1) : 0)));
              }
            }
            null = std::max(null, o::angular_null_residual(m, b));
            auto velocity = o::transform_vector(
                {derivative[0], derivative[1], derivative[2], derivative[3]},
                bl, o::Chart::boyer_lindquist, patch);
            auto da = o::angular_rhs(p, a);
            for (int i = 0; i < 4; ++i)
              flow = std::max(flow, std::abs(da[i] - velocity[i]) /
                                        (1 + std::abs(velocity[i])));
          }
        }
  }
  check("state and covector chart round trips", roundtrip, 3e-14);
  check("transformed frame orthonormality", frame_error, 5e-13);
  check("canonical covector/vector pairing", pairing, 5e-14);
  check("axial Killing momentum invariance", killing, 3e-14);
  check("transformed launch nullness", null, 3e-14);
  check("Hamiltonian coordinate velocity transform", flow, 3e-14);
  bool rejected = false;
  try {
    (void)o::transform_state({0, 2, 0, 0, -1, 0, 1, 0}, o::Chart::north,
                             o::Chart::south);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require("singular opposite-pole transition rejected", rejected);
}
std::array<double, 3> cartesian(const o::Point &p) {
  const auto &s = p.state;
  const double q = s[2] * s[2] + s[3] * s[3];
  const double sign = p.chart == o::Chart::north ? 1 : -1;
  return {s[1] * 2 * s[2] / (1 + q), s[1] * 2 * s[3] / (1 + q),
          sign * s[1] * (1 - q) / (1 + q)};
}
void pole_crossings() {
  g::Parameters p;
  p.family = g::Family::flat;
  p.spin = 0;
  o::Settings c;
  c.maximum_affine = 3;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  double line_error = 0;
  for (auto patch : {o::Chart::north, o::Chart::south}) {
    const double sign = patch == o::Chart::north ? 1 : -1;
    const double theta = patch == o::Chart::north ? .1 : std::numbers::pi - .1;
    const double phi = .3;
    auto bl = o::launch(p, {0, 8, theta, phi}, {0, -sign, 0});
    auto initial = o::transform_state(bl, o::Chart::boyer_lindquist, patch);
    auto ray = o::trace_angular(p, initial, patch, c);
    require("flat ray crosses pole and reaches budget",
            ray.outcome == o::Outcome::affine_limit && ray.final.state[2] < 0);
    const std::array<double, 3> start{8 * std::sin(theta) * std::cos(phi),
                                      8 * std::sin(theta) * std::sin(phi),
                                      8 * std::cos(theta)};
    const std::array<double, 3> v{-sign * std::cos(theta) * std::cos(phi),
                                  -sign * std::cos(theta) * std::sin(phi),
                                  sign * std::sin(theta)};
    for (const auto &point : ray.trajectory) {
      const auto x = cartesian(point);
      for (int i = 0; i < 3; ++i)
        line_error = std::max(line_error,
                              std::abs(x[i] - start[i] - point.affine * v[i]));
    }
    // Exact-pole initial data, future p_t=-1 and purely angular unit velocity.
    o::State pole{0, 8, 0, 0, -1, 0, -16, 0};
    auto from_pole = o::trace_angular(p, pole, patch, c);
    require("exact pole can launch a ray",
            from_pole.outcome == o::Outcome::affine_limit);
    auto end = cartesian(from_pole.final);
    line_error = std::max({line_error, std::abs(end[0] - 3), std::abs(end[1]),
                           std::abs(end[2] - sign * 8)});
  }
  check("flat pole-crossing Cartesian line", line_error, 3e-10);

  p.family = g::Family::kerr;
  auto bl = o::launch(p, {0, 3, .4, .2}, {0, 1, 0});
  auto initial =
      o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
  c.maximum_affine = 24;
  c.maximum_step = .2;
  double orbit_error = 0, lz_error = 0, constraint = 0;
  for (double threshold : {1.2, 1.5, 1.9}) {
    c.angular_switch_q = threshold;
    auto ray = o::trace_angular(p, initial, o::Chart::north, c);
    require("Schwarzschild polar photon orbit crosses multiple patches",
            ray.outcome == o::Outcome::affine_limit && ray.chart_switches >= 2);
    for (const auto &point : ray.trajectory) {
      auto x = cartesian(point);
      const double theta = .4 + point.affine / 3;
      const std::array<double, 3> exact{3 * std::sin(theta) * std::cos(.2),
                                        3 * std::sin(theta) * std::sin(.2),
                                        3 * std::cos(theta)};
      for (int i = 0; i < 3; ++i)
        orbit_error = std::max(orbit_error, std::abs(x[i] - exact[i]));
      orbit_error = std::max(
          orbit_error, std::abs(point.state[0] + std::sqrt(3.) * point.affine));
    }
    lz_error = std::max(lz_error, ray.angular_momentum_error);
    constraint = std::max(constraint, ray.maximum_null_error);
  }
  check("analytic polar photon orbit under three switch thresholds",
        orbit_error, 2e-7);
  check("polar photon orbit axial Killing drift", lz_error, 2e-11);
  check("polar photon orbit null residual", constraint, 2e-10);
}
void overlap_rays() {
  double state_error = 0, lz = 0;
  for (auto family : {g::Family::kerr, g::Family::toy}) {
    g::Parameters p;
    p.family = family;
    o::Settings c;
    c.absolute_tolerance = c.relative_tolerance = 1e-12;
    c.maximum_affine = 2;
    auto bl = o::launch(p, {0, 4, 1.1, .1}, {-1, .1, -.5});
    auto baseline = o::trace(p, bl, c);
    auto angular = o::trace_angular(
        p, o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north),
        o::Chart::north, c);
    require("angular and BL rays reach same affine budget",
            baseline.outcome == o::Outcome::affine_limit &&
                angular.outcome == o::Outcome::affine_limit);
    state_error = std::max(
        state_error,
        difference(o::transform_state(angular.final.state, angular.final.chart,
                                      o::Chart::boyer_lindquist),
                   baseline.final.state));
    lz = std::max(lz, angular.angular_momentum_error);
  }
  check("angular and BL ray endpoint agreement", state_error, 2e-9);
  check("rotating ray axial Killing drift", lz, 2e-10);
}
double carter(const g::Parameters &p, const o::State &s) {
  const double q = s[2] * s[2] + s[3] * s[3];
  const double lz = -s[3] * s[6] + s[2] * s[7];
  return (1 + q) * (1 + q) * (s[6] * s[6] + s[7] * s[7]) / 4 - lz * lz -
         p.spin * p.spin * s[4] * s[4] * std::pow((1 - q) / (1 + q), 2);
}
o::Vector separated_kerr(const g::Parameters &p, const o::State &initial,
                         double duration, int steps) {
  // Independent separated Kerr equations for Lz=0, inward r and decreasing
  // signed theta. This branch crosses the north pole without a theta
  // reflection. No production metric, derivative algebra or DP tableau is used
  // in the RHS.
  const double E = -initial[4], a = p.spin,
               Q = initial[6] * initial[6] -
                   a * a * E * E * std::pow(std::cos(initial[2]), 2);
  auto derivative = [&](const o::Vector &x) {
    const double r = x[1], theta = x[2], A = r * r + a * a;
    const double delta = A - 2 * p.mass * r;
    const double sigma = r * r + a * a * std::pow(std::cos(theta), 2);
    const double P = E * A;
    const double R = P * P - delta * (a * a * E * E + Q);
    const double T = Q + a * a * E * E * std::pow(std::cos(theta), 2);
    if (!(R > 0 && T > 0))
      throw std::runtime_error("separated fixture reached a turning point");
    return o::Vector{
        -(A * P / delta - a * a * E * std::pow(std::sin(theta), 2)) / sigma,
        -std::sqrt(R) / sigma, -std::sqrt(T) / sigma,
        -(a * P / delta - a * E) / sigma};
  };
  o::Vector x{initial[0], initial[1], initial[2], initial[3]};
  const double h = duration / steps;
  for (int n = 0; n < steps; ++n) {
    auto a1 = derivative(x);
    o::Vector v{};
    for (int k = 0; k < 4; ++k)
      v[k] = x[k] + h * a1[k] / 2;
    auto a2 = derivative(v);
    for (int k = 0; k < 4; ++k)
      v[k] = x[k] + h * a2[k] / 2;
    auto a3 = derivative(v);
    for (int k = 0; k < 4; ++k)
      v[k] = x[k] + h * a3[k];
    auto a4 = derivative(v);
    for (int k = 0; k < 4; ++k)
      x[k] += h * (a1[k] + 2 * a2[k] + 2 * a3[k] + a4[k]) / 6;
  }
  return x;
}
void kerr_pole() {
  double reference_refinement = 0, error = 0, conserved = 0;
  for (double spin : {-.875, .875}) {
    g::Parameters p;
    p.family = g::Family::kerr;
    p.spin = spin;
    auto bl = o::launch(p, {0, 6, .1, .2}, {-.7, -1, 0});
    check("Kerr polar fixture has zero axial momentum", std::abs(bl[7]), 1e-15);
    auto initial =
        o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
    auto coarse = separated_kerr(p, bl, 2, 500),
         fine = separated_kerr(p, bl, 2, 1000);
    for (int k = 0; k < 4; ++k)
      reference_refinement =
          std::max(reference_refinement, std::abs(coarse[k] - fine[k]));
    o::Settings c;
    c.maximum_affine = 2;
    c.absolute_tolerance = c.relative_tolerance = 1e-12;
    auto ray = o::trace_angular(p, initial, o::Chart::north, c);
    require("Kerr ray traverses north pole",
            ray.outcome == o::Outcome::affine_limit && fine[2] < 0 &&
                ray.final.state[2] < 0);
    auto x = cartesian(ray.final);
    const std::array<double, 3> expected{
        fine[1] * std::sin(fine[2]) * std::cos(fine[3]),
        fine[1] * std::sin(fine[2]) * std::sin(fine[3]),
        fine[1] * std::cos(fine[2])};
    for (int k = 0; k < 3; ++k)
      error = std::max(error, std::abs(x[k] - expected[k]));
    error = std::max(error, std::abs(ray.final.state[0] - fine[0]));
    for (const auto &point : ray.trajectory)
      conserved = std::max(
          conserved, std::abs(carter(p, point.state) - carter(p, initial)) /
                         (1 + std::abs(carter(p, initial))));
    check("Kerr polar axial Killing drift", ray.angular_momentum_error, 2e-11);
  }
  check("independent separated Kerr RK4 refinement", reference_refinement,
        2e-11);
  check("Kerr pole-crossing vs separated reference", error, 3e-10);
  check("Kerr pole-crossing regular Carter invariant", conserved, 2e-10);
}
void angular_events() {
  g::Parameters p;
  o::Settings c;
  c.absolute_tolerance = c.relative_tolerance = 1e-11;
  c.maximum_affine = 30;
  auto bl = o::launch(p, {0, 4, std::numbers::pi / 2, 0}, {-1, 0, -.35});
  auto y = o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
  auto ray = o::trace_angular(p, y, o::Chart::north, c);
  std::size_t inner = 0, outer = 0;
  double join_error = 0;
  for (const auto &hit : ray.crossings) {
    if (hit.surface == "inner_shell_join") {
      ++inner;
      join_error = std::max(join_error, std::abs(hit.point.state[1] - p.r_in));
    }
    if (hit.surface == "outer_shell_join") {
      ++outer;
      join_error = std::max(join_error, std::abs(hit.point.state[1] - p.r_out));
    }
    require("angular event carries chart and enforced null residual",
            hit.point.chart != o::Chart::boyer_lindquist &&
                hit.point.null_error <= c.null_tolerance &&
                hit.point.null_error <= ray.maximum_null_error);
  }
  require("angular toy ray crosses shell twice and escapes",
          ray.outcome == o::Outcome::escaped && inner == 2 && outer == 2);
  check("angular one-sided join event location", join_error, 3e-9);
  p.family = g::Family::flat;
  p.spin = 0;
  c.disk = true;
  const double theta = std::numbers::pi / 3, dx = 4 - 8 * std::sin(theta),
               dz = -8 * std::cos(theta);
  bl = o::launch(p, {0, 8, theta, 0},
                 {dx * std::sin(theta) + dz * std::cos(theta),
                  dx * std::cos(theta) - dz * std::sin(theta), 0});
  for (auto patch : {o::Chart::north, o::Chart::south}) {
    ray = o::trace_angular(
        p, o::transform_state(bl, o::Chart::boyer_lindquist, patch), patch, c);
    require("angular equatorial disk event",
            ray.outcome == o::Outcome::emitted);
    check("angular disk analytic radius", std::abs(ray.final.state[1] - 4),
          3e-9);
    check("angular disk analytic affine time",
          std::abs(ray.final.affine - std::hypot(dx, dz)), 3e-9);
  }
  c.disk = false;
  p.family = g::Family::kerr;
  c.capture_radius = 2.1;
  bl = o::launch(p, {0, 8, 1.1, 0}, {-1, 0, 0});
  y = o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
  ray = o::trace_angular(p, y, o::Chart::north, c);
  require("angular Schwarzschild capture event",
          ray.outcome == o::Outcome::capture_surface);
  check("angular Schwarzschild radial analytic time",
        std::abs(ray.final.state[0] + 8 - ray.final.state[1] +
                 2 * std::log(6 / (ray.final.state[1] - 2))),
        2e-8);
  for (double bad : {1., 0., 2.1, std::numeric_limits<double>::quiet_NaN()}) {
    c.angular_switch_q = bad;
    require("invalid angular switch threshold rejected",
            o::trace_angular(p, y, o::Chart::north, c).outcome ==
                o::Outcome::invalid_input);
  }
}
int main(int argc, char **argv) {
  try {
    transformations();
    pole_crossings();
    overlap_rays();
    kerr_pole();
    angular_events();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    require("uncaught exception", false);
  }
  bool pass = true;
  for (const auto &c : checks)
    pass = pass && c["passed"].get<bool>();
  if (argc == 3 && std::string(argv[1]) == "--report") {
    const auto root = std::filesystem::path(GRAVASTAR_SOURCE_ROOT);
    auto output = u::result_path(root, argv[2]);
    if (!pass)
      output.replace_filename(output.stem().string() + ".failed.json");
    u::atomic_json(output, {{"schema_version", 1},
                            {"passed", pass},
                            {"checks", checks},
                            {"compiler", __VERSION__},
                            {"computational_source_sha256",
                             u::manifest(root)["computational_sha256"]}});
  } else if (argc != 1)
    return 2;
  std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
            << " angular optics checks\n";
  return pass ? 0 : 1;
}
