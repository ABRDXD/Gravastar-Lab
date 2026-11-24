// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase D GPU image driver. Traces a scene's camera rays with the FP64 GPU
// tracer (shaders/gravastar_trace.comp), re-traces every ray the GPU flags
// as needs_cpu with the CPU tracer, and evaluates emission and background on
// the CPU from each final state (no painted colours). With --compare-cpu it
// also traces every ray on the CPU alone and reports per-pixel differences.
#include "gravastar/optics/scene.hpp"
#include "gravastar/rendering/GpuTracer.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

namespace o = gravastar::optics;
namespace u = gravastar::tools;
namespace v = gravastar::rendering::vulkan;
namespace fs = std::filesystem;
using J = u::Json;

namespace {
J distribution(std::vector<double> x) {
  if (x.empty())
    return nullptr;
  std::sort(x.begin(), x.end());
  auto at = [&](double f) { return x[std::size_t(f * double(x.size() - 1))]; };
  return {{"count", x.size()}, {"median", at(.5)}, {"p99", at(.99)}, {"max", x.back()}};
}
} // namespace

int main(int argc, char **argv) {
  try {
    fs::path root = GRAVASTAR_SOURCE_ROOT, config = "benchmarks/optics_keplerian_stars_v1.json",
             output = "results/p3_gpu_image.json";
    int scale = 1;
    std::size_t batch = 4096;
    bool compare = false, check = false, summary_only = false, fp32 = false;
    std::optional<double> tolerance;
    unsigned fallback_threads = 0; // 0: every hardware thread
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--compare-cpu" || option == "--check" || option == "--summary") {
        (option == "--compare-cpu" ? compare : option == "--check" ? check : summary_only) = true;
        continue;
      }
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      const std::string value = argv[++i];
      if (option == "--config") config = value;
      else if (option == "--output") output = value;
      else if (option == "--resolution-scale") scale = std::stoi(value);
      else if (option == "--batch") batch = std::stoul(value);
      else if (option == "--precision") {
        if (value != "fp64" && value != "fp32")
          throw std::invalid_argument("--precision must be fp64 or fp32");
        fp32 = value == "fp32";
      } else if (option == "--tolerance") tolerance = std::stod(value);
      else if (option == "--fallback-threads") fallback_threads = std::stoul(value);
      else throw std::invalid_argument("unknown option " + option);
    }
    if (scale < 1 || scale > 8 || batch < 64 || batch > (1u << 20))
      throw std::invalid_argument("invalid scale or batch");
    root = fs::canonical(root);
    config = fs::canonical(config.is_absolute() ? config : root / config);
    output = u::result_path(root, output);
    const auto input_bytes = u::read_file(config);
    auto configuration = J::parse(input_bytes);
    auto s = o::parse_scene(configuration);
    if (!s.angular)
      throw std::invalid_argument("the GPU tracer supports angular-patch scenes only");
    s.width *= scale;
    s.height *= scale;
    if (s.width > 128 || s.height > 128)
      throw std::invalid_argument("scaled image exceeds 128x128");
    s.settings.save_trajectory = false;
    const auto patch = s.position[2] <= std::numbers::pi / 2 ? o::Chart::north
                                                             : o::Chart::south;

    std::vector<o::State> initial, start;
    for (int row = 0; row < s.height; ++row)
      for (int col = 0; col < s.width; ++col) {
        initial.push_back(o::scene_pixel(s, col, row));
        start.push_back(o::transform_state(initial.back(), o::Chart::boyer_lindquist, patch));
      }
    const std::size_t n = initial.size();

    v::ComputeOptions options;
    options.validation = v::ValidationMode::disabled;
    // The reference (CPU comparison) keeps the scene's own settings; the GPU
    // path and its CPU fallback may use a looser tolerance (FP32 needs ~1e-6).
    const auto reference_settings = s.settings;
    if (tolerance)
      s = o::with_tolerance(s, *tolerance);
    gravastar::rendering::GpuTracer tracer(
        fp32 ? GRAVASTAR_TRACE32_SPV : GRAVASTAR_TRACE64_SPV, options,
        fp32 ? gravastar::rendering::GpuTracer::Precision::fp32
             : gravastar::rendering::GpuTracer::Precision::fp64);
    if (fallback_threads)
      tracer.set_fallback_threads(fallback_threads);
    const auto t_trace = std::chrono::steady_clock::now();
    const auto traced = tracer.trace(s, start, patch, batch);
    const double trace_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_trace).count();
    const double gpu_ms = traced.gpu_milliseconds;
    const auto &flagged = traced.fallback;

    // Assemble the image from the GPU results and the CPU re-traces.
    J pixels = J::array(), counts = J::object();
    const auto t_assembly = std::chrono::steady_clock::now();
    std::vector<o::Trace> combined(n);
    std::vector<int> gpu_outcome(n);
    for (std::size_t i = 0; i < n; ++i) {
      const auto &ray = traced.rays[i];
      combined[i] = ray.trace;
      gpu_outcome[i] = ray.gpu_outcome;
      auto value = o::ray_report(s, initial[i], ray.trace);
      value["x"] = int(i) % s.width;
      value["y"] = int(i) / s.width;
      value["gpu_outcome"] = ray.gpu_outcome;
      value["gpu_reason"] = gravastar::rendering::GpuTracer::reasons[ray.reason];
      const std::string outcome(o::outcome_name(ray.trace.outcome));
      counts[outcome] = counts.value(outcome, 0) + 1;
      pixels.push_back(std::move(value));
    }
    const double assembly_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_assembly).count();

    J flagged_json = J::object();
    std::size_t total_flagged = 0;
    for (int r = 1; r <= 6; ++r) {
      flagged_json[gravastar::rendering::GpuTracer::reasons[r]] = flagged[r];
      total_flagged += flagged[r];
    }
    J report{{"schema_version", 1},
             {"configuration", configuration},
             {"input_sha256", u::sha256(input_bytes)},
             {"resolution_scale", scale},
             {"device", tracer.device().name},
             {"precision", fp32 ? "FP32" : "FP64"},
             {"gpu_tolerance", s.settings.absolute_tolerance},
             {"reference_tolerance", reference_settings.absolute_tolerance},
             {"rays", n},
             {"gpu_resolved_rays", n - total_flagged},
             {"cpu_fallback_rays", total_flagged},
             {"cpu_fallback_reasons", flagged_json},
             {"classifications", counts},
             {"gpu_milliseconds", gpu_ms},
             {"trace_seconds_including_cpu_fallback", trace_seconds},
             {"cpu_fallback_seconds", traced.fallback_seconds},
             {"cpu_fallback_threads", tracer.fallback_threads()},
             {"report_assembly_seconds", assembly_seconds},
             {"pixels", pixels}};

    if (compare) {
      std::size_t mismatched = 0;
      std::vector<double> intensity, redshift, background, position;
      // CPU-only reference on every hardware thread, so the timing compares
      // the GPU with a parallel CPU rather than with one core.
      const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
      std::vector<o::Trace> cpu_traces(n);
      const auto t_cpu = std::chrono::steady_clock::now();
      {
        std::vector<std::jthread> pool;
        for (unsigned w = 0; w < threads; ++w)
          pool.emplace_back([&, w] {
            for (std::size_t i = w; i < n; i += threads)
              cpu_traces[i] = o::trace_angular(s.geometry, start[i], patch, reference_settings);
          });
      }
      const double cpu_parallel_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - t_cpu).count();
      for (std::size_t i = 0; i < n; ++i) {
        const auto &cpu = cpu_traces[i];
        if (gpu_outcome[i] == 0)
          continue; // identical CPU trace by construction
        const auto &t = combined[i];
        if (cpu.outcome != t.outcome) {
          ++mismatched;
          continue;
        }
        const auto a = o::ray_report(s, initial[i], t), b = o::ray_report(s, initial[i], cpu);
        if (cpu.outcome == o::Outcome::emitted) {
          intensity.push_back(std::abs(a["linear_intensity"].get<double>() -
                                       b["linear_intensity"].get<double>()) /
                              b["linear_intensity"].get<double>());
          redshift.push_back(std::abs(a["redshift"].get<double>() - b["redshift"].get<double>()));
        }
        if (cpu.outcome == o::Outcome::escaped && !b["background_intensity"].is_null()) {
          const double bb = b["background_intensity"].get<double>();
          background.push_back(std::abs(a["background_intensity"].get<double>() - bb) /
                               std::max(1e-12, bb));
        }
        // Chart-independent final position: r and the unit direction.
        if (cpu.final.chart != o::Chart::core_cartesian) {
          const auto na = o::sky_direction(t.final), nb = o::sky_direction(cpu.final);
          double e = std::abs(t.final.state[1] - cpu.final.state[1]);
          for (int k = 0; k < 3; ++k)
            e = std::max(e, std::abs(na[k] - nb[k]));
          position.push_back(e);
        }
      }
      report["comparison"] = {
          {"cpu_only_seconds_all_threads", cpu_parallel_seconds},
          {"cpu_threads", threads},
          {"gpu_resolved_outcome_mismatches", mismatched},
          {"disk_intensity_relative", distribution(intensity)},
          {"disk_redshift_absolute", distribution(redshift)},
          {"background_relative", distribution(background)},
          {"final_position_and_direction", distribution(position)},
          {"note", "Rays the GPU flagged are re-traced by the same CPU tracer, "
                   "so only GPU-resolved rays are compared."}};
    }
    report["computational_source_sha256"] = u::manifest(root)["computational_sha256"];
    // Ceilings from the 2026-11-24 RTX 3050 runs (docs/GPU_TRACER.md), with
    // margin: no GPU-resolved ray may change outcome, and GPU-resolved disk,
    // redshift and position values must stay well inside the CPU tracer's
    // own integration error at scene tolerances (~3e-7 per pixel).
    if (check) {
      if (!compare)
        throw std::invalid_argument("--check requires --compare-cpu");
      const auto &c = report["comparison"];
      auto worst = [&](const char *k) {
        return c[k].is_null() ? 0.0 : c[k]["max"].get<double>();
      };
      std::string failures;
      if (c["gpu_resolved_outcome_mismatches"] != 0) failures += "outcome mismatch; ";
      if (worst("disk_intensity_relative") > 1e-7) failures += "disk intensity; ";
      if (worst("disk_redshift_absolute") > 1e-7) failures += "disk redshift; ";
      if (worst("final_position_and_direction") > 1e-6) failures += "final position; ";
      if (report["gpu_resolved_rays"].get<std::size_t>() * 10 < n * 9)
        failures += "fewer than 90% of rays resolved on the GPU; ";
      report["checks_passed"] = failures.empty();
      report["check_failures"] = failures;
      if (!failures.empty())
        output.replace_filename(output.stem().string() + ".failed.json");
    }
    J summary = report;
    summary.erase("pixels");
    if (summary_only)
      report = summary;
    u::atomic_json(output, report);
    summary.erase("configuration");
    std::cout << summary.dump(2) << '\n';
    return report.value("checks_passed", true) ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "gpu trace: " << e.what() << '\n';
    return 1;
  }
}
