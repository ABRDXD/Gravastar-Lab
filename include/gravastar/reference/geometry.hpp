// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/reference/number.hpp"
#include <array>

namespace gravastar::reference {
template <class T> using Matrix = std::array<std::array<T, 4>, 4>;
template <class T> using Tensor3 = std::array<Matrix<T>, 4>;
template <class T> using Tensor4 = std::array<Tensor3<T>, 4>;
enum class Family { toy, kerr, de_sitter, flat };
enum class Branch { automatic, core, shell, exterior };
template <class T> struct Parameters {
  T m{1}, a{std::string(".875")}, L{std::string(".5")}, ri{std::string("1.5")},
      ro{std::string("1.8")};
};
template <class T>
std::pair<T, T> transition(T r, const Parameters<T> &p, Family family,
                           Branch branch) {
  if (family == Family::flat)
    return {T(0), T(0)};
  if (family == Family::kerr)
    return {T(1), T(0)};
  if (family == Family::de_sitter)
    return {T(0), T(1)};
  if (branch == Branch::core || (branch == Branch::automatic && r <= p.ri))
    return {T(0), T(1)};
  if (branch == Branch::exterior || (branch == Branch::automatic && r >= p.ro))
    return {T(1), T(0)};
  T x = (T(2) * r - p.ri - p.ro) / (p.ro - p.ri);
  T mu = T(1) / T(2) + T(35) / T(32) * x - T(35) / T(32) * power(x, 3) +
         T(21) / T(32) * power(x, 5) - T(5) / T(32) * power(x, 7);
  return {mu, T(1) - mu};
}
// Independently expanded metric; no production header or Taylor jets.
template <class T>
Matrix<T> metric(T r, T theta, const Parameters<T> &p, Family f = Family::toy,
                 Branch branch = Branch::automatic) {
  auto [mu, lam] = transition(r, p, f, branch);
  T m = p.m * mu, ell = p.L * lam / T(3);
  auto [s, c] = sincos(theta);
  T s2 = s * s, c2 = c * c, a2 = p.a * p.a, r2 = r * r;
  T rho = r2 + a2 * c2, b = r2 + a2, xi = T(1) + ell * a2,
    delta = b * (T(1) - ell * r2) - T(2) * m * r;
  Matrix<T> g{};
  g[0][0] = (T(-1) + ell * (r2 + a2 * s2) + T(2) * m * r / rho) / (xi * xi);
  g[0][3] = g[3][0] = -p.a * s2 * (ell * b + T(2) * m * r / rho) / (xi * xi);
  g[3][3] = s2 * (b * xi + T(2) * m * r * a2 * s2 / rho) / (xi * xi);
  g[1][1] = rho / delta;
  g[2][2] = rho / (T(1) + ell * a2 * c2);
  return g;
}
template <class T> Matrix<T> inverse(const Matrix<T> &g) {
  std::array<std::array<T, 8>, 4> rows{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      rows[i][j] = g[i][j];
      rows[i][j + 4] = T(i == j ? 1 : 0);
    }
  for (int k = 0; k < 4; ++k) {
    int pivot = k;
    for (int i = k + 1; i < 4; ++i)
      if (abs(rows[i][k]) > abs(rows[pivot][k]))
        pivot = i;
    std::swap(rows[k], rows[pivot]);
    T scale = rows[k][k];
    if (scale == T(0))
      throw std::domain_error("singular reference matrix");
    for (auto &v : rows[k])
      v = v / scale;
    for (int i = 0; i < 4; ++i)
      if (i != k) {
        T factor = rows[i][k];
        for (int j = 0; j < 8; ++j)
          rows[i][j] -= factor * rows[k][j];
      }
  }
  Matrix<T> out{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      out[i][j] = rows[i][j + 4];
  return out;
}
template <class T> struct Derivatives {
  Matrix<T> g{};
  Tensor3<T> dg{};
  Tensor4<T> ddg{};
};
template <class T>
Derivatives<T> derivatives(T r, T th, const Parameters<T> &p,
                           Family f = Family::toy, T h = T("1e-9"),
                           Branch branch = Branch::automatic) {
  const std::array<T, 5> w1{T(1) / T(12), T(-2) / T(3), T(0), T(2) / T(3),
                            T(-1) / T(12)};
  const std::array<T, 5> w2{T(-1) / T(12), T(4) / T(3), T(-5) / T(2),
                            T(4) / T(3), T(-1) / T(12)};
  Derivatives<T> out;
  out.g = metric(r, th, p, f, branch);
  for (int axis = 1; axis <= 2; ++axis) {
    for (int n = -2; n <= 2; ++n) {
      auto g = metric(r + (axis == 1 ? T(n) * h : T(0)),
                      th + (axis == 2 ? T(n) * h : T(0)), p, f, branch);
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
          out.dg[axis][i][j] += w1[n + 2] * g[i][j];
          out.ddg[axis][axis][i][j] += w2[n + 2] * g[i][j];
        }
    }
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) {
        out.dg[axis][i][j] = out.dg[axis][i][j] / h;
        out.ddg[axis][axis][i][j] = out.ddg[axis][axis][i][j] / (h * h);
      }
  }
  for (int n = -2; n <= 2; ++n)
    for (int m = -2; m <= 2; ++m)
      if (n && m) {
        auto g = metric(r + T(n) * h, th + T(m) * h, p, f, branch);
        for (int i = 0; i < 4; ++i)
          for (int j = 0; j < 4; ++j)
            out.ddg[1][2][i][j] += w1[n + 2] * w1[m + 2] * g[i][j];
      }
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      out.ddg[2][1][i][j] = out.ddg[1][2][i][j] = out.ddg[1][2][i][j] / (h * h);
  return out;
}
template <class T>
Matrix<T> radial_derivative(T r, T th, const Parameters<T> &p, int order,
                            Family f = Family::toy,
                            Branch branch = Branch::automatic,
                            T h = T("1e-7")) {
  if (order == 0)
    return metric(r, th, p, f, branch);
  if (order < 0 || order > 3)
    throw std::invalid_argument("reference radial derivative order");
  // Nine-node Vandermonde weights derived independently at working precision.
  std::array<std::array<T, 10>, 9> a{};
  for (int k = 0; k < 9; ++k) {
    for (int n = 0; n < 9; ++n)
      a[k][n] = power(T(n - 4), k);
    a[k][9] = T(k == order ? (order == 1 ? 1 : order == 2 ? 2 : 6) : 0);
  }
  for (int k = 0; k < 9; ++k) {
    int pivot = k;
    while (pivot < 9 && a[pivot][k] == T(0))
      ++pivot;
    if (pivot == 9)
      throw std::runtime_error("singular finite difference stencil");
    std::swap(a[k], a[pivot]);
    T div = a[k][k];
    for (auto &v : a[k])
      v = v / div;
    for (int i = 0; i < 9; ++i)
      if (i != k) {
        T v = a[i][k];
        for (int j = 0; j < 10; ++j)
          a[i][j] -= v * a[k][j];
      }
  }
  Matrix<T> out{};
  for (int n = 0; n < 9; ++n) {
    auto g = metric(r + T(n - 4) * h, th, p, f, branch);
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        out[i][j] += a[n][9] * g[i][j];
  }
  for (auto &row : out)
    for (auto &v : row)
      v = v / power(h, order);
  return out;
}
template <class T> struct Curvature {
  Matrix<T> ricci{};
  T scalar{}, kretschmann{};
  Tensor4<T> lower{};
};
template <class T> Curvature<T> curvature(const Derivatives<T> &in) {
  auto inv = inverse(in.g);
  Tensor3<T> gamma{};
  Curvature<T> out;
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int e = 0; e < 4; ++e)
          if (inv[a][e] != T(0))
            gamma[a][b][c] +=
                inv[a][e] * (in.dg[b][e][c] + in.dg[c][e][b] - in.dg[e][b][c]) /
                T(2);
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int d = 0; d < 4; ++d) {
          T v = (in.ddg[b][c][a][d] + in.ddg[a][d][b][c] - in.ddg[b][d][a][c] -
                 in.ddg[a][c][b][d]) /
                T(2);
          for (int e = 0; e < 4; ++e)
            for (int f = 0; f < 4; ++f)
              if (in.g[e][f] != T(0))
                v += in.g[e][f] * (gamma[e][b][c] * gamma[f][a][d] -
                                   gamma[e][b][d] * gamma[f][a][c]);
          out.lower[a][b][c][d] = v;
          out.ricci[b][d] += inv[a][c] * v;
        }
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      out.scalar += inv[a][b] * out.ricci[a][b];
  auto raised = out.lower;
  for (int axis = 0; axis < 4; ++axis) {
    Tensor4<T> next{};
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        for (int c = 0; c < 4; ++c)
          for (int d = 0; d < 4; ++d) {
            std::array<int, 4> idx{a, b, c, d};
            int row = idx[axis];
            for (int j = 0; j < 4; ++j)
              if (inv[row][j] != T(0)) {
                idx[axis] = j;
                next[a][b][c][d] +=
                    inv[row][j] * raised[idx[0]][idx[1]][idx[2]][idx[3]];
              }
          }
    raised = std::move(next);
  }
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int d = 0; d < 4; ++d)
          out.kretschmann += out.lower[a][b][c][d] * raised[a][b][c][d];
  return out;
}
} // namespace gravastar::reference
