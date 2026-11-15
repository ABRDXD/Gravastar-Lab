// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/integration/integrator.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <algorithm>

using namespace gravastar;

int main() {
    std::cout << "[TEST] Running conserved quantities & energy-drift validation..." << std::endl;

    geometry::Parameters params;
    params.family = geometry::Family::toy;
    params.mass = 1.0;
    params.spin = 0.875;
    params.cosmological_constant = 0.5;
    params.r_in = 1.5;
    params.r_out = 1.8;

    // Test a non-trivial inclined ray with angular momentum
    const double r0 = 15.0;
    const double th0 = 1.2; // ~68 degrees inclination
    const auto eval = std::get<geometry::Evaluation>(geometry::evaluate(params, r0, th0));

    // Conserved quantities:
    // E = -p_t
    // L_z = p_phi
    const double E_target = 1.0;
    const double L_z_target = 2.5;

    // Set p_t = -E, p_phi = L_z, p_theta = 0.2
    // Solve for p_r such that H = 0.5 * g^{mu nu} p_mu p_nu = 0
    // g^{rr} (p_r)^2 + g^{tt} (p_t)^2 + 2 g^{t phi} p_t p_phi + g^{phi phi} (p_phi)^2 + g^{theta theta} (p_theta)^2 = 0
    double p_t = -E_target;
    double p_phi = L_z_target;
    double p_th = 0.2;

    double quad = eval.inverse[0][0] * p_t * p_t
                + 2.0 * eval.inverse[0][3] * p_t * p_phi
                + eval.inverse[3][3] * p_phi * p_phi
                + eval.inverse[2][2] * p_th * p_th;

    assert(quad < 0.0 && "Spacetime geometry must allow ingoing physical photon");
    double p_r0 = -std::sqrt(-quad / eval.inverse[1][1]); // Inward

    integration::Vec4 x0 = {0.0, r0, th0, 0.5};
    integration::Vec4 p0 = {p_t, p_r0, p_th, p_phi};

    integration::IntegratorOptions opt;
    opt.max_steps = 1500;
    opt.h0 = 0.02;
    opt.escape_radius = 30.0;
    opt.record_trajectory = true;

    auto res = integration::trace_ray(params, x0, p0, opt);

    std::cout << "  Steps taken: " << res.steps << std::endl;
    std::cout << "  Trajectory samples recorded: " << res.trajectory.size() << std::endl;
    std::cout << "  Min radius reached: " << res.min_r << std::endl;

    double max_energy_drift = 0.0;
    double max_lz_drift = 0.0;

    for (const auto& step : res.trajectory) {
        double current_E = -step.p[0];
        double current_Lz = step.p[3];
        max_energy_drift = std::max(max_energy_drift, std::abs(current_E - E_target));
        max_lz_drift = std::max(max_lz_drift, std::abs(current_Lz - L_z_target));
    }

    std::cout << "  Max Energy |E - E0| drift: " << max_energy_drift << std::endl;
    std::cout << "  Max Angular Momentum |L_z - L_z0| drift: " << max_lz_drift << std::endl;
    std::cout << "  Max Null Constraint |H| drift: " << res.max_null_drift << std::endl;

    // In a stationary axisymmetric spacetime, Hamilton's equations guarantee dp_0/dlambda = 0 and dp_3/dlambda = 0 identically!
    assert(max_energy_drift < 1e-12);
    assert(max_lz_drift < 1e-12);
    assert(res.max_null_drift < 1e-4);

    std::cout << "[SUCCESS] Conserved quantities verified to machine precision!" << std::endl;
    return 0;
}
