// SPDX-License-Identifier: MIT
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace {
using Real = long double;
using ReferenceState = std::array<Real, 4>; // r, dr/ds, t, accumulated -phi
constexpr Real launch_radius = 10, escape_radius = 20, capture_radius = 2.2L;
constexpr Real maximum_affine = 150;
struct Reference {
  Real affine{};
  ReferenceState state{};
  bool escaped{};
};

// Independent Schwarzschild equations, M=E=1. Differentiating
// (dr/ds)^2=1-b^2/r^2+2*b^2/r^3 gives r''=b^2*(r-3)/r^4.
// Evolving radial velocity avoids a square-root branch choice at periapsis.
// No production geometry, Hamiltonian contraction or DP tableau is used.
ReferenceState derivative(const ReferenceState &y, Real b) {
  const Real r = y[0], r2 = r * r;
  if (!(r > 2))
    throw std::runtime_error("independent reference crossed the horizon");
  return {y[1], b * b * (r - 3) / (r2 * r2), -1 / (1 - 2 / r), b / r2};
}
ReferenceState rk4(const ReferenceState &y, Real h, Real b) {
  const auto k1 = derivative(y, b);
  ReferenceState stage{};
  for (int i = 0; i < 4; ++i)
    stage[i] = y[i] + h * k1[i] / 2;
  const auto k2 = derivative(stage, b);
  for (int i = 0; i < 4; ++i)
    stage[i] = y[i] + h * k2[i] / 2;
  const auto k3 = derivative(stage, b);
  for (int i = 0; i < 4; ++i)
    stage[i] = y[i] + h * k3[i];
  const auto k4 = derivative(stage, b);
  ReferenceState out{};
  for (int i = 0; i < 4; ++i)
    out[i] = y[i] + h * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]) / 6;
  return out;
}
Reference reference(double impact, Real h) {
  const Real b = impact, r = launch_radius;
  ReferenceState y{r, -std::sqrt(1 - b * b * (1 - 2 / r) / (r * r)), 0, 0};
  Real affine = 0;
  while (affine < maximum_affine) {
    h = std::min(h, maximum_affine - affine);
    auto next = rk4(y, h, b);
    if (next[0] <= capture_radius || next[0] >= escape_radius) {
      const bool escaped = next[0] >= escape_radius;
      const Real target = escaped ? escape_radius : capture_radius;
      Real lo = 0, hi = h;
      for (int iteration = 0; iteration < 80 && hi - lo > 1e-13L; ++iteration) {
        const Real mid = (lo + hi) / 2;
        auto candidate = rk4(y, mid, b);
        if (escaped ? candidate[0] >= target : candidate[0] <= target) {
          hi = mid;
          next = candidate;
        } else
          lo = mid;
      }
      return {affine + hi, next, escaped};
    }
    y = next;
    affine += h;
  }
  throw std::runtime_error("independent reference exhausted its affine budget");
}
Real turning_radius(double impact) {
  // The outer root of r^3-b^2*r+2*b^2=0 lies between 3 and r_launch
  // for these b>b_critical scattering fixtures. Exact tangent b=b_critical
  // is deliberately excluded from the classification tests.
  const Real b2 = Real(impact) * impact;
  Real lo = 3, hi = launch_radius;
  for (int n = 0; n < 100; ++n) {
    const Real mid = (lo + hi) / 2;
    if (mid * mid * mid - b2 * mid + 2 * b2 > 0)
      hi = mid;
    else
      lo = mid;
  }
  return (lo + hi) / 2;
}

u::Json checks = u::Json::array();
void check(const std::string &name, double measured, double ceiling) {
  const bool pass = std::isfinite(measured) && measured <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", measured},
                    {"ceiling", ceiling},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << measured << " > " << ceiling << '\n';
}
void require(const std::string &name, bool pass) {
  check(name, pass ? 0 : 1, 0);
}

