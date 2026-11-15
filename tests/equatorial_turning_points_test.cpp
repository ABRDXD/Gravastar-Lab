// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/integration/integrator.hpp"
#include "gravastar/tools/testing.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace gravastar;

int main() {
    std::cout << "=== Equatorial Radial Turning Point & Step-Halving Verification ===\n";
    std::cout << std::fixed << std::setprecision(9);

    geometry::Parameters p;
    p.family = geometry::Family::toy;
    p.mass = 1.0;
    p.spin = 0.875;
    p.cosmological_constant = 0.5;
    p.r_in = 1.5;
    p.r_out = 1.8;

    struct Target {
        double b;
        double expected_r;
        const char* label;
        bool expected_core;
    };

    Target targets[] = {
        {+1.80, 1.438750388, "Enters core", true},
        {+1.95, 1.551795042, "Turns in shell", false},
        {-3.60, 1.490704364, "Enters core", true},
        {-3.70, 1.511352187, "Turns in shell", false},
    };

    double r0 = 10.0;
    double th0 = M_PI / 2.0;
    const auto res0 = geometry::evaluate(p, r0, th0);
    const auto& g0 = std::get<geometry::Evaluation>(res0);

    for (const auto& t : targets) {
        double E = 1.0;
        double pt = -E;
        double pphi = t.b * E;
        double quad = g0.inverse[0][0] * pt * pt +
                      2.0 * g0.inverse[0][3] * pt * pphi +
                      g0.inverse[3][3] * pphi * pphi;
        double pr = -std::sqrt(-quad / g0.inverse[1][1]);

        integration::Vec4 x0 = {0.0, r0, th0, 0.0};
        integration::Vec4 p0 = {pt, pr, 0.0, pphi};

        // Run with h0 = 0.01
        integration::IntegratorOptions opt1;
        opt1.max_steps = 25000;
        opt1.h0 = 0.01;
        opt1.escape_radius = 25.0;

        // Run with h0 = 0.005 (step halved)
        integration::IntegratorOptions opt2;
        opt2.max_steps = 50000;
        opt2.h0 = 0.005;
        opt2.escape_radius = 25.0;

        auto r1 = integration::trace_ray(p, x0, p0, opt1);
        auto r2 = integration::trace_ray(p, x0, p0, opt2);

        double err1 = std::abs(r1.min_r - t.expected_r);
        double err2 = std::abs(r2.min_r - t.expected_r);

        std::cout << "\nRay b = " << (t.b > 0 ? "+" : "") << t.b << " (" << t.label << "):\n";
        std::cout << "  Expected Turning Radius: " << t.expected_r << "\n";
        std::cout << "  Measured (h=0.01):       " << r1.min_r << " | Absolute Error = " << std::scientific << std::setprecision(3) << err1 << "\n";
        std::cout << std::fixed << std::setprecision(9);
        std::cout << "  Measured (h=0.005):      " << r2.min_r << " | Absolute Error = " << std::scientific << std::setprecision(3) << err2 << "\n";
        std::cout << "  Entered Core:            " << ((r2.min_r < p.r_in) ? "YES" : "NO") << "\n";

        // Verification assertions
        GRAVASTAR_REQUIRE(err1 < 1e-4, "err1 within 1e-4");
        GRAVASTAR_REQUIRE(err2 < 1e-4, "err2 within 1e-4");
        GRAVASTAR_REQUIRE(err2 <= err1 * 1.5 || (err1 < 1e-6 && err2 < 1e-6), "Convergence or sub-micro error on halving");
        GRAVASTAR_REQUIRE((r2.min_r < p.r_in) == t.expected_core, "Core entry match");
    }

    std::cout << "\n[PASS] All four incoming equatorial rays verified against turning point references!\n";
    return 0;
}
