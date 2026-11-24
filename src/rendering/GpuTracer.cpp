// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/rendering/GpuTracer.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>

namespace gravastar::rendering {
namespace o = optics;
namespace {
constexpr std::size_t header_size = 32, input_stride = 16, output_stride = 16;

std::vector<std::uint32_t> read_spirv(const char *path) {
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
    const bool kerr = s.geometry.family == geometry::Family::kerr;
    if (!kerr && s.geometry.family != geometry::Family::toy)
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
} // namespace

GpuTracer::GpuTracer(const char *spirv_path, vulkan::ComputeOptions options, Precision precision)
    : gpu_(options), code_(read_spirv(spirv_path)), precision_(precision),
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
    for (std::size_t first = 0; first < n; first += batch) {
        if (cancel && cancel->load()) {
            out.cancelled = true;
            break;
        }
        const std::size_t count = std::min(batch, n - first);
        std::vector<Real> in(header_size + input_stride * count, Real(0));
        for (std::size_t i = 0; i < header_size; ++i)
            in[i] = Real(h[i]);
        for (std::size_t r = 0; r < count; ++r) {
            for (int i = 0; i < 8; ++i)
                in[header_size + input_stride * r + i] = Real(start[first + r][i]);
            in[header_size + input_stride * r + 8] = patch == o::Chart::north ? 0 : 1;
        }
        const auto result = gpu_.dispatch(code_, std::as_bytes(std::span(in)),
                                          count * output_stride * sizeof(Real),
                                          static_cast<std::uint32_t>((count + 63) / 64));
        out.gpu_milliseconds += result.gpu_milliseconds;
        std::vector<Real> values(count * output_stride);
        std::memcpy(values.data(), result.output.data(), result.output.size());
        for (std::size_t r = 0; r < count; ++r) {
            const Real *w = values.data() + output_stride * r;
            auto &ray = out.rays[first + r];
            ray.gpu_outcome = int(w[0]);
            ray.reason = std::clamp(int(w[1]), 0, 6);
            if (ray.gpu_outcome == 0) {
                ++out.fallback[ray.reason];
                flagged.push_back(first + r);
                continue;
            }
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
    // CPU re-traces of the flagged rays, shared out by an atomic counter.
    const auto t0 = std::chrono::steady_clock::now();
    std::atomic<std::size_t> next{0};
    std::atomic<bool> stopped{false};
    auto work = [&] {
        for (std::size_t k; (k = next.fetch_add(1)) < flagged.size();) {
            if (cancel && cancel->load()) {
                stopped = true;
                return;
            }
            const auto i = flagged[k];
            out.rays[i].trace = o::trace_angular(s.geometry, start[i], patch, s.settings);
        }
    };
    {
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
