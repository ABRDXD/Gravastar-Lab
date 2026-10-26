// SPDX-License-Identifier: MIT
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/math/jet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>

namespace {
using namespace gravastar::geometry;
int failures = 0;

void check(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL " << message << '\n';
    ++failures;
  }
}
void near(double actual, double expected, double tolerance,
          const std::string &message) {
  check(std::isfinite(actual) &&
            std::abs(actual - expected) <=
                tolerance * std::max(1.0, std::abs(expected)),
        message);
}
Evaluation at(const Parameters &p, double r, double theta,
              Region region = Region::automatic) {
  const auto result = evaluate(p, r, theta, region);
  if (const auto *error = std::get_if<Error>(&result)) {
    std::cerr << "Unexpected evaluation error: " << error->code << ": "
              << error->message << '\n';
    std::exit(2);
  }
  return std::get<Evaluation>(result);
}
void inverse_check(const Evaluation &e) {
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      double value = 0;
      for (int k = 0; k < 4; ++k)
        value += e.metric[i][k] * e.inverse[k][j];
      near(value, i == j ? 1 : 0, 3e-12, "inverse identity");
      near(e.metric[i][j], e.metric[j][i], 0, "metric symmetry");
      near(e.ricci[i][j], e.ricci[j][i], 5e-11, "Ricci symmetry");
    }
}
void reference_cases() {
  for (double spin : {0.0, 0.875, -0.875}) {
    Parameters p{Family::flat, 0, spin, 0, 1, 2};
    for (double r : {0.3, 1.5, 5.0})
      for (double theta : {0.2, 1.2, 2.8}) {
        const auto flat = at(p, r, theta);
        inverse_check(flat);
        near(flat.ricci_scalar, 0, 1e-11, "flat Ricci scalar");
        near(flat.kretschmann, 0, 1e-18, "flat Kretschmann");
      }
    p.family = Family::de_sitter;
    p.cosmological_constant = 0.5;
    for (double r : {0.3, 1.3, 3.0}) {
      const auto dS = at(p, r, 1.1);
      inverse_check(dS);
      near(dS.ricci_scalar, 2, 2e-11, "de Sitter R=4 Lambda");
      near(dS.kretschmann, 2.0 / 3.0, 2e-11, "de Sitter K=8 Lambda^2/3");
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
          near(dS.ricci[i][j], 0.5 * dS.metric[i][j], 2e-11,
               "de Sitter Ricci tensor");
    }
    p.family = Family::kerr;
    p.mass = 1;
    for (double r : {2.2, 4.0, 20.0}) {
      constexpr double theta = 1.2;
      const auto kerr = at(p, r, theta);
      inverse_check(kerr);
      for (const auto &row : kerr.ricci)
        for (double entry : row)
          near(entry, 0, 2e-11, "Kerr vacuum Ricci");
      const double x = spin * spin * std::pow(std::cos(theta), 2);
      const double r2 = r * r;
      const double expected =
          48 * (r2 * r2 * r2 - 15 * r2 * r2 * x + 15 * r2 * x * x - x * x * x) /
          std::pow(r2 + x, 6);
      near(kerr.kretschmann, expected, 2e-11, "analytic Kerr Kretschmann");
    }
  }
  const Parameters schwarzschild{Family::kerr, 1, 0, 0, 1, 2};
  const auto e = at(schwarzschild, 4, std::numbers::pi / 2);
  near(e.metric[0][0], -0.5, 1e-14, "Schwarzschild g_tt");
  near(e.metric[1][1], 2, 1e-14, "Schwarzschild g_rr");
  near(e.dg[1][0][0], -0.125, 1e-14, "Schwarzschild d_r g_tt");
  near(e.ddg[1][1][0][0], 0.0625, 1e-14, "Schwarzschild d_rr g_tt");
  near(e.drrr[0][0], -0.046875, 1e-14, "Schwarzschild d_rrr g_tt");
}
void matching_and_derivatives() {
  Parameters p{Family::toy, 1, 0.875, 0.5, 1.5, 1.8};
  for (double r : {p.r_in, p.r_out}) {
    auto reference = p;
    reference.family = r == p.r_in ? Family::de_sitter : Family::kerr;
    for (double theta : {0.3, 1.2, 2.7}) {
      const auto join = at(p, r, theta, Region::shell);
      const auto exact = at(reference, r, theta);
      inverse_check(join);
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
          near(join.metric[i][j], exact.metric[i][j], 1e-14, "join value");
          near(join.dg[1][i][j], exact.dg[1][i][j], 1e-14,
               "join first derivative");
          near(join.ddg[1][1][i][j], exact.ddg[1][1][i][j], 1e-14,
               "join second derivative");
          near(join.drrr[i][j], exact.drrr[i][j], 1e-14,
               "join third derivative");
          if (i != 1 && j != 1)
            near(join.dg[1][i][j] / (2 * std::sqrt(join.metric[1][1])),
                 exact.dg[1][i][j] / (2 * std::sqrt(exact.metric[1][1])), 2e-13,
                 "one-sided outward-normal extrinsic curvature");
        }
    }
  }
  near(at(p, 1.65, 1.2).mu, 0.5, 1e-14, "transition midpoint");
  const auto upper = at(p, p.r_out - 1e-8, 1.2);
  check(upper.lambda > 0 && upper.lambda < 1e-28,
        "upper transition tail survives cancellation");
  // Check approach from the shell, where the polynomial must tend to the
  // constant branch through third order. Third derivatives approach linearly.
  for (double join : {p.r_in, p.r_out}) {
    const auto endpoint = at(p, join, 1.2);
    const double direction = join == p.r_in ? 1 : -1;
    double previous_error = 0;
    for (double distance : {1e-5, 5e-6, 2.5e-6}) {
      const auto nearby = at(p, join + direction * distance, 1.2);
      double error = 0;
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
          error = std::max(error,
                           std::abs(nearby.drrr[i][j] - endpoint.drrr[i][j]));
      if (previous_error > 0)
        check(error < previous_error * 0.51,
              "third derivative converges to join from shell");
      previous_error = error;
    }
  }
  for (double r : {0.7, 1.57, 1.73, 3.0}) {
    constexpr double theta = 1.1;
    const auto exact = at(p, r, theta);
    inverse_check(exact);
    for (int axis : {1, 2}) {
      double previous_error = 0;
      for (double h : {0.001, 0.0005}) {
        const auto plus =
            at(p, r + (axis == 1 ? h : 0), theta + (axis == 2 ? h : 0));
        const auto minus =
            at(p, r - (axis == 1 ? h : 0), theta - (axis == 2 ? h : 0));
        double error = 0;
        for (int i = 0; i < 4; ++i)
          for (int j = 0; j < 4; ++j) {
            const double fd =
                (plus.metric[i][j] - minus.metric[i][j]) / (2 * h);
            error = std::max(error, std::abs(fd - exact.dg[axis][i][j]));
          }
        if (previous_error > 1e-8)
          check(error < previous_error * 0.3,
                "centered difference second-order convergence");
        previous_error = error;
      }
    }
  }
}
void jet_cases() {
  using namespace gravastar::math;
  const Jet x = Jet::variable(2, 0), y = Jet::variable(3, 1);
  const Jet polynomial = x * x * y + x * y * y;
  near(polynomial.derivative(1, 1), 10, 0, "jet mixed derivative");
  near(polynomial.derivative(2, 1), 2, 0, "jet third mixed derivative");
  near((Jet(1) / x).derivative(3, 0), -0.375, 1e-15,
       "jet reciprocal third derivative");
  near(sin(x).derivative(3, 0), -std::cos(2.0), 1e-15,
       "jet sine third derivative");
  near(cos(y).derivative(0, 2), -std::cos(3.0), 1e-15,
       "jet cosine second derivative");
}
void error_cases() {
  Parameters p{Family::toy, 1, 0.875, 0.5, 1, 2};
  for (double theta : {0.0, std::numbers::pi, -1.0})
    check(std::holds_alternative<Error>(evaluate(p, 1.5, theta)),
          "reject axis/outside angle");
  for (double r : {0.0, -1.0, std::numeric_limits<double>::infinity()})
    check(std::holds_alternative<Error>(evaluate(p, r, 1.0)),
          "reject invalid radius");
  p.mass = -1;
  check(std::holds_alternative<Error>(evaluate(p, 1.5, 1.0)),
        "reject negative mass");
  p.mass = std::numeric_limits<double>::quiet_NaN();
  check(std::holds_alternative<Error>(evaluate(p, 1.5, 1.0)), "reject NaN");
  p = Parameters{Family::kerr, 1, 0, 0, 1, 2};
  const auto horizon = evaluate(p, 2, 1);
  check(std::holds_alternative<Error>(horizon),
        "reject Schwarzschild BL horizon");
  if (const auto *error = std::get_if<Error>(&horizon))
    check(error->code == "chart_singularity", "structured chart error");
  p.r_out = p.r_in;
  check(std::holds_alternative<Error>(evaluate(p, 3, 1)),
        "reject unordered shell");
  p = Parameters{};
  p.cosmological_constant = -0.1;
  check(std::holds_alternative<Error>(evaluate(p, 3, 1)),
        "reject negative Lambda");
  p = Parameters{};
  p.spin = std::numeric_limits<double>::max();
  const auto overflow = evaluate(p, 3, 1);
  check(std::holds_alternative<Error>(overflow),
        "reject overflowed intermediate");
  if (const auto *error = std::get_if<Error>(&overflow))
    check(error->code == "numerical_failure", "structured overflow error");
  p = Parameters{};
  p.family = static_cast<Family>(100);
  check(std::holds_alternative<Error>(evaluate(p, 3, 1)),
        "reject unknown enum");
  p = Parameters{};
  check(std::holds_alternative<Error>(evaluate(p, 3, 1, Region::shell)),
        "reject shell extrapolation");
  check(std::holds_alternative<Error>(evaluate(p, 2, 1, Region::core)),
        "reject core extrapolation");
  check(std::holds_alternative<Error>(evaluate(p, 1, 1, Region::exterior)),
        "reject exterior extrapolation");
  check(std::holds_alternative<Error>(
            evaluate(p, 1.6, 1, static_cast<Region>(100))),
        "reject unknown region");
  p.family = Family::kerr;
  check(std::holds_alternative<Error>(evaluate(p, 3, 1, Region::exterior)),
        "explicit region requires toy");
}
void migrated_alternate_grid() {
  // Coverage carried over from the retired metric.hpp/geometry_tests.cpp track.
  Parameters p{Family::toy, 1, .875, .5, 1.8, 2.2};
  for (double r : {.2, 1.5, 1.8, 1.9, 2., 2.1, 2.2, 4., 12.})
    for (double th : {.05, .7, 1.57, 3.09}) {
      auto e = at(p, r, th);
      inverse_check(e);
      check(e.delta > 0, "alternate shell radial positivity");
    }
  for (double r : {1.8, 2.2})
    for (double th : {.2, 1., 2.7}) {
      auto shell = at(p, r, th, Region::shell),
           other = at(p, r, th, r == p.r_in ? Region::core : Region::exterior);
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
          near(shell.metric[i][j], other.metric[i][j], 1e-12,
               "alternate shell C0");
          near(shell.dg[1][i][j], other.dg[1][i][j], 1e-11,
               "alternate shell C1");
          near(shell.ddg[1][1][i][j], other.ddg[1][1][i][j], 1e-10,
               "alternate shell C2");
          near(shell.drrr[i][j], other.drrr[i][j], 1e-9, "alternate shell C3");
        }
    }
}
void coordinate_and_scale_checks() {
  Parameters p;
  for (double r : {0.7, 1.62, 3.0}) {
    const auto base = at(p, r, 1.1);
    const auto reflected = at(p, r, std::numbers::pi - 1.1);
    auto reverse = p;
    reverse.spin = -p.spin;
    const auto reversed = at(reverse, r, 1.1);
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) {
        near(reflected.metric[i][j], base.metric[i][j], 1e-12,
             "theta reflection metric");
        near(reflected.dg[2][i][j], -base.dg[2][i][j], 1e-11,
             "theta reflection covector derivative");
        const double sign = (i == 3) != (j == 3) ? -1 : 1;
        near(reversed.metric[i][j], sign * base.metric[i][j], 1e-12,
             "spin reversal phi orientation");
      }
    constexpr double scale = 3;
    auto larger = p;
    larger.mass *= scale;
    larger.spin *= scale;
    larger.r_in *= scale;
    larger.r_out *= scale;
    larger.cosmological_constant /= scale * scale;
    const auto changed = at(larger, scale * r, 1.1);
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) {
        const double factor = (i >= 2 ? scale : 1) * (j >= 2 ? scale : 1);
        near(changed.metric[i][j], factor * base.metric[i][j], 2e-11,
             "mass unit scaling");
      }
    near(changed.ricci_scalar * scale * scale, base.ricci_scalar, 2e-10,
         "Ricci unit scaling");
    near(changed.kretschmann * std::pow(scale, 4), base.kretschmann, 2e-10,
         "Kretschmann unit scaling");
  }
}
} // namespace

int main() {
  jet_cases();
  reference_cases();
  matching_and_derivatives();
  error_cases();
  coordinate_and_scale_checks();
  migrated_alternate_grid();
  if (failures != 0) {
    std::cerr << failures << " checks failed\n";
    return 1;
  }
  std::cout
      << "Geometry checks passed: jets, flat, Schwarzschild, Kerr, de Sitter, "
         "matching, derivative convergence, inverse, structured errors.\n";
  return 0;
}
