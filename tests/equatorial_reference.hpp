// SPDX-License-Identifier: GPL-3.0-or-later
//
// Independent equatorial reference for the toy gravastar, shared by tests.
// Built from the MODEL.md formulas only; it includes no production geometry.
//
// On the equator (cos theta = 0, sin theta = 1, T = 1) with p_t = -1,
// p_phi = b and p_theta = 0, the null condition at a radial turning point
// p_r = 0 reduces, after removing the positive factor Sigma X^2 and the
// t-phi block determinant, to
//   W(r) = (r^2 + a^2 - a b)^2 - D(r) (b - a)^2 = 0,
//   D(r) = (r^2 + a^2)(1 - L r^2 / 3) - 2 m r,
// where (m, L) = (0, Lambda) in the core, (M S(u), Lambda (1 - S(u))) in the
// shell with S(u) = 35u^4 - 84u^5 + 70u^6 - 20u^7, and (M, 0) outside.
// This is the Kerr equatorial radial potential with r-dependent m and L.
// An incoming ray from r_start turns at the largest root of W below it.
#pragma once
#include <stdexcept>

namespace gravastar::test_reference {
using Real = long double;

struct Model {
  Real M, a, Lambda, r_in, r_out;
};

inline Real W(const Model &q, Real r, Real b) {
  Real m = q.M, L = 0;
  if (r <= q.r_in) {
    m = 0;
    L = q.Lambda;
  } else if (r < q.r_out) {
    const Real x = (r - q.r_in) / (q.r_out - q.r_in);
    const Real x2 = x * x, x4 = x2 * x2;
    const Real S = x4 * (35 - 84 * x + 70 * x2 - 20 * x2 * x);
    m = q.M * S;
    L = q.Lambda * (1 - S);
  }
  const Real B = r * r + q.a * q.a;
  const Real D = B * (1 - L * r * r / 3) - 2 * m * r;
  const Real A = B - q.a * b;
  return A * A - D * (b - q.a) * (b - q.a);
}

// Largest root of W below r_start: scan down until W < 0, then bisect.
inline Real turning_radius(const Model &q, Real b, Real r_start) {
  Real hi = r_start;
  if (!(W(q, hi, b) > 0))
    throw std::runtime_error("launch radius is not in an allowed region");
  const Real step = 1e-4L;
  Real lo = hi - step;
  while (W(q, lo, b) > 0) {
    hi = lo;
    lo -= step;
    if (lo < .1L)
      throw std::runtime_error("no turning point above r = 0.1");
  }
  for (int i = 0; i < 200 && hi - lo > 1e-18L; ++i) {
    const Real mid = (lo + hi) / 2;
    (W(q, mid, b) > 0 ? hi : lo) = mid;
  }
  return (lo + hi) / 2;
}

} // namespace gravastar::test_reference
