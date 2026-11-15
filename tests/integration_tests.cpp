// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/integration/integrator.hpp"
#include "gravastar/tools/testing.hpp"
#include <iostream>
#include <cmath>

using namespace gravastar;

int main() {
    std::cout << "[TEST] Running null geodesic ray tracing tests..." << std::endl;

    geometry::Parameters params;
    params.family = geometry::Family::toy;
    params.mass = 1.0;
    params.spin = 0.875;
    params.cosmological_constant = 0.5;
    params.r_in = 1.5;
    params.r_out = 1.8;

    // Test 1: Ingoing radial photon starting from r = 10.0, equatorial theta = pi/2
    // Let's compute p_t and p_r such that H = 0 (null constraint)
    // At theta = pi/2:
    const double r0 = 10.0;
    const double th0 = M_PI / 2.0;
    const auto eval = std::get<geometry::Evaluation>(geometry::evaluate(params, r0, th0));

    // Choose p_t = -1.0, p_theta = 0, p_phi = 0
    // g^{tt} (p_t)^2 + g^{rr} (p_r)^2 = 0
    // (p_r)^2 = - g^{tt} / g^{rr} * (p_t)^2
    double g_inv_tt = eval.inverse[0][0];
    double g_inv_rr = eval.inverse[1][1];
    GRAVASTAR_REQUIRE(g_inv_tt < 0.0, "g^{tt} negative");
    GRAVASTAR_REQUIRE(g_inv_rr > 0.0, "g^{rr} positive");

    double pr_sq = - (g_inv_tt / g_inv_rr) * 1.0;
    double p_r0 = -std::sqrt(pr_sq); // Ingoing (- sign)

    integration::Vec4 x0 = {0.0, r0, th0, 0.0};
    integration::Vec4 p0 = {-1.0, p_r0, 0.0, 0.0};

    integration::IntegratorOptions opt;
    opt.max_steps = 1000;
    opt.h0 = 0.02;
    opt.escape_radius = 25.0;

    auto res = integration::trace_ray(params, x0, p0, opt);

    std::cout << "  Steps taken: " << res.steps << std::endl;
    std::cout << "  Min radius reached: " << res.min_r << std::endl;
    std::cout << "  Max null drift: " << res.max_null_drift << std::endl;
    std::cout << "  Crossed shell into core: " << (res.crossed_shell_in ? "YES" : "NO") << std::endl;

    GRAVASTAR_REQUIRE(res.max_null_drift < 1e-4, "Max null drift within 1e-4");
    GRAVASTAR_REQUIRE(res.crossed_shell_in == true, "Must cross shell into core");

    std::cout << "[SUCCESS] Geodesic integration verified!" << std::endl;
    return 0;
}
