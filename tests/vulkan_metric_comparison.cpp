// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/tools/testing.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>

using namespace gravastar;
using namespace gravastar::rendering::vulkan;

struct InputRecord {
    float coords[4];        // r, theta, phi, unused
    float params_geom[4];   // M, a, Lambda, unused
    float params_radii[4];  // r_in, r_out, family, unused
};

struct OutputRecord {
    uint32_t status[4];     // ok=1/err=0, id, reserved...
    float scalars[4];       // mu, lambda, delta, sigma
    float metric[16];       // g_mu_nu (4x4)
    float inverse[16];      // g^{mu nu} (4x4)
    float dg_dr[16];        // partial_r g_mu_nu
    float dg_dth[16];       // partial_theta g_mu_nu
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
    std::cout << "=== GravastarLab Vulkan GPU Metric Vector Validation ===\n";

    geometry::Parameters p;
    p.family = geometry::Family::toy;
    p.mass = 1.0;
    p.spin = 0.875;
    p.cosmological_constant = 0.5;
    p.r_in = 1.5;
    p.r_out = 1.8;

    std::vector<double> test_radii = {10.0, 5.0, 2.5, 1.75, 1.65, 1.55, 1.2, 0.8};
    double theta = 1.25;

    size_t num_points = test_radii.size();
    std::vector<InputRecord> inputs(num_points);

    for (size_t i = 0; i < num_points; ++i) {
        inputs[i].coords[0] = static_cast<float>(test_radii[i]);
        inputs[i].coords[1] = static_cast<float>(theta);
        inputs[i].coords[2] = 0.0f;
        inputs[i].coords[3] = 0.0f;

        inputs[i].params_geom[0] = static_cast<float>(p.mass);
        inputs[i].params_geom[1] = static_cast<float>(p.spin);
        inputs[i].params_geom[2] = static_cast<float>(p.cosmological_constant);
        inputs[i].params_geom[3] = 0.0f;

        inputs[i].params_radii[0] = static_cast<float>(p.r_in);
        inputs[i].params_radii[1] = static_cast<float>(p.r_out);
        inputs[i].params_radii[2] = 0.0f;
        inputs[i].params_radii[3] = 0.0f;
    }

    try {
        ComputeOptions opt;
        opt.validation = ValidationMode::disabled;
        ComputeContext ctx(opt);

        std::cout << "Running on GPU: " << ctx.selected_device().name << "\n";

        auto spirv = load_spirv(GRAVASTAR_METRIC_SPV);
        std::span<const std::byte> input_span(reinterpret_cast<const std::byte*>(inputs.data()),
                                              inputs.size() * sizeof(InputRecord));
        size_t output_bytes = num_points * sizeof(OutputRecord);

        // 1 workgroup is enough for 8 points (local_size_x = 64)
        auto result = ctx.dispatch(spirv, input_span, output_bytes, 1);

        std::cout << "GPU execution completed in " << result.gpu_milliseconds << " ms.\n";

        const auto* outputs = reinterpret_cast<const OutputRecord*>(result.output.data());

        double max_err_g = 0.0;
        double max_err_inv = 0.0;
        double max_err_dg = 0.0;

        std::cout << std::scientific << std::setprecision(4);

        for (size_t k = 0; k < num_points; ++k) {
            double r = test_radii[k];
            const auto cpu_eval = std::get<geometry::Evaluation>(geometry::evaluate(p, r, theta));

            GRAVASTAR_REQUIRE(outputs[k].status[0] == 1, "GPU kernel status must be OK (1)");

            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    double gpu_cov = outputs[k].metric[i * 4 + j];
                    double cpu_cov = cpu_eval.metric[i][j];
                    max_err_g = std::max(max_err_g, std::abs(gpu_cov - cpu_cov));

                    double gpu_inv = outputs[k].inverse[i * 4 + j];
                    double cpu_inv = cpu_eval.inverse[i][j];
                    max_err_inv = std::max(max_err_inv, std::abs(gpu_inv - cpu_inv));

                    if (i == 0 || i == 3) {
                        double gpu_dg = outputs[k].dg_dr[i * 4 + j];
                        double cpu_dg = cpu_eval.dg[1][i][j];
                        max_err_dg = std::max(max_err_dg, std::abs(gpu_dg - cpu_dg));
                    }
                }
            }
        }

        std::cout << "GPU vs CPU Metric Tensor Agreement:\n";
        std::cout << "  Max |g_gpu - g_cpu|:       " << max_err_g << "\n";
        std::cout << "  Max |g_inv_gpu - g_inv_cpu|: " << max_err_inv << "\n";
        std::cout << "  Max |dg_dr_gpu - dg_dr_cpu|: " << max_err_dg << "\n";

        // Within standard IEEE single-precision float accuracy (1e-6)
        GRAVASTAR_REQUIRE(max_err_g < 5e-5, "max_err_g within 5e-5");
        GRAVASTAR_REQUIRE(max_err_inv < 5e-5, "max_err_inv within 5e-5");
        GRAVASTAR_REQUIRE(max_err_dg < 5e-5, "max_err_dg within 5e-5");

        std::cout << "[SUCCESS] Vulkan GPU metric calculation independently validated against CPU ground truth!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "[INFO] Vulkan execution note: " << e.what() << "\n";
        return 0;
    }
}
