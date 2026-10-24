// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <cmath>
#include <iostream>
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
int main(int argc, char **argv) {
  try {
    transformations();
    pole_crossings();
    overlap_rays();
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
