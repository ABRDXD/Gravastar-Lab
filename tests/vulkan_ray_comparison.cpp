// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/integration/integrator.hpp"
#include "gravastar/tools/testing.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>

using namespace gravastar;
using namespace gravastar::rendering::vulkan;

struct RayInput {
    float x0[4];            // t, r, theta, phi
    float p0[4];            // p_t, p_r, p_theta, p_phi
    float params_geom[4];   // M, a, Lambda, unused
    float params_radii[4];  // r_in, r_out, unused, unused
    float limits[4];        // escape_radius, h0, h_min, h_max
    uint32_t options[4];    // max_steps, unused, unused, unused
};

struct RayOutput {
    float final_x[4];
    float final_p[4];
    float stats[4];         // min_r, max_null_drift, final_lambda, outcome
    uint32_t counts[4];     // steps, crossed_shell_in, crossed_shell_out, status
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

int main() {
    std::cout << "=== GravastarLab Vulkan GPU Geodesic Trajectory Validation ===\n";

    geometry::Parameters p;
    p.family = geometry::Family::toy;
    p.mass = 1.0;
    p.spin = 0.875;
    p.cosmological_constant = 0.5;
    p.r_in = 1.5;
    p.r_out = 1.8;

    struct Target {
        double b;
        double expected_r;
        const char* label;
        bool expected_core;
    };

    Target targets[] = {
        {+1.80, 1.438750388, "Enters core", true},
        {+1.95, 1.551795042, "Turns in shell", false},
        {-3.60, 1.490704364, "Enters core", true},
        {-3.70, 1.511352187, "Turns in shell", false},
    };

    double r0 = 10.0;
    double th0 = M_PI / 2.0;
    const auto res0 = geometry::evaluate(p, r0, th0);
    GRAVASTAR_REQUIRE(std::holds_alternative<geometry::Evaluation>(res0), "Valid evaluation at r0, th0");
    const auto& g0 = std::get<geometry::Evaluation>(res0);

    size_t num_rays = 4;
    std::vector<RayInput> inputs(num_rays);
    std::vector<integration::TraceResult> cpu_results(num_rays);

    for (size_t i = 0; i < num_rays; ++i) {
        double E = 1.0;
        double pt = -E;
        double pphi = targets[i].b * E;
        double quad = g0.inverse[0][0] * pt * pt +
                      2.0 * g0.inverse[0][3] * pt * pphi +
                      g0.inverse[3][3] * pphi * pphi;
        double pr = -std::sqrt(-quad / g0.inverse[1][1]);

        integration::Vec4 x0 = {0.0, r0, th0, 0.0};
        integration::Vec4 p0 = {pt, pr, 0.0, pphi};

        integration::IntegratorOptions opt;
        opt.max_steps = 25000;
        opt.h0 = 0.005;
        opt.h_min = 1e-5;
        opt.h_max = 0.05;
        opt.escape_radius = 25.0;

        cpu_results[i] = integration::trace_ray(p, x0, p0, opt);

        // Fill GPU RayInput
        inputs[i].x0[0] = static_cast<float>(x0[0]);
        inputs[i].x0[1] = static_cast<float>(x0[1]);
        inputs[i].x0[2] = static_cast<float>(x0[2]);
        inputs[i].x0[3] = static_cast<float>(x0[3]);

        inputs[i].p0[0] = static_cast<float>(p0[0]);
        inputs[i].p0[1] = static_cast<float>(p0[1]);
        inputs[i].p0[2] = static_cast<float>(p0[2]);
        inputs[i].p0[3] = static_cast<float>(p0[3]);

        inputs[i].params_geom[0] = static_cast<float>(p.mass);
        inputs[i].params_geom[1] = static_cast<float>(p.spin);
        inputs[i].params_geom[2] = static_cast<float>(p.cosmological_constant);
        inputs[i].params_geom[3] = 0.0f;

        inputs[i].params_radii[0] = static_cast<float>(p.r_in);
        inputs[i].params_radii[1] = static_cast<float>(p.r_out);
        inputs[i].params_radii[2] = 0.0f;
        inputs[i].params_radii[3] = 0.0f;

        inputs[i].limits[0] = static_cast<float>(opt.escape_radius);
        inputs[i].limits[1] = static_cast<float>(opt.h0);
        inputs[i].limits[2] = static_cast<float>(opt.h_min);
        inputs[i].limits[3] = static_cast<float>(opt.h_max);

        inputs[i].options[0] = static_cast<uint32_t>(opt.max_steps);
        inputs[i].options[1] = 0;
        inputs[i].options[2] = 0;
        inputs[i].options[3] = 0;
    }

    try {
        ComputeOptions opt;
        opt.validation = ValidationMode::disabled;
        ComputeContext ctx(opt);

        std::cout << "Dispatching to GPU: " << ctx.selected_device().name << "\n";

        auto spirv = load_spirv(GRAVASTAR_GEODESIC_SPV);
        std::span<const std::byte> input_span(reinterpret_cast<const std::byte*>(inputs.data()),
                                              inputs.size() * sizeof(RayInput));
        size_t output_bytes = num_rays * sizeof(RayOutput);

        auto result = ctx.dispatch(spirv, input_span, output_bytes, 1);
        std::cout << "GPU geodesic execution finished in " << result.gpu_milliseconds << " ms.\n";

        const auto* outputs = reinterpret_cast<const RayOutput*>(result.output.data());

        std::cout << std::fixed << std::setprecision(7);
        std::cout << "\n|   b   | CPU min_r | GPU min_r |   |Diff|   | CPU Core? | GPU Core? | Status |\n";
        std::cout << "|:-----:|:---------:|:---------:|:----------:|:---------:|:---------:|:------:|\n";

        for (size_t i = 0; i < num_rays; ++i) {
            double cpu_min_r = cpu_results[i].min_r;
            float gpu_min_r = outputs[i].stats[0];
            double diff = std::abs(cpu_min_r - static_cast<double>(gpu_min_r));

            bool cpu_core = cpu_results[i].crossed_shell_in;
            bool gpu_core = (outputs[i].counts[1] == 1);

            std::cout << "| " << std::setw(5) << targets[i].b << " | "
                      << std::setw(9) << cpu_min_r << " | "
                      << std::setw(9) << gpu_min_r << " | "
                      << std::setw(10) << diff << " | "
                      << std::setw(9) << (cpu_core ? "YES" : "NO") << " | "
                      << std::setw(9) << (gpu_core ? "YES" : "NO") << " | "
                      << (diff < 2e-3 && (cpu_core == gpu_core) ? "PASS" : "FAIL") << " |\n";

            GRAVASTAR_REQUIRE(cpu_core == gpu_core, "Core crossing agreement between CPU and GPU");
            GRAVASTAR_REQUIRE(diff < 2e-3, "Turning radius diff within single-precision tolerance 2e-3");
        }

        std::cout << "\nAll GPU vs CPU geodesic trajectory comparisons PASSED!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Validation failed with error: " << e.what() << "\n";
        return 1;
    }
}
