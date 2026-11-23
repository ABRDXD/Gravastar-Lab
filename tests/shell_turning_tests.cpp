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
// Independent reference. On the equator (cos theta = 0, sin theta = 1,
// T = 1) with p_t = -1, p_phi = b and p_theta = 0, the null condition at a
// radial turning point p_r = 0 reduces, after multiplying by the positive
// factor Sigma X^2 times the t-phi block determinant sign, to
//   W(r) = (r^2 + a^2 - a b)^2 - D(r) (b - a)^2 = 0,
//   D(r) = (r^2 + a^2)(1 - L r^2 / 3) - 2 m r,
// where (m, L) = (0, Lambda) in the core, (M S(u), Lambda (1 - S(u))) in the
// shell with S(u) = 35u^4 - 84u^5 + 70u^6 - 20u^7, and (M, 0) outside.
// This is the Kerr equatorial radial potential with r-dependent m and L.
// The incoming ray turns at the largest root of W below the launch radius.
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace {
using Real = long double;
constexpr double launch_radius = 10;

struct Model {
  Real M, a, Lambda, r_in, r_out;
};

Real W(const Model &q, Real r, Real b) {
  Real m = q.M, L = 0;
  if (r <= q.r_in) {
    m = 0;
    L = q.Lambda;
  } else if (r < q.r_out) {
    const Real x = (r - q.r_in) / (q.r_out - q.r_in);
    const Real x2 = x * x, x4 = x2 * x2;
    const Real S = x4 * (35 - 84 * x + 70 * x2 - 20 * x2 * x);
    m = q.M * S;
    L = q.Lambda * (1 - S);
  }
  const Real B = r * r + q.a * q.a;
  const Real D = B * (1 - L * r * r / 3) - 2 * m * r;
  const Real A = B - q.a * b;
  return A * A - D * (b - q.a) * (b - q.a);
}

// Largest root of W in (r_floor, r_start): scan down until W < 0, then bisect.
Real turning_radius(const Model &q, Real b) {
  Real hi = launch_radius;
  if (!(W(q, hi, b) > 0))
    throw std::runtime_error("launch radius is not in an allowed region");
  const Real step = 1e-4L;
  Real lo = hi - step;
  while (W(q, lo, b) > 0) {
    hi = lo;
    lo -= step;
    if (lo < .1L)
      throw std::runtime_error("no turning point above r = 0.1");
  }
  for (int i = 0; i < 200 && hi - lo > 1e-18L; ++i) {
    const Real mid = (lo + hi) / 2;
    (W(q, mid, b) > 0 ? hi : lo) = mid;
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
    const double exact = double(turning_radius(q, f.b));
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
  return {{"fixtures", rows}};
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
    std::cout << report["fixtures"].dump(2) << '\n'
              << (pass ? "PASS " : "FAIL ") << checks.size()
              << " shell turning checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "shell turning: " << error.what() << '\n';
    return 1;
  }
}
