// SPDX-License-Identifier: GPL-3.0-or-later
//
// Broader toy-gravastar ray families traced with the production adaptive
// tracer and compared with independent references:
//   - off-equatorial rays that cross a pole and the core in one trace,
//     against the closed-form-Hamiltonian reference in toy_ray_reference.hpp.
//
// Neither reference includes production geometry, chart or integrator code.
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "toy_ray_reference.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <optional>
#include <random>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace {
namespace ref = gravastar::test_reference;
using ref::Real;

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

ref::Model model(const g::Parameters &p) {
  return {p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out};
}

// The reference Hamiltonian and the production inverse metric are separate
// transcriptions of MODEL.md. Compare them at random phase-space points in
// the core, shell and exterior, on and off the equator.
void hamiltonian_transcription(const g::Parameters &p) {
  const auto q = model(p);
  std::mt19937_64 random(20261124);
  std::uniform_real_distribution<double> radius(.3, 9), angle(.1, 3.04),
      momentum(-2, 2);
  double worst = 0;
  for (int n = 0; n < 300; ++n) {
    o::State y{0, radius(random), angle(random), 0, momentum(random),
               momentum(random), momentum(random), momentum(random)};
    if (n % 3 == 0) // concentrate a third of the samples in the shell
      y[1] = p.r_in + (p.r_out - p.r_in) * (n % 97) / 96.;
    const auto e = o::metric(p, y);
    double production = 0, scale = 0;
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b) {
        production += .5 * e.inverse[a][b] * y[a + 4] * y[b + 4];
        scale += std::abs(e.inverse[a][b] * y[a + 4] * y[b + 4]);
      }
    ref::Phase z{};
    for (int i = 0; i < 8; ++i)
      z[i] = y[i];
    worst = std::max(
        worst, double(std::abs(ref::hamiltonian(q, z) - production)) / scale);
  }
  check("reference Hamiltonian vs production inverse metric (300 points)",
        worst, 1e-14);
}

// Unit direction of a north/south stereographic point (ANGULAR_TRACING.md).
std::array<double, 3> patch_direction(const o::Point &point) {
  const double x = point.state[2], y = point.state[3], q = x * x + y * y;
  const double pole = point.chart == o::Chart::north ? 1 : -1;
  return {2 * x / (1 + q), 2 * y / (1 + q), pole * (1 - q) / (1 + q)};
}

struct Event {
  std::string name;
  double affine;
};
// Reference radial crossings expressed as the events the production tracer
// publishes: joins outside the Cartesian core patch, core_entry on inward
// r = core_entry_radius and core_exit on the next outward r = core_exit_radius.
std::vector<Event> expected_events(const ref::RayResult &r,
                                   const g::Parameters &p,
                                   const o::Settings &c) {
  auto events = r.events;
  std::sort(events.begin(), events.end(),
            [](const auto &a, const auto &b) { return a.affine < b.affine; });
  std::vector<Event> out;
  bool in_core_patch = false;
  for (const auto &e : events) {
    const double radius = double(e.radius);
    if (!in_core_patch && radius == p.r_out)
      out.push_back({"outer_shell_join", double(e.affine)});
    else if (!in_core_patch && radius == p.r_in)
      out.push_back({"inner_shell_join", double(e.affine)});
    else if (!in_core_patch && radius == c.core_entry_radius && e.inward) {
      out.push_back({"core_entry", double(e.affine)});
      in_core_patch = true;
    } else if (in_core_patch && radius == c.core_exit_radius && !e.inward) {
      out.push_back({"core_exit", double(e.affine)});
      in_core_patch = false;
    }
  }
  return out;
}

struct PoleFixture {
  const char *label;
  double theta, p_theta, p_phi;
  std::size_t poles;      // axis crossings in the reference
  bool cartesian_core;    // enters the Cartesian core patch
};

