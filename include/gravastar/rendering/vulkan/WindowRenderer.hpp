// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include "gravastar/rendering/vulkan/SurfaceSource.hpp"
#include <array>
#include <filesystem>
#include <span>

namespace gravastar::rendering::vulkan {

enum class FrameStatus { presented, suspended, retry };

struct WindowStatistics final {
    std::uint64_t presented{}, recreations{}, suspended{}, acquire_retries{};
    std::uint64_t overlay_frames{}; // frames whose ImGui draw data was rendered
    std::uint32_t width{}, height{}, validation_errors{};
};

class WindowRenderer final {
  public:
    WindowRenderer(const SurfaceSource &source, ComputeOptions options,
                   std::span<const std::uint32_t> shader);
    ~WindowRenderer();
    WindowRenderer(const WindowRenderer &) = delete;
    WindowRenderer &operator=(const WindowRenderer &) = delete;
    [[nodiscard]] FrameStatus draw(std::uint32_t width, std::uint32_t height,
                                   std::uint32_t phase = 0);
    void capture(const std::filesystem::path &path);
    // ImGui overlay drawn over the presented image (not over captures). The
    // caller owns the ImGui context and its GLFW backend, which must outlive
    // this renderer. Call before the first draw(); it builds the swapchain
    // at the given size. Each frame, only while overlay_ready():
    // begin_overlay_frame(), build UI, ImGui::Render(), then draw().
    void enable_overlay(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] bool overlay_ready() const;
    void begin_overlay_frame();
    // Image for a view shader to read at binding 1 (RGBA8, r in the low byte,
    // row-major, width x height <= 65535 each). Waits for the previous frame.
    // Afterwards capture() no longer checks the calibration pattern.
    void set_source(std::uint32_t width, std::uint32_t height,
                    std::span<const std::uint32_t> rgba8);
    void finish();
    [[nodiscard]] WindowStatistics statistics() const;
    [[nodiscard]] const DeviceInfo &device() const;
    [[nodiscard]] bool validation_enabled() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gravastar::rendering::vulkan
