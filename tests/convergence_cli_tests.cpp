// SPDX-License-Identifier: GPL-3.0-or-later
//
// gravastar_convergence end to end on tiny scenes in a temporary root:
// exact tolerance labels, the three-passed-levels floor and the recorded
// reference. (Ultrareview findings on PR #7.)
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <iostream>

namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::invalid_argument("expected convergence and tracer executables");
    std::string pattern =
        (fs::temp_directory_path() / "gravastar-convergence-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    if (!mkdtemp(name.data()))
      throw std::runtime_error("temporary directory failed");
    const fs::path root = name.data();
    struct Cleanup {
      fs::path p;
      ~Cleanup() {
        std::error_code ec;
        fs::remove_all(p, ec);
      }
    } cleanup{root};
    fs::create_directories(root / "benchmarks");
    fs::create_directories(root / "results");
    auto scene = J::parse(u::read_file(fs::path(GRAVASTAR_SOURCE_ROOT) /
                                       "benchmarks/optics_keplerian_kerr_v1.json"));
    scene["image"] = {{"width", 4}, {"height", 3}};
    u::atomic_json(root / "benchmarks/kerr.json", scene);
    // A tight null ceiling makes loose levels fail their own check.
    scene["integrator"]["null_tolerance"] = 1e-9;
    u::atomic_json(root / "benchmarks/strict.json", scene);

    int checks = 0;
    auto check = [&](bool pass, const std::string &what) {
      ++checks;
      if (!pass)
        throw std::runtime_error(what);
    };
    auto run = [&](const std::string &scene_file, const std::string &levels,
                   const std::string &out) {
      return u::run({argv[1], "--root", root.string(), "--trace", argv[2],
                     "--scene", "benchmarks/" + scene_file, "--tolerances",
                     levels, "--output", "results/" + out});
    };

    // More than six significant digits: distinct labels, files and values.
    check(run("kerr.json", "1.2345678e-9,1.2345671e-9,1e-10", "labels.json") == 0,
          "label run succeeds");
    const auto labels = J::parse(u::read_file(root / "results/labels.json"));
    check(labels.at("levels").size() == 3, "three levels recorded");
    check(labels["levels"][0]["tolerance"] == 1.2345678e-9 &&
              labels["levels"][1]["tolerance"] == 1.2345671e-9,
          "recorded tolerances are exact");
    check(labels["levels"][0]["report"] != labels["levels"][1]["report"],
          "close tolerances do not share a report file");
    check(labels["levels"][1]["report"].get<std::string>().find(
              "tolerance-1.2345671e-09.json") != std::string::npos,
          "label is the shortest exact text");
    check(labels.at("comparison_complete") == true &&
              labels.at("reference_tolerance") == 1e-10,
          "complete run records its reference");
    check(labels.at("tracer_executable_sha256") ==
              u::sha256(u::read_file(argv[2])),
          "tracer hash recorded");

    // Floor on PASSED levels: 1e-6 fails the 1e-9 null ceiling, leaving two.
    check(run("strict.json", "1e-6,1e-11,1e-12", "floor.json") != 0,
          "too few passed levels exits non-zero");
    check(!fs::exists(root / "results/floor.json") &&
              fs::exists(root / "results/floor.failed.json"),
          "incomplete report is written as .failed.json");
    const auto floor = J::parse(u::read_file(root / "results/floor.failed.json"));
    check(floor.at("comparison_complete") == false &&
              floor.at("against_finest").empty() &&
              floor.at("passed_tolerances").size() == 2,
          "incomplete run publishes no comparisons");
    check(floor["levels"][0]["execution_passed"] == false,
          "the failed level is recorded as failed");

    std::cout << "PASS " << checks << " convergence CLI checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "convergence CLI: " << error.what() << '\n';
    return 1;
  }
}
