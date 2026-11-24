// SPDX-License-Identifier: GPL-3.0-or-later
//
// Image convergence in integration tolerance. Runs gravastar_trace on one
// scene at a list of tolerances (same pixels, same sampling scale) and
// compares every level with the tightest one and with its neighbour:
// classification mismatches, disk linear intensity, disk redshift and the
// separate background channel. Sample-count convergence is gravastar_sampling.
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>

namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;

namespace {
struct Pixel {
  std::string outcome;
  double intensity{}, redshift{}, background{};
  bool has_background{};
};
std::vector<Pixel> pixels(const J &report) {
  if (report.at("execution_passed") != true || report.at("source_unchanged") != true)
    throw std::runtime_error("trace level did not pass");
  std::vector<Pixel> out;
  for (const auto &p : report.at("pixels")) {
    Pixel x;
    x.outcome = p.at("outcome").get<std::string>();
    if (x.outcome == "emitted") {
      x.intensity = p.at("linear_intensity");
      x.redshift = p.at("redshift");
    }
    if (p.contains("background_intensity") && !p.at("background_intensity").is_null()) {
      x.background = p.at("background_intensity");
      x.has_background = true;
    }
    out.push_back(x);
  }
  return out;
}
J compare(const std::vector<Pixel> &a, const std::vector<Pixel> &b) {
  if (a.size() != b.size())
    throw std::runtime_error("levels have different pixel counts");
  std::size_t mismatched = 0, disk = 0, sky = 0;
  long double disk_difference = 0, disk_reference = 0, sky_difference = 0,
              sky_reference = 0;
  double disk_max_relative = 0, redshift_max = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].outcome != b[i].outcome) {
      ++mismatched;
      continue;
    }
    if (a[i].outcome == "emitted") {
      ++disk;
      const double d = std::abs(a[i].intensity - b[i].intensity);
      disk_difference += d;
      disk_reference += std::abs(b[i].intensity);
      disk_max_relative = std::max(disk_max_relative, d / std::abs(b[i].intensity));
      redshift_max = std::max(redshift_max, std::abs(a[i].redshift - b[i].redshift));
    }
    if (a[i].has_background && b[i].has_background) {
      ++sky;
      sky_difference += std::abs(a[i].background - b[i].background);
      sky_reference += std::abs(b[i].background);
    }
  }
  auto ratio = [](long double x, long double y) {
    return y > 0 ? J(double(x / y)) : J(nullptr);
  };
  return {{"classification_mismatches", mismatched},
          {"common_disk_pixels", disk},
          {"disk_relative_l1", ratio(disk_difference, disk_reference)},
          {"disk_maximum_relative", disk ? J(disk_max_relative) : J(nullptr)},
          {"disk_redshift_maximum_absolute", disk ? J(redshift_max) : J(nullptr)},
          {"common_background_pixels", sky},
          {"background_relative_l1", ratio(sky_difference, sky_reference)}};
}
std::vector<double> list(const std::string &text) {
  std::vector<double> out;
  std::stringstream in(text);
  for (std::string item; std::getline(in, item, ',');) {
    std::size_t used;
    out.push_back(std::stod(item, &used));
    if (used != item.size())
      throw std::invalid_argument("bad tolerance list");
  }
  for (std::size_t i = 1; i < out.size(); ++i)
    if (!(out[i] < out[i - 1]))
      throw std::invalid_argument("tolerances must decrease strictly");
  if (out.size() < 3)
    throw std::invalid_argument("at least three tolerances required");
  return out;
}
} // namespace

