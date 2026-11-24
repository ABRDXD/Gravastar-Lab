// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/scene.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
int main(int argc, char **argv) {
  try {
    std::string command = argc > 1 ? argv[1] : "help";
    if (command == "help" || command == "--help") {
      std::cout
          << "gravastar_trace image|ray [--root PATH] [--config "
             "benchmarks/optics_v1.json] [--output results/FILE.json] "
             "[--x N --y N] [--resolution-scale N] [--tolerance T]\n"
             "--tolerance sets absolute and relative tolerance to T and the "
             "event tolerance to 10 T (convergence studies).\n"
             "Future covariant photon momentum; past-directed "
             "integration. Optional angular patches; radial limits remain.\n";
      return 0;
    }
    if (command != "ray" && command != "image")
      throw std::invalid_argument("expected ray or image");
    fs::path root = GRAVASTAR_SOURCE_ROOT, config = "benchmarks/optics_v1.json",
             output = command == "ray" ? "results/p2_native_ray.json"
                                       : "results/p2_native_image.json";
    int x = -1, y = -1, scale = 1;
    std::optional<double> tolerance;
    for (int i = 2; i < argc; ++i) {
      std::string flag = argv[i];
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      std::string value = argv[++i];
      if (flag == "--root")
        root = value;
      else if (flag == "--config")
        config = value;
      else if (flag == "--output")
        output = value;
      else if (flag == "--resolution-scale") {
        std::size_t used;
        scale = std::stoi(value, &used);
        if (used != value.size() || scale < 1 || scale > 8)
          throw std::invalid_argument(
              "resolution scale must be an integer in [1,8]");
      } else if (flag == "--tolerance") {
        std::size_t used;
        const double value_t = std::stod(value, &used);
        if (used != value.size() || !(value_t >= 1e-14 && value_t <= 1e-4))
          throw std::invalid_argument("tolerance must be in [1e-14, 1e-4]");
        tolerance = value_t;
      } else if (flag == "--x" || flag == "--y") {
        std::size_t used;
        int n = std::stoi(value, &used);
        if (used != value.size() || n < 0)
          throw std::invalid_argument("invalid pixel index");
        (flag == "--x" ? x : y) = n;
      } else
        throw std::invalid_argument("unknown option " + flag);
    }
    root = fs::canonical(root);
    config = fs::canonical(config.is_absolute() ? config : root / config);
    auto rel = config.lexically_relative(fs::canonical(root / "benchmarks"));
    if (rel.empty() || *rel.begin() == "..")
      throw std::invalid_argument("configuration must be inside benchmarks/");
    output = u::result_path(root, output);
    if (output.extension() != ".json")
      throw std::invalid_argument(
          "trace report output must have .json extension");
    auto input = u::read_file(config);
    auto configuration = J::parse(input);
    auto s = o::parse_scene(configuration);
    if (command == "ray" && scale != 1)
      throw std::invalid_argument(
          "resolution scaling is only supported for images");
    if (s.width * scale > 128 || s.height * scale > 128)
      throw std::invalid_argument("scaled image exceeds 128x128");
    s.width *= scale;
    s.height *= scale;
    if (tolerance) {
      s.settings.absolute_tolerance = s.settings.relative_tolerance = *tolerance;
      s.settings.event_tolerance = 10 * *tolerance;
      configuration["integrator"]["absolute_tolerance"] = *tolerance;
      configuration["integrator"]["relative_tolerance"] = *tolerance;
      configuration["integrator"]["event_tolerance"] = 10 * *tolerance;
      configuration["integrator"]["tolerance_override"] = true;
    }
    configuration["image"]["width"] = s.width;
    configuration["image"]["height"] = s.height;
    s.settings.save_trajectory = command == "ray";
    if (x == -1)
      x = s.width / 2;
    if (y == -1)
      y = s.height / 2;
    if (x < 0 || x >= s.width || y < 0 || y >= s.height)
      throw std::invalid_argument("pixel outside image");
    auto before = u::manifest(root);
    auto start = std::chrono::steady_clock::now();
    J report{
        {"schema_version", 1},
        {"configuration", configuration},
        {"resolution_scale", scale},
        {"sampling", "uniform image-plane midpoint grid"},
        {"input_sha256", u::sha256(input)},
        {"compiler", __VERSION__},
        {"cplusplus", __cplusplus},
        {"executable_sha256",
         u::sha256(u::read_file(fs::canonical("/proc/self/exe")))},
        {"complete_P2_acceptance", false},
        {"GYOTO_comparison", "not implemented"},
        {"full_interior_continuation",
         s.settings.continue_core
             ? "single-sheet static Cartesian core; outer/static limits remain"
             : "disabled"},
        {"core_extension", s.settings.continue_core
                               ? "static_cartesian_single_sheet_v1"
                               : "disabled"},
        {"core_overlap", s.settings.continue_core
                             ? J{{"entry_radius", s.settings.core_entry_radius},
                                 {"exit_radius", s.settings.core_exit_radius}}
                             : J(nullptr)},
        {"inner_radius_role", s.settings.continue_core
                                  ? "replaced by core continuation"
                                  : "terminal chart limit"},
        {"integration_coordinates", s.angular ? "angular" : "boyer_lindquist"},
        {"angular_switch_q",
         s.angular ? J(s.settings.angular_switch_q) : J(nullptr)},
        {"momentum_convention",
         "future-directed p_mu, increasing affine parameter into past"},
        {"pixels", J::array()}};
    J counts = J::object();
    bool execution = true;
    std::size_t unresolved = 0, near_tangent = 0, outcome_uncertain = 0;
    std::string ppm = "P6\n" + std::to_string(s.width) + " " +
                      std::to_string(s.height) + "\n255\n";
    auto append = [&](int col, int row) {
      auto initial = o::scene_pixel(s, col, row);
      const auto patch = s.position[2] <= std::numbers::pi / 2
                             ? o::Chart::north
                             : o::Chart::south;
      auto traced =
          s.angular
              ? o::trace_angular(s.geometry,
                                 o::transform_state(
                                     initial, o::Chart::boyer_lindquist, patch),
                                 patch, s.settings)
              : o::trace(s.geometry, initial, s.settings);
      if (traced.outcome == o::Outcome::invalid_input)
        throw std::invalid_argument(traced.detail);
      auto value = o::ray_report(s, initial, traced);
      value["x"] = col;
      value["y"] = row;
      std::string outcome(o::outcome_name(traced.outcome));
      counts[outcome] = counts.value(outcome, 0) + 1;
      execution = execution && traced.outcome != o::Outcome::numerical_failure;
      unresolved += traced.outcome != o::Outcome::escaped &&
                    traced.outcome != o::Outcome::emitted &&
                    traced.outcome != o::Outcome::capture_surface;
      near_tangent += !traced.tangencies.empty();
      outcome_uncertain +=
          std::any_of(traced.tangencies.begin(), traced.tangencies.end(),
                      [](const auto &n) { return n.terminal; });
      if (command == "image")
        for (auto byte : o::diagnostic_color(value))
          ppm.push_back(static_cast<char>(byte));
      report["pixels"].push_back(std::move(value));
    };
    if (command == "ray")
      append(x, y);
    else
      for (int row = 0; row < s.height; ++row) {
        for (int col = 0; col < s.width; ++col)
          append(col, row);
        std::cout << "row " << row + 1 << '/' << s.height << std::endl;
      }
    auto after = u::manifest(root);
    bool unchanged =
        before["computational_sha256"] == after["computational_sha256"];
    report["computational_source_sha256"] = after["computational_sha256"];
    report["source_unchanged"] = unchanged;
    report["execution_passed"] = execution && unchanged;
    report["all_pixels_resolved"] = unresolved == 0;
    report["unresolved_pixels"] = unresolved;
    report["classifications"] = counts;
    report["background_channel"] =
        s.background ? "static_star_sphere_v1: separate from linear_intensity; "
                       "star field is artistic, the frequency shift is physics"
                     : "disabled";
    report["tangency_factor"] = s.settings.tangency_factor;
    report["near_tangent_pixels"] = near_tangent;
    report["outcome_uncertain_pixels"] = outcome_uncertain;
    report["elapsed_seconds"] =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    report["diagnostic_colors"] = {
        {"disk", "gray: gamma 1/2.2 after I/(1+I)"},
        {"escape", "dark blue, brightened by the separate background channel "
                   "when enabled; alpha zero and linear_intensity zero"},
        {"capture_surface", "black"},
        {"domain_limit", "magenta"},
        {"budget_limit", "amber"},
        {"numerical_failure", "red"}};
    if (!execution || !unchanged)
      output.replace_filename(output.stem().string() + ".failed.json");
    u::atomic_json(output, report);
    if (execution && unchanged) {
      u::atomic_json(output.parent_path() /
                         (output.stem().string() + "_manifest.json"),
                     after);
      if (command == "image") {
        auto image = output;
        image.replace_extension(".ppm");
        u::atomic_bytes(image, ppm);
      }
    }
    std::cout << counts.dump() << "; unresolved=" << unresolved
              << "; near-tangent=" << near_tangent
              << " (outcome uncertain " << outcome_uncertain << "); "
              << output << '\n';
    return execution && unchanged ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "optics error: " << e.what() << '\n';
    return 2;
  }
}
