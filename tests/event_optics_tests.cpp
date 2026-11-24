// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/core_chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace {
u::Json checks = u::Json::array(), fixtures = u::Json::array();
constexpr double boundary = .75, launch_x = -.005;
void check(const std::string &name, double measured, double ceiling) {
  const bool passed = std::isfinite(measured) && measured <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", measured},
                    {"ceiling", ceiling},
                    {"passed", passed}});
  if (!passed)
    std::cerr << name << ": " << measured << " > " << ceiling << '\n';
}
void require(const std::string &name, bool passed) {
  check(name, passed ? 0 : 1, 0);
}
o::State initial(double impact, double start = launch_x) {
  // Exact Minkowski line (X,Y,Z)=(start+s,impact,0), P=(-1,-1,0,0).
  // Direct spherical covector pullback, independent of the production launch.
  const double r = std::hypot(start, impact);
  return {0,
          r,
          std::numbers::pi / 2,
          std::atan2(impact, start),
          -1,
          -start / r,
          0,
          impact};
}
o::Trace trace(const g::Parameters &p, double impact, o::Chart chart,
               const o::Settings &settings, double start = launch_x) {
  auto state = initial(impact, start);
  if (chart == o::Chart::boyer_lindquist)
    return o::trace(p, state, settings);
  return o::trace_angular(
      p, o::transform_state(state, o::Chart::boyer_lindquist, chart), chart,
      settings);
}
double crossing_time(double impact, double radius, bool outward) {
  const long double b = impact, r = radius;
  const double x = double(std::sqrt((r - b) * (r + b)));
  return -launch_x + (outward ? x : -x);
}
void diagnostics(const std::string &label, const o::Trace &ray) {
  double worst = 0;
  for (const auto &crossing : ray.crossings)
    worst = std::max(worst, crossing.point.null_error);
  require(label + " crossing diagnostics included",
          worst <= ray.maximum_null_error);
  check(label + " null residual", ray.maximum_null_error, 1e-9);
  check(label + " Killing drift",
        std::max(ray.energy_error, ray.angular_momentum_error), 1e-11);
}
void run() {
  g::Parameters p;
  p.family = g::Family::flat;
  p.mass = p.spin = p.cosmological_constant = 0;
  o::Settings c;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  c.initial_step = c.maximum_step = c.maximum_affine = .01;
  c.minimum_step = 1e-12;
  c.event_tolerance = 1e-11;
  c.inner_radius = boundary;
  c.null_tolerance = 1e-9;
  const double hit_b = boundary - 1e-8, miss_b = boundary + 1e-8;
  const double first = crossing_time(hit_b, boundary, false);
  const double second = crossing_time(hit_b, boundary, true);
  require("both exact endpoints outside despite interior excursion",
          std::hypot(launch_x, hit_b) > boundary &&
              std::hypot(launch_x + .01, hit_b) > boundary && first > 0 &&
              second < .01);
  for (auto chart :
       {o::Chart::boyer_lindquist, o::Chart::north, o::Chart::south}) {
    const std::string label(o::chart_name(chart));
    const auto hit = trace(p, hit_b, chart, c);
    require(label + " shallow excursion terminates at first boundary",
            hit.outcome == o::Outcome::domain_limit &&
                hit.detail == "inner_chart_limit" && hit.crossings.size() == 1);
    check(label + " analytic first crossing",
          std::abs(hit.final.affine - first), 2e-9);
    auto refined = c;
    refined.initial_step = refined.maximum_step = .0001;
    refined.event_tolerance = 1e-12;
    const auto fine = trace(p, hit_b, chart, refined);
    require(label + " small-step baseline also finds crossing",
            fine.outcome == o::Outcome::domain_limit);
    check(label + " coarse and small-step crossing agreement",
          std::abs(hit.final.affine - fine.final.affine), 2e-9);
    const auto miss = trace(p, miss_b, chart, c);
    require(label + " nearby miss is not an absorbing boundary",
            miss.outcome == o::Outcome::affine_limit && miss.crossings.empty());
    diagnostics(label + " shallow crossing", hit);

    // Near-tangency flag. The line's closest radius is exactly its impact,
    // so the surface value at the resolved stationary point is b - 0.75.
    // Margin: factor 1000 * max(1e-12, 1e-12 * 0.75) = 1e-9.
    require(label + " 1e-8 hit and miss are beyond the tangency margin",
            hit.tangencies.empty() && miss.tangencies.empty());
    for (double offset : {2e-10, -2e-10}) {
      const auto ray = trace(p, boundary + offset, chart, c);
      const std::string tag =
          label + " offset " + u::Json(offset).dump() + " terminal tangency";
      require(tag + " outcome", offset > 0
                                    ? ray.outcome == o::Outcome::affine_limit
                                    : ray.outcome == o::Outcome::domain_limit);
      // For the hit, the stationary point lies beyond the clipping first
      // root; it must still be reported, because it is the uncertain one.
      require(tag + " flagged once on the terminal surface",
              ray.tangencies.size() == 1 &&
                  ray.tangencies[0].surface == "inner_chart_limit" &&
                  ray.tangencies[0].terminal &&
                  ray.tangencies[0].margin == 1e-9);
      if (ray.tangencies.size() == 1)
        check(tag + " value is the analytic closest approach",
              std::abs(ray.tangencies[0].value - offset), 1e-12);
    }
    // Same 2e-10 dip, swept along the line so that for some starts the
    // first root and the closest approach fall in different accepted steps.
    // Then the trial holding the root has same-sign endpoint rates and no
    // stationary point, so the flag must come from the root itself.
    // (Ultrareview finding.) Straddles are recognised by a tangency recorded
    // at the root rather than at the analytic closest approach.
    {
      const double offset = -2e-10;
      std::size_t unflagged = 0, straddles = 0;
      double worst = 0;
      // A straddle needs the root within ~1.7e-5 of a step end: about one
      // placement in 300, so sweep 2000 starts 1.2e-5 apart.
      for (int k = 0; k < 2000; ++k) {
        const double start = -.006 - 1.2e-5 * k;
        auto sweep = c;
        sweep.maximum_affine = -start + .02;
        const auto ray = trace(p, boundary + offset, chart, sweep, start);
        if (ray.outcome != o::Outcome::domain_limit ||
            ray.tangencies.size() != 1 || !ray.tangencies[0].terminal) {
          ++unflagged;
          continue;
        }
        const auto &n = ray.tangencies[0];
        straddles += std::abs(n.point.affine - ray.final.affine) < 1e-12;
        worst = std::max(worst, std::abs(n.value - offset));
      }
      const std::string tag = label + " 2e-10 dip at 2000 step placements";
      check(tag + ": rays not flagged exactly once as terminal", unflagged, 0);
      require(tag + ": some placements straddle a step boundary", straddles > 0);
      check(tag + ": flagged closest approach", worst, 1e-12);
      fixtures.push_back({{"chart", label},
                          {"sweep_starts", 2000},
                          {"straddling_placements", straddles},
                          {"unflagged", unflagged}});
    }

    // M=Lambda=a=0 makes the toy family exactly Minkowski while retaining
    // its two diagnostic joins. The full step must publish both inner roots.
    auto shell = p;
    shell.family = g::Family::toy;
    shell.r_in = boundary;
    shell.r_out = .9;
    auto joined = c;
    joined.inner_radius = .1;
    const auto joins = trace(shell, hit_b, chart, joined);
    require(label + " both nonterminal join roots retained",
            joins.outcome == o::Outcome::affine_limit &&
                joins.crossings.size() == 2);
    if (joins.crossings.size() == 2) {
      require(label + " join events ordered and named",
              joins.crossings[0].surface == "inner_shell_join" &&
                  joins.crossings[1].surface == "inner_shell_join" &&
                  joins.crossings[0].point.affine <
                      joins.crossings[1].point.affine);
      check(label + " first nonterminal analytic root",
            std::abs(joins.crossings[0].point.affine - first), 2e-9);
      check(label + " second nonterminal analytic root",
            std::abs(joins.crossings[1].point.affine - second), 2e-9);
    }
    diagnostics(label + " two join roots", joins);
    const auto graze = trace(shell, boundary + 2e-10, chart, joined);
    require(label + " join graze flagged as nonterminal, no crossings",
            graze.outcome == o::Outcome::affine_limit &&
                graze.crossings.empty() && graze.tangencies.size() == 1 &&
                graze.tangencies[0].surface == "inner_shell_join" &&
                !graze.tangencies[0].terminal);
    fixtures.push_back({{"chart", label},
                        {"impact", hit_b},
                        {"outcome", o::outcome_name(hit.outcome)},
                        {"affine", hit.final.affine},
                        {"expected_affine", first},
                        {"small_step_affine", fine.final.affine},
                        {"miss_outcome", o::outcome_name(miss.outcome)},
                        {"join_count", joins.crossings.size()},
                        {"rhs_calls", hit.rhs_calls}});
    if (chart == o::Chart::boyer_lindquist)
      continue;
    auto core = joined;
    core.continue_core = true;
    core.core_entry_radius = boundary;
    core.core_exit_radius = .8;
    core.maximum_affine = .35;
    const auto continued = trace(p, hit_b, chart, core);
    require(label + " shallow core entry continues to budget",
            continued.outcome == o::Outcome::affine_limit);
    std::size_t entries = 0, exits = 0;
    for (const auto &crossing : continued.crossings) {
      if (crossing.surface == "core_entry") {
        ++entries;
        check(label + " core first entry time",
              std::abs(crossing.point.affine - first), 2e-9);
      } else if (crossing.surface == "core_exit") {
        ++exits;
        check(label + " core analytic exit time",
              std::abs(crossing.point.affine - crossing_time(hit_b, .8, true)),
              2e-9);
      }
    }
    require(label + " discarded angular trial tail publishes no extra entry",
            entries == 1 && exits == 1);
    const auto end =
        o::to_core(p, continued.final.state, continued.final.chart);
    check(label + " continued Cartesian line",
          std::max({std::abs(end[1] - (launch_x + .35)),
                    std::abs(end[2] - hit_b), std::abs(end[3])}),
          2e-10);
    diagnostics(label + " core excursion", continued);
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    const auto before = u::manifest(root);
    run();
    const auto after = u::manifest(root);
    require("computational sources unchanged during validation",
            before["computational_sha256"] == after["computational_sha256"]);
    bool passed = true;
    for (const auto &check : checks)
      passed = passed && check["passed"].get<bool>();
    if (argc == 3) {
      auto output = u::result_path(root, argv[2]);
      if (!passed)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(
          output,
          {{"schema_version", 1},
           {"passed", passed},
           {"compiler", __VERSION__},
           {"computational_source_sha256", after["computational_sha256"]},
           {"checks", checks},
           {"fixtures", fixtures},
           {"complete_P2_acceptance", false},
           {"scope", "Analytic shallow flat-space crossing pairs, nearby "
                     "misses and transactional core entry; no global "
                     "certification of arbitrary extrema or tangencies."}});
    }
    std::cout << (passed ? "PASS " : "FAIL ") << checks.size()
              << " event optics checks\n";
    return passed ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "event optics: " << e.what() << '\n';
    return 1;
  }
}
