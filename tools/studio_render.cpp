// SPDX-License-Identifier: GPL-3.0-or-later
#include "studio_render.hpp"
#include <chrono>
#include <numbers>

namespace gravastar::studio {
namespace o = optics;

ProgressiveRender::ProgressiveRender(std::string trace_spirv)
    : spirv_(std::move(trace_spirv)),
      worker_([this](std::stop_token stop) { run(stop); }) {}

ProgressiveRender::~ProgressiveRender() {
    cancel_ = true;
    worker_.request_stop();
    wake_.notify_all();
}

void ProgressiveRender::start(RenderJob job) {
    std::lock_guard lock(mutex_);
    pending_ = std::move(job);
    cancel_ = true; // the current job stops at its next chunk
    wake_.notify_all();
}

bool ProgressiveRender::fetch(std::vector<std::uint32_t> &pixels, int &width, int &height) {
    std::lock_guard lock(mutex_);
    if (version_ == fetched_ || pixels_.empty())
        return false;
    pixels = pixels_;
    width = width_;
    height = height_;
    fetched_ = version_;
    return true;
}

RenderStatus ProgressiveRender::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

void ProgressiveRender::run(std::stop_token stop) {
    std::optional<rendering::GpuTracer> tracer;
    try {
        rendering::vulkan::ComputeOptions options;
        options.validation = rendering::vulkan::ValidationMode::automatic;
        tracer.emplace(spirv_.c_str(), options);
    } catch (const std::exception &e) {
        std::lock_guard lock(mutex_);
        status_.error = std::string("GPU tracer unavailable: ") + e.what();
        return;
    }
    while (!stop.stop_requested()) {
        RenderJob job;
        std::uint64_t generation;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, stop, [&] { return pending_.has_value(); });
            if (stop.stop_requested())
                return;
            job = std::move(*pending_);
            pending_.reset();
            cancel_ = false;
            generation = ++generation_;
            pixels_.assign(std::size_t(job.scene.width) * job.scene.height, 0xff100c0cU);
            width_ = job.scene.width;
            height_ = job.scene.height;
            status_ = {};
            status_.generation = generation;
            status_.rays_total = pixels_.size();
            status_.running = true;
            ++version_;
        }
        try {
            render(job, generation, *tracer);
        } catch (const std::exception &e) {
            std::lock_guard lock(mutex_);
            status_.error = e.what();
            status_.running = false;
        }
    }
}

void ProgressiveRender::render(const RenderJob &job, std::uint64_t generation,
                               rendering::GpuTracer &tracer) {
    const auto &s = job.scene;
    const int w = s.width, h = s.height;
    const auto patch =
        s.position[2] <= std::numbers::pi / 2 ? o::Chart::north : o::Chart::south;
    const auto begin = std::chrono::steady_clock::now();
    std::vector<char> traced(std::size_t(w) * h, 0);
    constexpr std::size_t chunk = 2048; // about 0.25 s of FP64 GPU work
    for (int level : {16, 8, 4, 2, 1}) {
        std::vector<int> xs, ys;
        for (int y = 0; y < h; y += level)
            for (int x = 0; x < w; x += level)
                if (!traced[std::size_t(y) * w + x]) {
                    xs.push_back(x);
                    ys.push_back(y);
                }
        {
            std::lock_guard lock(mutex_);
            status_.level = level;
        }
        for (std::size_t first = 0; first < xs.size(); first += chunk) {
            if (cancel_)
                return;
            const std::size_t count = std::min(chunk, xs.size() - first);
            std::vector<o::State> initial(count), start(count);
            for (std::size_t i = 0; i < count; ++i) {
                initial[i] = o::scene_pixel(s, xs[first + i], ys[first + i]);
                start[i] = o::transform_state(initial[i], o::Chart::boyer_lindquist, patch);
            }
            const auto result = tracer.trace(s, start, patch, chunk, &cancel_);
            if (result.cancelled)
                return;
            std::vector<std::uint32_t> colours(count);
            std::size_t tangent = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const auto &t = result.rays[i].trace;
                tangent += !t.tangencies.empty();
                const auto c = o::sample_color(t.outcome, o::ray_sample(s, initial[i], t));
                colours[i] = 0xff000000U | std::uint32_t(c[2]) << 16 | std::uint32_t(c[1]) << 8 |
                             std::uint32_t(c[0]);
            }
            std::lock_guard lock(mutex_);
            if (generation != generation_)
                return;
            for (std::size_t i = 0; i < count; ++i) {
                const int x0 = xs[first + i], y0 = ys[first + i];
                traced[std::size_t(y0) * w + x0] = 1;
                // Fill the block this sample stands for; finer levels overwrite it.
                for (int y = y0; y < std::min(h, y0 + level); ++y)
                    for (int x = x0; x < std::min(w, x0 + level); ++x)
                        if (level == 1 || !traced[std::size_t(y) * w + x] || (x == x0 && y == y0))
                            pixels_[std::size_t(y) * w + x] = colours[i];
            }
            status_.rays_done += count;
            status_.gpu_milliseconds += result.gpu_milliseconds;
            for (std::size_t r = 0; r < 7; ++r)
                status_.fallback[r] += result.fallback[r];
            status_.near_tangent += tangent;
            status_.seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
            ++version_;
        }
    }
    std::lock_guard lock(mutex_);
    if (generation == generation_) {
        status_.level = 0;
        status_.finished = true;
        status_.running = false;
    }
}
} // namespace gravastar::studio
