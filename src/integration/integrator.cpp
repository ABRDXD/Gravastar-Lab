// SPDX-License-Identifier: GPL-3.0-or-later
// ============================================================================
// GravastarLab Geodesic Engine — Hamiltonian RK4 Light-Ray Tracer
// ============================================================================
//
// 9TH-GRADER HIGH SCHOOL ANALOGY:
// ------------------------------
// Imagine rolling a marble across a giant, stretchy rubber sheet that has heavy
// weights sitting on it.
//
// 1. WHAT IS A "GEODESIC"?
//    A geodesic is simply the natural path light travels through bent spacetime.
//    Light has no steering wheel and no engine; it just glides along the curves
//    of space like a bobsled on an icy curved track.
//
// 2. WHY USE "HAMILTONIAN MECHANICS" INSTEAD OF NEWTON'S F = m*a?
//    In high school physics, you learn Force = Mass * Acceleration ($F = ma$).
//    In curved 4D spacetime, Newton's equations become an absolute nightmare with
//    64 messy acceleration terms (called Christoffel symbols).
//    Instead, 19th-century mathematician William Rowan Hamilton invented a brilliant
//    shortcut: track the marble's POSITION (x) and its MOMENTUM (p) together.
//    The total energy is called the "Hamiltonian" (H). For light, H is ALWAYS 0!
//    Hamilton's equations say:
//      - How fast position changes:  dx/dlambda =  dH / dp
//      - How fast momentum changes:  dp/dlambda = -dH / dx
//
// 3. WHY RUNGE-KUTTA 4TH ORDER (RK4)?
//    If you want to know where a thrown basketball will be in 1 second, you could
//    just use its current speed. But gravity is constantly curving its path!
//    RK4 is like taking 4 rapid snapshot-predictions:
//      k1: Check speed at the start.
//      k2: Estimate where you'll be halfway, and check speed there.
//      k3: Re-estimate the halfway point using k2's speed.
//      k4: Estimate where you'll be at the end.
//    Then you take a weighted average: (k1 + 2*k2 + 2*k3 + k4) / 6.
//    This gives an extraordinarily accurate step that makes almost zero error!
// ============================================================================

#include "gravastar/integration/integrator.hpp"
#include <cmath>
#include <algorithm>

namespace gravastar::integration {

namespace {

// ============================================================================
// STRUCT: State
// ----------------------------------------------------------------------------
// Holds the 4D position x = [t, r, theta, phi] and
// the 4D momentum p = [p_t, p_r, p_theta, p_phi] of the light ray.
// ============================================================================
struct State {
    Vec4 x; // Position in 4D spacetime: [0]=time, [1]=radius, [2]=polar angle, [3]=longitude
    Vec4 p; // Covariant 4-momentum: [0]=energy(-E), [1]=radial momentum, [2]=polar, [3]=angular momentum(Lz)
};

// ============================================================================
// STRUCT: Derivative
// ----------------------------------------------------------------------------
// Holds the velocities: dx/dlambda (how fast position changes) and
// dp/dlambda (how fast momentum changes due to gravitational curvature).
// ============================================================================
struct Derivative {
    Vec4 dx; // dx^mu / dlambda (4-velocity of the photon)
    Vec4 dp; // dp_mu / dlambda (rate of change of momentum)
};

// ============================================================================
// FUNCTION: compute_derivatives
// ----------------------------------------------------------------------------
// Computes Hamilton's equations of motion for a null photon at state 's':
//   dx^mu / dlambda = g^{mu nu} * p_nu
//   dp_mu / dlambda = 0.5 * (partial_mu g_{alpha beta}) * v^alpha * v^beta
//
// 9TH-GRADER DEBUGGING HINTS:
// - If energy or angular momentum drifts: Check lines setting dp[0] and dp[3].
// - If light curves the wrong way: Check the 0.5 factor on sum_r and sum_th.
// ============================================================================
bool compute_derivatives(const geometry::Parameters& params,
                         const State& s,
                         Derivative& d,
                         double& current_null_val) {
    const double r = s.x[1];
    const double theta = s.x[2];

    // [Safety Check: Coordinate singularities]
    // In spherical-polar coordinates, theta=0 is the North Pole and theta=pi is the South Pole.
    // At the poles, longitude (phi) loses meaning (like how all time zones meet at the North Pole).
    // We stop the trace if a ray hits within 1e-4 of the polar axis or center to prevent dividing by zero.
    if (r <= 1e-4 || theta <= 1e-4 || theta >= (M_PI - 1e-4)) {
        return false;
    }

    // Ask our metric engine for the curvature and metric inverse at this point
    const auto eval_res = geometry::evaluate(params, r, theta);
    if (std::holds_alternative<geometry::Error>(eval_res)) {
        return false;
    }

    const auto& geom = std::get<geometry::Evaluation>(eval_res);

    // [Step 1: Calculate contravariant velocity v^mu = g^{mu nu} * p_nu]
    // In relativity, multiplying the covariant momentum p_nu by the inverse metric g^{mu nu}
    // "raises the index", converting momentum into the physical 4-velocity v^mu = dx^mu/dlambda.
    Vec4 v{};
    for (int mu = 0; mu < 4; ++mu) {
        v[mu] = 0.0;
        for (int nu = 0; nu < 4; ++nu) {
            v[mu] += geom.inverse[mu][nu] * s.p[nu];
        }
        d.dx[mu] = v[mu];
    }

    // [Step 2: Check the Null Constraint (H = 0)]
    // For particles of light (photons), Einstein requires that the 4-velocity squared is ZERO:
    //   H = 0.5 * g^{mu nu} * p_mu * p_nu = 0.5 * v^mu * p_mu = 0
    // If our numerical integrator is working accurately, H should stay within 1e-4 of zero!
    double h_val = 0.0;
    for (int mu = 0; mu < 4; ++mu) {
        h_val += v[mu] * s.p[mu];
    }
    current_null_val = 0.5 * h_val;

    // [Step 3: Exact Conservation Laws (Noether's Theorem)]
    // Emmy Noether proved in 1915 that symmetries in physics create conservation laws:
    // 1. Time Translation Symmetry: The gravastar is not changing over time (stationary).
    //    Because partial_t g = 0, energy CANNOT change: dp_0/dlambda = 0.
    // 2. Rotational Symmetry: The gravastar looks identical from every angle around its spin axis.
    //    Because partial_phi g = 0, axial angular momentum CANNOT change: dp_3/dlambda = 0.
    d.dp[0] = 0.0; // Energy is strictly conserved!
    d.dp[3] = 0.0; // Angular momentum (L_z) is strictly conserved!

    // [Step 4: Radial gravitational force (dp_1/dlambda)]
    // Gravity pulls on the light ray radially. The force is proportional to
    // the radial derivative of the metric: 0.5 * (partial_r g_{ab}) * v^a * v^b.
    double sum_r = 0.0;
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            sum_r += geom.dg[1][a][b] * v[a] * v[b];
        }
    }
    d.dp[1] = 0.5 * sum_r;

