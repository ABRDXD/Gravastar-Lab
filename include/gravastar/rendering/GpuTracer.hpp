// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// GPU tracer (shaders/gravastar_trace.comp, FP64 reference or FP32 recording
// build) with CPU fallback, shared by gravastar_gpu_trace, the studio and the
// recorder. Contracts: docs/GPU_TRACER.md.
#include "gravastar/optics/scene.hpp"
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include <array>
#include <atomic>
#include <span>
#include <string>
#include <vector>

namespace gravastar::rendering {
class GpuTracer final {
  public:
    // Reasons a ray was handed back to the CPU (index into fallback counts).
    static constexpr const char *reasons[7] = {"none",          "core_entry",  "near_tangency",
                                               "affine_budget", "step_budget", "numerical_failure",
                                               "null_drift"};
    struct Ray {
        optics::Trace trace;  // GPU result, or the CPU re-trace when flagged
        int gpu_outcome{};    // 0 needs_cpu, 1 escaped, 2 emitted, 3 capture
        int reason{};         // index into reasons
    };
    struct Result {
        std::vector<Ray> rays;
        double gpu_milliseconds{};
        double fallback_seconds{}; // wall time of the CPU re-traces
        std::array<std::size_t, 7> fallback{};
        bool cancelled{};
    };
    // FP64 is the reference path. FP32 (built without -DGRAVASTAR_FP64) is the
    // preview/recording path and needs scene tolerances of about 1e-6.
    enum class Precision { fp64, fp32 };
    // spirv_path names the angular kernel build, gravastar_traceNN.spv; its
    // core-chart build gravastar_trace_coreNN.spv must sit next to it.
    GpuTracer(const char *spirv_path, vulkan::ComputeOptions options = {},
              Precision precision = Precision::fp64);
    // Traces launch states given in `patch` (north or south) for one scene.
    // With core continuation, rays pause at core entry and exit roots, are
    // transformed with optics::to_core/from_core and resume in the other
    // kernel build. Flagged rays are re-traced with the CPU tracer on
    // fallback_threads() threads (rays are independent, so the result does
    // not depend on the count). When `cancel` becomes true the call returns
    // early with cancelled set and only the finished rays filled in.
    [[nodiscard]] Result trace(const optics::Scene &, std::span<const optics::State> start,
                               optics::Chart patch, std::size_t batch = 4096,
                               const std::atomic<bool> *cancel = nullptr);
    [[nodiscard]] const vulkan::DeviceInfo &device() const;
    [[nodiscard]] Precision precision() const noexcept { return precision_; }
    [[nodiscard]] const std::string &core_kernel_path() const noexcept { return core_path_; }
    void set_fallback_threads(unsigned n) noexcept { threads_ = n ? n : 1; }
    [[nodiscard]] unsigned fallback_threads() const noexcept { return threads_; }

  private:
    template <class Real>
    Result run(const optics::Scene &, std::span<const optics::State>, optics::Chart, std::size_t,
               const std::atomic<bool> *);
    vulkan::ComputeContext gpu_;
    std::vector<std::uint32_t> code_;
    std::string core_path_;
    std::vector<std::uint32_t> core_code_;
    Precision precision_;
    unsigned threads_;
};
} // namespace gravastar::rendering
