// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/reference/geometry.hpp"

namespace gravastar::reference {
// Independent expansion of the BL components followed by their coordinate
// Jacobian. Unlike production this deliberately divides by q off the pole;
// use the arbitrary-precision backend for the near-pole finite differences.
// The exact pole value is the analytic limit. No production algebra or jets.
template <class T>
Matrix<T> angular_metric(T radius, T x, T y, const Parameters<T> &p,
                         Family family, Branch branch = Branch::automatic) {
  const auto [mu, lambda] = transition(radius, p, family, branch);
  const T q = x * x + y * y, den = (T(1) + q) * (T(1) + q), s2 = T(4) * q / den,
          c2 = (T(1) - q) * (T(1) - q) / den, a2 = p.a * p.a,
          r2 = radius * radius, k = p.L * lambda / T(3), m = p.m * mu,
          sigma = r2 + a2 * c2, A = r2 + a2, xi = T(1) + k * a2,
          theta = T(1) + k * a2 * c2,
          delta = A * (T(1) - k * r2) - T(2) * m * radius;
  Matrix<T> out{};
  out[0][0] =
      (T(-1) + k * (r2 + a2 * s2) + T(2) * m * radius / sigma) / (xi * xi);
  out[1][1] = sigma / delta;
  if (q == T(0)) {
    out[2][2] = out[3][3] = T(4) * sigma / theta;
    return out;
  }
  const T gtphi = -p.a * s2 * (k * A + T(2) * m * radius / sigma) / (xi * xi),
          gphiphi =
              s2 * (A * xi + T(2) * m * radius * a2 * s2 / sigma) / (xi * xi),
          radial_angular = T(4) * sigma / (theta * den),
          azimuthal_angular = gphiphi / q;
  // Normalize each Jacobian product before multiplication. With fixed decimal
  // places, forming an O(q^2) numerator first discards twice as many digits.
  out[0][2] = out[2][0] = -(gtphi / q) * y;
  out[0][3] = out[3][0] = (gtphi / q) * x;
  out[2][2] = radial_angular * (x * x / q) + azimuthal_angular * (y * y / q);
  out[3][3] = radial_angular * (y * y / q) + azimuthal_angular * (x * x / q);
  out[2][3] = out[3][2] =
      radial_angular * (x * y / q) - azimuthal_angular * (x * y / q);
  return out;
}
template <class T>
Derivatives<T> angular_derivatives(std::array<T, 3> position,
                                   const Parameters<T> &p, Family family, T h,
                                   Branch branch = Branch::automatic) {
  const std::array<T, 5> first{T(1) / T(12), T(-2) / T(3), T(0), T(2) / T(3),
                               T(-1) / T(12)};
  const std::array<T, 5> second{T(-1) / T(12), T(4) / T(3), T(-5) / T(2),
                                T(4) / T(3), T(-1) / T(12)};
  auto sample = [&](const std::array<T, 3> &point) {
    return angular_metric(point[0], point[1], point[2], p, family, branch);
  };
  Derivatives<T> out;
  out.g = sample(position);
  for (int axis = 0; axis < 3; ++axis) {
    for (int n = -2; n <= 2; ++n) {
      auto point = position;
      point[axis] += T(n) * h;
      const auto metric = sample(point);
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
          out.dg[axis + 1][i][j] += first[n + 2] * metric[i][j] / h;
          out.ddg[axis + 1][axis + 1][i][j] +=
              second[n + 2] * metric[i][j] / (h * h);
        }
    }
    for (int other = axis + 1; other < 3; ++other) {
      for (int n = -2; n <= 2; ++n)
        for (int m = -2; m <= 2; ++m)
          if (n && m) {
            auto point = position;
            point[axis] += T(n) * h;
            point[other] += T(m) * h;
            const auto metric = sample(point);
            for (int i = 0; i < 4; ++i)
              for (int j = 0; j < 4; ++j)
                out.ddg[axis + 1][other + 1][i][j] +=
                    first[n + 2] * first[m + 2] * metric[i][j] / (h * h);
          }
      out.ddg[other + 1][axis + 1] = out.ddg[axis + 1][other + 1];
    }
  }
  return out;
}
} // namespace gravastar::reference
