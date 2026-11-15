// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/integration/critical_impact.hpp"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace gravastar;

int main() {
    std::cout << "[TEST] Running Critical Impact Parameters Validation (C++)..." << std::endl;

    geometry::Parameters p;
    p.family = geometry::Family::toy;
    p.mass = 1.0;
    p.spin = 0.875;
    p.cosmological_constant = 0.5;
    p.r_in = 1.5;
    p.r_out = 1.8;

    auto res = integration::compute_critical_impact(p);

    std::cout << "  r_ph (Kerr Prograde):     " << res.r_ph_pro_kerr << std::endl;
    std::cout << "  r_ph (Kerr Retrograde):   " << res.r_ph_ret_kerr << std::endl;
    std::cout << "  b_crit,shell (Prograde):   " << res.b_crit_pro_shell << std::endl;
    std::cout << "  b_crit,shell (Retrograde): " << res.b_crit_ret_shell << std::endl;
    std::cout << "  b_crit,core (Prograde):    " << res.b_crit_pro_core << std::endl;
    std::cout << "  b_crit,core (Retrograde):  " << res.b_crit_ret_core << std::endl;

    // Numerical checks matching theoretical predictions:
    // b_crit_pro_shell = 3.018017...
    assert(std::abs(res.b_crit_pro_shell - 3.0180173587) < 1e-6);
    // b_crit_ret_shell = -6.790071...
    assert(std::abs(res.b_crit_ret_shell - (-6.7900713688)) < 1e-6);
    // b_crit_pro_core = 1.875948...
    assert(std::abs(res.b_crit_pro_core - 1.8759486035) < 1e-6);
    // b_crit_ret_core = -3.644273...
    assert(std::abs(res.b_crit_ret_core - (-3.6442732108)) < 1e-6);

    std::cout << "[SUCCESS] Critical impact parameters match theory within 1e-6!" << std::endl;
    return 0;
}
