// SPDX-License-Identifier: MIT
#include "gravastar/geometry/angular.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <sys/utsname.h>

namespace g = gravastar::geometry;
namespace u = gravastar::tools;
using J = u::Json;
struct Sample {
  g::Parameters p;
  double r, x, y, theta;
};
template <class T> T require(std::variant<T, g::Error> result) {
  if (auto e = std::get_if<g::Error>(&result))
    throw std::runtime_error(e->message);
  return std::get<T>(result);
}
template <class T> double consume(const T &a) {
  double sum = a.determinant;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      sum += a.metric[i][j] + .5 * a.inverse[i][j];
      for (int k = 1; k < 4; ++k)
        sum += .25 * a.dg[k][i][j];
    }
  return sum;
}
template <class A> double difference(const A &a, const A &b) {
  double maximum = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if constexpr (std::is_same_v<typename A::value_type, double>)
      maximum = std::max(maximum, std::abs(a[i] - b[i]) / (1 + std::abs(b[i])));
    else
      maximum = std::max(maximum, difference(a[i], b[i]));
  }
  return maximum;
}
int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--output"))
      throw std::invalid_argument("expected [--output results/FILE.json]");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    const auto output =
        argc == 3 ? u::result_path(root, argv[2]) : std::filesystem::path{};
    const auto before = u::manifest(root);
    const auto executable = u::sha256(u::read_file("/proc/self/exe"));
    constexpr int rounds = 7, repetitions = 64;
    std::vector<Sample> samples;
    J dataset = J::array();
    for (auto family : {g::Family::toy, g::Family::kerr, g::Family::de_sitter,
                        g::Family::flat})
      for (int n = 0; n < 64; ++n) {
        const double r = 1.5 + .3 * n / 63,
                     rho = .03 + 1.47 * ((n * 23) % 64) / 63,
                     phi = 2 * std::numbers::pi * ((n * 41) % 64) / 64;
        samples.push_back({{family},
                           r,
                           rho * std::cos(phi),
                           rho * std::sin(phi),
                           2 * std::atan(rho)});
        const auto &s = samples.back();
        dataset.push_back({{"family", g::family_name(family)},
                           {"r", s.r},
                           {"x", s.x},
                           {"y", s.y},
                           {"theta", s.theta}});
      }
    double agreement = 0;
    for (const auto &s : samples) {
      auto first = require(g::evaluate_angular(s.p, s.r, s.x, s.y));
      auto full = require(g::evaluate_angular_curvature(s.p, s.r, s.x, s.y));
      agreement = std::max({agreement, difference(first.metric, full.metric),
                            difference(first.inverse, full.inverse),
                            difference(first.dg, full.dg)});
    }
    if (!std::isfinite(agreement) || agreement > 2e-13)
      throw std::runtime_error("first/full geometry disagreement");
    auto evaluate = [&](int kernel, const Sample &s) {
      if (kernel == 0)
        return consume(require(g::evaluate_angular(s.p, s.r, s.x, s.y)));
      if (kernel == 1)
        return consume(
            require(g::evaluate_angular_curvature(s.p, s.r, s.x, s.y)));
      return consume(require(g::evaluate(s.p, s.r, s.theta)));
    };
    std::array<std::vector<double>, 3> times;
    std::array<double, 3> checksums{}, warmup{};
    for (int kernel = 0; kernel < 3; ++kernel)
      for (const auto &s : samples)
        warmup[kernel] += evaluate(kernel, s);
    for (int round = 0; round < rounds; ++round)
      for (int slot = 0; slot < 3; ++slot) {
        const int kernel = (slot + round) % 3;
        double sum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int repeat = 0; repeat < repetitions; ++repeat)
          for (const auto &s : samples)
            sum += evaluate(kernel, s);
        const double seconds = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        if (!std::isfinite(sum) || seconds <= 0)
          throw std::runtime_error("invalid benchmark timing or checksum");
        times[kernel].push_back(seconds * 1e9 / (repetitions * samples.size()));
        checksums[kernel] += sum;
      }
    std::array<double, 3> medians{};
    J kernels = J::object();
    constexpr std::array names{"angular_first_derivatives",
                               "angular_with_curvature",
                               "original_BL_with_curvature"};
    for (int k = 0; k < 3; ++k) {
      auto sorted = times[k];
      std::sort(sorted.begin(), sorted.end());
      medians[k] = sorted[rounds / 2];
      kernels[names[k]] = {{"nanoseconds_per_evaluation", times[k]},
                           {"median_ns", medians[k]},
                           {"min_ns", sorted.front()},
                           {"max_ns", sorted.back()},
                           {"checksum", checksums[k]},
                           {"warmup_checksum", warmup[k]}};
    }
    std::string cpu = "unavailable";
    std::ifstream info("/proc/cpuinfo");
    std::string line;
    while (std::getline(info, line))
      if (line.starts_with("model name")) {
        cpu = line.substr(line.find(':') + 2);
        break;
      }
    struct utsname host{};
    const bool has_host = uname(&host) == 0;
    const auto after = u::manifest(root);
    if (before.at("computational_sha256") != after.at("computational_sha256") ||
        executable != u::sha256(u::read_file("/proc/self/exe")))
      throw std::runtime_error(
          "sources or executable changed during benchmark");
    J report{
        {"schema_version", 1},
        {"passed", true},
        {"complete_P2_acceptance", false},
        {"compiler", __VERSION__},
        {"build_type", GRAVASTAR_BUILD_TYPE},
        {"cpu_model", cpu},
        {"system", has_host ? std::string(host.sysname) + " " + host.release +
                                  " " + host.machine
                            : "unavailable"},
        {"computational_source_sha256", after.at("computational_sha256")},
        {"executable_sha256", executable},
        {"samples", dataset},
        {"sample_sha256", u::sha256(dataset.dump())},
        {"rounds", rounds},
        {"repetitions_per_round", repetitions},
        {"evaluations_per_kernel", rounds * repetitions * samples.size()},
        {"kernels", kernels},
        {"first_vs_full_max_scaled_residual", agreement},
        {"accuracy_ceiling", 2e-13},
        {"median_speed_ratio_full_angular_over_first", medians[1] / medians[0]},
        {"median_speed_ratio_original_BL_over_first", medians[2] / medians[0]},
        {"method", "One warmup sweep per kernel; seven measured rounds with "
                   "cyclic kernel order; same 256 physical points, four metric "
                   "families, 64 repeats. Every metric, inverse and spatial "
                   "first-derivative component consumed. Single process, no "
                   "affinity/frequency control; no timing pass threshold."},
        {"interpretation",
         "Kernel microbenchmark only. First vs full angular isolates omitted "
         "Hessian/curvature work in the same chart. Original BL performs "
         "additional P1 outputs and uses different coordinates; it is the "
         "current tracer cost baseline, not an equal-output algorithm "
         "comparison. No ray or end-to-end speedup is claimed."}};
    if (argc == 3)
      u::atomic_json(output, report);
    std::cout << kernels.dump(2) << "\nFull angular / first median ratio: "
              << medians[1] / medians[0]
              << "\nOriginal BL / first median ratio: "
              << medians[2] / medians[0] << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "angular benchmark: " << e.what() << '\n';
    return 1;
  }
}
