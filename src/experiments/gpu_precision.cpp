// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase D step 1: FP32 versus FP64 GPU ray integration, measured.
// The same fixed-step RK4 on the closed-form stereographic Hamiltonian runs
// on the GPU (shaders/gravastar_precision.comp, built FP32 and FP64) and on
// the CPU in float, double and long double (tests/stereographic_hamiltonian.hpp).
// The long-double CPU run is the reference for this algorithm, so the
// differences measure arithmetic precision only, not the RK4 truncation
// error. Throughput comes from GPU timestamps around one dispatch.
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/ray.hpp"
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "stereographic_hamiltonian.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace v = gravastar::rendering::vulkan;
namespace R = gravastar::test_reference;
using J = u::Json;

namespace {
struct Settings {
  double h = .01, r_outer = 10, r_inner = .75, switch_q = 1.5;
  int steps = 4000, width = 64, height = 48, repeats = 3;
  int reference_stride = 1; // CPU reference on every n-th ray only
};
struct Result {
  std::array<long double, 8> state{};
  int chart{}, status{}, steps{}, switches{};
  long double H{};
};

template <class Real>
Result cpu_trace(const R::StereoModel<Real> &m, const std::array<Real, 8> &start,
                 const Settings &s) {
  std::array<Real, 8> y = start, k1, k2, k3, k4, z;
  const Real h = Real(s.h);
  Result out;
  Real H{};
  for (int n = 0; n < s.steps; ++n) {
    auto bad = [](const std::array<Real, 8> &d) {
      for (Real x : d)
        if (!std::isfinite(double(x)))
          return true;
      return false;
    };
    k1 = R::stereographic_rhs(m, y, H);
    for (int i = 0; i < 8; ++i) z[i] = y[i] + h * Real(.5) * k1[i];
    k2 = R::stereographic_rhs(m, z, H);
    for (int i = 0; i < 8; ++i) z[i] = y[i] + h * Real(.5) * k2[i];
    k3 = R::stereographic_rhs(m, z, H);
    for (int i = 0; i < 8; ++i) z[i] = y[i] + h * k3[i];
    k4 = R::stereographic_rhs(m, z, H);
    if (bad(k1) || bad(k2) || bad(k3) || bad(k4)) {
      out.status = 3;
      break;
    }
    for (int i = 0; i < 8; ++i)
      y[i] += h * (k1[i] + Real(2) * k2[i] + Real(2) * k3[i] + k4[i]) / Real(6);
    out.steps = n + 1;
    if (y[1] >= Real(s.r_outer)) { out.status = 1; break; }
    if (y[1] <= Real(s.r_inner)) { out.status = 2; break; }
    if (y[2] * y[2] + y[3] * y[3] >= Real(s.switch_q)) {
      y = R::switch_patch(y);
      out.chart ^= 1;
      ++out.switches;
    }
  }
  (void)R::stereographic_rhs(m, y, H);
  for (int i = 0; i < 8; ++i)
    out.state[i] = y[i];
  out.H = H;
  return out;
}

// Chart-independent comparison: r, t, and the unit direction of the patch
// point; the null value |H| is reported separately.
std::array<long double, 3> direction(const Result &x) {
  const long double a = x.state[2], b = x.state[3], q = a * a + b * b;
  const long double pole = x.chart == 0 ? 1 : -1;
  return {2 * a / (1 + q), 2 * b / (1 + q), pole * (1 - q) / (1 + q)};
}
double difference(const Result &x, const Result &ref) {
  const auto n = direction(x), m = direction(ref);
  long double e = std::max(std::abs(x.state[1] - ref.state[1]),
                           std::abs(x.state[0] - ref.state[0]) /
                               std::max<long double>(1, std::abs(ref.state[0])));
  for (int k = 0; k < 3; ++k)
    e = std::max(e, std::abs(n[k] - m[k]));
  return double(e);
}

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

template <class Real>
std::pair<std::vector<Result>, double>
gpu_trace(v::ComputeContext &gpu, const std::vector<std::uint32_t> &code,
          const g::Parameters &p, const std::vector<o::State> &rays,
          const Settings &s) {
  std::vector<Real> input(16 + 8 * rays.size(), Real(0));
  const double header[10] = {p.mass,  p.spin,        p.cosmological_constant,
                             p.r_in,  p.r_out,       s.h,
                             double(s.steps), s.r_outer, s.r_inner,
                             s.switch_q};
  for (int i = 0; i < 10; ++i)
    input[i] = Real(header[i]);
  for (std::size_t r = 0; r < rays.size(); ++r)
    for (int i = 0; i < 8; ++i)
      input[16 + 8 * r + i] = Real(rays[r][i]);
  const auto groups = static_cast<std::uint32_t>((rays.size() + 63) / 64);
  double best = 1e300;
  std::vector<Result> out(rays.size());
  for (int repeat = 0; repeat < s.repeats; ++repeat) {
    const auto result = gpu.dispatch(
        code, std::as_bytes(std::span(input)), rays.size() * 16 * sizeof(Real),
        groups);
    if (!result.timestamp_available)
      throw std::runtime_error("GPU timestamps unavailable");
    best = std::min(best, result.gpu_milliseconds);
    std::vector<Real> values(rays.size() * 16);
    std::memcpy(values.data(), result.output.data(), result.output.size());
    for (std::size_t r = 0; r < rays.size(); ++r) {
      const Real *w = values.data() + 16 * r;
      for (int i = 0; i < 8; ++i)
        out[r].state[i] = w[i];
      out[r].chart = int(w[8]);
      out[r].status = int(w[9]);
      out[r].steps = int(w[10]);
      out[r].switches = int(w[11]);
      out[r].H = w[12];
    }
  }
  return {out, best};
}

J distribution(std::vector<double> x) {
  if (x.empty())
    return nullptr;
  std::sort(x.begin(), x.end());
  auto at = [&](double f) { return x[std::size_t(f * double(x.size() - 1))]; };
  return {{"median", at(.5)}, {"p90", at(.9)}, {"p99", at(.99)}, {"max", x.back()}};
}
} // namespace

