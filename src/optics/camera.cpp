// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/camera.hpp"
#include <cmath>
#include <stdexcept>

namespace gravastar::optics {

Tetrad make_observer_tetrad(const geometry::Parameters& params, double r, double theta) {
    const auto eval_res = geometry::evaluate(params, r, theta);
    if (std::holds_alternative<geometry::Error>(eval_res)) {
        throw std::runtime_error("Cannot construct observer frame: invalid coordinate or domain");
    }
    const auto& g = std::get<geometry::Evaluation>(eval_res);

    Tetrad tetrad{};

    // Static observer has u^mu proportional to (1, 0, 0, 0)
    // g_00 u^0 u^0 = -1 => u^0 = 1 / sqrt(-g_00)
    // For Boyer-Lindquist outside ergosphere, g_00 < 0
    if (g.metric[0][0] >= 0.0) {
        throw std::runtime_error("Static observer requires g_00 < 0 (outside ergosphere)");
    }

    const double u0 = 1.0 / std::sqrt(-g.metric[0][0]);
    tetrad.e0 = {u0, 0.0, 0.0, 0.0};

    // e1 (radial basis vector): aligned with -partial_r (looking inwards towards center)
    // g_11 (e1^1)^2 = 1 => e1^1 = -1 / sqrt(g_11)
    if (g.metric[1][1] <= 0.0) {
        throw std::runtime_error("Invalid radial metric signature");
    }
    tetrad.e1 = {0.0, -1.0 / std::sqrt(g.metric[1][1]), 0.0, 0.0};

    // e2 (polar basis vector): aligned with partial_theta
    // g_22 (e2^2)^2 = 1 => e2^2 = 1 / sqrt(g_22)
    if (g.metric[2][2] <= 0.0) {
        throw std::runtime_error("Invalid polar metric signature");
    }
    tetrad.e2 = {0.0, 0.0, 1.0 / std::sqrt(g.metric[2][2]), 0.0};

    // e3 (azimuthal basis vector): orthogonal to e0
    // e3 has components in t and phi due to frame dragging g_03:
    // g(e0, e3) = g_00 e0^0 e3^0 + g_03 e0^0 e3^3 = 0 => e3^0 = - (g_03 / g_00) e3^3
    // g(e3, e3) = g_00 (e3^0)^2 + 2 g_03 e3^0 e3^3 + g_33 (e3^3)^2 = 1
    // (g_33 - g_03^2 / g_00) (e3^3)^2 = 1
    const double delta_phi = g.metric[3][3] - (g.metric[0][3] * g.metric[0][3]) / g.metric[0][0];
    if (delta_phi <= 0.0) {
        throw std::runtime_error("Invalid azimuthal metric signature");
    }
    const double e3_phi = 1.0 / std::sqrt(delta_phi);
    const double e3_t = - (g.metric[0][3] / g.metric[0][0]) * e3_phi;
    tetrad.e3 = {e3_t, 0.0, 0.0, e3_phi};

    return tetrad;
}

Vec4 pixel_ray_momentum(const Tetrad& tetrad,
                        const geometry::Evaluation& geom,
                        double ndc_x,
                        double ndc_y,
                        double tan_half_fov,
                        double aspect) {
    // In observer local orthonormal frame:
    // Forward direction is along e1 (inwards to gravastar)
    // Horizontal (right) is along e3 (phi)
    // Vertical (up) is along -e2 (opposite to theta, pointing north)
    const double dx = ndc_x * tan_half_fov * aspect;
    const double dy = ndc_y * tan_half_fov;
    const double dz = 1.0; // forward

    const double norm = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double nx = dx / norm;
    const double ny = dy / norm;
    const double nz = dz / norm;

    // Past-directed null vector k^a = ( -1, nz, -ny, nx ) in orthonormal frame:
    // k^a k_a = -(-1)^2 + nz^2 + ny^2 + nx^2 = -1 + 1 = 0
    // Contravariant 4-momentum k^mu = -e0^mu + nz * e1^mu - ny * e2^mu + nx * e3^mu
    Vec4 k_up{};
    for (int mu = 0; mu < 4; ++mu) {
        k_up[mu] = -tetrad.e0[mu]
                   + nz * tetrad.e1[mu]
                   - ny * tetrad.e2[mu]
                   + nx * tetrad.e3[mu];
    }

    // Lower index to get covariant momentum p_mu = g_{mu nu} k^nu
    Vec4 p_down{};
    for (int mu = 0; mu < 4; ++mu) {
        p_down[mu] = 0.0;
        for (int nu = 0; nu < 4; ++nu) {
            p_down[mu] += geom.metric[mu][nu] * k_up[nu];
        }
    }

    return p_down;
}

} // namespace gravastar::optics
