// SPDX-License-Identifier: GPL-3.0-or-later
//
// Independent off-equatorial null-ray reference for the toy gravastar.
// Built from the MODEL.md formulas only; it includes no production geometry,
// chart, derivative or integrator code.
//
// The MODEL.md metric has the algebraic Kerr-de Sitter form at every point,
// with r-dependent m, L (hence D, T, X). Its inverse contracted with p gives
//   2 Sigma H = D p_r^2 + T p_theta^2 - X^2 (B p_t + a p_phi)^2 / D
//             + X^2 (a^2 sin^2 p_t^2 + 2 a p_t p_phi + p_phi^2 / sin^2) / T,
// B = r^2 + a^2. This is a closed form, not the production numerical inverse.
// Inside the shell T and X depend on r, so the Carter constant is not
// conserved there; this reference does not assume separability.
//
// Partial derivatives use the complex step Im H(x + i h) / h, which has no
// subtractive cancellation. Integration is classical fourth-order Runge-Kutta
// with a fixed affine step, in long double. Like the production tracer, the
// parameter increases into the past: dx/ds = -dH/dp, dp/ds = +dH/dx.
//
// Theta is signed. With p_phi = 0 exactly, every term above is regular at
// sin(theta) = 0, so axial-momentum-free rays cross either pole without a
// chart change; the physical direction is (sin th cos ph, sin th sin ph,
// cos th) for any signed theta. Rays with p_phi != 0 must stay off the axis.
// The radius must stay positive: the BL oblate chart ends at r = 0.
#pragma once
#include "equatorial_reference.hpp"
#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

namespace gravastar::test_reference {
using Phase = std::array<Real, 8>; // t r theta phi p_t p_r p_theta p_phi

template <class Z>
Z hamiltonian(const Model &q, const std::array<Z, 8> &y) {
  using std::cos, std::sin, std::real;
  const Z r = y[1], theta = y[2], pt = y[4], pr = y[5], pth = y[6],
          pphi = y[7];
  Z m = Z(q.M), L = Z(0);
  if (real(r) <= q.r_in) {
    m = Z(0);
    L = Z(q.Lambda);
  } else if (real(r) < q.r_out) {
    const Z x = (r - q.r_in) / (q.r_out - q.r_in);
    const Z x2 = x * x, x4 = x2 * x2;
    const Z S = x4 * (Real(35) - Real(84) * x + Real(70) * x2 -
                      Real(20) * x2 * x);
    m = q.M * S;
    L = q.Lambda * (Real(1) - S);
  }
  const Real a = q.a;
  const Z c = cos(theta), s = sin(theta), c2 = c * c, s2 = s * s;
  const Z B = r * r + a * a, Sigma = r * r + a * a * c2;
  const Z D = B * (Real(1) - L * r * r / Real(3)) - Real(2) * m * r;
  const Z T = Real(1) + L * a * a * c2 / Real(3);
  const Z X = Real(1) + L * a * a / Real(3);
  Z angular = a * a * s2 * pt * pt + Real(2) * a * pt * pphi;
  // p_phi^2/sin^2 has zero value and zero first derivative at p_phi = 0, so
  // dropping it there is exact for H and for the complex-step gradient.
  if (real(pphi) != 0)
    angular += pphi * pphi / s2;
  const Z AB = B * pt + a * pphi;
  return (D * pr * pr + T * pth * pth - X * X * AB * AB / D +
          X * X * angular / T) /
         (Real(2) * Sigma);
}

inline Real hamiltonian(const Model &q, const Phase &y) {
  return hamiltonian<Real>(q, y);
}

inline Real partial(const Model &q, const Phase &y, int i) {
  using C = std::complex<Real>;
  constexpr Real h = 1e-100L;
  std::array<C, 8> z{};
  for (int k = 0; k < 8; ++k)
    z[k] = C(y[k], 0);
  z[i] += C(0, h);
  return std::imag(hamiltonian<C>(q, z)) / h;
}

inline Phase derivative(const Model &q, const Phase &y) {
  Phase out{};
  for (int mu = 0; mu < 4; ++mu)
    out[mu] = -partial(q, y, mu + 4);
  // t and phi are cyclic: dp_t/ds = dp_phi/ds = 0.
  out[5] = partial(q, y, 1);
  out[6] = partial(q, y, 2);
  for (Real v : out)
    if (!std::isfinite(v))
      throw std::runtime_error("reference derivative is not finite");
  return out;
}

inline Phase rk4(const Model &q, const Phase &y, Real h) {
  auto shifted = [&](const Phase &k, Real f) {
    Phase v = y;
    for (int i = 0; i < 8; ++i)
      v[i] += f * h * k[i];
    return v;
  };
  const Phase k1 = derivative(q, y), k2 = derivative(q, shifted(k1, .5L)),
              k3 = derivative(q, shifted(k2, .5L)),
              k4 = derivative(q, shifted(k3, 1));
  Phase out = y;
  for (int i = 0; i < 8; ++i)
    out[i] += h * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]) / 6;
  return out;
}