u::Json pole_and_core_family(const g::Parameters &p) {
  const auto q = model(p);
  o::Settings fine;
  fine.absolute_tolerance = fine.relative_tolerance = 1e-12;
  fine.maximum_step = .05;
  fine.event_tolerance = 1e-12;
  fine.null_tolerance = 1e-8;
  fine.outer_radius = 20;
  fine.maximum_affine = 200;
  fine.continue_core = true;
  auto coarse = fine;
  coarse.absolute_tolerance = coarse.relative_tolerance = 1e-10;
  coarse.maximum_step = .1;
  coarse.event_tolerance = 1e-10;
  coarse.null_tolerance = 1e-7;
  const std::vector<Real> radii{q.r_out, q.r_in, Real(fine.core_exit_radius),
                                Real(fine.core_entry_radius)};

  // Launch at r = 10 with p_t = -1. p_phi = 0 rays cross the axis; the scan
  // that chose them is recorded in docs/EVENT_COVERAGE.md.
  const PoleFixture fixtures[] = {
      {"pole inside Cartesian core patch", 1.2, .8, 0, 1, true},
      {"pole in core, angular chart", .8, 1.0, 0, 1, true},
      {"pole in core, no Cartesian patch", 1.0, 1.2, 0, 1, false},
      {"pole outside shell, then core", .5, 1.2, 0, 1, true},
      {"two poles, shell only", .3, 4.0, 0, 2, false},
      {"rotating ray, core without Cartesian patch", 1.0, 1.0, .5, 0, false},
      {"prograde rotating ray through Cartesian core", 1.0, .6, .5, 0, true},
      {"retrograde rotating ray through Cartesian core", 1.0, .8, -.3, 0,
       true}};
  u::Json rows = u::Json::array();
  for (const auto &f : fixtures) {
    const std::string label = f.label;
    const ref::Phase start = ref::null_launch(q, 10, f.theta, -1, f.p_theta,
                                              f.p_phi);
    const auto r1 = ref::trace(q, start, 1e-3L, 20, 200, radii);
    const auto r2 = ref::trace(q, start, 5e-4L, 20, 200, radii);
    require(label + " reference escapes", r1.escaped && r2.escaped);
    require(label + " reference pole crossings", r2.poles.size() == f.poles);
    require(label + " reference core-patch entry matches fixture",
            (r2.minimum_radius < fine.core_entry_radius) == f.cartesian_core);
    check(label + " reference null drift", double(r2.maximum_hamiltonian),
          1e-10);

    // Reference self-convergence on what the production trace is compared
    // with: event and escape affine, escape time and direction.
    double refinement = double(std::abs(r1.affine - r2.affine));
    refinement = std::max(refinement, double(std::abs(r1.final[0] - r2.final[0])));
    const auto n1 = ref::direction(r1.final), n2 = ref::direction(r2.final);
    for (int k = 0; k < 3; ++k)
      refinement = std::max(refinement, double(std::abs(n1[k] - n2[k])));
    if (r1.events.size() == r2.events.size())
      for (std::size_t i = 0; i < r1.events.size(); ++i)
        refinement = std::max(
            refinement,
            double(std::abs(r1.events[i].affine - r2.events[i].affine)));
    else
      refinement = 1;
    check(label + " reference step-halving change", refinement, 2e-9);

    o::State bl{};
    for (int i = 0; i < 8; ++i)
      bl[i] = double(start[i]);
    check(label + " production null residual of reference launch",
          o::null_residual(o::metric(p, bl), bl), 1e-14);
    const auto initial =
        o::transform_state(bl, o::Chart::boyer_lindquist, o::Chart::north);
    const auto expected = expected_events(r2, p, fine);

    u::Json row{{"label", label},
                {"theta", f.theta},
                {"p_theta", f.p_theta},
                {"p_phi", f.p_phi},
                {"reference_minimum_radius", double(r2.minimum_radius)},
                {"reference_escape_affine", double(r2.affine)},
                {"reference_step_halving_change", refinement}};
    u::Json poles = u::Json::array();
    for (const auto &pole : r2.poles)
      poles.push_back({{"affine", double(pole.affine)},
                       {"radius", double(pole.radius)}});
    row["reference_pole_crossings"] = poles;
    for (const auto *settings : {&coarse, &fine}) {
      const bool is_fine = settings == &fine;
      const std::string tag = label + (is_fine ? " fine" : " coarse");
      const auto t = o::trace_angular(p, initial, o::Chart::north, *settings);
      require(tag + " escapes", t.outcome == o::Outcome::escaped);
      check(tag + " null residual", t.maximum_null_error,
            settings->null_tolerance);
      check(tag + " energy and axial momentum drift",
            std::max(t.energy_error, t.angular_momentum_error),
            is_fine ? 1e-11 : 1e-9);

      std::vector<Event> got;
      for (const auto &c : t.crossings)
        if (c.surface != "escape")
          got.push_back({c.surface, c.point.affine});
      bool same = got.size() == expected.size();
      double event_error = 0;
      for (std::size_t i = 0; same && i < got.size(); ++i) {
        same = got[i].name == expected[i].name;
        event_error = std::max(event_error,
                               std::abs(got[i].affine - expected[i].affine));
      }
      require(tag + " event sequence matches reference", same);
      if (same)
        check(tag + " event affine vs reference", event_error,
              is_fine ? 5e-8 : 5e-7);

      double escape_error = 0;
      if (t.outcome == o::Outcome::escaped &&
          t.final.chart != o::Chart::core_cartesian) {
        const auto n = patch_direction(t.final);
        escape_error = std::max({std::abs(t.final.affine - double(r2.affine)),
                                 std::abs(t.final.state[0] - double(r2.final[0])),
                                 std::abs(t.final.state[5] - double(r2.final[5]))});
        for (int k = 0; k < 3; ++k)
          escape_error = std::max(escape_error, std::abs(n[k] - double(n2[k])));
      } else
        escape_error = 1;
      check(tag + " escape affine, time, p_r and direction vs reference",
            escape_error, is_fine ? 5e-8 : 2e-6);
      row[is_fine ? "fine" : "coarse"] = {
          {"outcome", o::outcome_name(t.outcome)},
          {"accepted_steps", t.accepted},
          {"chart_switches", t.chart_switches},
          {"event_count", got.size()},
          {"event_affine_error", event_error},
          {"escape_error", escape_error},
          {"maximum_null_error", t.maximum_null_error}};
    }
    // A ray with p_phi != 0 avoids the axis and stays above the BL inner
    // limit, so the bounded BL tracer (no core patch) can follow it too.
    if (f.p_phi != 0) {
      auto bounded = fine;
      bounded.continue_core = false;
      const auto t = o::trace(p, bl, bounded);
      const std::string tag = label + " BL chart";
      require(tag + " escapes", t.outcome == o::Outcome::escaped);
      std::vector<Event> want;
      for (const auto &e : r2.events)
        if (double(e.radius) == p.r_out || double(e.radius) == p.r_in)
          want.push_back({double(e.radius) == p.r_out ? "outer_shell_join"
                                                      : "inner_shell_join",
                          double(e.affine)});
      std::sort(want.begin(), want.end(), [](const auto &a, const auto &b) {
        return a.affine < b.affine;
      });
      std::vector<Event> got;
      for (const auto &c : t.crossings)
        if (c.surface != "escape")
          got.push_back({c.surface, c.point.affine});
      bool same = got.size() == want.size();
      double error = 0;
      for (std::size_t i = 0; same && i < got.size(); ++i) {
        same = got[i].name == want[i].name;
        error = std::max(error, std::abs(got[i].affine - want[i].affine));
      }
      require(tag + " join sequence matches reference", same);
      const auto &y = t.final.state;
      const std::array<double, 3> n{std::sin(y[2]) * std::cos(y[3]),
                                    std::sin(y[2]) * std::sin(y[3]),
                                    std::cos(y[2])};
      error = std::max({error, std::abs(t.final.affine - double(r2.affine)),
                        std::abs(y[0] - double(r2.final[0]))});
      for (int k = 0; k < 3; ++k)
        error = std::max(error, std::abs(n[k] - double(n2[k])));
      check(tag + " events and escape vs reference", error, 5e-8);
      row["bl_chart"] = {{"outcome", o::outcome_name(t.outcome)},
                         {"accepted_steps", t.accepted},
                         {"error", error}};
    }
    rows.push_back(row);
  }
  return rows;
}

