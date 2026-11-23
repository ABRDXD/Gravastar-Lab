// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>

namespace gravastar::math {
// Forward derivatives in (r,x,y). First-order instantiations do not compute or
// store Hessians. Entries are derivatives, not factorial-scaled coefficients.
template <int Order> struct SpatialJet {
  static_assert(Order == 1 || Order == 2);
  double v{};
  std::array<double, 3> d{};
  std::array<std::array<double, 3>, Order == 2 ? 3 : 0> h{};
  SpatialJet() = default;
  SpatialJet(double value) : v(value) {} // NOLINT: scalar algebra
  double value() const { return v; }
  static SpatialJet variable(double value, std::size_t axis) {
    SpatialJet result(value);
    result.d.at(axis) = 1;
    return result;
  }
};
template <int O>
SpatialJet<O> operator+(const SpatialJet<O> &a, const SpatialJet<O> &b) {
  SpatialJet<O> out(a.v + b.v);
  for (int i = 0; i < 3; ++i) {
    out.d[i] = a.d[i] + b.d[i];
    if constexpr (O == 2)
      for (int j = 0; j < 3; ++j)
        out.h[i][j] = a.h[i][j] + b.h[i][j];
  }
  return out;
}
template <int O>
SpatialJet<O> operator-(const SpatialJet<O> &a, const SpatialJet<O> &b) {
  SpatialJet<O> out(a.v - b.v);
  for (int i = 0; i < 3; ++i) {
    out.d[i] = a.d[i] - b.d[i];
    if constexpr (O == 2)
      for (int j = 0; j < 3; ++j)
        out.h[i][j] = a.h[i][j] - b.h[i][j];
  }
  return out;
}
template <int O> SpatialJet<O> operator-(const SpatialJet<O> &a) {
  return SpatialJet<O>(0) - a;
}
template <int O>
SpatialJet<O> operator*(const SpatialJet<O> &a, const SpatialJet<O> &b) {
  SpatialJet<O> out(a.v * b.v);
  for (int i = 0; i < 3; ++i) {
    out.d[i] = a.d[i] * b.v + a.v * b.d[i];
    if constexpr (O == 2)
      for (int j = 0; j < 3; ++j)
        out.h[i][j] = a.h[i][j] * b.v + a.d[i] * b.d[j] + a.d[j] * b.d[i] +
                      a.v * b.h[i][j];
  }
  return out;
}
template <int O> SpatialJet<O> reciprocal(const SpatialJet<O> &a) {
  const double inv = 1 / a.v, inv2 = inv * inv;
  SpatialJet<O> out(inv);
  for (int i = 0; i < 3; ++i) {
    out.d[i] = -a.d[i] * inv2;
    if constexpr (O == 2)
      for (int j = 0; j < 3; ++j)
        out.h[i][j] = 2 * inv2 * inv * a.d[i] * a.d[j] - inv2 * a.h[i][j];
  }
  return out;
}
template <int O>
SpatialJet<O> operator/(const SpatialJet<O> &a, const SpatialJet<O> &b) {
  return a * reciprocal(b);
}
} // namespace gravastar::math
