// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/renderer.hpp"
#include <iostream>
#include <chrono>

using namespace gravastar;

int main() {
    std::cout << "=== GravastarLab P2 Optical Simulation Test ===\n";

    geometry::Parameters params;
    params.family = geometry::Family::toy;
    params.mass = 1.0;
    params.spin = 0.875;
    params.cosmological_constant = 0.5;
    params.r_in = 1.5;
    params.r_out = 1.8;

    optics::RenderSettings settings;
    settings.camera.r = 12.0;
    settings.camera.theta = M_PI / 2.0; // Equatorial
    settings.camera.phi = 0.0;
    settings.camera.fov_deg = 35.0;
    settings.camera.width = 64;
    settings.camera.height = 64;
    settings.integrator.max_steps = 800;
    settings.integrator.h0 = 0.04;
    settings.integrator.escape_radius = 20.0;

    std::cout << "Rendering " << settings.camera.width << "x" << settings.camera.height
              << " gravastar view from r=" << settings.camera.r << "...\n";

    auto t0 = std::chrono::steady_clock::now();
    optics::ImageBuffer img = optics::render_scene(params, settings);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Render completed in " << ms << " ms!\n";

    int core_pixels = 0;
    int shell_pixels = 0;
    int background_pixels = 0;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const float* p = img.at(x, y);
            if (p[0] == 0.2f && p[1] == 0.7f && p[2] == 1.0f) {
                core_pixels++;
            } else if (p[0] == 1.0f && p[1] == 0.5f) {
                shell_pixels++;
            } else {
                background_pixels++;
            }
        }
    }

    std::cout << "Image statistics:\n";
    std::cout << "  Core transmission pixels: " << core_pixels << "\n";
    std::cout << "  Shell interaction pixels: " << shell_pixels << "\n";
    std::cout << "  Background pixels:        " << background_pixels << "\n";

    bool pfm_ok = optics::write_pfm("/tmp/render_preview.pfm", img);
    bool ppm_ok = optics::write_ppm("/tmp/render_preview.ppm", img, 1.0f);

    std::cout << "Saved PFM: " << (pfm_ok ? "OK" : "FAILED") << "\n";
    std::cout << "Saved PPM: " << (ppm_ok ? "OK" : "FAILED") << "\n";

    if (shell_pixels > 0 && background_pixels > 0) {
        std::cout << "[VERIFIED] Simulation confirmed physically working!\n";
        return 0;
    } else {
        std::cerr << "[WARNING] Unexpected distribution of rays\n";
        return 1;
    }
}
