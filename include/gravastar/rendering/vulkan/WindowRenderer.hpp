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
    void finish();
    [[nodiscard]] WindowStatistics statistics() const;
    [[nodiscard]] const DeviceInfo &device() const;
    [[nodiscard]] bool validation_enabled() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gravastar::rendering::vulkan
