// SPDX-License-Identifier: MIT
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
    // The budget-40 spatial pilot censored this outward-moving ray just
    // before r=10. Preserve that classification, then test the explicit
    // longer-budget preset and an independently tightened integration.
    auto short_ray = cfg;
    short_ray["image"] = {{"width", 128}, {"height", 96}};
    auto long_ray =
        J::parse(u::read_file(actual / "benchmarks/optics_core_long_v1.json"));
    auto comparable = long_ray;
    comparable["preset_id"] = cfg["preset_id"];
    comparable["scope"] = cfg["scope"];
    comparable["integrator"]["maximum_affine"] = 40.0;
    check("long-budget preset changes only the affine budget",
          comparable == cfg);
    long_ray["image"] = short_ray["image"];
    auto fine_ray = tighter;
    fine_ray["image"] = short_ray["image"];
    fine_ray["integrator"]["maximum_affine"] = 80.0;
    check("budget-40 witness executes",
          run(short_ray, "short", "ray", 42, 0) == 0);
    check("budget-80 witness executes",
          run(long_ray, "long", "ray", 42, 0) == 0);
    check("refined budget-80 witness executes",
          run(fine_ray, "long_fine", "ray", 42, 0) == 0);
    const auto short_result =
        J::parse(u::read_file(root / "results/short.json"))["pixels"][0];
    const auto long_result =
        J::parse(u::read_file(root / "results/long.json"))["pixels"][0];
    const auto fine_result =
        J::parse(u::read_file(root / "results/long_fine.json"))["pixels"][0];
    check("budget-40 witness remains explicitly unresolved",
          short_result["outcome"] == "affine_limit" &&
              short_result["linear_intensity"].is_null() &&
              short_result["final"]["affine"] == 40.0);
    check("both budget-80 witnesses escape",
          long_result["outcome"] == "escaped" &&
              fine_result["outcome"] == "escaped");
    const double escape_affine = long_result["final"]["affine"];
    const double fine_affine = fine_result["final"]["affine"];
    double endpoint_error = 0;
    for (int component = 0; component < 8; ++component) {
      const double a = long_result["final"]["state"][component];
      const double b = fine_result["final"]["state"][component];
      endpoint_error =
          std::max(endpoint_error, std::abs(a - b) / (1 + std::abs(b)));
    }
    check("escape event and endpoint stable under refinement",
          long_result["final"]["chart"] == fine_result["final"]["chart"] &&
              escape_affine > 40 && escape_affine < 80 &&
              std::abs(escape_affine - fine_affine) < 2e-5 &&
              endpoint_error < 2e-5 &&
              std::abs(long_result["final"]["state"][1].get<double>() - 10) <
                  2e-8);
    for (const auto *result : {&long_result, &fine_result})
      check("long-budget null and Killing drift bounded",
            (*result)["maximum_null_residual"].get<double>() <= 1e-6 &&
                (*result)["energy_error"].get<double>() <= 1e-6 &&
                (*result)["angular_momentum_error"].get<double>() <= 1e-6);
    const J budget_witness = {
        {"selection", "Observed budget-40 pilot pixel (42,0) at 128x96"},
        {"budget_40_outcome", short_result["outcome"]},
        {"budget_40_final", short_result["final"]},
        {"budget_80_outcome", long_result["outcome"]},
        {"budget_80_final", long_result["final"]},
        {"refined_budget_80_final", fine_result["final"]},
        {"escape_affine_difference", std::abs(escape_affine - fine_affine)},
        {"scaled_endpoint_difference", endpoint_error},
        {"maximum_null_residual", long_result["maximum_null_residual"]},
        {"refined_maximum_null_residual",
         fine_result["maximum_null_residual"]}};
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
           {"budget_witness", budget_witness},
           {"complete_P2_acceptance", false},
           {"scope", "Fixed 16x12 pixel integration/patch refinement; separate "
                     "from spatial acceptance and GYOTO. Includes the observed "
                     "128x96 budget-limit witness and a refined longer run."}});
    std::cout << "PASS " << checks.size() << " core CLI checks; newly resolved "
              << newly_resolved << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "core CLI: " << e.what() << '\n';
    return 1;
  }
}
