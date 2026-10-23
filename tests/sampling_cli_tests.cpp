// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <cmath>
#include <iostream>
#include <numbers>

namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::invalid_argument("expected sampler and tracer executables");
    std::string pattern =
        (fs::temp_directory_path() / "gravastar-sampling-XXXXXX").string();
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
    const fs::path actual_root = GRAVASTAR_SOURCE_ROOT;
    auto scene =
        J::parse(u::read_file(actual_root / "benchmarks/optics_v1.json"));
    scene["geometry"]["family"] = "flat";
    scene["geometry"]["a"] = 0;
    scene["image"] = {{"width", 2}, {"height", 2}};
    auto policy =
        J::parse(u::read_file(actual_root / "benchmarks/sampling_v1.json"));
    policy["scales"] = {1, 2, 4};
    const auto scene_path = root / "benchmarks/scene.json",
               policy_path = root / "benchmarks/policy.json",
               output = root / "results/image.json";
    u::atomic_json(scene_path, scene);
    u::atomic_json(policy_path, policy);
    auto invoke = [&](const std::string &target = "results/image.json") {
      return u::run({argv[1], "--trace", argv[2], "--root", root.string(),
                     "--scene", "benchmarks/scene.json", "--config",
                     "benchmarks/policy.json", "--output", target});
    };
    int checks = 0;
    auto check = [&](bool pass, const char *message) {
      ++checks;
      if (!pass)
        throw std::runtime_error(message);
    };
    u::atomic_json(output, {{"previous", true}});
    for (J bad_scales :
         {J::array({}), J::array({1}), J::array({2, 4}), J::array({1, 1}),
          J::array({1, 2.5}), J::array({1, 9}), J("1,2")}) {
      auto bad = policy;
      bad["scales"] = bad_scales;
      u::atomic_json(policy_path, bad);
      check(invoke() == 2, "invalid scales rejected before tracing");
    }
    for (J value : {J(-1), J(2), J(0), J(".1")}) {
      auto bad = policy;
      bad["relative_l1_intensity_ceiling"] = value;
      u::atomic_json(policy_path, bad);
      check(invoke() == 2, "invalid criteria rejected before tracing");
    }
    check(J::parse(u::read_file(output)) == J{{"previous", true}},
          "invalid input preserves prior report");
    u::atomic_json(policy_path, policy);
    check(invoke("../escape.json") == 2, "output traversal rejected");
    check(invoke() == 0, "three-level scaled ray experiment executes");
    const auto report = J::parse(u::read_file(output));
    check(report.at("execution_passed") == true &&
              report.at("complete_P2_acceptance") == false,
          "measurement does not claim complete P2");
    check(report.at("levels").size() == 3 &&
              report.at("comparisons").size() == 2 &&
              report.at("common_resolved_mask").size() == 4,
          "report shape and comparisons");
    check(report.at("comparisons")[0].at("common_resolved_pixels") ==
              report.at("comparisons")[1].at("common_resolved_pixels"),
          "fixed comparison coverage");
    for (const auto &level : report.at("levels")) {
      const auto path = root / level.at("raw_report").get<std::string>();
      const auto bytes = u::read_file(path);
      check(u::sha256(bytes) ==
                level.at("raw_report_sha256").get<std::string>(),
            "raw evidence digest verified");
      const auto raw = J::parse(bytes);
      const int scale = level.at("scale"), width = 2 * scale;
      check(raw.at("configuration").at("image").at("width") == width &&
                raw.at("pixels").size() ==
                    static_cast<std::size_t>(width * width),
            "effective dimensions and ray count");
      // Independent flat-space covariant launch formula for top-left pixel.
      const double tangent = std::tan(25 * std::numbers::pi / 180),
                   up = (1 - 1.0 / width) * tangent, right = -up,
                   norm = std::sqrt(1 + up * up + right * right),
                   theta = scene.at("camera").at("position")[2];
      const auto initial =
          raw.at("pixels")[0].at("initial_state").get<std::array<double, 8>>();
      check(std::abs(initial[4] + 1) < 1e-13 &&
                std::abs(initial[5] - 1 / norm) < 1e-13 &&
                std::abs(initial[6] - 8 * up / norm) < 1e-13 &&
                std::abs(initial[7] + 8 * std::sin(theta) * right / norm) <
                    1e-13,
            "scaled launch agrees with independent flat camera formula");
      // Independent traversal of the top-left block, without the aggregation
      // helper.
      double sum = 0;
      bool resolved = true;
      for (int y = 0; y < scale; ++y)
        for (int x = 0; x < scale; ++x) {
          const auto &value =
              raw.at("pixels")[y * width + x].at("linear_intensity");
          if (value.is_null())
            resolved = false;
          else
            sum += value.get<double>();
        }
      const auto &pixel = level.at("pixels")[0];
      check(std::abs(pixel.at("known_contribution").get<double>() -
                     sum / (scale * scale)) < 1e-13 &&
                pixel.at("intensity").is_null() == !resolved,
            "aggregate evidence matches raw rays");
    }
    check(report.at("scene_sha256") == u::sha256(u::read_file(scene_path)) &&
              report.at("policy_sha256") ==
                  u::sha256(u::read_file(policy_path)) &&
              report.at("computational_source_sha256") ==
                  u::manifest(root).at("computational_sha256"),
          "input and computational provenance");
    const auto prior = u::read_file(output);
    const auto prior_manifest =
        u::read_file(root / "results/image_manifest.json");
    auto bad_scene = scene;
    bad_scene["integrator"]["relative_tolerance"] = 0;
    u::atomic_json(scene_path, bad_scene);
    check(invoke() == 1 && u::read_file(output) == prior &&
              u::read_file(root / "results/image_manifest.json") ==
                  prior_manifest,
          "failed child preserves accepted report and manifest");
    std::cout << "PASS " << checks << " sampling CLI checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "sampling CLI test failed: " << e.what() << '\n';
    return 1;
  }
}
