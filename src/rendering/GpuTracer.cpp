// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/rendering/GpuTracer.hpp"
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/core_chart.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

namespace gravastar::rendering {
namespace o = optics;
namespace {
constexpr std::size_t header_size = 32, input_stride = 16, output_stride = 18;
// Kernel reasons for a ray paused at a core entry or exit root.
constexpr int paused_entry = 7, paused_exit = 8;
constexpr double core_chart_code = 2; // input/output chart: 0 north, 1 south, 2 core

std::vector<std::uint32_t> read_spirv(const std::string &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("missing SPIR-V " + path);
    const auto size = static_cast<std::size_t>(in.tellg());
    std::vector<std::uint32_t> words(size / 4);
    in.seekg(0);
    in.read(reinterpret_cast<char *>(words.data()), static_cast<std::streamsize>(size));
    return words;
}

// gravastar_traceNN.spv -> gravastar_trace_coreNN.spv in the same directory.
std::string core_path(const char *spirv_path) {
    const std::filesystem::path p(spirv_path);
    const std::string name = p.filename().string(), stem = "gravastar_trace";
    if (name.rfind(stem, 0) != 0)
        throw std::invalid_argument("GPU tracer kernel must be named gravastar_trace*.spv");
    return (p.parent_path() / (stem + "_core" + name.substr(stem.size()))).string();
}

std::array<double, header_size> header(const o::Scene &s) {
    const auto &c = s.settings;
    // The kernel's closed form is the toy family. With r_in = r_out = 0 every
    // positive radius is exterior (m = M, no Lambda), which is exactly Kerr.
    const bool kerr = s.geometry.family == geometry::Family::kerr;
    if (!kerr && s.geometry.family != geometry::Family::toy)
        throw std::invalid_argument("the GPU tracer supports the toy and Kerr families only");
    // The CPU tracer refuses core continuation outside an exact core.
    if (kerr && c.continue_core)
        throw std::invalid_argument("core continuation needs the toy family");
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
        c.capture_radius.value_or(0), c.angular_switch_q, c.tangency_factor,
        // With core continuation, rays pause at the entry and exit roots and
        // resume in the other kernel build after transition() below.
        c.continue_core ? 1. : 0., c.core_exit_radius};
    std::copy(std::begin(values), std::end(values), h.begin());
    return h;
}

// Kernel input of one ray: state, chart code, then the integrator state of a
// paused ray (next step, affine, accepted, rejected, rhs calls, maximum null
// residual, attempts used), all 0 for a launch.
using Input = std::array<double, input_stride>;

bool finite(const o::State &y) {
    return std::all_of(y.begin(), y.end(), [](double v) { return std::isfinite(v); });
}

// The chart change of a ray paused at a core entry or exit root, made with the
// CPU tracer's own functions in the order of change_core and switch_patch in
// src/optics/ray.cpp, with their null-residual checks. Returns false where
// the CPU tracer throws (it then rejects the trial or fails the ray), so the
// ray is re-traced on the CPU from its launch.
bool transition(const o::Scene &s, const double *w, int reason, Input &next) {
    const auto &p = s.geometry;
    const auto &c = s.settings;
    o::State y{};
    for (int i = 0; i < 8; ++i)
        y[i] = w[2 + i];
    const double code = w[10];
    try {
        o::State z;
        double chart;
        if (reason == paused_entry) {
            if (code != 0 && code != 1)
                return false;
            z = o::to_core(p, y, code == 0 ? o::Chart::north : o::Chart::south);
            const double null = o::core_null_residual(o::core_metric(p, z), z);
            if (!(null <= c.null_tolerance))
                return false;
            chart = core_chart_code;
        } else {
            if (code != core_chart_code)
                return false;
            auto destination = y[3] >= 0 ? o::Chart::north : o::Chart::south;
            z = o::from_core(p, y, destination);
            double null = o::angular_null_residual(o::angular_metric(p, z), z);
            if (!(null <= c.null_tolerance))
                return false;
            if (z[2] * z[2] + z[3] * z[3] >= c.angular_switch_q) {
                const auto other = destination == o::Chart::north ? o::Chart::south : o::Chart::north;
                z = o::transform_state(z, destination, other);
                destination = other;
                null = o::angular_null_residual(o::angular_metric(p, z), z);
                if (!(null <= c.null_tolerance))
                    return false;
            }
            chart = destination == o::Chart::north ? 0 : 1;
        }
        if (!finite(z))
            return false;
        for (int i = 0; i < 8; ++i)
            next[i] = z[i];
        next[8] = chart;
        next[9] = w[16];  // the controller's next step
        next[10] = w[11]; // affine
        next[11] = w[12]; // accepted
        next[12] = w[13]; // rejected
        next[13] = w[14]; // rhs calls
        next[14] = w[15]; // maximum null residual
        next[15] = w[17]; // attempts used
        return true;
    } catch (const std::exception &) {
        return false;
    }
}
} // namespace

GpuTracer::GpuTracer(const char *spirv_path, vulkan::ComputeOptions options, Precision precision)
    : gpu_(options), code_(read_spirv(spirv_path)), core_path_(core_path(spirv_path)),
      core_code_(read_spirv(core_path_)), precision_(precision),
      threads_(std::max(1u, std::thread::hardware_concurrency())) {
    if (precision_ == Precision::fp64 && !gpu_.selected_device().float64)
        throw std::runtime_error("selected GPU has no shaderFloat64");
}

const vulkan::DeviceInfo &GpuTracer::device() const { return gpu_.selected_device(); }

