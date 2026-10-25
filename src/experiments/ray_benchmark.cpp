// SPDX-License-Identifier: MIT
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
using J = u::Json;
struct Sample {
  g::Parameters parameters;
  o::State initial;
};
int main(int argc, char **argv) {
  try {
    if (argc != 3 || std::string(argv[1]) != "--output")
      throw std::invalid_argument("expected --output results/FILE.json");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    const auto output = u::result_path(root, argv[2]);
    const auto before = u::manifest(root);
    const auto executable = u::sha256(u::read_file("/proc/self/exe"));
    std::vector<Sample> samples;
    J dataset = J::array();
    for (auto family : {g::Family::flat, g::Family::kerr, g::Family::toy})
      for (double polar : {-.2, -.1, .1, .2})
        for (double azimuthal : {-.55, -.25, .25, .55}) {
          g::Parameters p;
          p.family = family;
          const double radius = family == g::Family::toy ? 2.5 : 4;
          auto state =
              o::launch(p, {0, radius, 1.1, .2}, {-1, polar, azimuthal});
          samples.push_back({p, state});
          dataset.push_back({{"family", g::family_name(family)},
                             {"M", p.mass},
                             {"a", p.spin},
                             {"Lambda", p.cosmological_constant},
                             {"r_in", p.r_in},
                             {"r_out", p.r_out},
                             {"initial_BL_state", state}});
        }
    o::Settings settings;
    settings.absolute_tolerance = settings.relative_tolerance = 1e-11;
    settings.maximum_affine = 1.5;
    settings.save_trajectory = false;
    auto evaluate = [&](int mode, const Sample &sample) {
      auto trace =
          mode == 0
              ? o::trace(sample.parameters, sample.initial, settings)
              : o::trace_angular(sample.parameters,
                                 o::transform_state(sample.initial,
                                                    o::Chart::boyer_lindquist,
                                                    o::Chart::north),
                                 o::Chart::north, settings);
      if (trace.outcome != o::Outcome::affine_limit)
        throw std::runtime_error(
            "benchmark fixture must reach affine budget: " + trace.detail);
      if (mode == 1) {
        trace.final.state = o::transform_state(
            trace.final.state, trace.final.chart, o::Chart::boyer_lindquist);
        trace.final.chart = o::Chart::boyer_lindquist;
      }
      return trace;
    };
    double agreement = 0, max_null = 0, max_lz = 0;
    std::array<std::size_t, 2> rhs_calls{}, accepted{}, rejected{};
    // Paired accuracy sweep also warms both paths before measured rounds.
    for (const auto &sample : samples) {
      const auto a = evaluate(0, sample), b = evaluate(1, sample);
      for (int i = 0; i < 8; ++i) {
        const double delta =
            i == 3 ? std::remainder(a.final.state[i] - b.final.state[i],
                                    2 * std::numbers::pi)
                   : a.final.state[i] - b.final.state[i];
        agreement = std::max(agreement, std::abs(delta) /
                                            (1 + std::abs(a.final.state[i])));
      }
      const std::array traces{a, b};
      for (int k = 0; k < 2; ++k) {
        rhs_calls[k] += traces[k].rhs_calls;
        accepted[k] += traces[k].accepted;
        rejected[k] += traces[k].rejected;
        max_null = std::max(max_null, traces[k].maximum_null_error);
        max_lz = std::max(max_lz, traces[k].angular_momentum_error);
      }
    }
    constexpr double accuracy_ceiling = 2e-8;
    if (!std::isfinite(agreement) || agreement > accuracy_ceiling ||
        max_null > settings.null_tolerance || max_lz > 1e-8)
      throw std::runtime_error("paired ray accuracy validation failed");
    constexpr int rounds = 7;
    std::array<std::vector<double>, 2> times;
    std::array<double, 2> checksums{};
    for (int round = 0; round < rounds; ++round)
      for (int slot = 0; slot < 2; ++slot) {
        const int mode = (round + slot) % 2;
        double checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (const auto &sample : samples) {
          const auto result = evaluate(mode, sample);
          for (int i = 0; i < 8; ++i)
            checksum += (i + 1) * result.final.state[i];
        }
        const double seconds = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        if (!std::isfinite(checksum) || seconds <= 0)
          throw std::runtime_error("invalid timing/checksum");
        checksums[mode] += checksum;
        times[mode].push_back(seconds / samples.size());
      }
    J kernels = J::object();
    std::array<double, 2> medians{};
    constexpr std::array names{"original_BL", "regular_angular"};
    for (int k = 0; k < 2; ++k) {
      auto sorted = times[k];
      std::sort(sorted.begin(), sorted.end());
      medians[k] = sorted[rounds / 2];
      kernels[names[k]] = {{"seconds_per_ray", times[k]},
                           {"median_seconds_per_ray", medians[k]},
                           {"checksum", checksums[k]},
                           {"warmup_rhs_calls", rhs_calls[k]},
                           {"warmup_accepted_steps", accepted[k]},
                           {"warmup_rejected_steps", rejected[k]}};
    }
    std::string cpu = "unavailable", line;
    std::ifstream info("/proc/cpuinfo");
    while (std::getline(info, line))
      if (line.starts_with("model name")) {
        cpu = line.substr(line.find(':') + 2);
        break;
      }
    const auto after = u::manifest(root);
    if (before["computational_sha256"] != after["computational_sha256"] ||
        executable != u::sha256(u::read_file("/proc/self/exe")))
      throw std::runtime_error(
          "source or executable changed during measurement");
    J report{{"schema_version", 1},
             {"passed", true},
             {"complete_P2_acceptance", false},
             {"compiler", __VERSION__},
             {"build_type", GRAVASTAR_BUILD_TYPE},
             {"cpu_model", cpu},
             {"computational_source_sha256", after["computational_sha256"]},
             {"executable_sha256", executable},
             {"samples", dataset},
             {"sample_sha256", u::sha256(dataset.dump())},
             {"rounds", rounds},
             {"kernels", kernels},
             {"settings",
              {{"absolute_tolerance", settings.absolute_tolerance},
               {"relative_tolerance", settings.relative_tolerance},
               {"initial_step", settings.initial_step},
               {"minimum_step", settings.minimum_step},
               {"maximum_step", settings.maximum_step},
               {"event_tolerance", settings.event_tolerance},
               {"null_tolerance", settings.null_tolerance},
               {"maximum_affine", settings.maximum_affine},
               {"inner_radius", settings.inner_radius},
               {"outer_radius", settings.outer_radius},
               {"polar_margin", settings.polar_margin},
               {"angular_switch_q", settings.angular_switch_q},
               {"maximum_attempts", settings.maximum_attempts},
               {"disk", false},
               {"save_trajectory", false}}},
             {"maximum_scaled_state_difference", agreement},
             {"accuracy_ceiling", accuracy_ceiling},
             {"maximum_null_residual", max_null},
             {"maximum_axial_momentum_error", max_lz},
             {"median_BL_over_angular_ratio", medians[0] / medians[1]},
             {"method",
              "48 fixed launches, three families, one paired accuracy/warmup "
              "sweep, seven measured rounds alternating order. Includes chart "
              "conversion, stepping, error control and events; excludes camera "
              "launch, image emission, file I/O and trajectory storage. No "
              "affinity/frequency control or timing pass threshold."},
             {"interpretation",
              "Bounded ray integration experiment where both charts are valid. "
              "Angular mode computes only first geometry derivatives and may "
              "choose different adaptive steps. Ratio does not isolate a "
              "single algebraic optimization and is not a whole-image or "
              "pole-continuation speedup."}};
    u::atomic_json(output, report);
    std::cout << "paired state error " << agreement
              << "; median BL/angular ratio " << medians[0] / medians[1]
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "ray benchmark: " << e.what() << '\n';
    return 1;
  }
}
