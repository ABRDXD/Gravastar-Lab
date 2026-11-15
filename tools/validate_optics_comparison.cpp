// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/integration/integrator.hpp"
#include "gravastar/optics/camera.hpp"
#include "gravastar/optics/renderer.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cmath>
#include <chrono>

using namespace gravastar;

enum class DetailedOutcome : uint8_t {
    ExteriorEscaped = 0,    // Stayed outside shell (r_min >= r_out) and escaped
    ShellTurnEscaped = 1,   // Entered shell (r_min < r_out but >= r_in), turned and escaped
    CoreTurnEscaped = 2,    // Entered core (r_min < r_in), turned/passed and escaped
    CapturedHorizon = 3,    // Hit event horizon (r <= r_plus) [Kerr]
    ExhaustedSteps = 4,     // Hit max_steps before escaping
    ChartBoundary = 5       // Axis or domain boundary
};

struct RayDetail {
    DetailedOutcome outcome;
    double min_r;
    double final_th;
    double final_ph;
    int steps;
};

// Specialized tracer that records detailed fate
RayDetail trace_detailed(const geometry::Parameters& p,
                         const integration::Vec4& x0,
                         const integration::Vec4& p0,
                         const integration::IntegratorOptions& opt) {
    // We can call trace_ray or step manually
    // Let's use trace_ray
    auto res = integration::trace_ray(p, x0, p0, opt);

    double r_plus = p.mass + std::sqrt(std::max(0.0, p.mass * p.mass - p.spin * p.spin));

    DetailedOutcome out;
    if (res.outcome == integration::RayOutcome::Escaped || res.outcome == integration::RayOutcome::CorePassed) {
        if (res.min_r >= p.r_out) {
            out = DetailedOutcome::ExteriorEscaped;
        } else if (res.min_r >= p.r_in) {
            out = DetailedOutcome::ShellTurnEscaped;
        } else {
            out = DetailedOutcome::CoreTurnEscaped;
        }
    } else if (p.family == geometry::Family::kerr && res.min_r <= r_plus * 1.01) {
        out = DetailedOutcome::CapturedHorizon;
    } else if (res.steps >= opt.max_steps) {
        out = DetailedOutcome::ExhaustedSteps;
    } else {
        out = DetailedOutcome::ChartBoundary;
    }

    return {out, res.min_r, res.final_x[2], res.final_x[3], res.steps};
}

