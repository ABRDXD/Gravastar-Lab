// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/integration/integrator.hpp"
#include <cmath>
#include <algorithm>

namespace gravastar::integration {

namespace {

struct State {
    Vec4 x; // (t, r, theta, phi)
    Vec4 p; // (p_t, p_r, p_theta, p_phi)
};

struct Derivative {
    Vec4 dx; // dx^mu / dlambda
    Vec4 dp; // dp_mu / dlambda
};

// Compute Hamilton's RHS:
// dx^mu/dlambda = g^{mu nu} p_nu
// dp_mu/dlambda = 0.5 * (partial_mu g_{alpha beta}) (g^{alpha rho} p_rho) (g^{beta sigma} p_sigma)
bool compute_derivatives(const geometry::Parameters& params,
                         const State& s,
                         Derivative& d,
                         double& current_null_val) {
    const double r = s.x[1];
    const double theta = s.x[2];

    // Coordinate domain bounds: r > 0, 0 < theta < pi
    if (r <= 1e-4 || theta <= 1e-4 || theta >= (M_PI - 1e-4)) {
        return false;
    }

    const auto eval_res = geometry::evaluate(params, r, theta);
    if (std::holds_alternative<geometry::Error>(eval_res)) {
        return false;
    }

    const auto& geom = std::get<geometry::Evaluation>(eval_res);

    // dx^mu = g^{mu nu} p_nu (contravariant velocity v^mu)
    Vec4 v{};
    for (int mu = 0; mu < 4; ++mu) {
        v[mu] = 0.0;
        for (int nu = 0; nu < 4; ++nu) {
            v[mu] += geom.inverse[mu][nu] * s.p[nu];
        }
        d.dx[mu] = v[mu];
    }

    // Null constraint H = 0.5 * v^mu p_mu
    double h_val = 0.0;
    for (int mu = 0; mu < 4; ++mu) {
        h_val += v[mu] * s.p[mu];
    }
    current_null_val = 0.5 * h_val;

    // dp_mu = 0.5 * (partial_mu g_{alpha beta}) v^alpha v^beta
    // For stationary axis-symmetric spacetime:
    // partial_t g = 0  => dp_0 = 0 (Conservation of Energy)
    // partial_phi g = 0 => dp_3 = 0 (Conservation of Axial Angular Momentum)
    d.dp[0] = 0.0;
    d.dp[3] = 0.0;

    // mu = 1 (r): dg[1] is partial_r g
    double sum_r = 0.0;
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            sum_r += geom.dg[1][a][b] * v[a] * v[b];
        }
    }
    d.dp[1] = 0.5 * sum_r;

    // mu = 2 (theta): dg[2] is partial_theta g
    double sum_th = 0.0;
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            sum_th += geom.dg[2][a][b] * v[a] * v[b];
        }
    }
    d.dp[2] = 0.5 * sum_th;

    return true;
}

State add_scaled(const State& s, const Derivative& d, double h) {
    State out;
    for (int i = 0; i < 4; ++i) {
        out.x[i] = s.x[i] + h * d.dx[i];
        out.p[i] = s.p[i] + h * d.dp[i];
    }
    return out;
}

} // namespace

TraceResult trace_ray(const geometry::Parameters& params,
                      const Vec4& x0,
                      const Vec4& p0,
                      const IntegratorOptions& opt) {
    TraceResult result;
    State current{x0, p0};
    double lambda = 0.0;

    result.min_r = current.x[1];
    if (opt.record_trajectory) {
        result.trajectory.push_back({lambda, current.x, current.p, 0.0});
    }

    for (int step = 0; step < opt.max_steps; ++step) {
        result.steps = step + 1;
        const double r = current.x[1];

        result.min_r = std::min(result.min_r, r);
        if (r < params.r_in) {
            result.crossed_shell_in = true;
        }
        if (result.crossed_shell_in && r > params.r_out) {
            result.crossed_shell_out = true;
        }

        // Termination 1: Escaped to outer boundary
        if (r >= opt.escape_radius) {
            result.outcome = result.crossed_shell_in ? RayOutcome::CorePassed : RayOutcome::Escaped;
            break;
        }

        // Adaptive step based on distance from core and shell
        double dist_to_shell = std::min(std::abs(r - params.r_in), std::abs(r - params.r_out));
        double factor = std::clamp(dist_to_shell / params.r_in, 0.05, 1.0);
        double h = std::clamp(opt.h0 * factor, opt.h_min, opt.h_max);

        // RK4 Integration step
        Derivative k1, k2, k3, k4;
        double null_val = 0.0;

        if (!compute_derivatives(params, current, k1, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }
        result.max_null_drift = std::max(result.max_null_drift, std::abs(null_val));

        State s2 = add_scaled(current, k1, 0.5 * h);
        if (!compute_derivatives(params, s2, k2, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }

        State s3 = add_scaled(current, k2, 0.5 * h);
        if (!compute_derivatives(params, s3, k3, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }

        State s4 = add_scaled(current, k3, h);
        if (!compute_derivatives(params, s4, k4, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }

        for (int i = 0; i < 4; ++i) {
            current.x[i] += (h / 6.0) * (k1.dx[i] + 2.0 * k2.dx[i] + 2.0 * k3.dx[i] + k4.dx[i]);
            current.p[i] += (h / 6.0) * (k1.dp[i] + 2.0 * k2.dp[i] + 2.0 * k3.dp[i] + k4.dp[i]);
        }
        lambda += h;

        if (opt.record_trajectory) {
            result.trajectory.push_back({lambda, current.x, current.p, null_val});
        }
    }

    result.final_lambda = lambda;
    result.final_x = current.x;
    result.final_p = current.p;
    return result;
}

} // namespace gravastar::integration