// Equatorial rays that graze a shell join. At the graze value b_c the
// turning radius equals the join radius; b = b_c (1 -+ eps) turns a depth
// delta below or above it. Near the turn r(s) = r_t + r'' (s - s_t)^2 / 2, so
// the two join crossings are separated by chord = 2 sqrt(2 delta / |r''|),
// with r'' from the reference Hamiltonian; relative corrections are O(delta).
//
// Whether a graze is classified correctly is limited by the radial position
// error the trace has accumulated before the turn, not by the event search.
// Each level states the smallest |delta| it is required to classify; below
// that, cases are reported, and only the even crossing count is required.
// The chord also measures that position error: curvature * chord^2 / 8 is
// the depth the trace actually reached.
struct GrazeSettings {
  const char *name;
  o::Settings settings;
  double resolved_depth, depth_error;
};
u::Json graze_family(const g::Parameters &p) {
  const auto q = model(p);
  constexpr Real launch = 10;
  o::Settings fine;
  fine.absolute_tolerance = fine.relative_tolerance = 1e-12;
  fine.maximum_step = .02;
  fine.event_tolerance = 1e-12;
  fine.null_tolerance = 1e-8;
  fine.outer_radius = 20;
  fine.maximum_affine = 100;
  auto coarse = fine;
  coarse.absolute_tolerance = coarse.relative_tolerance = 1e-10;
  coarse.maximum_step = .05;
  coarse.event_tolerance = 1e-10;
  coarse.null_tolerance = 1e-7;
  // Measured position error at the inner join: ~1.2e-9 coarse, ~8e-12 fine.
  const GrazeSettings levels[] = {{"coarse", coarse, 5e-9, 5e-9},
                                  {"fine", fine, 5e-11, 3e-11}};

  struct Graze {
    const char *name, *surface;
    double radius, reach, miss;
  };
  const Graze grazes[] = {
      {"outer join prograde graze", "outer_shell_join", p.r_out, 2.5, 3.5},
      {"inner join prograde graze", "inner_shell_join", p.r_in, 1.0, 2.5},
      {"inner join retrograde graze", "inner_shell_join", p.r_in, -3.0, -4.5}};
  u::Json rows = u::Json::array();
  for (const auto &graze : grazes) {
    const Real bc = ref::threshold(q, graze.radius, graze.reach, graze.miss,
                                   launch);
    // The reference graze is a simple root of W at the join, not a double
    // root, so this family tests tangency rather than photon-orbit winding.
    u::Json row{{"name", graze.name}, {"graze_impact", double(bc)}};
    u::Json cases = u::Json::array();
    for (double eps : {1e-4, 1e-6, 1e-8, 1e-10, 1e-12})
      for (double side : {-1.0, 1.0}) {
        const Real b = bc * (1 + side * eps);
        const Real r_t = ref::turning_radius(q, b, launch);
        const Real depth = graze.radius - r_t; // > 0: ray enters
        ref::Phase turn{0, r_t, std::numbers::pi_v<Real> / 2, 0, -1, 0, 0, b};
        const Real rest = ref::hamiltonian(q, turn);
        turn[5] = 1;
        const Real d_over_sigma = 2 * (ref::hamiltonian(q, turn) - rest);
        turn[5] = 0;
        const Real curvature =
            std::abs(d_over_sigma * ref::derivative(q, turn)[5]);
        const double chord =
            depth > 0 ? double(2 * std::sqrt(2 * depth / curvature)) : 0;
        const auto start = ref::null_launch(q, launch, std::numbers::pi / 2,
                                            -1, 0, double(b));
        o::State bl{};
        for (int i = 0; i < 8; ++i)
          bl[i] = double(start[i]);
        const std::string tag = std::string(graze.name) + " eps=" +
                                u::Json(side * eps).dump();
        u::Json entry{{"relative_offset", side * eps},
                      {"impact", double(b)},
                      {"reference_depth", double(depth)},
                      {"reference_chord", chord}};
        for (const auto &level : levels) {
          const auto t = o::trace(p, bl, level.settings);
          std::vector<double> at;
          for (const auto &c : t.crossings)
            if (c.surface == graze.surface)
              at.push_back(c.point.affine);
          const std::string name = tag + " " + level.name;
          require(name + " escapes", t.outcome == o::Outcome::escaped);
          // An odd count would leave the ray labelled inside a region it
          // has left; that is never acceptable, resolved or not.
          require(name + " even join crossing count", at.size() % 2 == 0);
          const bool resolved = (depth > 0) == (at.size() == 2);
          const bool required = std::abs(double(depth)) >= level.resolved_depth;
          if (required)
            require(name + " join crossings match reference", resolved);
          const double measured_chord =
              at.size() == 2 ? at[1] - at[0] : 0;
          entry[level.name] = {{"join_crossings", at.size()},
                               {"chord", measured_chord},
                               {"accepted_steps", t.accepted},
                               {"matches_reference", resolved},
                               {"classification_required", required}};
          if (resolved && at.size() == 2) {
            const double reached =
                double(curvature) * measured_chord * measured_chord / 8;
            const double error = std::abs(reached - double(depth));
            entry[level.name]["chord_relative_error"] =
                std::abs(measured_chord - chord) / chord;
            entry[level.name]["implied_depth_error"] = error;
            // Above ~1e-6 the quadratic turn model's O(delta^2) error dominates.
            if (required && depth <= 1e-6)
              check(name + " depth implied by crossing chord", error,
                    level.depth_error);
          }
        }
        cases.push_back(entry);
      }
    row["cases"] = cases;
    rows.push_back(row);
  }
  return rows;
}

