// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include "gravastar/integration/integrator.hpp"
#include "gravastar/optics/camera.hpp"
#include <string>
#include <vector>

namespace gravastar::optics {

struct ImageBuffer {
    int width{0};
    int height{0};
    std::vector<float> data; // RGB float32 per pixel (width * height * 3)

    float* at(int x, int y) {
        return &data[3 * (y * width + x)];
    }
    const float* at(int x, int y) const {
        return &data[3 * (y * width + x)];
    }
};

struct RenderSettings {
    CameraConfig camera;
    integration::IntegratorOptions integrator;
    bool show_interior_glow{true};
    bool show_shell_ring{true};
};

// Headless reference CPU renderer that shoots rays for each pixel
ImageBuffer render_scene(const geometry::Parameters& params, const RenderSettings& settings);

// Saves the rendered HDR image to Portable Float Map (PFM)
bool write_pfm(const std::string& filepath, const ImageBuffer& image);

// Saves to a simple Netpbm PPM format (8-bit sRGB with tone mapping) for instant viewing
bool write_ppm(const std::string& filepath, const ImageBuffer& image, float exposure = 1.0f);

} // namespace gravastar::optics
