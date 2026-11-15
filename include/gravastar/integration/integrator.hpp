// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace gravastar::integration {

using Vec4 = std::array<double, 4>;

enum class RayOutcome : std::uint8_t {
    Escaped,     // Reached r > escape_radius
    CorePassed,  // Traversed through core and escaped
    Captured,    // (For reference Kerr comparison or interior trapped)
    Exhausted    // Hit max_steps without escaping or capturing
};

struct RayStep {
    double lambda;
    Vec4 x; // (t, r, theta, phi)
    Vec4 p; // p_mu (covariant momentum)
    double null_constraint; // 0.5 * g^{mu nu} p_mu p_nu
};

struct TraceResult {
    RayOutcome outcome{RayOutcome::Exhausted};
    int steps{0};
    double final_lambda{0.0};
    Vec4 final_x{};
    Vec4 final_p{};
    double max_null_drift{0.0};
    double min_r{1e30};
    bool crossed_shell_in{false};
    bool crossed_shell_out{false};
    std::vector<RayStep> trajectory{};
};

struct IntegratorOptions {
    int max_steps{2048};
    double h0{0.05};
    double h_min{1e-5};
    double h_max{0.5};
    double escape_radius{50.0};
    double r_capture_tolerance{1e-3};
    bool record_trajectory{false};
};

// Hamilton's equations for null geodesics:
// H = 0.5 * g^{mu nu} p_mu p_nu = 0
// dx^mu / dlambda =  dH / dp_mu = g^{mu nu} p_nu
// dp_mu / dlambda = -dH / dx^mu = -0.5 * (partial_mu g^{alpha beta}) p_alpha p_beta
// where partial_mu g^{alpha beta} = - g^{alpha sigma} g^{beta rho} partial_mu g_{sigma rho}
TraceResult trace_ray(const geometry::Parameters& params,
                      const Vec4& x0,
                      const Vec4& p0,
                      const IntegratorOptions& options);

} // namespace gravastar::integration