GpuTracer::Result GpuTracer::trace(const o::Scene &s, std::span<const o::State> start,
                                   o::Chart patch, std::size_t batch,
                                   const std::atomic<bool> *cancel) {
    return precision_ == Precision::fp64 ? run<double>(s, start, patch, batch, cancel)
                                         : run<float>(s, start, patch, batch, cancel);
}

template <class Real>
GpuTracer::Result GpuTracer::run(const o::Scene &s, std::span<const o::State> start,
                                 o::Chart patch, std::size_t batch,
                                 const std::atomic<bool> *cancel) {
    if (!s.angular)
        throw std::invalid_argument("the GPU tracer supports angular-patch scenes only");
    if (patch != o::Chart::north && patch != o::Chart::south)
        throw std::invalid_argument("launch states must be in a stereographic patch");
    if (batch < 64 || batch > (1u << 20))
        throw std::invalid_argument("invalid batch size");
    Result out;
    const std::size_t n = start.size();
    out.rays.resize(n);
    const auto h = header(s);
    std::vector<std::size_t> flagged;
    // Rounds: every unfinished ray runs in the kernel build of its chart. A
    // ray paused at a core entry or exit is transformed here and runs in the
    // next round; one that passes through the core takes three rounds.
    std::vector<std::pair<std::size_t, Input>> pending(n);
    for (std::size_t i = 0; i < n; ++i) {
        pending[i].first = i;
        Input &in = pending[i].second;
        in.fill(0);
        for (int k = 0; k < 8; ++k)
            in[k] = start[i][k];
        in[8] = patch == o::Chart::north ? 0 : 1;
    }
    while (!pending.empty() && !out.cancelled) {
        std::vector<std::pair<std::size_t, Input>> next;
        for (const bool core : {false, true}) {
            std::vector<std::size_t> group; // indices into pending
            for (std::size_t j = 0; j < pending.size(); ++j)
                if ((pending[j].second[8] == core_chart_code) == core)
                    group.push_back(j);
            for (std::size_t first = 0; first < group.size(); first += batch) {
                if (cancel && cancel->load()) {
                    out.cancelled = true;
                    break;
                }
                const std::size_t count = std::min(batch, group.size() - first);
                std::vector<Real> in(header_size + input_stride * count, Real(0));
                for (std::size_t i = 0; i < header_size; ++i)
                    in[i] = Real(h[i]);
                for (std::size_t r = 0; r < count; ++r)
                    for (std::size_t k = 0; k < input_stride; ++k)
                        in[header_size + input_stride * r + k] =
                            Real(pending[group[first + r]].second[k]);
                const auto result = gpu_.dispatch(core ? core_code_ : code_,
                                                  std::as_bytes(std::span(in)),
                                                  count * output_stride * sizeof(Real),
                                                  static_cast<std::uint32_t>((count + 63) / 64));
                out.gpu_milliseconds += result.gpu_milliseconds;
                std::vector<Real> values(count * output_stride);
                std::memcpy(values.data(), result.output.data(), result.output.size());
                for (std::size_t r = 0; r < count; ++r) {
                    const std::size_t index = pending[group[first + r]].first;
                    double w[output_stride];
                    for (std::size_t k = 0; k < output_stride; ++k)
                        w[k] = double(values[output_stride * r + k]);
                    auto &ray = out.rays[index];
                    ray.gpu_outcome = int(w[0]);
                    int reason = int(w[1]);
                    if (ray.gpu_outcome == 0 && (reason == paused_entry || reason == paused_exit)) {
                        Input resumed{};
                        if (transition(s, w, reason, resumed)) {
                            next.emplace_back(index, resumed);
                            continue;
                        }
                        reason = 5; // numerical failure: the CPU tracer decides
                    }
                    if (ray.gpu_outcome == 0) {
                        ray.reason = std::clamp(reason, 1, 6);
                        ++out.fallback[ray.reason];
                        flagged.push_back(index);
                        continue;
                    }
                    ray.reason = 0;
                    auto &t = ray.trace;
                    t.outcome = ray.gpu_outcome == 1   ? o::Outcome::escaped
                                : ray.gpu_outcome == 2 ? o::Outcome::emitted
                                                       : o::Outcome::capture_surface;
                    for (int i = 0; i < 8; ++i)
                        t.final.state[i] = w[2 + i];
                    t.final.chart = w[10] == 0 ? o::Chart::north : o::Chart::south;
                    t.final.affine = w[11];
                    t.accepted = std::size_t(w[12]);
                    t.rejected = std::size_t(w[13]);
                    t.rhs_calls = std::size_t(w[14]);
                    t.maximum_null_error = w[15];
                }
            }
            if (out.cancelled)
                break;
        }
        pending = std::move(next);
    }
    // CPU re-traces of the flagged rays, shared out by an atomic counter.
    const auto t0 = std::chrono::steady_clock::now();
    std::atomic<std::size_t> cursor{0};
    std::atomic<bool> stopped{false};
    auto work = [&] {
        for (std::size_t k; (k = cursor.fetch_add(1)) < flagged.size();) {
            if (cancel && cancel->load()) {
                stopped = true;
                return;
            }
            const auto i = flagged[k];
            out.rays[i].trace = o::trace_angular(s.geometry, start[i], patch, s.settings);
        }
    };
    if (!out.cancelled) {
        const unsigned workers = unsigned(std::min<std::size_t>(threads_, flagged.size()));
        std::vector<std::jthread> pool;
        for (unsigned w = 1; w < workers; ++w)
            pool.emplace_back(work);
        work();
    }
    out.fallback_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    out.cancelled = out.cancelled || stopped;
    return out;
}
} // namespace gravastar::rendering
