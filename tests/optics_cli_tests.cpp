// SPDX-License-Identifier: MIT
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <cmath>
#include <iostream>
#include <numbers>

namespace u = gravastar::tools;
namespace fs = std::filesystem;
namespace o = gravastar::optics;
using J = u::Json;
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 4)
      throw std::invalid_argument(
          "expected trace executable [--report results/FILE.json]");
    std::string pattern =
        (fs::temp_directory_path() / "gravastar-optics-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    if (!mkdtemp(name.data()))
      throw std::runtime_error("temporary directory failed");
    fs::path root = name.data();
    struct Cleanup {
      fs::path p;
      ~Cleanup() {
        std::error_code ec;
        fs::remove_all(p, ec);
      }
    } cleanup{root};
    auto actual_root = fs::path(GRAVASTAR_SOURCE_ROOT);
    auto before = u::manifest(actual_root);
    auto cfg =
        J::parse(u::read_file(actual_root / "benchmarks/optics_v1.json"));
    auto input = root / "benchmarks/scene.json",
         output = root / "results/image.json";
    auto invoke = [&](const std::string &target = "results/image.json",
                      const std::string &command = "image") {
      return u::run({argv[1], command, "--root", root.string(), "--config",
                     "benchmarks/scene.json", "--output", target});
    };
    J checks = J::array(), comparisons = J::array();
    auto check = [&](const std::string &label, bool pass) {
      checks.push_back({{"name", label}, {"passed", pass}});
      if (!pass)
        throw std::runtime_error(label);
    };
    u::atomic_json(output, {{"previous", true}});
    auto bad = cfg;
    bad["schema_version"] = 0;
    u::atomic_json(input, bad);
    check("unsupported scene schema rejected", invoke() == 2);
    check("invalid input preserves prior report",
          J::parse(u::read_file(output)) == J{{"previous", true}});
    bad = cfg;
    bad["image"]["width"] = 0;
    u::atomic_json(input, bad);
    check("invalid image size rejected", invoke() == 2);
    bad["image"]["width"] = 16.5;
    u::atomic_json(input, bad);
    check("fractional image size rejected", invoke() == 2);
    bad = cfg;
    bad["integrator"]["maximum_attempts"] = -1;
    u::atomic_json(input, bad);
    check("negative attempt budget rejected", invoke() == 2);
    bad = cfg;
    bad["integrator"]["relative_tolerance"] = 0;
    u::atomic_json(input, bad);
    check("invalid solver settings rejected", invoke() == 2);
    bad = cfg;
    bad["integrator"]["coordinates"] = "unknown";
    u::atomic_json(input, bad);
    check("unknown coordinate mode rejected", invoke() == 2);
    bad["integrator"]["coordinates"] = "angular";
    bad["integrator"]["angular_switch_q"] = 1;
    u::atomic_json(input, bad);
    check("invalid angular switch threshold rejected", invoke() == 2);
    u::atomic_json(input, cfg);
    u::atomic_bytes(root / "results/collision.ppm", "previous display");
    auto collision = cfg;
    collision["image"] = {{"width", 1}, {"height", 1}};
    u::atomic_json(input, collision);
    check("report cannot alias its PPM preview",
          invoke("results/collision.ppm") == 2 &&
              u::read_file(root / "results/collision.ppm") ==
                  "previous display" &&
              !fs::exists(root / "results/collision_manifest.json"));
    u::atomic_json(input, cfg);
    check("output traversal rejected", invoke("../escape.json") == 2);
    check("negative pixel index rejected",
          u::run({argv[1], "ray", "--root", root.string(), "--config",
                  "benchmarks/scene.json", "--x", "-1"}) == 2);
    for (const std::string value : {"0", "-1", "2.5", "9"})
      check("invalid resolution scale rejected: " + value,
            u::run({argv[1], "image", "--root", root.string(), "--config",
                    "benchmarks/scene.json", "--resolution-scale", value}) ==
                2);
    check("ray resolution scaling rejected",
          u::run({argv[1], "ray", "--root", root.string(), "--config",
                  "benchmarks/scene.json", "--resolution-scale", "2"}) == 2);
    bad = cfg;
    bad["image"]["width"] = 128;
    u::atomic_json(input, bad);
    check("scaled image dimension limit enforced",
          u::run({argv[1], "image", "--root", root.string(), "--config",
                  "benchmarks/scene.json", "--resolution-scale", "2"}) == 2);
    for (const std::string family : {"toy", "kerr"}) {
      auto baseline_config = cfg;
      baseline_config["geometry"]["family"] = family;
      if (family == "kerr")
        baseline_config["domain"]["capture_radius"] = 1.55;
      u::atomic_json(input, baseline_config);
      check(family + " baseline image executes", invoke() == 0);
      auto baseline = J::parse(u::read_file(output));
      auto fine_config = baseline_config;
      fine_config["integrator"]["absolute_tolerance"] = 1e-11;
      fine_config["integrator"]["relative_tolerance"] = 1e-11;
      fine_config["integrator"]["maximum_step"] = .05;
      fine_config["integrator"]["event_tolerance"] = 1e-10;
      u::atomic_json(input, fine_config);
      check(family + " refined image executes",
            invoke("results/fine.json") == 0);
      auto fine = J::parse(u::read_file(root / "results/fine.json"));
      check(family + " pixel count",
            baseline["pixels"].size() == 192 && fine["pixels"].size() == 192);
      double state_error = 0, intensity_error = 0, shift_error = 0,
             null_error = 0;
      int mismatch = 0;
      bool encoding = true;
      for (std::size_t i = 0; i < baseline["pixels"].size(); ++i) {
        const auto &a = baseline["pixels"][i], &b = fine["pixels"][i];
        mismatch += a["outcome"] != b["outcome"];
        auto label = a["outcome"].get<std::string>();
        if (label == "domain_limit" || label == "affine_limit" ||
            label == "step_limit")
          encoding = encoding && a["linear_intensity"].is_null() &&
                     a["alpha"].is_null();
        if (label == "escaped")
          encoding = encoding && a["alpha"] == 0;
        for (int k = 0; k < 8; ++k) {
          double x = a["final"]["state"][k], y = b["final"]["state"][k];
          state_error =
              std::max(state_error, std::abs(x - y) / (1 + std::abs(y)));
        }
        if (label == "emitted" && b["outcome"] == "emitted") {
          double x = a["linear_intensity"], y = b["linear_intensity"];
          intensity_error =
              std::max(intensity_error, std::abs(x - y) / (1 + std::abs(y)));
          x = a["redshift"];
          y = b["redshift"];
          shift_error =
              std::max(shift_error, std::abs(x - y) / (1 + std::abs(y)));
        }
        null_error =
            std::max(null_error, a["maximum_null_residual"].get<double>());
      }
      std::cout << family << " image refinement: state=" << state_error
                << " intensity=" << intensity_error << " shift=" << shift_error
                << " mismatches=" << mismatch << '\n';
      check(family + " event classifications agree under refinement",
            mismatch == 0);
      check(family + " event states converge", state_error < 2e-5);
      check(family + " intensities converge", intensity_error < 2e-5);
      check(family + " frequency shifts converge", shift_error < 5e-6);
      check(family + " null residual bounded", null_error < 1e-6);
      check(family + " unresolved pixels remain explicit", encoding);
      check(family + " display artifact generated",
            u::read_file(root / "results/image.ppm")
                .starts_with("P6\n16 12\n255\n"));
      comparisons.push_back(
          {{"family", family},
           {"classifications", baseline["classifications"]},
           {"unresolved_pixels", baseline["unresolved_pixels"]},
           {"maximum_state_error", state_error},
           {"maximum_intensity_error", intensity_error},
           {"maximum_redshift_error", shift_error},
           {"maximum_null_residual", null_error},
           {"baseline_configuration", baseline_config},
           {"refined_configuration", fine_config},
           {"pixel_classification_mismatches", mismatch}});
      auto angular_config = baseline_config;
      angular_config["integrator"]["coordinates"] = "angular";
      angular_config["integrator"]["angular_switch_q"] = 1.5;
      u::atomic_json(input, angular_config);
      check(family + " angular image executes",
            invoke("results/angular.json") == 0);
      auto angular = J::parse(u::read_file(root / "results/angular.json"));
      auto angular_fine_config = fine_config;
      angular_fine_config["integrator"]["coordinates"] = "angular";
      angular_fine_config["integrator"]["angular_switch_q"] = 1.2;
      u::atomic_json(input, angular_fine_config);
      check(family + " refined angular image executes",
            invoke("results/angular_fine.json") == 0);
      auto angular_fine =
          J::parse(u::read_file(root / "results/angular_fine.json"));
      double angular_state = 0, angular_intensity = 0, baseline_intensity = 0,
             angular_null = 0, angular_lz = 0;
      int angular_mismatch = 0, baseline_mismatch = 0, newly_resolved = 0;
      bool chart_tags = angular["integration_coordinates"] == "angular";
      auto to_bl = [](const J &pixel) {
        const auto chart = pixel["final"]["chart"].get<std::string>();
        return o::transform_state(pixel["final"]["state"].get<o::State>(),
                                  chart == "north" ? o::Chart::north
                                                   : o::Chart::south,
                                  o::Chart::boyer_lindquist);
      };
      for (std::size_t i = 0; i < angular["pixels"].size(); ++i) {
        const auto &a = angular["pixels"][i], &b = angular_fine["pixels"][i],
                   &old = baseline["pixels"][i];
        chart_tags =
            chart_tags && a["initial_chart"] == "boyer_lindquist" &&
            (a["final"]["chart"] == "north" || a["final"]["chart"] == "south");
        angular_mismatch += a["outcome"] != b["outcome"];
        const auto sa = to_bl(a), sb = to_bl(b);
        for (int k = 0; k < 8; ++k) {
          const double delta =
              k == 3 ? std::remainder(sa[k] - sb[k], 2 * std::numbers::pi)
                     : sa[k] - sb[k];
          angular_state =
              std::max(angular_state, std::abs(delta) / (1 + std::abs(sb[k])));
        }
        const bool old_resolved = !old["linear_intensity"].is_null();
        if (old_resolved)
          baseline_mismatch += old["outcome"] != a["outcome"];
        else
          newly_resolved += !a["linear_intensity"].is_null();
        if (a["outcome"] == "emitted" && b["outcome"] == "emitted") {
          const double ia = a["linear_intensity"], ib = b["linear_intensity"];
          angular_intensity = std::max(angular_intensity,
                                       std::abs(ia - ib) / (1 + std::abs(ib)));
          if (old["outcome"] == "emitted") {
            const double io = old["linear_intensity"];
            baseline_intensity = std::max(
                baseline_intensity, std::abs(ia - io) / (1 + std::abs(io)));
          }
        }
        angular_null =
            std::max(angular_null, a["maximum_null_residual"].get<double>());
        angular_lz =
            std::max(angular_lz, a["angular_momentum_error"].get<double>());
      }
      check(family + " angular report retains coordinate tags", chart_tags);
      check(
          family +
              " angular classifications stable under step and patch refinement",
          angular_mismatch == 0);
      check(family + " angular states converge", angular_state < 2e-5);
      check(family + " angular intensities converge", angular_intensity < 2e-5);
      check(family + " resolved BL classifications preserved",
            baseline_mismatch == 0);
      check(family + " resolved BL intensities agree",
            baseline_intensity < 2e-5);
      check(family + " angular null and Killing drift bounded",
            angular_null < 1e-6 && angular_lz < 1e-6);
      comparisons.push_back(
          {{"family", family},
           {"coordinates", "angular"},
           {"classifications", angular["classifications"]},
           {"unresolved_pixels", angular["unresolved_pixels"]},
           {"newly_resolved_pixels", newly_resolved},
           {"maximum_state_error", angular_state},
           {"maximum_intensity_error", angular_intensity},
           {"maximum_BL_intensity_error", baseline_intensity},
           {"maximum_null_residual", angular_null},
           {"maximum_axial_momentum_error", angular_lz},
           {"pixel_classification_mismatches", angular_mismatch},
           {"resolved_BL_classification_mismatches", baseline_mismatch},
           {"baseline_configuration", angular_config},
           {"refined_configuration", angular_fine_config}});
    }
    // A fixed, long step cannot satisfy the local error tolerance. This tests
    // a runtime failure without depending on initial null-roundoff magnitude.
    auto previous = u::read_file(output);
    auto previous_ppm = u::read_file(root / "results/image.ppm");
    bad = cfg;
    bad["image"] = {{"width", 1}, {"height", 1}};
    bad["integrator"]["initial_step"] = 0.5;
    bad["integrator"]["minimum_step"] = 0.5;
    bad["integrator"]["maximum_step"] = 0.5;
    bad["integrator"]["absolute_tolerance"] = 1e-16;
    bad["integrator"]["relative_tolerance"] = 1e-16;
    u::atomic_json(input, bad);
    check("runtime failure preserves accepted data and display",
          invoke() == 1 && u::read_file(output) == previous &&
              u::read_file(root / "results/image.ppm") == previous_ppm);
    auto failure = J::parse(u::read_file(root / "results/image.failed.json"));
    check("runtime failure emits explicit failed artifact",
          failure["execution_passed"] == false &&
              failure["classifications"]["numerical_failure"] == 1 &&
              failure["pixels"][0]["linear_intensity"].is_null());
    auto after = u::manifest(actual_root);
    check("test source unchanged",
          before["computational_sha256"] == after["computational_sha256"]);
    if (argc == 4) {
      if (std::string(argv[2]) != "--report")
        throw std::invalid_argument("unknown report flag");
      u::atomic_json(
          u::result_path(actual_root, argv[3]),
          {{"schema_version", 1},
           {"passed", true},
           {"checks", checks},
           {"comparisons", comparisons},
           {"compiler", __VERSION__},
           {"computational_source_sha256", after["computational_sha256"]},
           {"scope",
            "integration convergence at fixed 16x12 pixel centers; not "
            "spatial/sample convergence or full interior acceptance"}});
    }
    std::cout << "PASS " << checks.size() << " optics CLI and image checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "optics CLI test failed: " << e.what() << '\n';
    return 1;
  }
}