int main() {
    std::cout << "=== GravastarLab Stage P2 Optical Validation Suite ===\n\n";

    geometry::Parameters p_grav;
    p_grav.family = geometry::Family::toy;
    p_grav.mass = 1.0;
    p_grav.spin = 0.875;
    p_grav.cosmological_constant = 0.5;
    p_grav.r_in = 1.5;
    p_grav.r_out = 1.8;

    geometry::Parameters p_kerr = p_grav;
    p_kerr.family = geometry::Family::kerr;

    optics::CameraConfig cam;
    cam.r = 15.0;
    cam.theta = 1.48; // ~85 deg, near equatorial with slight incline to show 2D shadow structure
    cam.phi = 0.0;
    cam.fov_deg = 35.0;
    cam.width = 128;
    cam.height = 128;

    integration::IntegratorOptions opt;
    opt.max_steps = 2500;
    opt.h0 = 0.02;
    opt.escape_radius = 25.0;

    std::cout << "Camera: r=" << cam.r << ", theta=" << cam.theta
              << ", FOV=" << cam.fov_deg << " deg, Res=" << cam.width << "x" << cam.height << "\n";
    std::cout << "Tracing " << cam.width * cam.height << " rays for Gravastar and Kerr...\n";

    auto eval_grav = std::get<geometry::Evaluation>(geometry::evaluate(p_grav, cam.r, cam.theta));
    auto eval_kerr = std::get<geometry::Evaluation>(geometry::evaluate(p_kerr, cam.r, cam.theta));

    auto tetrad_grav = optics::make_observer_tetrad(p_grav, cam.r, cam.theta);
    auto tetrad_kerr = optics::make_observer_tetrad(p_kerr, cam.r, cam.theta);

    double tan_half_fov = std::tan((cam.fov_deg * M_PI / 180.0) * 0.5);
    double aspect = (double)cam.width / (double)cam.height;

    integration::Vec4 x0 = {0.0, cam.r, cam.theta, cam.phi};

    optics::ImageBuffer img_grav; img_grav.width = cam.width; img_grav.height = cam.height;
    img_grav.data.assign(cam.width * cam.height * 3, 0.0f);

    optics::ImageBuffer img_kerr; img_kerr.width = cam.width; img_kerr.height = cam.height;
    img_kerr.data.assign(cam.width * cam.height * 3, 0.0f);

    optics::ImageBuffer img_mask; img_mask.width = cam.width; img_mask.height = cam.height;
    img_mask.data.assign(cam.width * cam.height * 3, 0.0f);

    std::vector<RayDetail> details_grav(cam.width * cam.height);
    std::vector<RayDetail> details_kerr(cam.width * cam.height);

    double max_diff_th = 0.0;
    double max_diff_ph = 0.0;
    int exterior_count = 0;

    int count_ext = 0;
    int count_shell_turn = 0;
    int count_core_turn = 0;
    int count_exhausted = 0;
    int count_boundary = 0;
    int count_kerr_captured = 0;

    auto t0 = std::chrono::steady_clock::now();

    #pragma omp parallel for collapse(2) reduction(max:max_diff_th, max_diff_ph) reduction(+:exterior_count, count_ext, count_shell_turn, count_core_turn, count_exhausted, count_boundary, count_kerr_captured) schedule(dynamic)
    for (int y = 0; y < cam.height; ++y) {
        for (int x = 0; x < cam.width; ++x) {
            int idx = y * cam.width + x;
            double ndc_x = (2.0 * (x + 0.5) / cam.width) - 1.0;
            double ndc_y = 1.0 - (2.0 * (y + 0.5) / cam.height);

            // Gravastar ray
            auto p0_grav = optics::pixel_ray_momentum(tetrad_grav, eval_grav, ndc_x, ndc_y, tan_half_fov, aspect);
            auto r_grav = trace_detailed(p_grav, x0, p0_grav, opt);
            details_grav[idx] = r_grav;

            // Kerr ray
            auto p0_kerr = optics::pixel_ray_momentum(tetrad_kerr, eval_kerr, ndc_x, ndc_y, tan_half_fov, aspect);
            auto r_kerr = trace_detailed(p_kerr, x0, p0_kerr, opt);
            details_kerr[idx] = r_kerr;

            // Stats
            if (r_grav.outcome == DetailedOutcome::ExteriorEscaped) count_ext++;
            else if (r_grav.outcome == DetailedOutcome::ShellTurnEscaped) count_shell_turn++;
            else if (r_grav.outcome == DetailedOutcome::CoreTurnEscaped) count_core_turn++;
            else if (r_grav.outcome == DetailedOutcome::ExhaustedSteps) count_exhausted++;
            else count_boundary++;

            if (r_kerr.outcome == DetailedOutcome::CapturedHorizon) count_kerr_captured++;

            // Checkerboard sky helper
            auto checker = [](double th, double ph) -> float {
                int u = static_cast<int>(std::floor(ph * 10.0 / M_PI));
                int v = static_cast<int>(std::floor(th * 10.0 / M_PI));
                return ((u + v) % 2 == 0) ? 0.9f : 0.15f;
            };

            // Color Gravastar
            float* pix_g = img_grav.at(x, y);
            if (r_grav.outcome == DetailedOutcome::ExteriorEscaped ||
                r_grav.outcome == DetailedOutcome::ShellTurnEscaped ||
                r_grav.outcome == DetailedOutcome::CoreTurnEscaped) {
                float c = checker(r_grav.final_th, r_grav.final_ph);
                if (r_grav.outcome == DetailedOutcome::CoreTurnEscaped) {
                    pix_g[0] = 0.2f * c; pix_g[1] = 0.7f * c; pix_g[2] = 1.0f * c; // Cyan tint
                } else if (r_grav.outcome == DetailedOutcome::ShellTurnEscaped) {
                    pix_g[0] = 1.0f * c; pix_g[1] = 0.6f * c; pix_g[2] = 0.2f * c; // Amber tint
                } else {
                    pix_g[0] = c; pix_g[1] = c; pix_g[2] = c; // White/gray sky
                }
            } else {
                pix_g[0] = pix_g[1] = pix_g[2] = 0.0f; // Trapped / Axis
            }

            // Color Kerr
            float* pix_k = img_kerr.at(x, y);
            if (r_kerr.outcome == DetailedOutcome::ExteriorEscaped ||
                r_kerr.outcome == DetailedOutcome::ShellTurnEscaped ||
                r_kerr.outcome == DetailedOutcome::CoreTurnEscaped) {
                float c = checker(r_kerr.final_th, r_kerr.final_ph);
                pix_k[0] = c; pix_k[1] = c; pix_k[2] = c;
            } else {
                pix_k[0] = pix_k[1] = pix_k[2] = 0.0f; // Black Hole shadow
            }

            // Mask image: Color by exact outcome!
            float* pix_m = img_mask.at(x, y);
            switch (r_grav.outcome) {
                case DetailedOutcome::ExteriorEscaped:
                    pix_m[0] = 0.2f; pix_m[1] = 0.2f; pix_m[2] = 0.2f; // Gray: Exterior escaped
                    break;
                case DetailedOutcome::ShellTurnEscaped:
                    pix_m[0] = 1.0f; pix_m[1] = 0.6f; pix_m[2] = 0.0f; // Orange: Turned in shell
                    break;
                case DetailedOutcome::CoreTurnEscaped:
                    pix_m[0] = 0.0f; pix_m[1] = 0.8f; pix_m[2] = 1.0f; // Cyan: Traversed core & escaped
                    break;
                case DetailedOutcome::ExhaustedSteps:
                    pix_m[0] = 0.8f; pix_m[1] = 0.0f; pix_m[2] = 0.0f; // Red: Exhausted steps
                    break;
                case DetailedOutcome::ChartBoundary:
                    pix_m[0] = 0.6f; pix_m[1] = 0.0f; pix_m[2] = 0.8f; // Purple: Axis / Boundary
                    break;
                default:
                    pix_m[0] = pix_m[1] = pix_m[2] = 0.0f;
            }

            // Compare sky directions for exterior rays
            if (r_grav.min_r >= p_grav.r_out && r_kerr.min_r >= p_kerr.r_out) {
                if ((r_grav.outcome == DetailedOutcome::ExteriorEscaped) &&
                    (r_kerr.outcome == DetailedOutcome::ExteriorEscaped)) {
                    double dth = std::abs(r_grav.final_th - r_kerr.final_th);
                    double dph = std::abs(r_grav.final_ph - r_kerr.final_ph);
                    max_diff_th = std::max(max_diff_th, dth);
                    max_diff_ph = std::max(max_diff_ph, dph);
                    exterior_count++;
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Rendering completed in " << ms << " ms (" << ms / 1000.0 << " s).\n\n";

    std::cout << "--- Gravastar Ray Outcome Breakdown (" << cam.width * cam.height << " total rays) ---\n";
    std::cout << "  1. Exterior Escaped (shared Kerr geometry, r_min >= 1.8): "
              << count_ext << " (" << 100.0 * count_ext / (cam.width * cam.height) << "%)\n";
    std::cout << "  2. Shell-Turn Escaped (1.5 <= r_min < 1.8):             "
              << count_shell_turn << " (" << 100.0 * count_shell_turn / (cam.width * cam.height) << "%)\n";
    std::cout << "  3. Core-Passed Escaped (r_min < 1.5):                   "
              << count_core_turn << " (" << 100.0 * count_core_turn / (cam.width * cam.height) << "%)\n";
    std::cout << "  4. Step-Limit Exhausted (within budget 2500):            "
              << count_exhausted << " (" << 100.0 * count_exhausted / (cam.width * cam.height) << "%)\n";
    std::cout << "  5. Coordinate Axis Boundary (theta -> 0 or pi):         "
              << count_boundary << " (" << 100.0 * count_boundary / (cam.width * cam.height) << "%)\n\n";

    std::cout << "--- Kerr Black Hole Breakdown ---\n";
    std::cout << "  Captured by Event Horizon (r <= r_+ = 1.484):            "
              << count_kerr_captured << " (" << 100.0 * count_kerr_captured / (cam.width * cam.height) << "%)\n\n";

    std::cout << "--- Shared Exterior Lensing Numerical Agreement ---\n";
    std::cout << "  Compared Rays (both stayed r >= 1.8): " << exterior_count << "\n";
    std::cout << "  Max |theta_grav - theta_kerr|: " << std::scientific << std::setprecision(6) << max_diff_th << " rad\n";
    std::cout << "  Max |phi_grav - phi_kerr|:     " << std::scientific << std::setprecision(6) << max_diff_ph << " rad\n";

    // Write outputs to results/
    system("mkdir -p results");
    optics::write_ppm("results/gravastar_checker.ppm", img_grav, 1.0f);
    optics::write_ppm("results/kerr_checker.ppm", img_kerr, 1.0f);
    optics::write_ppm("results/ray_outcome_mask.ppm", img_mask, 1.0f);

    std::cout << "\nSaved renders:\n";
    std::cout << "  - results/gravastar_checker.ppm\n";
    std::cout << "  - results/kerr_checker.ppm\n";
    std::cout << "  - results/ray_outcome_mask.ppm\n";

    return 0;
}
