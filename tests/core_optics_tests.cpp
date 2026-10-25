// SPDX-License-Identifier: MIT
#include "gravastar/optics/core_chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
using J = u::Json;
J checks = J::array();
void check(const std::string &name, double measured, double ceiling) {
  const bool pass = std::isfinite(measured) && measured <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", measured},
                    {"ceiling", ceiling},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << measured << " > " << ceiling << '\n';
}
void require(const std::string &name, bool condition) {
  check(name, condition ? 0 : 1, 0);
}
o::Settings settings() {
  o::Settings c;
  c.continue_core = true;
  c.core_entry_radius = .75;
  c.core_exit_radius = 1.;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  c.event_tolerance = 1e-10;
  c.maximum_affine = 1.;
  return c;
}
o::State in_core(const g::Parameters &p, const o::Point &point) {
  if (point.chart == o::Chart::core_cartesian)
    return point.state;
  return o::to_core(p, point.state, point.chart);
}
// Independent physical Killing contractions, with explicit static-core time
// rescaling and rotation. Neither helper from core_chart.cpp is used here.
std::array<double, 2> invariants(const g::Parameters &p,
                                 const o::Point &point) {
  const auto &s = point.state;
  if (point.chart == o::Chart::core_cartesian) {
    const double k =
        p.family == g::Family::flat ? 0 : p.cosmological_constant / 3;
    const double lz = s[1] * s[6] - s[2] * s[5];
    return {(s[4] - k * p.spin * lz) / (1 + k * p.spin * p.spin), lz};
  }
  return {s[4], point.chart == o::Chart::boyer_lindquist
                    ? s[7]
                    : -s[3] * s[6] + s[2] * s[7]};
}
void conserved(const std::string &label, const g::Parameters &p,
               const o::Trace &ray, const o::Point &initial, double ceiling) {
  const auto expected = invariants(p, initial);
  double energy = 0, angular = 0, max_null = 0;
  auto inspect = [&](const o::Point &point) {
    const auto values = invariants(p, point);
    energy = std::max(energy, std::abs(values[0] - expected[0]) /
                                  (1 + std::abs(expected[0])));
    angular = std::max(angular, std::abs(values[1] - expected[1]) /
                                    (1 + std::abs(expected[1])));
    max_null = std::max(max_null, point.null_error);
  };
  for (const auto &point : ray.trajectory)
    inspect(point);
  for (const auto &hit : ray.crossings)
    inspect(hit.point);
  check(label + " physical stationary momentum", energy, ceiling);
  check(label + " physical axial momentum", angular, ceiling);
  check(label + " reported null constraint", ray.maximum_null_error, ceiling);
  require(label + " every published point included in null diagnostics",
          max_null <= ray.maximum_null_error);
  // Published trajectory/event maxima must be included; the implementation may
  // additionally inspect the other representation at each chart transition.
  require(label + " invariant diagnostics include published points",
          energy <= ray.energy_error + 1e-15 &&
              angular <= ray.angular_momentum_error + 1e-15);
}
std::array<int, 2> core_transitions(const o::Trace &ray, o::Chart initial) {
  std::array<int, 2> out{};
  bool core = initial == o::Chart::core_cartesian;
  for (const auto &point : ray.trajectory) {
    const bool next = point.chart == o::Chart::core_cartesian;
    out[0] += !core && next;
    out[1] += core && !next;
    core = next;
  }
  return out;
}
void flat_lines() {
  g::Parameters p{g::Family::flat};
  p.spin = 0;
  auto c = settings();
  c.maximum_affine = 1.2;
  const o::State origin_crossing{0, -.6, 0, 0, -1, -1, 0, 0};
  auto ray = o::trace_angular(p, origin_crossing, o::Chart::core_cartesian, c);
  require("flat Cartesian ray crosses the center",
          ray.outcome == o::Outcome::affine_limit &&
              ray.final.chart == o::Chart::core_cartesian &&
              ray.final.state[1] > 0);
  double error = 0;
  for (const auto &point : ray.trajectory) {
    const auto s = in_core(p, point);
    error = std::max({error, std::abs(s[0] + point.affine),
                      std::abs(s[1] + .6 - point.affine), std::abs(s[2]),
                      std::abs(s[3])});
  }
  check("flat center-crossing exact affine line", error, 2e-11);
  conserved("flat center", p, ray,
            {0, origin_crossing, 0, o::Chart::core_cartesian}, 2e-11);

  p.spin = .875;
  c.maximum_affine = 1.;
  const o::State disk_crossing{0, .4, .1, .5, -1, 0, 0, 1};
  ray = o::trace_angular(p, disk_crossing, o::Chart::core_cartesian, c);
  require("flat oblate focal disk is traversed",
          ray.outcome == o::Outcome::affine_limit && ray.final.state[3] < 0);
  error = 0;
  for (const auto &point : ray.trajectory) {
    const auto s = in_core(p, point);
    error = std::max({error, std::abs(s[0] + point.affine), std::abs(s[1] - .4),
                      std::abs(s[2] - .1), std::abs(s[3] - .5 + point.affine)});
  }
  check("flat oblate focal-disk exact affine line", error, 2e-11);
  conserved("flat focal disk", p, ray,
            {0, disk_crossing, 0, o::Chart::core_cartesian}, 2e-11);

  const o::State polar{0, 0, 0, 1.2, -1, 0, 0, 1};
  const auto angular = o::from_core(p, polar, o::Chart::north);
  c.maximum_affine = 2.5;
  ray = o::trace_angular(p, angular, o::Chart::north, c);
  const auto switches = core_transitions(ray, o::Chart::north);
  require("flat ray enters core once and exits once",
          ray.outcome == o::Outcome::affine_limit && switches[0] == 1 &&
              switches[1] == 1 && ray.chart_switches >= 2);
  require("flat core traversal exits in southern patch",
          ray.final.chart == o::Chart::south);
  error = 0;
  for (const auto &point : ray.trajectory) {
    const auto s = in_core(p, point);
    error = std::max({error, std::abs(s[0] + point.affine), std::abs(s[1]),
                      std::abs(s[2]), std::abs(s[3] - 1.2 + point.affine)});
  }
  check("flat entry-center-exit analytic ray", error, 3e-9);
  conserved("flat chart transitions", p, ray, {0, angular, 0, o::Chart::north},
            3e-10);
}
void de_sitter_lines() {
  g::Parameters p{g::Family::de_sitter};
  const double k = p.cosmological_constant / 3;
  auto c = settings();
  c.maximum_affine = .7;
  const o::State radial{0, 0, 0, 0, -1, -1, 0, 0};
  auto ray = o::trace_angular(p, radial, o::Chart::core_cartesian, c);
  require("de Sitter origin launch reaches budget",
          ray.outcome == o::Outcome::affine_limit);
  double radial_error = 0;
  for (const auto &point : ray.trajectory) {
    const auto s = in_core(p, point);
    const double exact_time =
        -std::atanh(std::sqrt(k) * point.affine) / std::sqrt(k);
    const double f = 1 - k * point.affine * point.affine;
    radial_error = std::max({radial_error, std::abs(s[0] - exact_time),
                             std::abs(s[1] - point.affine), std::abs(s[2]),
                             std::abs(s[3]), std::abs(s[5] + 1 / f)});
  }
  check("de Sitter origin analytic time position and momentum", radial_error,
        2e-10);
  conserved("de Sitter origin", p, ray,
            {0, radial, 0, o::Chart::core_cartesian}, 2e-10);

  // Null de Sitter geodesics have Cartesian embedding coordinates affine in s.
  // Choose an oblique line with nonzero Lz and derive its covector
  // independently.
  const std::array x{.3, -.2, 1.2}, v{.1, .15, -.9};
  double a = 0, b = 0, d = 0;
  for (int i = 0; i < 3; ++i) {
    a += x[i] * x[i];
    b += x[i] * v[i];
    d += v[i] * v[i];
  }
  const double f = 1 - k * a, E = std::sqrt(f * d + k * b * b);
  o::State initial{0, x[0], x[1], x[2], -E, 0, 0, 0};
  for (int i = 0; i < 3; ++i)
    initial[i + 5] = -(v[i] + k * x[i] * b / f);
  c.maximum_affine = 2.9;
  const auto angular_initial = o::from_core(p, initial, o::Chart::north);
  ray = o::trace_angular(p, angular_initial, o::Chart::north, c);
  require("nonradial de Sitter null ray reaches budget",
          ray.outcome == o::Outcome::affine_limit);
  const auto transitions = core_transitions(ray, o::Chart::north);
  require("oblique de Sitter reference crosses both core transitions",
          transitions[0] == 1 && transitions[1] == 1 &&
              ray.final.chart == o::Chart::south);
  const double beta = k * b, gamma = k * d,
               root = std::sqrt(beta * beta + f * gamma);
  double error = 0;
  for (const auto &point : ray.trajectory) {
    const auto s = in_core(p, point);
    const double exact_time =
        -E / root *
        (std::atanh((gamma * point.affine + beta) / root) -
         std::atanh(beta / root));
    error = std::max(error, std::abs(s[0] - exact_time));
    for (int i = 0; i < 3; ++i)
      error = std::max(error, std::abs(s[i + 1] - x[i] - point.affine * v[i]));
  }
  check("nonradial de Sitter independent Cartesian line and time integral",
        error, 2e-10);
  conserved("nonradial de Sitter", p, ray,
            {0, angular_initial, 0, o::Chart::north}, 2e-10);
}
double endpoint_difference(const o::Trace &a, const o::Trace &b) {
  auto canonical = [](const o::Point &point) {
    return point.chart == o::Chart::boyer_lindquist
               ? point.state
               : o::transform_state(point.state, point.chart,
                                    o::Chart::boyer_lindquist);
  };
  const auto x = canonical(a.final), y = canonical(b.final);
  double error = 0;
  for (int i = 0; i < 8; ++i) {
    const double delta = i == 3
                             ? std::remainder(x[i] - y[i], 2 * std::numbers::pi)
                             : x[i] - y[i];
    error = std::max(error, std::abs(delta) / (1 + std::abs(y[i])));
  }
  return error;
}
void toy_continuation() {
  g::Parameters p;
  const auto bl = o::launch(p, {0, 4, std::numbers::pi / 2, .2}, {-1, 0, 0});
  const auto initial =
      o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
  auto coarse = settings();
  coarse.absolute_tolerance = coarse.relative_tolerance = 1e-10;
  coarse.event_tolerance = 1e-9;
  coarse.maximum_affine = 40;
  auto bounded = coarse;
  bounded.continue_core = false;
  auto old = o::trace_angular(p, initial, o::Chart::north, bounded);
  require("toy fixture previously reached the inner radial boundary",
          old.outcome == o::Outcome::domain_limit &&
              old.detail == "inner_chart_limit");
  auto continued = o::trace_angular(p, initial, o::Chart::north, coarse);
  require("toy central continuation now escapes",
          continued.outcome == o::Outcome::escaped);
  auto transitions = core_transitions(continued, o::Chart::north);
  require("toy ray enters and leaves core", transitions[0] == 1 &&
                                                transitions[1] == 1 &&
                                                continued.chart_switches >= 2);
  conserved("toy core traversal", p, continued,
            {0, initial, 0, o::Chart::north}, 1e-6);
  auto fine = coarse;
  fine.absolute_tolerance = fine.relative_tolerance = 1e-12;
  fine.event_tolerance = 1e-11;
  fine.maximum_step = .05;
  fine.core_entry_radius = .55;
  fine.core_exit_radius = .9;
  auto refined = o::trace_angular(p, initial, o::Chart::north, fine);
  require("toy outcome stable under core threshold and tolerance refinement",
          refined.outcome == o::Outcome::escaped);
  if (continued.outcome == o::Outcome::escaped &&
      refined.outcome == o::Outcome::escaped) {
    check("toy continued endpoint convergence",
          endpoint_difference(continued, refined), 2e-6);
    check("toy continued affine convergence",
          std::abs(continued.final.affine - refined.final.affine), 2e-6);
  }
}
void invalid_settings() {
  g::Parameters p;
  const auto bl = o::launch(p, {0, 4, 1.1, .2}, {-1, 0, 0});
  const auto initial =
      o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
  auto c = settings();
  for (const auto radii :
       {std::array{0., 1.}, std::array{1., .75}, std::array{.75, 2.},
        std::array{.75, std::numeric_limits<double>::quiet_NaN()}}) {
    auto bad = c;
    bad.core_entry_radius = radii[0];
    bad.core_exit_radius = radii[1];
    require("invalid core overlap settings rejected",
            o::trace_angular(p, initial, o::Chart::north, bad).outcome ==
                o::Outcome::invalid_input);
  }
  g::Parameters kerr{g::Family::kerr};
  require("Kerr cannot silently acquire a de Sitter continuation",
          o::trace_angular(kerr, initial, o::Chart::north, c).outcome ==
              o::Outcome::invalid_input);
  c.continue_core = false;
  require("core initial data requires explicit continuation mode",
          o::trace_angular(p, {0, 0, 0, 0, -1, -1, 0, 0},
                           o::Chart::core_cartesian, c)
                  .outcome == o::Outcome::invalid_input);
}
int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    const auto before = u::manifest(root);
    flat_lines();
    de_sitter_lines();
    toy_continuation();
    invalid_settings();
    const auto after = u::manifest(root);
    require("computational sources unchanged during validation",
            before["computational_sha256"] == after["computational_sha256"]);
    bool pass = true;
    for (const auto &c : checks)
      pass = pass && c["passed"].get<bool>();
    if (argc == 3) {
      auto output = u::result_path(root, argv[2]);
      if (!pass)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(
          output,
          {{"schema_version", 1},
           {"passed", pass},
           {"checks", checks},
           {"compiler", __VERSION__},
           {"computational_source_sha256", after["computational_sha256"]},
           {"complete_P2_acceptance", false},
           {"scope", "Native static-core continuation and analytic ray checks; "
                     "no GYOTO comparison or full P2 acceptance."}});
    }
    std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
              << " core optics checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "core optics: " << e.what() << '\n';
    return 1;
  }
}
