// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// FP64 GPU tracer (shaders/gravastar_trace.comp) with CPU fallback, shared by
// gravastar_gpu_trace and the studio. Contracts: docs/GPU_TRACER.md.
#include "gravastar/optics/scene.hpp"
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include <array>
#include <atomic>
#include <span>
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
        std::array<std::size_t, 7> fallback{};
        bool cancelled{};
    };
    GpuTracer(const char *spirv_path, vulkan::ComputeOptions options = {});
    // Traces launch states given in `patch` (north or south) for one scene.
    // Flagged rays are re-traced with the CPU tracer. When `cancel` becomes
    // true the call returns after the current batch with cancelled set and
    // only the finished rays filled in (the rest default-initialized).
    [[nodiscard]] Result trace(const optics::Scene &, std::span<const optics::State> start,
                               optics::Chart patch, std::size_t batch = 4096,
                               const std::atomic<bool> *cancel = nullptr);
    [[nodiscard]] const vulkan::DeviceInfo &device() const;

  private:
    vulkan::ComputeContext gpu_;
    std::vector<std::uint32_t> code_;
};
} // namespace gravastar::rendering
