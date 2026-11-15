// SPDX-License-Identifier: GPL-3.0-or-later
// ============================================================================
// GravastarLab Geodesic Engine — Hamiltonian Raytracer Header
// ============================================================================
//
// 9TH-GRADER CHEAT-SHEET:
// -----------------------
// This header defines the flight recorder and settings for tracking light rays:
// 1. RayOutcome       : What happened to the ray? (Did it escape into space? Did it pass through the core?)
// 2. RayStep          : A single point on the ray's trajectory (like GPS breadcrumbs).
// 3. TraceResult      : The complete mission report after the ray finishes flying.
// 4. IntegratorOptions: The engine settings (how many steps to take, step size h0, escape radius).
// ============================================================================
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace gravastar::integration {

// 4-component vector in spacetime: [t, r, theta, phi]
using Vec4 = std::array<double, 4>;

// ============================================================================
// ENUM: RayOutcome
// ----------------------------------------------------------------------------
// Describes the final fate of a light ray:
// - Escaped   : Light ray flew past the gravastar without hitting the core and escaped to deep space.
// - CorePassed: Light ray passed completely through the de Sitter core and came back out.
// - Captured  : Light ray fell into a horizon or got trapped in an unstable orbit.
// - Exhausted : Light ray took too many steps (hit max_steps budget) while orbiting.
// ============================================================================
enum class RayOutcome : std::uint8_t {
    Escaped,     // Reached r > escape_radius
    CorePassed,  // Traversed through core and escaped
    Captured,    // (For reference Kerr comparison or interior trapped)
    Exhausted    // Hit max_steps without escaping or capturing
};

// ============================================================================
// STRUCT: RayStep
// ----------------------------------------------------------------------------
// A single breadcrumb recorded along the ray's flight path.
// ============================================================================
struct RayStep {
    double lambda;          // Affine parameter (photon's elapsed flight time)
    Vec4 x;                 // 4D position: (t, r, theta, phi)
    Vec4 p;                 // 4D momentum: (p_t, p_r, p_theta, p_phi)
    double null_constraint; // 0.5 * g^{mu nu} * p_mu * p_nu (Must stay ~0 for light!)
};

// ============================================================================
// STRUCT: TraceResult
// ----------------------------------------------------------------------------
// The complete mission report returned after tracing a ray.
// ============================================================================
struct TraceResult {
    RayOutcome outcome{RayOutcome::Exhausted}; // Final fate of the ray
    int steps{0};                              // Total RK4 integration steps taken
    double final_lambda{0.0};                  // Total flight parameter reached
    Vec4 final_x{};                            // Final position in spacetime
    Vec4 final_p{};                            // Final momentum vector
    double max_null_drift{0.0};                // Max deviation of H from 0 (accuracy check)
    double min_r{1e30};                        // Closest approach radius to gravastar center
    bool crossed_shell_in{false};              // True if ray crossed into core (r < r_in)
    bool crossed_shell_out{false};             // True if core ray escaped back to exterior
    std::vector<RayStep> trajectory{};         // Full path breadcrumbs (if recording enabled)
};

// ============================================================================
// STRUCT: IntegratorOptions
// ----------------------------------------------------------------------------
// Flight control knobs for the numerical integrator.
// ============================================================================
struct IntegratorOptions {
    int max_steps{2048};              // Maximum allowed steps before giving up (exhausted)
    double h0{0.05};                  // Base step size (smaller = more accurate, larger = faster)
    double h_min{1e-5};               // Minimum step size clamp (prevents freezing near boundaries)
    double h_max{0.5};                // Maximum step size clamp (prevents overshoot in flat space)
    double escape_radius{50.0};       // Outer boundary radius considered "deep space"
    double r_capture_tolerance{1e-3}; // Proximity to horizon considered captured
    bool record_trajectory{false};    // If true, stores every step in TraceResult::trajectory
};

// ============================================================================
// FUNCTION: trace_ray
// ----------------------------------------------------------------------------
// Evaluates Hamilton's equations for null geodesics:
//   H = 0.5 * g^{mu nu} * p_mu * p_nu = 0
//   dx^mu / dlambda =  dH / dp_mu = g^{mu nu} * p_nu
//   dp_mu / dlambda = -dH / dx^mu = 0.5 * (partial_mu g_{alpha beta}) * v^alpha * v^beta
// ============================================================================
TraceResult trace_ray(const geometry::Parameters& params,
                      const Vec4& x0,
                      const Vec4& p0,
                      const IntegratorOptions& options);

} // namespace gravastar::integration
