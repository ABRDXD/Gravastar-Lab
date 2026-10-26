// SPDX-License-Identifier: MIT
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
struct Scene {
  g::Parameters geometry;
  o::Settings settings;
  o::Vector position;
  std::optional<double> omega;
  double fov;
  int width, height;
  bool angular{};
};
Scene scene(const J &j) {
  if (j.at("schema_version") != 1)
    throw std::invalid_argument("unsupported optics schema");
  Scene s;
  const auto &p = j.at("geometry");
  auto f = g::parse_family(p.at("family").get<std::string>());
  if (auto *error = std::get_if<std::string>(&f))
    throw std::invalid_argument(*error);
  s.geometry = {std::get<g::Family>(f), p.at("M"),    p.at("a"),
                p.at("Lambda"),         p.at("r_in"), p.at("r_out")};
  const auto &d = j.at("domain");
  auto &c = s.settings;
  c.inner_radius = d.at("inner_radius");
  c.outer_radius = d.at("outer_radius");
  c.polar_margin = d.at("polar_margin");
  if (!d.at("capture_radius").is_null())
    c.capture_radius = d.at("capture_radius").get<double>();
  const auto &i = j.at("integrator");
  const auto coordinates =
      i.value("coordinates", std::string("boyer_lindquist"));
  if (coordinates != "boyer_lindquist" && coordinates != "angular")
    throw std::invalid_argument("unknown integration coordinates");
  s.angular = coordinates == "angular";
  c.angular_switch_q = i.value("angular_switch_q", 1.5);
  c.continue_core = i.value("continue_core", false);
  c.core_entry_radius = i.value("core_entry_radius", .75);
  c.core_exit_radius = i.value("core_exit_radius", 1.0);
  c.absolute_tolerance = i.at("absolute_tolerance");
  c.relative_tolerance = i.at("relative_tolerance");
  c.initial_step = i.at("initial_step");
  c.minimum_step = i.at("minimum_step");
  c.maximum_step = i.at("maximum_step");
  c.event_tolerance = i.at("event_tolerance");
  c.null_tolerance = i.at("null_tolerance");
  c.maximum_affine = i.at("maximum_affine");
  if (!i.at("maximum_attempts").is_number_integer() ||
      i.at("maximum_attempts") < 1 || i.at("maximum_attempts") > 10000000)
    throw std::invalid_argument(
        "maximum_attempts must be an integer in [1,10000000]");
  c.maximum_attempts = i.at("maximum_attempts");
  const auto &disk = j.at("disk");
  if (disk.at("motion") != "ZAMO" ||
      disk.at("emissivity") != "bolometric_r_inverse_cubed")
    throw std::invalid_argument("unsupported emission prescription");
  c.disk = disk.at("enabled");
  c.disk_inner = disk.at("inner_radius");
  c.disk_outer = disk.at("outer_radius");
  const auto &camera = j.at("camera");
  s.position = camera.at("position").get<o::Vector>();
  if (!camera.at("omega").is_null())
    s.omega = camera.at("omega").get<double>();
  s.fov = camera.at("vertical_field_of_view_degrees").get<double>() *
          std::numbers::pi / 180;
  for (const auto *axis : {"width", "height"})
    if (!j.at("image").at(axis).is_number_integer() ||
        j.at("image").at(axis) < 1 || j.at("image").at(axis) > 128)
      throw std::invalid_argument(
          "image dimensions must be integers in [1,128]");
  s.width = j.at("image").at("width");
  s.height = j.at("image").at("height");
  if (!std::isfinite(s.fov) || s.fov <= 0 ||
      s.fov >= 170 * std::numbers::pi / 180 || s.width < 1 || s.height < 1 ||
      s.width > 128 || s.height > 128)
    throw std::invalid_argument(
        "invalid camera field of view or image size (maximum 128x128)");
  return s;
}
o::State pixel(const Scene &s, int x, int y) {
  double tangent = std::tan(s.fov / 2);
  double right = (2 * (x + .5) / s.width - 1) * tangent * s.width / s.height;
  double up = (1 - 2 * (y + .5) / s.height) * tangent;
  return o::launch(s.geometry, s.position, {-1, -up, right}, s.omega);
}
J point(const o::Point &p) {
  return {{"affine", p.affine},
          {"state", p.state},
          {"chart", o::chart_name(p.chart)},
          {"null_residual", p.null_error}};
}
J ray(const Scene &s, const o::State &initial, const o::Trace &trace) {
  J result{{"outcome", o::outcome_name(trace.outcome)},
           {"reason", trace.detail},
           {"initial_state", initial},
           {"initial_chart", "boyer_lindquist"},
           {"final", point(trace.final)},
           {"accepted_steps", trace.accepted},
           {"rejected_steps", trace.rejected},
           {"rhs_calls", trace.rhs_calls},
           {"chart_switches", trace.chart_switches},
           {"maximum_null_residual", trace.maximum_null_error},
           {"energy_error", trace.energy_error},
           {"angular_momentum_error", trace.angular_momentum_error},
           {"linear_intensity", nullptr},
           {"redshift", nullptr},
           {"alpha", nullptr}};
  if (trace.outcome == o::Outcome::emitted) {
    auto frame = o::circular_frame(o::metric(s.geometry, initial), s.omega);
    // Disk hits are at q=1, where conversion to the original emission chart
    // is regular. The camera and physical ZAMO emission prescription are
    // shared.
    auto hit = trace.final.chart == o::Chart::boyer_lindquist
                   ? trace.final.state
                   : o::transform_state(trace.final.state, trace.final.chart,
                                        o::Chart::boyer_lindquist);
    auto emission =
        o::disk_emission(s.geometry, hit, o::frequency(initial, frame.basis[0]),
                         s.settings.disk_inner);
    result["linear_intensity"] = emission.linear_intensity;
    result["redshift"] = emission.redshift;
    result["alpha"] = 1;
  } else if (trace.outcome == o::Outcome::escaped) {
    result["linear_intensity"] = 0;
    result["alpha"] = 0;
  } else if (trace.outcome == o::Outcome::capture_surface) {
    result["linear_intensity"] = 0;
    result["alpha"] = 1;
  }
  result["crossings"] = J::array();
  for (const auto &c : trace.crossings)
    result["crossings"].push_back(
        {{"surface", c.surface}, {"point", point(c.point)}});
  if (s.settings.save_trajectory) {
    result["trajectory"] = J::array();
    for (const auto &p : trace.trajectory)
      result["trajectory"].push_back(point(p));
  }
  return result;
}
std::array<unsigned char, 3> color(const J &p) {
  auto outcome = p.at("outcome").get<std::string>();
  if (outcome == "emitted") {
    double intensity = p.at("linear_intensity");
    auto v = static_cast<unsigned char>(
        std::lround(255 * std::pow(intensity / (1 + intensity), 1 / 2.2)));
    return {v, v, v};
  }
  if (outcome == "escaped")
    return {16, 24, 48};
  if (outcome == "capture_surface")
    return {0, 0, 0};
  if (outcome == "domain_limit")
    return {255, 0, 255};
  if (outcome == "affine_limit" || outcome == "step_limit")
    return {255, 192, 0};
  return {255, 0, 0};
}
int main(int argc, char **argv) {
  try {
    std::string command = argc > 1 ? argv[1] : "help";
    if (command == "help" || command == "--help") {
      std::cout
          << "gravastar_trace image|ray [--root PATH] [--config "
             "benchmarks/optics_v1.json] [--output results/FILE.json] "
             "[--x N --y N] [--resolution-scale N]\n"
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
    auto s = scene(configuration);
    if (command == "ray" && scale != 1)
      throw std::invalid_argument(
          "resolution scaling is only supported for images");
    if (s.width * scale > 128 || s.height * scale > 128)
      throw std::invalid_argument("scaled image exceeds 128x128");
    s.width *= scale;
    s.height *= scale;
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
    std::size_t unresolved = 0;
    std::string ppm = "P6\n" + std::to_string(s.width) + " " +
                      std::to_string(s.height) + "\n255\n";
    auto append = [&](int col, int row) {
      auto initial = pixel(s, col, row);
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
      auto value = ray(s, initial, traced);
      value["x"] = col;
      value["y"] = row;
      std::string outcome(o::outcome_name(traced.outcome));
      counts[outcome] = counts.value(outcome, 0) + 1;
      execution = execution && traced.outcome != o::Outcome::numerical_failure;
      unresolved += traced.outcome != o::Outcome::escaped &&
                    traced.outcome != o::Outcome::emitted &&
                    traced.outcome != o::Outcome::capture_surface;
      if (command == "image")
        for (auto byte : color(value))
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
    report["elapsed_seconds"] =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    report["diagnostic_colors"] = {
        {"disk", "gray: gamma 1/2.2 after I/(1+I)"},
        {"escape", "dark blue; alpha zero in linear data"},
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
    std::cout << counts.dump() << "; unresolved=" << unresolved << "; "
              << output << '\n';
    return execution && unchanged ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "optics error: " << e.what() << '\n';
    return 2;
  }
}
