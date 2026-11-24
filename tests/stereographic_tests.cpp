// SPDX-License-Identifier: GPL-3.0-or-later
//
// The closed-form stereographic Hamiltonian (tests/stereographic_hamiltonian.hpp,
// GPU twin shaders/gravastar_stereographic.glsl) against the production
// angular RHS and inverse metric, which use automatic differentiation of the
// full metric instead, and its patch switch against transform_state.
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "stereographic_hamiltonian.hpp"
#include <iostream>
#include <random>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace R = gravastar::test_reference;
int main() {
  try {
    g::Parameters p; // toy preset
    const R::StereoModel<double> m{p.mass, p.spin, p.cosmological_constant, p.r_in,
                                   p.r_out};
    std::mt19937_64 random(20261124);
    std::uniform_real_distribution<double> radius(.3, 9), patch(-1.33, 1.33),
        momentum(-2, 2);
    double rhs = 0, hamiltonian = 0, inversion = 0;
    std::size_t switched = 0;
    for (int n = 0; n < 4000; ++n) {
      o::State y{0, radius(random), patch(random), patch(random),
                 momentum(random), momentum(random), momentum(random),
                 momentum(random)};
      if (n % 4 == 0) // a quarter of the samples inside the shell
        y[1] = p.r_in + (p.r_out - p.r_in) * (n % 101) / 100.0;
      if (n % 7 == 0) { // near the pole
        y[2] *= 1e-4;
        y[3] *= 1e-4;
      }
      const auto production = o::angular_rhs(p, y);
      double H;
      const auto closed = R::stereographic_rhs(m, y, H);
      double scale = 0;
      for (double v : production)
        scale = std::max(scale, std::abs(v));
      for (int i = 0; i < 8; ++i)
        rhs = std::max(rhs, std::abs(production[i] - closed[i]) / scale);
      const auto metric = o::angular_metric(p, y);
      double sum = 0, size = 0;
      for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b) {
          const double t = .5 * metric.inverse[a][b] * y[a + 4] * y[b + 4];
          sum += t;
          size += std::abs(t);
        }
      hamiltonian = std::max(hamiltonian, std::abs(H - sum) / size);
      const double q = y[2] * y[2] + y[3] * y[3];
      if (q >= .25 && q <= 4) {
        ++switched;
        const auto mine = R::switch_patch(y);
        const auto theirs = o::transform_state(y, o::Chart::north, o::Chart::south);
        for (int i = 0; i < 8; ++i)
          inversion = std::max(inversion, std::abs(mine[i] - theirs[i]) /
                                              (1 + std::abs(theirs[i])));
      }
    }
    int failures = 0;
    auto check = [&](const char *name, double measured, double ceiling) {
      const bool pass = measured <= ceiling;
      failures += !pass;
      std::cout << (pass ? "  ok   " : "  FAIL ") << name << ": " << measured
                << " (ceiling " << ceiling << ")\n";
    };
    check("closed-form RHS vs production angular RHS (4000 states)", rhs, 2e-12);
    check("closed-form H vs production inverse metric", hamiltonian, 2e-13);
    check("closed-form patch switch vs transform_state", inversion, 1e-14);
    check("patch switch samples", switched >= 1000 ? 0 : 1, 0);
    std::cout << (failures ? "FAIL" : "PASS") << " stereographic closed form\n";
    return failures ? 1 : 0;
  } catch (const std::exception &e) {
    std::cerr << "stereographic: " << e.what() << '\n';
    return 1;
  }
}
