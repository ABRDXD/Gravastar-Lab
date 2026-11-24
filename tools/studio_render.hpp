// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Progressive studio render on a worker thread. The worker owns an FP64
// GpuTracer (docs/GPU_TRACER.md) and refines the image coarse to fine: one
// ray per 16x16 block, then 8, 4, 2 and 1, each sample filling its block
// until finer samples replace it. A new job cancels the current one between
// chunks and restarts from the coarsest level.
#include "gravastar/optics/scene.hpp"
#include "gravastar/rendering/GpuTracer.hpp"
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace gravastar::studio {
struct RenderJob {
    optics::Scene scene;   // width/height are the render resolution
    std::string name;
};
struct RenderStatus {
    std::uint64_t generation{};
    int level{};                       // current block size (16..1), 0 when done
    std::size_t rays_done{}, rays_total{};
    double gpu_milliseconds{}, seconds{};
    std::array<std::size_t, 7> fallback{}; // GpuTracer::reasons
    std::size_t near_tangent{};        // CPU re-traces that recorded a tangency
    bool finished{}, running{};
    std::string error;
};

class ProgressiveRender final {
  public:
    explicit ProgressiveRender(std::string trace_spirv);
    ~ProgressiveRender();
    ProgressiveRender(const ProgressiveRender &) = delete;
    ProgressiveRender &operator=(const ProgressiveRender &) = delete;
    void start(RenderJob job); // cancels any current job
    // Copies the latest image if it changed since the last call.
    bool fetch(std::vector<std::uint32_t> &pixels, int &width, int &height);
    [[nodiscard]] RenderStatus status() const;

  private:
    void run(std::stop_token stop);
    void render(const RenderJob &job, std::uint64_t generation, rendering::GpuTracer &tracer);

    std::string spirv_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::optional<RenderJob> pending_;
    std::atomic<bool> cancel_{false};
    std::uint64_t generation_{};
    std::vector<std::uint32_t> pixels_;
    int width_{}, height_{};
    std::uint64_t version_{}, fetched_{};
    RenderStatus status_;
    std::jthread worker_; // last: started after the members it uses
};
} // namespace gravastar::studio
