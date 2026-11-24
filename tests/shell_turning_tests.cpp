// SPDX-License-Identifier: GPL-3.0-or-later
//
// Equatorial null rays of the toy gravastar that turn inside the transition
// shell or the de Sitter core, traced with the production adaptive tracer and
// compared with an independent turning radius.
//
// Ported from backup/astra-work tests/equatorial_turning_points_test.cpp
// (same four impact parameters), re-pointed at optics::trace. The old test
// compared against hard-coded radii of unrecorded origin; here the reference
// is computed from the MODEL.md formulas without production geometry code.
//
// The independent reference is tests/equatorial_reference.hpp.
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "equatorial_reference.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace {
using namespace gravastar::test_reference;
constexpr double launch_radius = 10;

Real threshold(const Model &q, Real r_target, Real reach, Real miss) {
  return gravastar::test_reference::threshold(q, r_target, reach, miss,
                                              launch_radius);
}

o::State equatorial_launch(const g::Parameters &p, double b) {
  o::State s{0, launch_radius, std::numbers::pi / 2, 0, -1, 0, 0, b};
  const auto &gi = o::metric(p, s).inverse;
  // Future-directed outgoing p_r: tracing into the past moves inward.
  s[5] = std::sqrt(-(gi[0][0] - 2 * gi[0][3] * b + gi[3][3] * b * b) / gi[1][1]);
  return s;
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

struct Fixture {
  double b;
  double astra_value; // hard-coded radius from the ported test, origin unknown
  bool enters_core;
};

u::Json run() {
  g::Parameters p; // toy preset: M=1, a=7/8, Lambda=1/2, r_in=1.5, r_out=1.8
  const Model q{p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out};
  o::Settings coarse;
  coarse.absolute_tolerance = coarse.relative_tolerance = 1e-10;
  coarse.maximum_step = .05;
  coarse.event_tolerance = 1e-10;
  coarse.outer_radius = 20;
  coarse.maximum_affine = 100;
  coarse.null_tolerance = 1e-7; // b=-3.7 peaks at 5.6e-8 at this tolerance
  auto fine = coarse;
  fine.absolute_tolerance = fine.relative_tolerance = 1e-12;
  fine.maximum_step = .02;
  fine.event_tolerance = 1e-12;
  fine.null_tolerance = 1e-8;

  const Fixture fixtures[] = {{+1.80, 1.438750388, true},
                              {+1.95, 1.551795042, false},
                              {-3.60, 1.490704364, true},
                              {-3.70, 1.511352187, false}};
  u::Json rows = u::Json::array();
  for (const auto &f : fixtures) {
    const std::string label = "b=" + u::Json(f.b).dump();
    const double exact = double(turning_radius(q, f.b, launch_radius));
    require(label + " reference region matches fixture label",
            (exact < p.r_in) == f.enters_core && exact < p.r_out);

    o::State initial{0, launch_radius, std::numbers::pi / 2, 0, -1, 0, 0, f.b};
    const auto start = o::metric(p, initial);
    const auto &gi = start.inverse;
    const double quad = gi[0][0] + 2 * gi[0][3] * -1 * f.b + gi[3][3] * f.b * f.b;
    // Future-directed outgoing p_r: tracing into the past moves inward.
    initial[5] = std::sqrt(-quad / gi[1][1]);
    check(label + " launch null residual", o::null_residual(start, initial),
          1e-14);

    u::Json row{{"b", f.b}, {"reference_turning_radius", exact},
                {"astra_hardcoded_radius", f.astra_value},
                {"astra_minus_reference", f.astra_value - exact}};
    double previous_error = 0;
    for (const auto *settings : {&coarse, &fine}) {
      const bool is_fine = settings == &fine;
      const std::string tag = label + (is_fine ? " fine" : " coarse");
      const auto t = o::trace(p, initial, *settings);
      require(tag + " escapes", t.outcome == o::Outcome::escaped);
      check(tag + " null residual", t.maximum_null_error,
            settings->null_tolerance);
      check(tag + " energy and axial momentum drift",
            std::max(t.energy_error, t.angular_momentum_error), 1e-12);

      // Equatorial symmetry: theta and p_theta stay at their initial values.
      double theta_drift = 0, sampled_min = launch_radius;
      std::size_t turn = 0;
      for (std::size_t i = 0; i < t.trajectory.size(); ++i) {
        const auto &s = t.trajectory[i].state;
        theta_drift = std::max({theta_drift, std::abs(s[2] - std::numbers::pi / 2),
                                std::abs(s[6])});
        sampled_min = std::min(sampled_min, s[1]);
        if (i && !turn && t.trajectory[i - 1].state[5] > 0 && s[5] <= 0)
          turn = i;
      }
      check(tag + " stays equatorial", theta_drift, 1e-12);
      require(tag + " radial momentum reverses once", turn > 0);
      if (!turn)
        continue;
      // Near the turn r = r_t + c p_r^2; eliminate c with the bracketing pair.
      const auto &s1 = t.trajectory[turn - 1].state, &s2 = t.trajectory[turn].state;
      const double q1 = s1[5] * s1[5], q2 = s2[5] * s2[5];
      const double fitted = (s1[1] * q2 - s2[1] * q1) / (q2 - q1);
      const double error = std::abs(fitted - exact);
      check(tag + " turning radius vs independent root", error,
            is_fine ? 1e-8 : 1e-6);
      check(tag + " no sample below the independent root",
            std::max(0.0, exact - sampled_min), 1e-9);
      if (is_fine)
        require(label + " turning radius does not worsen when refined",
                error <= std::max(1e-10, previous_error));
      previous_error = error;

      std::size_t inner = 0, outer = 0;
      for (const auto &c : t.crossings) {
        inner += c.surface == "inner_shell_join";
        outer += c.surface == "outer_shell_join";
      }
      require(tag + " shell join crossings match the turning region",
              outer == 2 && inner == (f.enters_core ? 2u : 0u));
      row[is_fine ? "fine" : "coarse"] = {
          {"fitted_turning_radius", fitted},
          {"error", error},
          {"sampled_minimum_radius", sampled_min},
          {"accepted_steps", t.accepted},
          {"inner_join_crossings", inner},
          {"outer_join_crossings", outer}};
    }
    rows.push_back(row);
  }

  // Critical impact parameters for entering the shell and the core, ported
  // from backup/astra-work critical_impact.{hpp,cpp} and its test. That module
  // used closed forms: the graze b = a + r^2/(a +- sqrt D(r)) at a join, and
  // the Kerr circular photon orbit for the retrograde shell threshold. Here
  // each threshold is found by bisection on the global "reaches" condition,
  // which also detects any barrier between the launch radius and the join.
  struct Threshold {
    const char *name;
    double r_target, reach, miss, astra_value;
  };
  const Threshold thresholds[] = {
      {"prograde shell entry", p.r_out, 2.5, 3.5, 3.0180173587},
      {"prograde core entry", p.r_in, 1.0, 2.5, 1.8759486035},
      {"retrograde core entry", p.r_in, -3.0, -4.5, -3.6442732108},
      {"retrograde shell entry", p.r_out, -6.0, -7.5, -6.7900713688}};
  o::Settings near = fine;
  near.maximum_affine = 400; // near-critical retrograde rays wind first
  u::Json critical = u::Json::array();
  for (const auto &t : thresholds) {
    const double b = double(threshold(q, t.r_target, t.reach, t.miss));
    check(std::string(t.name) + " vs old closed-form value",
          std::abs(b - t.astra_value), 1e-9);
    u::Json row{{"name", t.name}, {"reference_threshold", b},
                {"astra_value", t.astra_value},
                {"astra_minus_reference", t.astra_value - b}};
    const bool core = t.r_target == p.r_in;
    for (double side : {-1.0, 1.0}) {
      // side -1: |b| below threshold (ray reaches the target), +1: above.
      const double bb = b * (1 + side * 1e-4);
      const auto tr = o::trace(p, equatorial_launch(p, bb), near);
      std::size_t inner = 0, outer = 0;
      for (const auto &c : tr.crossings) {
        inner += c.surface == "inner_shell_join";
        outer += c.surface == "outer_shell_join";
      }
      const bool inward = side < 0;
      const std::size_t want_outer = core ? 2 : (inward ? 2 : 0);
      const std::size_t want_inner = core && inward ? 2 : 0;
      const std::string tag = std::string(t.name) + (inward ? " -1e-4" : " +1e-4");
      require(tag + " escapes", tr.outcome == o::Outcome::escaped);
      require(tag + " join crossings", outer == want_outer && inner == want_inner);
      check(tag + " null residual", tr.maximum_null_error, near.null_tolerance);
      row[inward ? "reaching_side" : "missing_side"] = {
          {"b", bb}, {"outcome", o::outcome_name(tr.outcome)},
          {"inner_join_crossings", inner}, {"outer_join_crossings", outer},
          {"accepted_steps", tr.accepted}};
    }
    critical.push_back(row);
  }
  return {{"fixtures", rows}, {"critical_impact", critical}};
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const auto root = std::filesystem::path(GRAVASTAR_SOURCE_ROOT);
    auto report = run();
    bool pass = true;
    for (const auto &entry : checks)
      pass = pass && entry["passed"].get<bool>();
    report["schema_version"] = 1;
    report["passed"] = pass;
    report["checks"] = checks;
    report["compiler"] = __VERSION__;
    report["computational_source_sha256"] =
        u::manifest(root)["computational_sha256"];
    report["complete_P2_acceptance"] = false;
    report["scope"] =
        "Four equatorial toy-preset rays turning in the shell or core, ported "
        "from backup/astra-work. Independent equatorial radial potential; no "
        "off-equatorial, near-tangent or GYOTO comparison.";
    if (argc == 3) {
      auto output = u::result_path(root, argv[2]);
      if (!pass)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(output, report);
    }
    std::cout << u::Json{{"fixtures", report["fixtures"]},
                         {"critical_impact", report["critical_impact"]}}
                     .dump(2)
              << '\n'
              << (pass ? "PASS " : "FAIL ") << checks.size()
              << " shell turning checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "shell turning: " << error.what() << '\n';
    return 1;
  }
}
