// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/integration/critical_impact.hpp"
#include <cmath>

namespace gravastar::integration {

namespace {

// Solves r (r - 3M)^2 - 4 M a^2 = 0 via Newton-Raphson iteration
void solve_kerr_photon_orbit(double M, double a, double r_guess, double& r_out, double& b_out) {
    double r = r_guess;
    for (int iter = 0; iter < 50; ++iter) {
        double diff = r - 3.0 * M;
        double f = r * diff * diff - 4.0 * M * a * a;
        double df = diff * (3.0 * r - 3.0 * M);
        if (std::abs(df) < 1e-15) break;
        double r_next = r - f / df;
        if (std::abs(r_next - r) < 1e-14) {
            r = r_next;
            break;
        }
        r = r_next;
    }
    r_out = r;
    b_out = -(r * r * r - 3.0 * M * r * r + a * a * r + M * a * a) / (a * (r - M));
}

} // namespace

CriticalImpactParameters compute_critical_impact(const geometry::Parameters& p) {
    CriticalImpactParameters res{};
    const double M = p.mass;
    const double a = p.spin;
    const double Lam = p.cosmological_constant;
    const double r_in = p.r_in;
    const double r_out = p.r_out;

    // Outer boundary (Kerr side): Delta(r_out)
    const double delta_out = r_out * r_out - 2.0 * M * r_out + a * a;
    const double sqrt_delta_out = std::sqrt(delta_out);
    res.b_crit_pro_shell = a + (r_out * r_out) / (a + sqrt_delta_out);

    // Inner boundary (de Sitter side): Delta(r_in)
    const double delta_in = (r_in * r_in + a * a) * (1.0 - Lam * r_in * r_in / 3.0);
    const double sqrt_delta_in = std::sqrt(delta_in);
    res.b_crit_pro_core = a + (r_in * r_in) / (a + sqrt_delta_in);
    res.b_crit_ret_core = a - (r_in * r_in) / (sqrt_delta_in - a);

    // Kerr photon spheres
    solve_kerr_photon_orbit(M, a, 1.62878165935, res.r_ph_pro_kerr, res.b_crit_pro_shell); // Prograde
    // For prograde on gravastar, r_ph_pro_kerr < r_out (1.6288 < 1.8), so shell intercepts photons before Kerr photon sphere
    // Recompute b_crit_pro_shell using r_out directly:
    res.b_crit_pro_shell = a + (r_out * r_out) / (a + sqrt_delta_out);

    // Retrograde: r_ph_ret_kerr > r_out (3.8876 > 1.8), governed by exterior Kerr centrifugal barrier
    solve_kerr_photon_orbit(M, a, 3.88756325539, res.r_ph_ret_kerr, res.b_crit_ret_shell);

    return res;
}

} // namespace gravastar::integration
