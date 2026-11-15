// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/rendering/vulkan/SurfaceSource.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace gravastar::platform {

// Main-thread, single-window platform adapter using GLFW 3.4.
class Window final {
  public:
    Window(int width, int height, const std::string &title);
    ~Window();
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    [[nodiscard]] rendering::vulkan::SurfaceSource surface_source();
    [[nodiscard]] std::array<std::uint32_t, 2> framebuffer_size() const;
    [[nodiscard]] bool should_close() const;
    [[nodiscard]] bool minimized() const;
    void poll();
    void wait(double seconds);
    void title(const std::string &text);
    void resize(int width, int height);
    void minimize();
    void restore();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gravastar::platform
