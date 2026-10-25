// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <cmath>
#include <iostream>

namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
int main(int argc, char **argv) {
  try {
    if (argc != 2 && (argc != 4 || std::string(argv[2]) != "--report"))
      throw std::invalid_argument(
          "expected trace executable [--report results/FILE.json]");
    const fs::path actual = GRAVASTAR_SOURCE_ROOT;
    const auto before = u::manifest(actual);
    std::string pattern =
        (fs::temp_directory_path() / "gravastar-core-XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (!mkdtemp(buffer.data()))
      throw std::runtime_error("temporary directory failed");
    const fs::path root = buffer.data();
    struct Cleanup {
      fs::path p;
      ~Cleanup() {
        std::error_code e;
        fs::remove_all(p, e);
      }
    } cleanup{root};
    const auto input = root / "benchmarks/scene.json";
    J checks = J::array();
    auto check = [&](const std::string &name, bool pass) {
      checks.push_back({{"name", name}, {"passed", pass}});
      if (!pass)
        throw std::runtime_error(name);
    };
    auto run = [&](const J &cfg, const std::string &name,
                   const std::string &command = "image", int x = 0, int y = 0) {
      u::atomic_json(input, cfg);
      return u::run({argv[1], command, "--root", root.string(), "--config",
                     "benchmarks/scene.json", "--output",
                     "results/" + name + ".json", "--x", std::to_string(x),
                     "--y", std::to_string(y)});
    };
    const auto cfg =
        J::parse(u::read_file(actual / "benchmarks/optics_core_v1.json"));
    auto bounded = cfg;
    bounded["integrator"]["continue_core"] = false;
    check("bounded angular comparison image executes",
          run(bounded, "bounded") == 0);
    check("continued core image executes", run(cfg, "core") == 0);
    const auto old = J::parse(u::read_file(root / "results/bounded.json"));
    const auto core = J::parse(u::read_file(root / "results/core.json"));
    auto tighter = cfg;
    tighter["integrator"]["absolute_tolerance"] = 1e-11;
    tighter["integrator"]["relative_tolerance"] = 1e-11;
    tighter["integrator"]["maximum_step"] = .05;
    tighter["integrator"]["event_tolerance"] = 1e-10;
    tighter["integrator"]["core_entry_radius"] = .55;
    tighter["integrator"]["core_exit_radius"] = .9;
    tighter["integrator"]["angular_switch_q"] = 1.2;
    check("refined core overlap and integration image executes",
          run(tighter, "fine") == 0);
    const auto fine = J::parse(u::read_file(root / "results/fine.json"));
    check("extension choice and unchanged P2 gate explicit",
          core["core_extension"] == "static_cartesian_single_sheet_v1" &&
              core["complete_P2_acceptance"] == false &&
              core["inner_radius_role"] == "replaced by core continuation");
    int newly_resolved = 0, mismatch = 0, old_mismatch = 0, witness = -1;
    double intensity = 0, old_intensity = 0, null_error = 0, energy = 0, lz = 0;
    for (std::size_t i = 0; i < core["pixels"].size(); ++i) {
      const auto &a = core["pixels"][i], &b = fine["pixels"][i],
                 &prior = old["pixels"][i];
      mismatch += a["outcome"] != b["outcome"];
      if (prior["linear_intensity"].is_null())
        newly_resolved += !a["linear_intensity"].is_null();
      else
        old_mismatch += prior["outcome"] != a["outcome"];
      if (a["outcome"] == "emitted" && b["outcome"] == "emitted") {
        const double x = a["linear_intensity"], y = b["linear_intensity"];
        intensity = std::max(intensity, std::abs(x - y) / (1 + std::abs(y)));
        if (prior["outcome"] == "emitted") {
          const double z = prior["linear_intensity"];
          old_intensity =
              std::max(old_intensity, std::abs(x - z) / (1 + std::abs(z)));
        }
      }
      for (const auto &crossing : a["crossings"])
        if (crossing["surface"] == "core_entry")
          witness = static_cast<int>(i);
      null_error =
          std::max(null_error, a["maximum_null_residual"].get<double>());
      energy = std::max(energy, a["energy_error"].get<double>());
      lz = std::max(lz, a["angular_momentum_error"].get<double>());
    }
    check("all 192 pixel records retained",
          core["pixels"].size() == 192 && fine["pixels"].size() == 192);
    check("core continuation resolves previously stopped rays",
          newly_resolved > 0);
    check("core image classifications stable under refinement", mismatch == 0);
    check("previously resolved classifications preserved", old_mismatch == 0);
    check("core image intensity refinement", intensity < 2e-5);
    check("previously resolved intensities preserved", old_intensity < 2e-5);
    check("core image null and Killing drift bounded",
          null_error <= 1e-6 && energy <= 1e-6 && lz <= 1e-6);
    check("core-entry witness available", witness >= 0);
    const auto &pixel = core["pixels"][witness];
    check("core trajectory report executes",
          run(cfg, "ray", "ray", pixel["x"], pixel["y"]) == 0);
    const auto ray = J::parse(u::read_file(root / "results/ray.json"));
    bool tagged_core = false, tagged_exit = false;
    for (const auto &point : ray["pixels"][0]["trajectory"])
      tagged_core = tagged_core || point["chart"] == "core_cartesian";
    for (const auto &crossing : ray["pixels"][0]["crossings"])
      tagged_exit =
          tagged_exit || (crossing["surface"] == "core_exit" &&
                          crossing["point"]["chart"] == "core_cartesian");
    check("Cartesian states and exit crossings correctly tagged",
          tagged_core && tagged_exit);
    const auto accepted = u::read_file(root / "results/core.json");
    auto bad = cfg;
    bad["geometry"]["family"] = "kerr";
    check("Kerr core mode rejected without replacing accepted report",
          run(bad, "core") == 2 &&
              u::read_file(root / "results/core.json") == accepted);
    bad = cfg;
    bad["integrator"]["core_exit_radius"] = 1.6;
    check("shell-crossing core patch rejected", run(bad, "core") == 2);
    bad = cfg;
    bad["integrator"]["coordinates"] = "boyer_lindquist";
    check("core flag cannot silently use BL tracing", run(bad, "core") == 2);
    bad = cfg;
    bad["disk"]["inner_radius"] = .5;
    check("unsupported disk inside Cartesian patch rejected",
          run(bad, "core") == 2);
    const auto after = u::manifest(actual);
    check("computational sources unchanged",
          before["computational_sha256"] == after["computational_sha256"]);
    if (argc == 4)
      u::atomic_json(
          u::result_path(actual, argv[3]),
          {{"passed", true},
           {"checks", checks},
           {"compiler", __VERSION__},
           {"computational_source_sha256", after["computational_sha256"]},
           {"configuration", cfg},
           {"refined_configuration", tighter},
           {"classifications", core["classifications"]},
           {"newly_resolved_pixels", newly_resolved},
           {"classification_mismatches", mismatch},
           {"maximum_intensity_error", intensity},
           {"maximum_previous_intensity_error", old_intensity},
           {"maximum_null_residual", null_error},
           {"maximum_energy_error", energy},
           {"maximum_axial_momentum_error", lz},
           {"complete_P2_acceptance", false},
           {"scope", "Fixed 16x12 pixel integration/patch refinement; separate "
                     "from spatial acceptance and GYOTO."}});
    std::cout << "PASS " << checks.size() << " core CLI checks; newly resolved "
              << newly_resolved << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "core CLI: " << e.what() << '\n';
    return 1;
  }
}