int main(int argc, char **argv) {
  try {
    fs::path root = GRAVASTAR_SOURCE_ROOT, scene = "benchmarks/optics_keplerian_stars_v1.json",
             output = "results/p2_tolerance_convergence.json",
             tracer = fs::canonical("/proc/self/exe").parent_path() / "gravastar_trace";
    std::string tolerances = "1e-8,1e-9,1e-10,1e-11,1e-12", scale = "1";
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        std::cout << "gravastar_convergence [--scene benchmarks/X.json] "
                     "[--tolerances 1e-8,...,1e-12] [--scale N] "
                     "[--output results/FILE.json] [--trace EXECUTABLE]\n";
        return 0;
      }
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      const std::string value = argv[++i];
      if (option == "--scene")
        scene = value;
      else if (option == "--tolerances")
        tolerances = value;
      else if (option == "--scale")
        scale = value;
      else if (option == "--output")
        output = value;
      else if (option == "--trace")
        tracer = value;
      else
        throw std::invalid_argument("unknown option " + option);
    }
    const auto levels = list(tolerances);
    root = fs::canonical(root);
    output = u::result_path(root, output);
    const auto local = u::result_path(root, "results/local/convergence");
    fs::create_directories(local);
    std::string pattern = (local / "run-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    if (!mkdtemp(name.data()))
      throw std::runtime_error("cannot create convergence run directory");
    const fs::path run = name.data();
    const auto before = u::manifest(root);
    const auto start = std::chrono::steady_clock::now();
    std::vector<std::vector<Pixel>> images;
    std::vector<double> passed;
    J raw = J::array();
    for (double tolerance : levels) {
      std::ostringstream label;
      label << tolerance;
      const auto path = run / ("tolerance-" + label.str() + ".json");
      const bool ok =
          u::run({tracer.string(), "image", "--root", root.string(), "--config",
                  scene.string(), "--output", path.string(), "--resolution-scale",
                  scale, "--tolerance", label.str()}) == 0;
      // A level whose rays fail their own accuracy checks is recorded, not
      // compared: the tracer refused to produce data there.
      auto file = path;
      if (!ok)
        file.replace_filename(path.stem().string() + ".failed.json");
      if (!fs::exists(file))
        throw std::runtime_error("trace level produced no report");
      const auto bytes = u::read_file(file);
      const auto report = J::parse(bytes);
      if (ok) {
        images.push_back(pixels(report));
        passed.push_back(tolerance);
      }
      raw.push_back({{"tolerance", tolerance},
                     {"execution_passed", ok},
                     {"report", fs::relative(file, root).generic_string()},
                     {"report_sha256", u::sha256(bytes)},
                     {"classifications", report.at("classifications")},
                     {"near_tangent_pixels", report.at("near_tangent_pixels")},
                     {"elapsed_seconds", report.at("elapsed_seconds")}});
      std::cout << "tolerance " << tolerance << " done" << std::endl;
    }
    if (images.size() < 2)
      throw std::runtime_error("fewer than two tolerance levels passed");
    J against_finest = J::array(), successive = J::array();
    for (std::size_t i = 0; i + 1 < images.size(); ++i) {
      auto x = compare(images[i], images.back());
      x["tolerance"] = passed[i];
      against_finest.push_back(x);
      auto y = compare(images[i], images[i + 1]);
      y["coarse_tolerance"] = passed[i];
      y["fine_tolerance"] = passed[i + 1];
      successive.push_back(y);
    }
    const auto after = u::manifest(root);
    J report{{"schema_version", 1},
             {"scene", fs::relative(fs::canonical(root / scene), root).generic_string()},
             {"scene_sha256", u::sha256(u::read_file(root / scene))},
             {"resolution_scale", std::stoi(scale)},
             {"tolerances", levels},
             {"event_tolerance_rule", "10 x tolerance"},
             {"levels", raw},
             {"against_finest", against_finest},
             {"successive", successive},
             {"computational_source_sha256", after["computational_sha256"]},
             {"source_unchanged",
              before["computational_sha256"] == after["computational_sha256"]},
             {"elapsed_seconds", std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - start)
                                     .count()},
             {"scope", "Differences between tolerance levels on a fixed pixel "
                       "set; the tightest level is a reference, not the exact "
                       "image. No global error bound."}};
    u::atomic_json(output, report);
    std::cout << against_finest.dump(2) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "convergence: " << e.what() << '\n';
    return 1;
  }
}
