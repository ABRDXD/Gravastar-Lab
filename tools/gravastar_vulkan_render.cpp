// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/optics/camera.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cassert>
#include <iomanip>

using namespace gravastar;
using namespace gravastar::rendering::vulkan;

struct SceneParams {
    float cam_pos[4];          // r, theta, phi, tan_half_fov
    float params_geom[4];      // M, a, Lambda, aspect
    float params_radii[4];     // r_in, r_out, width, height
    float limits[4];           // escape_radius, h0, h_min, h_max
    uint32_t options[4];       // max_steps, render_mode, unused, unused

    float tetrad_e0[4];
    float tetrad_e1[4];
    float tetrad_e2[4];
    float tetrad_e3[4];

    float metric_diag[4];      // g00, g11, g22, g33
    float metric_off[4];       // g03, unused, unused, unused
};

struct Pixel {
    float r, g, b, a;
};

std::vector<uint32_t> load_spirv(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Failed to open SPIR-V file: " + path);
    size_t size = stream.tellg();
    stream.seekg(0);
    std::vector<uint32_t> spirv(size / sizeof(uint32_t));
    stream.read(reinterpret_cast<char*>(spirv.data()), size);
    return spirv;
}

inline uint8_t to_byte(float val) {
    val = std::clamp(val, 0.0f, 1.0f);
    return static_cast<uint8_t>(val * 255.0f + 0.5f);
}

void write_ppm(const std::string& path, int width, int height, const Pixel* pixels) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write PPM: " + path);
    out << "P6\n" << width << " " << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        uint8_t rgb[3] = {to_byte(pixels[i].r), to_byte(pixels[i].g), to_byte(pixels[i].b)};
        out.write(reinterpret_cast<const char*>(rgb), 3);
    }
}

int main(int argc, char** argv) {
    std::cout << "=== GravastarLab Vulkan GPU Headless Raymarcher ===\n";

    int width = 128;
    int height = 128;
    if (argc >= 3) {
        width = std::atoi(argv[1]);
        height = std::atoi(argv[2]);
    }

    geometry::Parameters p;
    p.family = geometry::Family::toy;
    p.mass = 1.0;
    p.spin = 0.875;
    p.cosmological_constant = 0.5;
    p.r_in = 1.5;
    p.r_out = 1.8;

    double cam_r = 10.0;
    double cam_theta = 1.25;
    double cam_phi = 0.0;
    double fov_deg = 60.0;

    const auto eval_res = geometry::evaluate(p, cam_r, cam_theta);
    if (!std::holds_alternative<geometry::Evaluation>(eval_res)) {
        throw std::runtime_error("Failed to evaluate geometry at camera position");
    }
    const auto& geom = std::get<geometry::Evaluation>(eval_res);

    optics::Tetrad tetrad = optics::make_observer_tetrad(p, cam_r, cam_theta);
    double tan_half_fov = std::tan((fov_deg * M_PI / 180.0) * 0.5);
    double aspect = static_cast<double>(width) / static_cast<double>(height);

    SceneParams sp{};
    sp.cam_pos[0] = static_cast<float>(cam_r);
    sp.cam_pos[1] = static_cast<float>(cam_theta);
    sp.cam_pos[2] = static_cast<float>(cam_phi);
    sp.cam_pos[3] = static_cast<float>(tan_half_fov);

    sp.params_geom[0] = static_cast<float>(p.mass);
    sp.params_geom[1] = static_cast<float>(p.spin);
    sp.params_geom[2] = static_cast<float>(p.cosmological_constant);
    sp.params_geom[3] = static_cast<float>(aspect);

    sp.params_radii[0] = static_cast<float>(p.r_in);
    sp.params_radii[1] = static_cast<float>(p.r_out);
    sp.params_radii[2] = static_cast<float>(width);
    sp.params_radii[3] = static_cast<float>(height);

    sp.limits[0] = 25.0f;   // escape_radius
    sp.limits[1] = 0.01f;   // h0
    sp.limits[2] = 1e-5f;   // h_min
    sp.limits[3] = 0.05f;   // h_max

    sp.options[0] = 2500;   // max_steps
    sp.options[1] = 0;

    for (int i = 0; i < 4; ++i) {
        sp.tetrad_e0[i] = static_cast<float>(tetrad.e0[i]);
        sp.tetrad_e1[i] = static_cast<float>(tetrad.e1[i]);
        sp.tetrad_e2[i] = static_cast<float>(tetrad.e2[i]);
        sp.tetrad_e3[i] = static_cast<float>(tetrad.e3[i]);
    }

    sp.metric_diag[0] = static_cast<float>(geom.metric[0][0]);
    sp.metric_diag[1] = static_cast<float>(geom.metric[1][1]);
    sp.metric_diag[2] = static_cast<float>(geom.metric[2][2]);
    sp.metric_diag[3] = static_cast<float>(geom.metric[3][3]);
    sp.metric_off[0]  = static_cast<float>(geom.metric[0][3]);

    try {
        ComputeOptions opt;
        opt.validation = ValidationMode::disabled;
        ComputeContext ctx(opt);

        std::cout << "Rendering on GPU: " << ctx.selected_device().name << "\n";
        std::cout << "Resolution: " << width << "x" << height << " (" << (width * height) << " rays)\n";

        auto spirv = load_spirv(GRAVASTAR_RAYMARCH_SPV);
        std::span<const std::byte> input_span(reinterpret_cast<const std::byte*>(&sp), sizeof(SceneParams));
        size_t output_bytes = width * height * sizeof(Pixel);
        uint32_t workgroups = (width * height + 63) / 64;

        auto result = ctx.dispatch(spirv, input_span, output_bytes, workgroups);

        std::cout << "GPU Raymarch completed in " << std::fixed << std::setprecision(2)
                  << result.gpu_milliseconds << " ms.\n";
        double mrays_per_sec = (width * height / 1e6) / (result.gpu_milliseconds / 1000.0);
        std::cout << "Throughput: " << mrays_per_sec << " Megarays/sec.\n";

        const auto* pixels = reinterpret_cast<const Pixel*>(result.output.data());
        write_ppm("results/vulkan_gravastar_render.ppm", width, height, pixels);
        std::cout << "Wrote output image to results/vulkan_gravastar_render.ppm\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Vulkan raymarch failed: " << e.what() << "\n";
        return 1;
    }
}
