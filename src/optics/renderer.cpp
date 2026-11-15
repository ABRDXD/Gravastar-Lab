// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/renderer.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace gravastar::optics {

namespace {

// Simple Reinhard tone mapping from linear HDR radiance to [0, 1] sRGB
inline float tone_map(float hdr, float exposure) {
    float x = hdr * exposure;
    return std::clamp(x / (1.0f + x), 0.0f, 1.0f);
}

} // namespace

ImageBuffer render_scene(const geometry::Parameters& params, const RenderSettings& settings) {
    ImageBuffer buffer;
    buffer.width = settings.camera.width;
    buffer.height = settings.camera.height;
    buffer.data.assign(buffer.width * buffer.height * 3, 0.0f);

    const auto eval_res = geometry::evaluate(params, settings.camera.r, settings.camera.theta);
    if (std::holds_alternative<geometry::Error>(eval_res)) {
        std::cerr << "[RENDER ERROR] Observer position invalid\n";
        return buffer;
    }
    const auto& geom = std::get<geometry::Evaluation>(eval_res);

    Tetrad tetrad = make_observer_tetrad(params, settings.camera.r, settings.camera.theta);
    const double tan_half_fov = std::tan((settings.camera.fov_deg * M_PI / 180.0) * 0.5);
    const double aspect = static_cast<double>(buffer.width) / static_cast<double>(buffer.height);

    const Vec4 x0 = {0.0, settings.camera.r, settings.camera.theta, settings.camera.phi};

    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int y = 0; y < buffer.height; ++y) {
        for (int x = 0; x < buffer.width; ++x) {
            // Screen coordinates NDC in [-1, 1]
            const double ndc_x = (2.0 * (x + 0.5) / buffer.width) - 1.0;
            const double ndc_y = 1.0 - (2.0 * (y + 0.5) / buffer.height); // top to bottom

            const Vec4 p0 = pixel_ray_momentum(tetrad, geom, ndc_x, ndc_y, tan_half_fov, aspect);

            const auto result = integration::trace_ray(params, x0, p0, settings.integrator);

            float* pixel = buffer.at(x, y);

            if (result.outcome == integration::RayOutcome::CorePassed) {
                // Ray penetrated through the shell into the de Sitter core and escaped
                // Gravastar interior optical signature: distinct energetic core luminescence
                pixel[0] = 0.2f;
                pixel[1] = 0.7f;
                pixel[2] = 1.0f; // Cyan-blue core refraction
            } else if (result.crossed_shell_in) {
                // Skimmed shell
                pixel[0] = 1.0f;
                pixel[1] = 0.5f;
                pixel[2] = 0.1f; // Amber ring
            } else if (result.outcome == integration::RayOutcome::Escaped) {
                // Background sky with simple celestial coordinates pattern
                double final_th = result.final_x[2];
                double final_ph = result.final_x[3];
                // Checker / grid stars
                int u = static_cast<int>(std::floor(final_ph * 3.0 / M_PI));
                int v = static_cast<int>(std::floor(final_th * 6.0 / M_PI));
                if ((u + v) % 2 == 0) {
                    pixel[0] = 0.05f; pixel[1] = 0.05f; pixel[2] = 0.08f;
                } else {
                    pixel[0] = 0.02f; pixel[1] = 0.02f; pixel[2] = 0.03f;
                }
            } else {
                // Trapped / Captured
                pixel[0] = 0.0f;
                pixel[1] = 0.0f;
                pixel[2] = 0.0f;
            }
        }
    }

    return buffer;
}

bool write_pfm(const std::string& filepath, const ImageBuffer& image) {
    if (image.width <= 0 || image.height <= 0) return false;
    std::ofstream stream(filepath, std::ios::binary);
    if (!stream) return false;

    stream << "PF\n" << image.width << " " << image.height << "\n-1.0\n";
    for (int y = image.height - 1; y >= 0; --y) {
        stream.write(reinterpret_cast<const char*>(image.at(0, y)),
                     static_cast<std::streamsize>(sizeof(float) * 3 * image.width));
    }
    return stream.good();
}

bool write_ppm(const std::string& filepath, const ImageBuffer& image, float exposure) {
    if (image.width <= 0 || image.height <= 0) return false;
    std::ofstream stream(filepath, std::ios::binary);
    if (!stream) return false;

    stream << "P6\n" << image.width << " " << image.height << "\n255\n";
    std::vector<unsigned char> bytes(image.width * 3);

    for (int y = 0; y < image.height; ++y) {
        const float* row = image.at(0, y);
        for (int i = 0; i < image.width * 3; ++i) {
            bytes[i] = static_cast<unsigned char>(tone_map(row[i], exposure) * 255.0f);
        }
        stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    return stream.good();
}

} // namespace gravastar::optics
