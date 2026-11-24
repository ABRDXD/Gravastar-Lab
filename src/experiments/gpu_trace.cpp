// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase D GPU image driver. Traces a scene's camera rays with the FP64 GPU
// tracer (shaders/gravastar_trace.comp), re-traces every ray the GPU flags
// as needs_cpu with the CPU tracer, and evaluates emission and background on
// the CPU from each final state (no painted colours). With --compare-cpu it
// also traces every ray on the CPU alone and reports per-pixel differences.
#include "gravastar/optics/scene.hpp"
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

namespace o = gravastar::optics;
namespace u = gravastar::tools;
namespace v = gravastar::rendering::vulkan;
namespace fs = std::filesystem;
using J = u::Json;

namespace {
constexpr std::size_t header_size = 32, input_stride = 16, output_stride = 16;
const char *reasons[] = {"none",          "core_entry",  "near_tangency",
                         "affine_budget", "step_budget", "numerical_failure",
                         "null_drift"};

std::vector<std::uint32_t> spirv(const char *path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    throw std::runtime_error(std::string("missing SPIR-V ") + path);
  const auto size = static_cast<std::size_t>(in.tellg());
  std::vector<std::uint32_t> words(size / 4);
  in.seekg(0);
  in.read(reinterpret_cast<char *>(words.data()), static_cast<std::streamsize>(size));
  return words;
}

std::array<double, header_size> header(const o::Scene &s) {
  const auto &c = s.settings;
  // The kernel's closed form is the toy family. With r_in = r_out = 0 every
  // positive radius is exterior (m = M, no Lambda), which is exactly Kerr.
  const bool kerr = s.geometry.family == gravastar::geometry::Family::kerr;
  if (!kerr && s.geometry.family != gravastar::geometry::Family::toy)
    throw std::invalid_argument("the GPU tracer supports the toy and Kerr families only");
  std::array<double, header_size> h{};
  const double values[] = {
      s.geometry.mass, s.geometry.spin, s.geometry.cosmological_constant,
      kerr ? 0. : s.geometry.r_in, kerr ? 0. : s.geometry.r_out, c.absolute_tolerance,
      c.relative_tolerance, c.initial_step, c.minimum_step, c.maximum_step,
      c.event_tolerance, c.null_tolerance, c.maximum_affine,
      double(c.maximum_attempts), c.outer_radius, c.disk ? 1. : 0.,
      c.disk_inner, c.disk_outer,
      // Without core continuation the inner chart limit is terminal on the
      // CPU; the GPU hands rays reaching either radius to the CPU.
      c.continue_core ? c.core_entry_radius : c.inner_radius,
      c.capture_radius.value_or(0), c.angular_switch_q, c.tangency_factor};
  std::copy(std::begin(values), std::end(values), h.begin());
  return h;
}

struct GpuRay {
  int outcome{}, reason{};
  o::Point final;
  std::size_t accepted{}, rejected{}, rhs_calls{};
  double maximum_null{};
};

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
    bool compare = false, check = false, summary_only = false;
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
    v::ComputeContext gpu(options);
    if (!gpu.selected_device().float64)
      throw std::runtime_error("selected GPU has no shaderFloat64");
    const auto code = spirv(GRAVASTAR_TRACE64_SPV);
    const auto h = header(s);
    std::vector<GpuRay> gpu_rays(n);
    double gpu_ms = 0;
    for (std::size_t first = 0; first < n; first += batch) {
      const std::size_t count = std::min(batch, n - first);
      std::vector<double> in(header_size + input_stride * count, 0);
      std::copy(h.begin(), h.end(), in.begin());
      for (std::size_t r = 0; r < count; ++r) {
        for (int i = 0; i < 8; ++i)
          in[header_size + input_stride * r + i] = start[first + r][i];
        in[header_size + input_stride * r + 8] = patch == o::Chart::north ? 0 : 1;
      }
      const auto result = gpu.dispatch(code, std::as_bytes(std::span(in)),
                                       count * output_stride * sizeof(double),
                                       static_cast<std::uint32_t>((count + 63) / 64));
      gpu_ms += result.gpu_milliseconds;
      std::vector<double> out(count * output_stride);
      std::memcpy(out.data(), result.output.data(), result.output.size());
      for (std::size_t r = 0; r < count; ++r) {
        const double *w = out.data() + output_stride * r;
        auto &g = gpu_rays[first + r];
        g.outcome = int(w[0]);
        g.reason = int(w[1]);
        for (int i = 0; i < 8; ++i)
          g.final.state[i] = w[2 + i];
        g.final.chart = w[10] == 0 ? o::Chart::north : o::Chart::south;
        g.final.affine = w[11];
        g.accepted = std::size_t(w[12]);
        g.rejected = std::size_t(w[13]);
        g.rhs_calls = std::size_t(w[14]);
        g.maximum_null = w[15];
      }
    }

    // Assemble the image: GPU-resolved rays use the GPU final state; flagged
    // rays are re-traced on the CPU from the same launch.
    J pixels = J::array(), counts = J::object();
    std::array<std::size_t, 7> flagged{};
    const auto t_fallback = std::chrono::steady_clock::now();
    std::vector<o::Trace> combined(n);
    for (std::size_t i = 0; i < n; ++i) {
      const auto &g = gpu_rays[i];
      o::Trace t;
      if (g.outcome == 0) {
        ++flagged[std::clamp(g.reason, 0, 6)];
        t = o::trace_angular(s.geometry, start[i], patch, s.settings);
      } else {
        t.outcome = g.outcome == 1   ? o::Outcome::escaped
                    : g.outcome == 2 ? o::Outcome::emitted
                                     : o::Outcome::capture_surface;
        t.final = g.final;
        t.accepted = g.accepted;
        t.rejected = g.rejected;
        t.rhs_calls = g.rhs_calls;
        t.maximum_null_error = g.maximum_null;
      }
      combined[i] = t;
      auto value = o::ray_report(s, initial[i], t);
      value["x"] = int(i) % s.width;
      value["y"] = int(i) / s.width;
      value["gpu_outcome"] = g.outcome;
      value["gpu_reason"] = reasons[std::clamp(g.reason, 0, 6)];
      const std::string outcome(o::outcome_name(t.outcome));
      counts[outcome] = counts.value(outcome, 0) + 1;
      pixels.push_back(std::move(value));
    }
    const double fallback_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_fallback).count();

    J flagged_json = J::object();
    std::size_t total_flagged = 0;
    for (int r = 1; r <= 6; ++r) {
      flagged_json[reasons[r]] = flagged[r];
      total_flagged += flagged[r];
    }
    J report{{"schema_version", 1},
             {"configuration", configuration},
             {"input_sha256", u::sha256(input_bytes)},
             {"resolution_scale", scale},
             {"device", gpu.selected_device().name},
             {"precision", "FP64"},
             {"rays", n},
             {"gpu_resolved_rays", n - total_flagged},
             {"cpu_fallback_rays", total_flagged},
             {"cpu_fallback_reasons", flagged_json},
             {"classifications", counts},
             {"gpu_milliseconds", gpu_ms},
             {"assembly_and_cpu_fallback_seconds", fallback_seconds},
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
              cpu_traces[i] = o::trace_angular(s.geometry, start[i], patch, s.settings);
          });
      }
      const double cpu_parallel_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - t_cpu).count();
      for (std::size_t i = 0; i < n; ++i) {
        const auto &cpu = cpu_traces[i];
        if (gpu_rays[i].outcome == 0)
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
