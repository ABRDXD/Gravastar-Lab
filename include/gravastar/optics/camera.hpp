// SPDX-License-Identifier: GPL-3.0-or-later
// ============================================================================
// GravastarLab Optics Engine — Camera Configuration & Tetrad Header
// ============================================================================
//
// 9TH-GRADER CHEAT-SHEET:
// -----------------------
// This file defines the "cockpit" of our virtual camera.
// If GravastarLab were a movie production studio, this header file defines:
// 1. Where the camera tripod is placed (CameraConfig).
// 2. The 4 perpendicular ruler-sticks attached to the camera lens (Tetrad).
// 3. How to shoot light rays out of each pixel on the camera screen (pixel_ray_momentum).
// ============================================================================
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include "gravastar/integration/integrator.hpp"
#include <array>
#include <cmath>

namespace gravastar::optics {

// Fixed-size math vectors:
// Vec3 is 3 numbers [x, y, z] like in 3D video games.
// Vec4 is 4 numbers [t, r, theta, phi] representing spacetime events or 4-momentum.
using Vec3 = std::array<double, 3>;
using Vec4 = std::array<double, 4>;

// ============================================================================
// STRUCT: Tetrad
// ----------------------------------------------------------------------------
// A "Tetrad" (from the Greek word for "four") is a set of 4 perpendicular
// 4-dimensional ruler-arrows attached to our observer.
//
// WHY 4 RULERS?
// Because we live in 4D spacetime (1 time dimension + 3 space dimensions):
// - e0: The "Time" arrow (your spaceship's 4-velocity; moving forward into the future).
// - e1: The "Forward" arrow (pointing straight inwards toward the gravastar center).
// - e2: The "Up" arrow (pointing towards the gravastar's North Pole).
// - e3: The "Right" arrow (pointing East / in the direction the gravastar is spinning).
//
// ORTHONORMALITY CONDITION:
// In the observer's local frame, spacetime feels completely flat (Minkowski):
//   g(e_a, e_b) = diag(-1, +1, +1, +1)
// The time ruler has squared length -1 (timelike), and the 3 spatial rulers
// each have squared length +1 (spacelike) and are perpendicular to each other.
// ============================================================================
struct Tetrad {
    Vec4 e0; // u_obs (timelike 4-velocity of the observer)
    Vec4 e1; // Radial basis vector (pointing inward toward center)
    Vec4 e2; // Polar basis vector (pointing North along theta)
    Vec4 e3; // Azimuthal basis vector (pointing East along phi)
};

// ============================================================================
// STRUCT: CameraConfig
// ----------------------------------------------------------------------------
// Holds the position, zoom level, and resolution of our virtual camera.
// ============================================================================
struct CameraConfig {
    double r{15.0};        // Distance from gravastar center (15.0 Schwarzschild masses away)
    double theta{M_PI / 2.0}; // Polar latitude angle (M_PI/2 = 90 degrees = Equatorial plane)
    double phi{0.0};       // Azimuthal longitude angle (0.0 = Prime meridian)
    double fov_deg{45.0};  // Field of view in degrees (45 deg is standard normal lens; 90 deg is fish-eye)
    int width{256};        // Image width in pixels
    int height{256};       // Image height in pixels
};

// ============================================================================
// FUNCTION: make_observer_tetrad
// ----------------------------------------------------------------------------
// Constructs the orthonormal tetrad for a Zero-Angular-Momentum Observer (ZAMO)
// at radial distance 'r' and polar angle 'theta'.
// Works safely anywhere outside or inside the ergoregion!
// ============================================================================
Tetrad make_observer_tetrad(const geometry::Parameters& params, double r, double theta);

// ============================================================================
// FUNCTION: pixel_ray_momentum
// ----------------------------------------------------------------------------
// Generates the initial 4-momentum p_mu of a photon shot backward out of
// a specific screen pixel (ndc_x, ndc_y) on the camera sensor.
// ============================================================================
Vec4 pixel_ray_momentum(const Tetrad& tetrad,
                        const geometry::Evaluation& geom,
                        double ndc_x,
                        double ndc_y,
                        double tan_half_fov,
                        double aspect);

} // namespace gravastar::optics
