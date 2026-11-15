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

    // ZAMO (Bardeen Locally Non-Rotating Frame) Orthonormal Tetrad:
    // Regular everywhere outside, on the boundary, and inside the ergoregion (unlike static observers where g_00 >= 0 fails).
    const double g_tt = g.metric[0][0];
    const double g_rr = g.metric[1][1];
    const double g_thth = g.metric[2][2];
    const double g_phiphi = g.metric[3][3];
    const double g_tphi = g.metric[0][3];

    if (g_phiphi <= 0.0 || g_rr <= 0.0 || g_thth <= 0.0) {
        throw std::runtime_error("Invalid metric signature for observer frame");
    }

    const double omega = -g_tphi / g_phiphi;
    const double lapse_sq = -g_tt + omega * omega * g_phiphi;
    if (lapse_sq <= 0.0) {
        throw std::runtime_error("ZAMO lapse squared must be strictly positive");
    }
    const double alpha = std::sqrt(lapse_sq);

    // e0: timelike 4-velocity u^mu = (1/alpha) * (delta^mu_t + omega * delta^mu_phi)
    tetrad.e0 = {1.0 / alpha, 0.0, 0.0, omega / alpha};

    // e1: radial basis vector (pointing inwards towards center: -partial_r)
    tetrad.e1 = {0.0, -1.0 / std::sqrt(g_rr), 0.0, 0.0};

    // e2: polar basis vector (partial_theta)
    tetrad.e2 = {0.0, 0.0, 1.0 / std::sqrt(g_thth), 0.0};

    // e3: azimuthal basis vector (partial_phi)
    tetrad.e3 = {0.0, 0.0, 0.0, 1.0 / std::sqrt(g_phiphi)};

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