u::Json run() {
  g::Parameters p;
  p.family = g::Family::kerr;
  p.mass = 1;
  p.spin = p.cosmological_constant = 0;
  o::Settings coarse;
  coarse.absolute_tolerance = coarse.relative_tolerance = 2e-11;
  coarse.maximum_step = .1;
  coarse.event_tolerance = 1e-10;
  coarse.maximum_affine = maximum_affine;
  coarse.outer_radius = escape_radius;
  coarse.capture_radius = double(capture_radius);
  coarse.null_tolerance = 1e-8;
  auto fine = coarse;
  fine.absolute_tolerance = fine.relative_tolerance = 2e-13;
  fine.maximum_step = .05;
  fine.event_tolerance = 1e-12;
  u::Json fixtures = u::Json::array();
  const double critical = 3 * std::sqrt(3.);
  for (int sign : {-1, 1}) {
    double prior_azimuth = 0, prior_dwell = 0;
    for (double epsilon : {1e-3, 1e-5, 1e-7}) {
      const double b = critical * (1 + sign * epsilon);
      const Real r = launch_radius, f = 1 - 2 / r;
      o::State initial{
          0, double(r), std::numbers::pi / 2,
          0, -1,        double(std::sqrt(1 - Real(b) * b * f / (r * r)) / f),
          0, b};
      const auto baseline = o::trace(p, initial, coarse);
      const auto refined = o::trace(p, initial, fine);
      const auto ref = reference(b, .002L);
      const auto ref_fine = reference(b, .001L);
      const auto expected =
          sign > 0 ? o::Outcome::escaped : o::Outcome::capture_surface;
      const std::string label =
          (sign > 0 ? "scatter " : "capture ") + u::Json(epsilon).dump();
      require(label + " expected fate at both integration tolerances",
              baseline.outcome == expected && refined.outcome == expected &&
                  ref.escaped == (sign > 0) && ref_fine.escaped == (sign > 0));
      const double reference_affine_delta =
          double(std::abs(ref.affine - ref_fine.affine));
      const double reference_azimuth_delta =
          double(std::abs(ref.state[3] - ref_fine.state[3]));
      check(label + " independent RK4 affine refinement",
            reference_affine_delta, 2e-6);
      check(label + " independent RK4 azimuth refinement",
            reference_azimuth_delta, 2e-7);
      const double affine_error =
          std::abs(refined.final.affine - double(ref_fine.affine));
      const double time_error =
          std::abs(refined.final.state[0] - double(ref_fine.state[2]));
      const double azimuth = -refined.final.state[3];
      const double azimuth_error =
          std::abs(azimuth - double(ref_fine.state[3]));
      const double baseline_azimuth_error =
          std::abs(-baseline.final.state[3] - double(ref_fine.state[3]));
      check(label + " affine endpoint vs independent reference", affine_error,
            2e-4);
      check(label + " coordinate time vs independent reference", time_error,
            8e-4);
      check(label + " unwrapped azimuth vs independent reference",
            azimuth_error, 3e-5);
      require(label + " production azimuth converges toward reference",
              azimuth_error <= std::max(3e-8, baseline_azimuth_error));
      check(label + " normalized null residual", refined.maximum_null_error,
            1e-8);
      check(label + " stationary and axial momentum drift",
            std::max(refined.energy_error, refined.angular_momentum_error),
            1e-12);
      double minimum_radius = double(launch_radius), dwell = 0;
      for (std::size_t i = 0; i < refined.trajectory.size(); ++i) {
        const auto &point = refined.trajectory[i];
        minimum_radius = std::min(minimum_radius, point.state[1]);
        if (i) {
          const auto &previous = refined.trajectory[i - 1];
          const double midpoint_radius =
              (point.state[1] + previous.state[1]) / 2;
          if (std::abs(midpoint_radius - 3) < .1)
            dwell += point.affine - previous.affine;
        }
      }
      require(label + " increased winding near critical impact",
              azimuth > prior_azimuth + 1);
      require(label + " increased sampled photon-region dwell",
              dwell > prior_dwell + 1);
      prior_azimuth = azimuth;
      prior_dwell = dwell;
      u::Json turning = nullptr;
      if (sign > 0) {
        const double exact_turning = double(turning_radius(b));
        check(label + " sampled periapsis vs independent cubic root",
              std::abs(minimum_radius - exact_turning), 2e-4);
        require(label + " radial momentum reverses through the turn",
                initial[5] > 0 && refined.final.state[5] < 0 &&
                    minimum_radius > 3);
        turning = {{"analytic_periapsis", exact_turning},
                   {"minimum_sampled_radius", minimum_radius}};
      }
      fixtures.push_back(
          {{"relative_critical_offset", sign * epsilon},
           {"impact_parameter", b},
           {"initial_state", initial},
           {"baseline_outcome", o::outcome_name(baseline.outcome)},
           {"refined_outcome", o::outcome_name(refined.outcome)},
           {"refined_detail", refined.detail},
           {"refined_affine", refined.final.affine},
           {"reference_affine", double(ref_fine.affine)},
           {"affine_error", affine_error},
           {"coordinate_time_error", time_error},
           {"unwrapped_azimuth", azimuth},
           {"reference_unwrapped_azimuth", double(ref_fine.state[3])},
           {"baseline_azimuth_error", baseline_azimuth_error},
           {"refined_azimuth_error", azimuth_error},
           {"reference_affine_refinement", reference_affine_delta},
           {"reference_azimuth_refinement", reference_azimuth_delta},
           {"sampled_affine_dwell_within_point_one_of_r3", dwell},
           {"maximum_null_residual", refined.maximum_null_error},
           {"periapsis", turning}});
    }
  }
  return {{"critical_impact_parameter", critical},
          {"mass", 1},
          {"energy", 1},
          {"launch_radius", double(launch_radius)},
          {"escape_radius", double(escape_radius)},
          {"capture_proxy_radius", double(capture_radius)},
          {"maximum_affine", double(maximum_affine)},
          {"reference", "Independent long-double RK4 of r,v,t,-phi; no "
                        "production metric or integrator"},
          {"reference_steps", {.002, .001}},
          {"baseline",
           {{"atol", 2e-11},
            {"rtol", 2e-11},
            {"maximum_step", .1},
            {"event_tolerance", 1e-10}}},
          {"refined",
           {{"atol", 2e-13},
            {"rtol", 2e-13},
            {"maximum_step", .05},
            {"event_tolerance", 1e-12}}},
          {"fixtures", fixtures}};
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const auto root = std::filesystem::path(GRAVASTAR_SOURCE_ROOT);
    const auto before = u::manifest(root);
    auto report = run();
    const auto after = u::manifest(root);
    require("computational sources unchanged during validation",
            before["computational_sha256"] == after["computational_sha256"]);
    bool pass = true;
    for (const auto &entry : checks)
      pass = pass && entry["passed"].get<bool>();
    report["schema_version"] = 1;
    report["passed"] = pass;
    report["checks"] = checks;
    report["compiler"] = __VERSION__;
    report["computational_source_sha256"] = after["computational_sha256"];
    report["complete_P2_acceptance"] = false;
    report["scope"] =
        "Six selected equatorial Schwarzschild near-critical rays, finite "
        "capture/escape boundaries and sampled periapsis/dwell. No global "
        "grazing-event certification or GYOTO comparison.";
    if (argc == 3) {
      auto output = u::result_path(root, argv[2]);
      if (!pass)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(output, report);
    }
    std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
              << " critical optics checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "critical optics: " << error.what() << '\n';
    return 1;
  }
}
