// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/sampling.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <chrono>
#include <iostream>

namespace s = gravastar::optics::sampling;
namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
J number(const std::optional<double> &x) { return x ? J(*x) : J(nullptr); }
fs::path benchmark(const fs::path &root, const fs::path &path) {
  auto p = fs::canonical(path.is_absolute() ? path : root / path);
  auto relative = p.lexically_relative(fs::canonical(root / "benchmarks"));
  if (relative.empty() || *relative.begin() == "..")
    throw std::invalid_argument("input must be inside benchmarks/");
  return p;
}
int integer(const J &j, int lower, int upper) {
  if (!j.is_number_integer() || j < lower || j > upper)
    throw std::invalid_argument("integer outside permitted bounds");
  return j.get<int>();
}
double fraction(const J &j, bool positive) {
  if (!j.is_number())
    throw std::invalid_argument("expected a numeric criterion");
  double value = j.get<double>();
  if (!std::isfinite(value) || value < 0 || value > 1 ||
      (positive && value == 0))
    throw std::invalid_argument("criterion must be finite and within [0,1]");
  return value;
}
s::Grid grid(const J &report, int width, int height, int scale) {
  if (report.at("schema_version") != 1 ||
      report.at("execution_passed") != true ||
      report.at("source_unchanged") != true ||
      report.at("resolution_scale") != scale ||
      report.at("pixels").size() !=
          static_cast<std::size_t>(width * height * scale * scale))
    throw std::runtime_error("incomplete or incompatible trace report");
  std::vector<s::Sample> samples;
  for (const auto &pixel : report.at("pixels")) {
    const int i = static_cast<int>(samples.size());
    if (pixel.at("x") != i % (width * scale) ||
        pixel.at("y") != i / (width * scale))
      throw std::runtime_error("unordered or duplicate trace pixels");
    const auto outcome = pixel.at("outcome").get<std::string>();
    s::Class classification;
    if (outcome == "emitted")
      classification = s::Class::emitted;
    else if (outcome == "escaped")
      classification = s::Class::escaped;
    else if (outcome == "capture_surface")
      classification = s::Class::captured;
    else if (outcome == "domain_limit" || outcome == "affine_limit" ||
             outcome == "step_limit")
      classification = s::Class::unresolved;
    else
      throw std::runtime_error("failed or unknown trace outcome");
    auto optional = [&](const char *key) -> std::optional<double> {
      return pixel.at(key).is_null()
                 ? std::nullopt
                 : std::optional(pixel.at(key).get<double>());
    };
    samples.push_back(
        {classification, optional("linear_intensity"), optional("alpha")});
  }
  return s::aggregate(width, height, scale, samples);
}
int main(int argc, char **argv) {
  try {
    fs::path root = GRAVASTAR_SOURCE_ROOT,
             config = "benchmarks/sampling_v1.json",
             scene = "benchmarks/optics_v1.json",
             output = "results/p2_sampling.json",
             executable = fs::canonical("/proc/self/exe"),
             tracer = executable.parent_path() / "gravastar_trace";
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        std::cout << "gravastar_sampling [--root PATH] [--trace EXECUTABLE] "
                     "[--scene benchmarks/optics_v1.json] [--config "
                     "benchmarks/sampling_v1.json] "
                     "[--output results/FILE.json]\n";
        return 0;
      }
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      const std::string value = argv[++i];
      if (option == "--root")
        root = value;
      else if (option == "--trace")
        tracer = value;
      else if (option == "--scene")
        scene = value;
      else if (option == "--config")
        config = value;
      else if (option == "--output")
        output = value;
      else
        throw std::invalid_argument("unknown option " + option);
    }
    root = fs::canonical(root);
    config = benchmark(root, config);
    scene = benchmark(root, scene);
    output = u::result_path(root, output);
    tracer = fs::canonical(tracer);
    const auto config_bytes = u::read_file(config),
               scene_bytes = u::read_file(scene);
    const auto policy = J::parse(config_bytes),
               configuration = J::parse(scene_bytes);
    if (policy.at("schema_version") != 1 ||
        configuration.at("schema_version") != 1)
      throw std::invalid_argument("unsupported sampling or scene schema");
    std::vector<int> scales;
    if (!policy.at("scales").is_array())
      throw std::invalid_argument("scales must be an array");
    for (const auto &value : policy.at("scales")) {
      int n = integer(value, 1, 8);
      if ((!scales.empty() && n <= scales.back()) || (scales.empty() && n != 1))
        throw std::invalid_argument("scales must increase strictly from one");
      scales.push_back(n);
    }
    if (scales.size() < 2)
      throw std::invalid_argument("at least two sampling levels required");
    const int width = integer(configuration.at("image").at("width"), 1,
                              128 / scales.back()),
              height = integer(configuration.at("image").at("height"), 1,
                               128 / scales.back());
    const double intensity_ceiling =
                     fraction(policy.at("relative_l1_intensity_ceiling"), true),
                 classification_ceiling = fraction(
                     policy.at("mean_classification_tv_ceiling"), true),
                 coverage_floor = fraction(
                     policy.at("minimum_common_resolved_fraction"), true);
    auto before = u::manifest(root);
    const auto tracer_hash = u::sha256(u::read_file(tracer));
    const auto start = std::chrono::steady_clock::now();
    // Separate run directories preserve the raw evidence for earlier reports.
    const auto local = u::result_path(root, "results/local/sampling");
    fs::create_directories(local);
    std::string pattern = (local / "run-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    if (!mkdtemp(name.data()))
      throw std::runtime_error("cannot create sampling run directory");
    const fs::path run_directory = name.data();
    std::vector<s::Grid> levels;
    J reports = J::array();
    bool all_resolved = true;
    for (int scale : scales) {
      auto path = run_directory / ("scale-" + std::to_string(scale) + ".json");
      if (u::run({tracer.string(), "image", "--root", root.string(), "--config",
                  scene.string(), "--output", path.string(),
                  "--resolution-scale", std::to_string(scale)}) != 0)
        throw std::runtime_error(
            "ray batch failed; previous sampling report preserved");
      const auto bytes = u::read_file(path);
      const auto raw = J::parse(bytes);
      auto expected = configuration;
      expected["image"]["width"] = width * scale;
      expected["image"]["height"] = height * scale;
      if (raw.at("configuration") != expected ||
          raw.at("input_sha256") != u::sha256(scene_bytes) ||
          raw.at("computational_source_sha256") !=
              before.at("computational_sha256") ||
          raw.at("executable_sha256") != tracer_hash)
        throw std::runtime_error(
            "ray batch configuration or provenance mismatch");
      levels.push_back(grid(raw, width, height, scale));
      J pixels = J::array();
      std::array<double, 4> fractions{};
      std::size_t unresolved_pixels = 0;
      double known_mean = 0;
      for (const auto &p : levels.back().pixels) {
        pixels.push_back({{"intensity", number(p.intensity)},
                          {"alpha", number(p.alpha)},
                          {"class_fractions", p.fractions},
                          {"known_contribution", p.known_contribution}});
        unresolved_pixels += !p.intensity;
        known_mean += p.known_contribution / (width * height);
        for (std::size_t c = 0; c < 4; ++c)
          fractions[c] += p.fractions[c] / (width * height);
      }
      all_resolved = all_resolved && unresolved_pixels == 0;
      reports.push_back(
          {{"scale", scale},
           {"ray_count", width * height * scale * scale},
           {"raw_report", fs::relative(path, root).generic_string()},
           {"raw_report_sha256", u::sha256(bytes)},
           {"classifications", raw.at("classifications")},
           {"class_fractions", fractions},
           {"unresolved_base_pixels", unresolved_pixels},
           {"known_image_plane_mean_contribution", known_mean},
           {"trace_elapsed_seconds", raw.at("elapsed_seconds")},
           {"pixels", pixels}});
      std::cout << "sampling scale " << scale
                << ": unresolved base pixels=" << unresolved_pixels
                << std::endl;
    }
    const auto differences = s::compare(levels);
    J comparisons = J::array(), mask = J::array();
    for (std::size_t i = 0; i < levels.front().pixels.size(); ++i) {
      bool resolved = true;
      for (const auto &g : levels)
        resolved = resolved && g.pixels[i].intensity.has_value();
      mask.push_back(resolved);
    }
    for (std::size_t i = 0; i < differences.size(); ++i) {
      const auto &d = differences[i];
      comparisons.push_back(
          {{"coarse_scale", scales[i]},
           {"fine_scale", scales[i + 1]},
           {"common_resolved_pixels", d.common_pixels},
           {"mean_absolute_intensity", number(d.mean_absolute_intensity)},
           {"maximum_absolute_intensity", number(d.maximum_absolute_intensity)},
           {"relative_l1_intensity", number(d.relative_l1_intensity)},
           {"mean_classification_tv", d.mean_classification_tv},
           {"maximum_classification_tv", d.maximum_classification_tv},
           {"mean_unresolved_fraction_change",
            d.mean_unresolved_fraction_change}});
    }
    const auto &last = differences.back();
    const double coverage =
        static_cast<double>(last.common_pixels) / (width * height);
    const bool enough_levels = differences.size() >= 2;
    bool decreasing = false;
    if (enough_levels) {
      const auto &prior = differences[differences.size() - 2];
      decreasing =
          last.mean_absolute_intensity && prior.mean_absolute_intensity &&
          *last.mean_absolute_intensity <= *prior.mean_absolute_intensity &&
          last.mean_classification_tv <= prior.mean_classification_tv;
    }
    const bool criteria = enough_levels && decreasing &&
                          coverage >= coverage_floor &&
                          last.relative_l1_intensity &&
                          *last.relative_l1_intensity <= intensity_ceiling &&
                          last.mean_classification_tv <= classification_ceiling;
    auto after = u::manifest(root);
    if (before.at("computational_sha256") != after.at("computational_sha256") ||
        u::sha256(u::read_file(tracer)) != tracer_hash)
      throw std::runtime_error(
          "sources or tracer changed during sampling experiment");
    J report{
        {"schema_version", 1},
        {"execution_passed", true},
        {"complete_P2_acceptance", false},
        {"all_samples_resolved", all_resolved},
        {"pilot_refinement_criteria_passed_on_common_mask", criteria},
        {"full_image_refinement_criteria_passed", criteria && all_resolved},
        {"last_differences_nonincreasing", decreasing},
        {"at_least_three_levels", enough_levels},
        {"common_resolved_fraction", coverage},
        {"common_resolved_mask", mask},
        {"configuration", configuration},
        {"sampling_policy", policy},
        {"scene_sha256", u::sha256(scene_bytes)},
        {"policy_sha256", u::sha256(config_bytes)},
        {"computational_source_sha256", after.at("computational_sha256")},
        {"source_unchanged", true},
        {"compiler", __VERSION__},
        {"cplusplus", __cplusplus},
        {"executable_sha256", u::sha256(u::read_file(executable))},
        {"tracer_executable_sha256", tracer_hash},
        {"levels", reports},
        {"comparisons", comparisons},
        {"class_order",
         {"emitted", "escaped", "capture_surface", "unresolved"}},
        {"scope",
         "Uniform image-plane midpoint averages; not solid-angle flux. Fixed "
         "common "
         "resolved mask for intensity, full image for class fractions. Pilot "
         "differences "
         "are not global error bounds. Integration tolerances are fixed."},
        {"elapsed_seconds",
         std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
             .count()}};
    u::atomic_json(output, report);
    u::atomic_json(output.parent_path() /
                       (output.stem().string() + "_manifest.json"),
                   after);
    std::cout << comparisons.dump(2) << "\nPilot criteria=" << criteria
              << "; full image criteria=" << (criteria && all_resolved) << '\n';
    return 0; // A completed measurement may legitimately fail the pilot
              // criteria.
  } catch (const std::invalid_argument &e) {
    std::cerr << "sampling input error: " << e.what() << '\n';
    return 2;
  } catch (const std::exception &e) {
    std::cerr << "sampling experiment failed: " << e.what() << '\n';
    return 1;
  }
}