int main(int argc, char **argv) {
  try {
    Settings s;
    std::filesystem::path output = "results/p3_gpu_precision.json";
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      const std::string value = argv[++i];
      if (option == "--output") output = value;
      else if (option == "--width") s.width = std::stoi(value);
      else if (option == "--height") s.height = std::stoi(value);
      else if (option == "--steps") s.steps = std::stoi(value);
      else if (option == "--h") s.h = std::stod(value);
      else if (option == "--reference-stride") s.reference_stride = std::stoi(value);
      else throw std::invalid_argument("unknown option " + option);
    }
    if (s.width < 1 || s.height < 1 || s.width * s.height > 1 << 20 || s.steps < 1 ||
        s.reference_stride < 1)
      throw std::invalid_argument("invalid study size");
    const auto root = std::filesystem::path(GRAVASTAR_SOURCE_ROOT);
    output = u::result_path(root, output);

    // Camera of the optics_v1 toy scene: ZAMO at r=8, theta=pi/3, 50 deg.
    g::Parameters p;
    const o::Vector camera{0, 8, 1.0471975511965976, 0};
    const double tangent = std::tan(50 * std::numbers::pi / 360);
    std::vector<o::State> rays;
    for (int row = 0; row < s.height; ++row)
      for (int col = 0; col < s.width; ++col) {
        const double right = (2 * (col + .5) / s.width - 1) * tangent * s.width / s.height;
        const double up = (1 - 2 * (row + .5) / s.height) * tangent;
        rays.push_back(o::transform_state(o::launch(p, camera, {-1, -up, right}),
                                          o::Chart::boyer_lindquist, o::Chart::north));
      }

    // CPU runs: long double is the reference for this algorithm.
    const R::StereoModel<long double> ml{p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out};
    const R::StereoModel<double> md{p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out};
    const R::StereoModel<float> mf{float(p.mass), float(p.spin),
                                   float(p.cosmological_constant), float(p.r_in),
                                   float(p.r_out)};
    std::vector<Result> ref, cpu64, cpu32;
    std::vector<std::size_t> sample;
    for (std::size_t r = 0; r < rays.size(); r += std::size_t(s.reference_stride))
      sample.push_back(r);
    const auto t0 = std::chrono::steady_clock::now();
    for (const std::size_t index : sample) {
      const auto &y = rays[index];
      std::array<long double, 8> yl;
      std::array<double, 8> yd;
      std::array<float, 8> yf;
      for (int i = 0; i < 8; ++i) {
        yl[i] = y[i];
        yd[i] = y[i];
        yf[i] = float(y[i]);
      }
      ref.push_back(cpu_trace(ml, yl, s));
      cpu64.push_back(cpu_trace(md, yd, s));
      cpu32.push_back(cpu_trace(mf, yf, s));
    }
    const double cpu_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    v::ComputeOptions options;
    options.validation = v::ValidationMode::disabled;
    v::ComputeContext gpu(options);
    const auto &device = gpu.selected_device();
    if (!device.float64)
      throw std::runtime_error("selected GPU has no shaderFloat64");
    auto [gpu32, ms32] =
        gpu_trace<float>(gpu, spirv(GRAVASTAR_PRECISION32_SPV), p, rays, s);
    auto [gpu64, ms64] =
        gpu_trace<double>(gpu, spirv(GRAVASTAR_PRECISION64_SPV), p, rays, s);
    auto [gpumixed, msmixed] =
        gpu_trace<double>(gpu, spirv(GRAVASTAR_PRECISIONMIXED_SPV), p, rays, s);

    J levels = J::object();
    // GPU runs cover every ray (for throughput); accuracy uses the sample.
    auto summarize = [&](const char *name, const std::vector<Result> &run, double ms,
                         bool full) {
      std::vector<double> error, null;
      std::size_t diverged = 0, invalid = 0;
      long long steps = 0;
      for (const auto &x : run)
        steps += x.steps;
      for (std::size_t j = 0; j < sample.size(); ++j) {
        const auto &x = run[full ? sample[j] : j];
        invalid += x.status == 3;
        null.push_back(double(std::abs(x.H)));
        if (x.status != ref[j].status || x.steps != ref[j].steps ||
            x.switches != ref[j].switches)
          ++diverged; // a stop or patch switch fell on another step
        else
          error.push_back(difference(x, ref[j]));
      }
      J level{{"rays_compared", sample.size()},
              {"same_stop_and_switch_steps", sample.size() - diverged},
              {"different_stop_or_switch_step", diverged},
              {"invalid", invalid},
              {"error_vs_long_double", distribution(error)},
              {"final_abs_H", distribution(null)},
              {"ray_steps", steps}};
      if (ms > 0) {
        level["gpu_milliseconds"] = ms;
        level["ray_steps_per_second"] = double(steps) / (ms / 1000);
      }
      levels[name] = level;
    };
    summarize("gpu_fp32", gpu32, ms32, true);
    summarize("gpu_fp64", gpu64, ms64, true);
    summarize("gpu_mixed", gpumixed, msmixed, true);
    summarize("cpu_fp32", cpu32, 0, false);
    summarize("cpu_fp64", cpu64, 0, false);
    std::size_t status_counts[4]{};
    for (const auto &x : ref)
      ++status_counts[x.status];
    J report{{"schema_version", 1},
             {"device", device.name},
             {"study", "fixed-step RK4, closed-form stereographic Hamiltonian, "
                       "toy preset, optics_v1 camera"},
             {"h", s.h},
             {"maximum_steps", s.steps},
             {"rays", rays.size()},
             {"image", {s.width, s.height}},
             {"reference", "CPU long double, same algorithm"},
             {"reference_status_counts",
              {{"budget", status_counts[0]},
               {"outer", status_counts[1]},
               {"inner_core_stop", status_counts[2]},
               {"invalid", status_counts[3]}}},
             {"levels", levels},
             {"rays_compared", sample.size()},
             {"reference_stride", s.reference_stride},
             {"fp64_over_fp32_time", ms64 / ms32},
             {"mixed_over_fp32_time", msmixed / ms32},
             {"cpu_reference_seconds_all_three", cpu_seconds},
             {"error_measure", "max(|dr|, |dt|/max(1,|t|), |d unit direction|) at the "
                               "final step; rays whose stop or patch switch fell on "
                               "another step are counted, not measured"},
             {"computational_source_sha256", u::manifest(root)["computational_sha256"]},
             {"scope", "Arithmetic precision of one fixed-step algorithm; not the "
                       "RK4 truncation error and not the production tracer."}};
    u::atomic_json(output, report);
    std::cout << report["levels"].dump(2) << "\nFP64/FP32 GPU time " << ms64 / ms32
              << ", mixed/FP32 " << msmixed / ms32 << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "gpu precision: " << e.what() << '\n';
    return 1;
  }
}
