// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <iostream>
namespace u = gravastar::tools;
namespace fs = std::filesystem;
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("validator and source root arguments required");
    auto root = fs::temp_directory_path() /
                ("gravastar failure test " + std::to_string(getpid()));
    fs::create_directories(root / "benchmarks");
    struct Cleanup {
      fs::path p;
      ~Cleanup() {
        std::error_code ec;
        fs::remove_all(p, ec);
      }
    } cleanup{root};
    auto cfg = u::Json::parse(
        u::read_file(fs::path(argv[2]) / "benchmarks/rotating_v1.json"));
    auto input = root / "benchmarks/test.json",
         output = root / "results/accepted.json";
    u::atomic_json(output, {{"previous", true}});
    auto invoke = [&] {
      return u::run({argv[1], "--root", root.string(), "--config",
                     input.string(), "--output", output.string()});
    };
    auto bad = cfg;
    bad["schema_version"] = 999;
    u::atomic_json(input, bad);
    if (invoke() != 2)
      throw std::runtime_error("unknown schema accepted");
    bad = cfg;
    bad["parameters"]["r_out"] = "1.4";
    u::atomic_json(input, bad);
    if (invoke() != 2)
      throw std::runtime_error("invalid shell accepted");
    bad = cfg;
    bad["reference"]["decimal_places"] = 30;
    u::atomic_json(input, bad);
    if (invoke() != 2)
      throw std::runtime_error("unsupported precision silently accepted");
    bad = cfg;
    bad["thresholds"]["metric"] = 0;
    bad["domain"]["radial_samples"] = 2;
    bad["domain"]["angular_samples"] = 2;
    u::atomic_json(input, bad);
    if (invoke() != 1)
      throw std::runtime_error("scientific failure did not propagate");
    if (u::Json::parse(u::read_file(output)) != u::Json{{"previous", true}})
      throw std::runtime_error("failed run overwrote previous accepted report");
    auto failure =
        u::Json::parse(u::read_file(root / "results/accepted.failed.json"));
    if (failure.at("passed") != false)
      throw std::runtime_error("failure report missing");
    std::cout << "Validation failure isolation and input rejection passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