    // [Step 5: Polar gravitational force (dp_2/dlambda)]
    // Centrifugal and frame-dragging forces push on the light ray in the polar (theta) direction.
    // The force is proportional to the polar derivative: 0.5 * (partial_theta g_{ab}) * v^a * v^b.
    double sum_th = 0.0;
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            sum_th += geom.dg[2][a][b] * v[a] * v[b];
        }
    }
    d.dp[2] = 0.5 * sum_th;

    return true;
}

// Helper to advance a state along a derivative vector by step size h: state + h * derivative
State add_scaled(const State& s, const Derivative& d, double h) {
    State out;
    for (int i = 0; i < 4; ++i) {
        out.x[i] = s.x[i] + h * d.dx[i];
        out.p[i] = s.p[i] + h * d.dp[i];
    }
    return out;
}

} // namespace

// ============================================================================
// FUNCTION: trace_ray
// ----------------------------------------------------------------------------
// Marches a single light ray backward through curved spacetime using 4th-order
// Runge-Kutta integration with adaptive step size.
//
// 9TH-GRADER MOUNTAIN ROAD ANALOGY:
// When driving on a straight desert highway, you can drive fast with big steps.
// But when driving through treacherous mountain curves (the transition shell!),
// you must slow down and take tiny careful steps so you don't skid off the cliff!
// ============================================================================
TraceResult trace_ray(const geometry::Parameters& params,
                      const Vec4& x0,
                      const Vec4& p0,
                      const IntegratorOptions& opt) {
    TraceResult result;
    State current{x0, p0};
    double lambda = 0.0; // Affine parameter (like the stopwatch ticking along the photon's path)

    result.min_r = current.x[1];
    if (opt.record_trajectory) {
        result.trajectory.push_back({lambda, current.x, current.p, 0.0});
    }

    // [Main Raymarching Loop]
    for (int step = 0; step < opt.max_steps; ++step) {
        result.steps = step + 1;
        const double r = current.x[1];

        // Track the closest approach to the gravastar center (turning point diagnostic)
        result.min_r = std::min(result.min_r, r);

        // Check if the ray penetrated into the de Sitter core (r < r_in)
        if (r < params.r_in) {
            result.crossed_shell_in = true;
        }
        // Check if a core-penetrating ray emerged back out into the universe
        if (result.crossed_shell_in && r > params.r_out) {
            result.crossed_shell_out = true;
        }

        // [Termination Condition 1: Escaped to outer space]
        // If the ray reached r >= escape_radius (e.g. 25 or 50 masses away),
        // it has escaped the gravastar's gravitational pull and hit the background stars!
        if (r >= opt.escape_radius) {
            result.outcome = result.crossed_shell_in ? RayOutcome::CorePassed : RayOutcome::Escaped;
            break;
        }

        // [Adaptive Step Size Selection]
        // dist_to_shell: How close are we to either the inner wall (r_in) or outer wall (r_out)?
        // When close to the shell, factor shrinks down towards 0.05, shrinking step size h!
        // This guarantees high numerical precision across the polynomial transition boundary.
        double dist_to_shell = std::min(std::abs(r - params.r_in), std::abs(r - params.r_out));
        double factor = std::clamp(dist_to_shell / params.r_in, 0.05, 1.0);
        double h = std::clamp(opt.h0 * factor, opt.h_min, opt.h_max);

        // [RK4 Stage 1: Evaluate at the current state]
        Derivative k1, k2, k3, k4;
        double null_val = 0.0;

        if (!compute_derivatives(params, current, k1, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }
        result.max_null_drift = std::max(result.max_null_drift, std::abs(null_val));

        // [RK4 Stage 2: First midpoint estimate]
        State s2 = add_scaled(current, k1, 0.5 * h);
        if (!compute_derivatives(params, s2, k2, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }

        // [RK4 Stage 3: Second midpoint estimate]
        State s3 = add_scaled(current, k2, 0.5 * h);
        if (!compute_derivatives(params, s3, k3, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }

        // [RK4 Stage 4: Full-step endpoint estimate]
        State s4 = add_scaled(current, k3, h);
        if (!compute_derivatives(params, s4, k4, null_val)) {
            result.outcome = RayOutcome::Captured;
            break;
        }

        // [Combine the 4 stages using Simpson's Rule]
        // Weighted average: (k1 + 2*k2 + 2*k3 + k4) / 6
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
