// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include "gravastar/integration/integrator.hpp"
#include <array>
#include <cmath>

namespace gravastar::optics {

using Vec3 = std::array<double, 3>;
using Vec4 = std::array<double, 4>;

// Static observer orthonormal tetrad (e_0, e_1, e_2, e_3)
// such that g(e_a, e_b) = eta_{ab} = diag(-1, +1, +1, +1)
struct Tetrad {
    Vec4 e0; // u_obs (timelike 4-velocity)
    Vec4 e1; // radial basis / pointing towards center
    Vec4 e2; // polar basis (theta direction)
    Vec4 e3; // azimuthal basis (phi direction)
};

struct CameraConfig {
    double r{15.0};        // Distance from center
    double theta{M_PI / 2.0}; // Polar angle (pi/2 is equatorial)
    double phi{0.0};       // Azimuthal angle
    double fov_deg{45.0};  // Field of view in degrees
    int width{256};        // Resolution
    int height{256};
};

// Constructs the orthonormal tetrad for a zero-angular-momentum observer (ZAMO) or static observer
Tetrad make_observer_tetrad(const geometry::Parameters& params, double r, double theta);

// Generates the initial null covariant momentum p_mu for a camera pixel (ndc_x, ndc_y in [-1, 1])
Vec4 pixel_ray_momentum(const Tetrad& tetrad,
                        const geometry::Evaluation& geom,
                        double ndc_x,
                        double ndc_y,
                        double tan_half_fov,
                        double aspect);

} // namespace gravastar::optics