// Future-directed outgoing p_r for given p_t, p_theta, p_phi (H = 0), so the
// past-directed trace moves inward. Requires the null cone to allow it.
inline Phase null_launch(const Model &q, Real r, Real theta, Real pt,
                         Real ptheta, Real pphi) {
  Phase y{0, r, theta, 0, pt, 0, ptheta, pphi};
  const Real rest = hamiltonian(q, y);
  y[5] = 1;
  const Real coefficient = hamiltonian(q, y) - rest; // D/(2 Sigma)
  if (!(coefficient > 0 && rest < 0))
    throw std::runtime_error("reference launch is not an inward null ray");
  y[5] = std::sqrt(-rest / coefficient);
  return y;
}

struct RadialEvent {
  Real radius;
  bool inward; // r decreasing along the past-directed parameter
  Real affine;
  Phase state;
};
struct PoleEvent {
  Real affine, radius;
};
struct RayResult {
  bool escaped{};
  Real affine{}, minimum_radius{}, maximum_hamiltonian{};
  Phase final{};
  std::vector<RadialEvent> events;
  std::vector<PoleEvent> poles; // sin(theta) changes sign: axis crossing
};

// Integrate until r reaches r_escape (outward) or the affine budget ends.
// The shell profile has large high derivatives (S'''' ~ 1e5 for a 0.3-wide
// shell), so steps that start within 0.05 of the shell are h/shell_divisor.
// Crossings of the listed radii are located by bisection on the length of a
// single RK4 substep from the bracketing step's start. Fixed steps can miss
// an excursion shallower than a step resolves; callers use non-grazing rays.
inline RayResult trace(const Model &q, const Phase &initial, Real h,
                       Real r_escape, Real budget,
                       const std::vector<Real> &radii,
                       int shell_divisor = 32) {
  RayResult out;
  Phase y = initial;
  Real s = 0;
  out.minimum_radius = y[1];
  const Real scale = std::abs(initial[4]) * std::abs(initial[4]);
  auto locate = [&](const Phase &start, Real r_target, Real length) {
    Real lo = 0, hi = length;
    const bool start_above = start[1] > r_target;
    for (int i = 0; i < 200 && hi - lo > 1e-30L; ++i) {
      const Real mid = (lo + hi) / 2;
      ((rk4(q, start, mid)[1] > r_target) == start_above ? lo : hi) = mid;
    }
    return hi;
  };
  const Real base = h;
  while (s < budget) {
    h = (y[1] > q.r_in - .05L && y[1] < q.r_out + .05L) ? base / shell_divisor
                                                        : base;
    const Phase next = rk4(q, y, h);
    if (next[1] <= 0)
      throw std::runtime_error("reference reached the r = 0 chart boundary");
    for (Real radius : radii)
      if ((y[1] - radius) * (next[1] - radius) < 0) {
        const Real tau = locate(y, radius, h);
        out.events.push_back(
            {radius, next[1] < radius, s + tau, rk4(q, y, tau)});
      }
    if (std::sin(y[2]) * std::sin(next[2]) < 0) {
      Real lo = 0, hi = h;
      for (int i = 0; i < 200 && hi - lo > 1e-30L; ++i) {
        const Real mid = (lo + hi) / 2;
        (std::sin(rk4(q, y, mid)[2]) * std::sin(y[2]) > 0 ? lo : hi) = mid;
      }
      out.poles.push_back({s + hi, rk4(q, y, hi)[1]});
    }
    if (next[1] >= r_escape && y[1] < r_escape) {
      const Real tau = locate(y, r_escape, h);
      out.final = rk4(q, y, tau);
      out.affine = s + tau;
      out.escaped = true;
      return out;
    }
    y = next;
    s += h;
    out.minimum_radius = std::min(out.minimum_radius, y[1]);
    out.maximum_hamiltonian =
        std::max(out.maximum_hamiltonian, std::abs(hamiltonian(q, y)) / scale);
  }
  out.final = y;
  out.affine = s;
  return out;
}

inline std::array<Real, 3> direction(const Phase &y) {
  return {std::sin(y[2]) * std::cos(y[3]), std::sin(y[2]) * std::sin(y[3]),
          std::cos(y[2])};
}

} // namespace gravastar::test_reference