// Turning radius from a trace: the first accepted pair where p_r changes
// sign, with r = r_t + c p_r^2 eliminated between the two points.
std::optional<double> fitted_turn(const o::Trace &t) {
  for (std::size_t i = 1; i < t.trajectory.size(); ++i) {
    const auto &a = t.trajectory[i - 1].state, &b = t.trajectory[i].state;
    if (a[5] > 0 && b[5] <= 0) {
      const double qa = a[5] * a[5], qb = b[5] * b[5];
      return (a[1] * qb - b[1] * qa) / (qb - qa);
    }
  }
  return {};
}
double azimuth_sweep(const o::Trace &t, const o::State &initial) {
  return std::abs(t.final.state[3] - initial[3]);
}

// The Kerr prograde equatorial photon orbit of the exterior parameters lies
// at r = 1.6288, inside the toy shell [1.5, 1.8]. In Kerr, rays near its
// critical impact either fall in or wind; in the toy model the exterior that
// would hold the orbit is replaced, so the same rays must turn and leave.
// The independent equatorial potential (scan in docs/EVENT_COVERAGE.md) has
// no local minimum for any prograde b in [1.8, 3.1]: no toy photon orbit.
u::Json buried_photon_sphere(const g::Parameters &toy) {
  const auto q = model(toy);
  constexpr Real launch = 10;
  const Real a = q.a, M = q.M;
  const Real r_ph = 2 * M * (1 + std::cos(2 * std::acos(-a / M) / 3));
  const Real b_ph = -(r_ph * r_ph * r_ph - 3 * M * r_ph * r_ph + a * a * r_ph +
                      a * a * M) /
                    (a * (r_ph - M));
  // A pure-Kerr potential: the shell is moved far inside the domain used.
  const ref::Model kerr_model{M, a, 0, .01L, .02L};
  const Real dr = 1e-6L;
  check("Kerr photon orbit is a double root of the Kerr potential",
        double(std::max(std::abs(ref::W(kerr_model, r_ph, b_ph)),
                        std::abs(ref::W(kerr_model, r_ph + dr, b_ph) -
                                 ref::W(kerr_model, r_ph - dr, b_ph)) /
                            (2 * dr))),
        1e-9);
  require("Kerr prograde photon orbit lies inside the toy shell",
          r_ph > q.r_in && r_ph < q.r_out);

  g::Parameters kerr = toy;
  kerr.family = g::Family::kerr;
  o::Settings c;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  c.maximum_step = .02;
  c.event_tolerance = 1e-12;
  c.null_tolerance = 1e-8;
  c.outer_radius = 20;
  c.maximum_affine = 400;
  auto kerr_settings = c;
  kerr_settings.capture_radius = 1.55; // between r_+ = 1.484 and r_ph

  u::Json rows = u::Json::array();
  std::array<double, 2> toy_sweep{};
  std::vector<double> slopes;
  for (double eps : {1e-3, 1e-6}) {
    for (double side : {-1.0, 1.0}) {
      const Real b = b_ph * (1 + side * eps);
      const std::string tag = "b = b_ph(1" + std::string(side < 0 ? "-" : "+") +
                              u::Json(eps).dump() + ")";
      const auto start =
          ref::null_launch(q, launch, std::numbers::pi / 2, -1, 0, double(b));
      o::State bl{};
      for (int i = 0; i < 8; ++i)
        bl[i] = double(start[i]);
      u::Json row{{"relative_offset", side * eps}, {"impact", double(b)}};

      const auto t = o::trace(toy, bl, c);
      const Real r_t = ref::turning_radius(q, b, launch);
      std::size_t inner = 0, outer = 0;
      for (const auto &x : t.crossings) {
        inner += x.surface == "inner_shell_join";
        outer += x.surface == "outer_shell_join";
      }
      require("toy " + tag + " escapes", t.outcome == o::Outcome::escaped);
      require("toy " + tag + " turns in the shell",
              outer == 2 && inner == 0 && r_t > q.r_in && r_t < q.r_out);
      const auto fit = fitted_turn(t);
      require("toy " + tag + " has one radial turn", fit.has_value());
      if (fit)
        check("toy " + tag + " turning radius vs independent root",
              std::abs(*fit - double(r_t)), 1e-10);
      const double sweep = azimuth_sweep(t, bl);
      toy_sweep[side < 0 ? 0 : 1] = sweep;
      row["toy"] = {{"outcome", o::outcome_name(t.outcome)},
                    {"reference_turning_radius", double(r_t)},
                    {"fitted_turning_radius", fit.value_or(0)},
                    {"azimuth_sweep", sweep},
                    {"affine", t.final.affine}};

      // Kerr with a numerical capture proxy: below b_ph rays cross the orbit
      // and reach the proxy; above it they turn just outside r_ph and wind.
      // The launch radius is in the exact Kerr exterior of both models, so
      // the same state is null for Kerr too; that is checked here.
      const auto k = o::trace(kerr, bl, kerr_settings);
      check("Kerr " + tag + " launch null residual",
            o::null_residual(o::metric(kerr, bl), bl), 1e-14);
      u::Json kerr_row{{"outcome", o::outcome_name(k.outcome)},
                       {"azimuth_sweep", azimuth_sweep(k, bl)},
                       {"affine", k.final.affine}};
      if (side < 0)
        require("Kerr " + tag + " reaches the capture proxy",
                k.outcome == o::Outcome::capture_surface);
      else {
        require("Kerr " + tag + " escapes", k.outcome == o::Outcome::escaped);
        const Real kr = ref::turning_radius(kerr_model, b, launch);
        const auto kfit = fitted_turn(k);
        require("Kerr " + tag + " has one radial turn", kfit.has_value());
        if (kfit)
          check("Kerr " + tag + " turning radius vs independent root",
                std::abs(*kfit - double(kr)), 1e-10);
        kerr_row["reference_turning_radius"] = double(kr);
        kerr_row["fitted_turning_radius"] = kfit.value_or(0);
        require("Kerr " + tag + " winds more than the toy ray",
                azimuth_sweep(k, bl) > sweep + std::numbers::pi);
      }
      row["kerr"] = kerr_row;
      rows.push_back(row);
    }
    // Difference quotient of the toy azimuth sweep across b_ph.
    slopes.push_back((toy_sweep[1] - toy_sweep[0]) / double(2 * eps * b_ph));
  }
  // No critical behaviour in the toy model: the sweep is differentiable at
  // b_ph, so the quotients over 2e-3 and 2e-6 relative widths agree. Near a
  // photon orbit the sweep instead grows like -log|b - b_c|.
  check("toy azimuth sweep is smooth across b_ph (quotient change)",
        std::abs(slopes[1] - slopes[0]) / std::abs(slopes[1]), 1e-2);
  return {{"kerr_photon_orbit_radius", double(r_ph)},
          {"kerr_photon_orbit_impact", double(b_ph)},
          {"toy_sweep_quotients", slopes},
          {"rays", rows}};
}

u::Json run() {
  g::Parameters p; // toy preset: M=1, a=7/8, Lambda=1/2, r_in=1.5, r_out=1.8
  hamiltonian_transcription(p);
  return {{"pole_and_core", pole_and_core_family(p)},
          {"join_grazes", graze_family(p)},
          {"buried_photon_sphere", buried_photon_sphere(p)}};
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
        "Toy-preset ray families against independent references: pole and "
        "core crossings in one ray; equatorial grazes of both shell joins; "
        "prograde rays at the buried Kerr photon orbit, with Kerr contrast. "
        "Sampled fixtures, not a certificate for arbitrary rays; grazes "
        "shallower than the stated depth are reported, not classified; no "
        "GYOTO comparison.";
    if (argc == 3) {
      auto output = u::result_path(root, argv[2]);
      if (!pass)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(output, report);
    }
    std::cout << report["pole_and_core"].dump(2) << '\n'
              << (pass ? "PASS " : "FAIL ") << checks.size()
              << " ray family checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "ray families: " << error.what() << '\n';
    return 1;
  }
}
