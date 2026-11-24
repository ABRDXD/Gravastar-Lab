// SPDX-License-Identifier: GPL-3.0-or-later
//
// Closed-form toy-gravastar Hamiltonian and its gradient in a stereographic
// patch, with no trigonometric functions. This is the CPU twin of
// shaders/gravastar_stereographic.glsl (Phase D precision study); both are
// checked against the production angular RHS, which uses automatic
// differentiation of the full metric instead.
//
// State (t, r, x, y, p_t, p_r, p_x, p_y), q = x^2 + y^2. In either patch
//   cos^2 theta = ((1-q)/(1+q))^2,  sin^2 theta = 4q/(1+q)^2,
//   L = p_phi = x p_y - y p_x,  R = x p_x + y p_y,  R^2 + L^2 = q P^2.
// With k = Lambda lambda(r) / 3, T = 1 + k a^2 c^2, X = 1 + k a^2,
// D = B (1 - k r^2) - 2 m r, B = r^2 + a^2, W = B p_t + a L, and
// Sigma = r^2 + a^2 c^2, the Boyer-Lindquist form of MODEL.md becomes
//   2 Sigma H = D p_r^2 + (1+q)^2/4 T P^2 + k a^2 (X+T) L^2 / T
//             + (X^2/T)(a^2 s^2 p_t^2 + 2 a p_t L) - X^2 W^2 / D,
// regular at q = 0 because X - T = k a^2 s^2. Production sign convention:
// ds is past-directed, dx/ds = -dH/dp, dp/ds = +dH/dx.
#pragma once
#include <array>
#include <cmath>

namespace gravastar::test_reference {
template <class Real> struct StereoModel {
  Real M, a, Lambda, r_in, r_out;
};

// Returns d(state)/ds and writes H. Assumes the state is in a valid patch.
template <class Real>
std::array<Real, 8> stereographic_rhs(const StereoModel<Real> &q_model,
                                      const std::array<Real, 8> &y, Real &H) {
  const Real r = y[1], x = y[2], v = y[3];
  const Real pt = y[4], pr = y[5], px = y[6], py = y[7];
  const Real a = q_model.a, a2 = a * a;
  // Radial profile and its r-derivative (C3 smoothstep of MODEL.md).
  Real m = q_model.M, dm = 0, k = 0, dk = 0;
  if (r <= q_model.r_in) {
    m = 0;
    k = q_model.Lambda / 3;
  } else if (r < q_model.r_out) {
    const Real w = q_model.r_out - q_model.r_in, u = (r - q_model.r_in) / w;
    const Real u2 = u * u, u3 = u2 * u, u4 = u2 * u2;
    const Real S = u4 * (35 - 84 * u + 70 * u2 - 20 * u3);
    const Real dS = u3 * (140 - 420 * u + 420 * u2 - 140 * u3) / w;
    m = q_model.M * S;
    dm = q_model.M * dS;
    k = q_model.Lambda * (1 - S) / 3;
    dk = -q_model.Lambda * dS / 3;
  }
  const Real q = x * x + v * v, op = 1 + q;
  const Real c = (1 - q) / op, c2 = c * c, s2 = 4 * q / (op * op);
  const Real dc2 = 2 * c * (-2 / (op * op)); // d(c^2)/dq
  const Real ds2 = -dc2;
  const Real B = r * r + a2, sigma = r * r + a2 * c2;
  const Real T = 1 + k * a2 * c2, X = 1 + k * a2;
  const Real D = B * (1 - k * r * r) - 2 * m * r;
  const Real L = x * py - v * px, P2 = px * px + py * py;
  const Real W = B * pt + a * L;
  const Real ang = a2 * s2 * pt * pt + 2 * a * pt * L;
  const Real E1 = op * op / 4 * T * P2;
  const Real E2 = k * a2 * (X + T) * L * L / T;
  const Real E3 = X * X / T * ang;
  const Real E4 = X * X * W * W / D;
  const Real N = D * pr * pr + E1 + E2 + E3 - E4;
  H = N / (2 * sigma);

  // r derivatives at fixed q, L and momenta.
  const Real dB = 2 * r, dX = a2 * dk, dT = a2 * c2 * dk;
  const Real dD = dB * (1 - k * r * r) + B * (-dk * r * r - 2 * k * r) -
                  2 * (dm * r + m);
  const Real dN_dr =
      dD * pr * pr + op * op / 4 * P2 * dT +
      a2 * L * L * (dk * (X + T) / T + k * (dX + dT) / T - k * (X + T) * dT / (T * T)) +
      ang * (2 * X * dX / T - X * X * dT / (T * T)) -
      ((2 * X * dX * W * W + X * X * 2 * W * dB * pt) / D - E4 * dD / D);
  // q derivatives at fixed r, L and momenta.
  const Real dTq = k * a2 * dc2;
  const Real dN_dq = P2 * (op / 2 * T + op * op / 4 * dTq) -
                     k * a2 * L * L * X * dTq / (T * T) +
                     X * X * (a2 * pt * pt * ds2 / T - ang * dTq / (T * T));
  // L derivative at fixed r, q and momenta.
  const Real dN_dL = 2 * k * a2 * (X + T) * L / T + X * X / T * 2 * a * pt -
                     X * X * 2 * W * a / D;
  const Real dsigma_dr = 2 * r, dsigma_dq = a2 * dc2;
  const Real inv = 1 / (2 * sigma);
  const Real dH_dr = dN_dr * inv - H * dsigma_dr / sigma;
  const Real dH_dq = dN_dq * inv - H * dsigma_dq / sigma;
  const Real dH_dL = dN_dL * inv;
  // Chain rule: q = x^2 + y^2, L = x p_y - y p_x.
  const Real dH_dx = dH_dq * 2 * x + dH_dL * py;
  const Real dH_dy = dH_dq * 2 * v - dH_dL * px;
  const Real dH_dpt = (X * X / T * (2 * a2 * s2 * pt + 2 * a * L) -
                       X * X * 2 * W * B / D) * inv;
  const Real dH_dpr = 2 * D * pr * inv;
  const Real dH_dpx = op * op / 2 * T * px * inv - dH_dL * v;
  const Real dH_dpy = op * op / 2 * T * py * inv + dH_dL * x;
  return {-dH_dpt, -dH_dpr, -dH_dpx, -dH_dpy, 0, dH_dr, dH_dx, dH_dy};
}

// Inversion to the opposite patch: (u, v) = (x, y)/q, and covariant momenta
// p' = q p - 2 (x, y)((x, y).p). Preserves L.
template <class Real> std::array<Real, 8> switch_patch(const std::array<Real, 8> &y) {
  const Real q = y[2] * y[2] + y[3] * y[3];
  const Real R = y[2] * y[6] + y[3] * y[7];
  return {y[0], y[1], y[2] / q, y[3] / q, y[4], y[5],
          q * y[6] - 2 * y[2] * R, q * y[7] - 2 * y[3] * R};
}
} // namespace gravastar::test_reference
