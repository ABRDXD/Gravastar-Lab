// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include <cmath>

namespace gravastar::integration {

struct CriticalImpactParameters {
    double r_ph_pro_kerr;        // Prograde Kerr photon sphere
    double r_ph_ret_kerr;        // Retrograde Kerr photon sphere
    double b_crit_pro_shell;     // Threshold to enter shell (prograde)
    double b_crit_ret_shell;     // Threshold to enter shell (retrograde)
    double b_crit_pro_core;      // Threshold to enter de Sitter core (prograde)
    double b_crit_ret_core;      // Threshold to enter de Sitter core (retrograde)
};

// Computes analytical and semi-analytical critical impact parameters b = L_z / E
// for equatorial null geodesics traversing the gravastar manifold (arXiv:2309.00551v2).
CriticalImpactParameters compute_critical_impact(const geometry::Parameters& p);

} // namespace gravastar::integration
