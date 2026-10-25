// SPDX-License-Identifier: MIT
#include "gravastar/optics/core_chart.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gravastar::optics {
namespace {
void finite(const State &s) {
  for (double x : s)
    if (!std::isfinite(x))
      throw std::invalid_argument("nonfinite core state");
}
double curvature_scale(const Parameters &p) {
  if (p.family != geometry::Family::toy &&
      p.family != geometry::Family::de_sitter &&
      p.family != geometry::Family::flat)
    throw std::invalid_argument(
        "Cartesian core requires an exact constant-curvature region");
  return p.family == geometry::Family::flat ? 0 : p.cosmological_constant / 3;
}
void angular_patch(Chart c) {
  if (c != Chart::north && c != Chart::south)
    throw std::invalid_argument("core transition requires an angular patch");
}
struct Map {
  State position;
  geometry::Matrix jacobian;
};
Map forward(const Parameters &p, const State &s, Chart chart) {
  finite(s);
  angular_patch(chart);
  // Validate model/domain through the existing canonical angular evaluator.
  (void)angular_metric(p, s);
  if (p.family == geometry::Family::toy && s[1] > p.r_in)
    throw std::invalid_argument(
        "cannot transform shell/exterior to exact core");
  const double k = curvature_scale(p), xi = 1 + k * p.spin * p.spin;
  const double r = s[1], x = s[2], y = s[3], q = x * x + y * y, den = 1 + q;
  const double sign = chart == Chart::north ? 1 : -1;
  const double nx = 2 * x / den, ny = 2 * y / den, nz = sign * (1 - q) / den;
  const double h = std::sqrt((r * r + p.spin * p.spin) / xi);
  const double time = s[0] / xi, phase = k * p.spin * time;
  const double co = std::cos(phase), si = std::sin(phase);
  Map m{};
  m.position = {
      time, h * (co * nx + si * ny), h * (-si * nx + co * ny), r * nz, 0, 0, 0,
      0};
  auto &j = m.jacobian;
  j[0][0] = 1 / xi;
  const double omega = k * p.spin / xi;
  j[1][0] = omega * m.position[2];
  j[2][0] = -omega * m.position[1];
  j[1][1] = r * m.position[1] / (r * r + p.spin * p.spin);
  j[2][1] = r * m.position[2] / (r * r + p.spin * p.spin);
  j[3][1] = nz;
  const double dnx[2]{2 / den - 4 * x * x / (den * den),
                      -4 * x * y / (den * den)};
  const double dny[2]{-4 * x * y / (den * den),
                      2 / den - 4 * y * y / (den * den)};
  const double dnz[2]{-4 * sign * x / (den * den), -4 * sign * y / (den * den)};
  for (int a = 0; a < 2; ++a) {
    j[1][a + 2] = h * (co * dnx[a] + si * dny[a]);
    j[2][a + 2] = h * (-si * dnx[a] + co * dny[a]);
    j[3][a + 2] = r * dnz[a];
  }
  (void)core_metric(p, m.position);
  return m;
}
geometry::Matrix inverse(geometry::Matrix a) {
  geometry::Matrix b{};
  for (int i = 0; i < 4; ++i)
    b[i][i] = 1;
  for (int col = 0; col < 4; ++col) {
    int pivot = col;
    for (int row = col + 1; row < 4; ++row)
      if (std::abs(a[row][col]) > std::abs(a[pivot][col]))
        pivot = row;
    if (!std::isfinite(a[pivot][col]) || a[pivot][col] == 0)
      throw std::invalid_argument("singular core coordinate transition");
    std::swap(a[pivot], a[col]);
    std::swap(b[pivot], b[col]);
    const double d = a[col][col];
    for (int j = 0; j < 4; ++j) {
      a[col][j] /= d;
      b[col][j] /= d;
    }
    for (int row = 0; row < 4; ++row)
      if (row != col) {
        const double factor = a[row][col];
        for (int j = 0; j < 4; ++j) {
          a[row][j] -= factor * a[col][j];
          b[row][j] -= factor * b[col][j];
        }
      }
  }
  for (const auto &row : b)
    for (double value : row)
      if (!std::isfinite(value))
        throw std::invalid_argument("nonfinite inverse core Jacobian");
  return b;
}
Vector multiply(const geometry::Matrix &j, const Vector &v) {
  Vector out{};
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      out[a] += j[a][b] * v[b];
  for (double value : out)
    if (!std::isfinite(value))
      throw std::invalid_argument("nonfinite transformed core vector");
  return out;
}
State angular_position(const Parameters &p, const State &s, Chart chart) {
  finite(s);
  angular_patch(chart);
  (void)core_metric(p, s);
  const double r = core_oblate_radius(p, s);
  if (!(r > 0))
    throw std::invalid_argument(
        "focal disk has no regular inverse oblate coordinates");
  const double k = curvature_scale(p), xi = 1 + k * p.spin * p.spin;
  const double h = std::sqrt((r * r + p.spin * p.spin) / xi);
  const double phase = k * p.spin * s[0], co = std::cos(phase),
               si = std::sin(phase);
  const double sign = chart == Chart::north ? 1 : -1;
  const double den = 1 + sign * s[3] / r;
  State angular{xi * s[0],
                r,
                (co * s[1] - si * s[2]) / (h * den),
                (si * s[1] + co * s[2]) / (h * den),
                0,
                0,
                0,
                0};
  finite(angular);
  (void)angular_metric(p, angular);
  return angular;
}
} // namespace
geometry::CoreMetric core_metric(const Parameters &p, const State &s) {
  auto result = geometry::evaluate_core(p, s[1], s[2], s[3]);
  if (auto *e = std::get_if<geometry::Error>(&result))
    throw std::runtime_error(e->code + ": " + e->message);
  return std::get<geometry::CoreMetric>(result);
}
double core_oblate_radius(const Parameters &p, const State &s) {
  finite(s);
  const double k = curvature_scale(p), a2 = p.spin * p.spin, xi = 1 + k * a2;
  const double b = xi * (s[1] * s[1] + s[2] * s[2]) + s[3] * s[3] - a2;
  const double d = std::hypot(b, 2 * p.spin * s[3]);
  // Rationalized negative-b branch avoids subtracting equal magnitudes near
  // the focal disk. Its interior has r=0, while the Cartesian metric is
  // regular.
  const double r2 = b >= 0 ? (b + d) / 2 : 2 * a2 * s[3] * s[3] / (d - b);
  if (!std::isfinite(r2) || r2 < 0)
    throw std::invalid_argument("nonfinite oblate radius");
  return std::sqrt(r2);
}
State to_core(const Parameters &p, const State &s, Chart chart) {
  auto m = forward(p, s, chart);
  const auto inv = inverse(m.jacobian);
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      m.position[a + 4] += inv[b][a] * s[b + 4];
  finite(m.position);
  return m.position;
}
State from_core(const Parameters &p, const State &s, Chart chart) {
  auto out = angular_position(p, s, chart);
  const auto m = forward(p, out, chart);
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      out[a + 4] += m.jacobian[b][a] * s[b + 4];
  finite(out);
  return out;
}
Vector vector_to_core(const Parameters &p, const Vector &v, const State &s,
                      Chart c) {
  return multiply(forward(p, s, c).jacobian, v);
}
Vector vector_from_core(const Parameters &p, const Vector &v, const State &s,
                        Chart c) {
  return multiply(inverse(forward(p, angular_position(p, s, c), c).jacobian),
                  v);
}
Frame frame_to_core(const Parameters &p, const Frame &f, const State &s,
                    Chart c) {
  Frame out = f;
  for (auto &v : out.basis)
    v = vector_to_core(p, v, s, c);
  return out;
}
Frame frame_from_core(const Parameters &p, const Frame &f, const State &s,
                      Chart c) {
  Frame out = f;
  for (auto &v : out.basis)
    v = vector_from_core(p, v, s, c);
  return out;
}
double stationary_momentum(const Parameters &p, const State &s, Chart chart) {
  if (chart != Chart::core_cartesian)
    return s[4];
  const double k = curvature_scale(p), xi = 1 + k * p.spin * p.spin;
  const double lz = s[1] * s[6] - s[2] * s[5];
  return (s[4] - k * p.spin * lz) / xi;
}
State core_rhs(const Parameters &p, const State &s) {
  finite(s);
  const auto g = core_metric(p, s);
  Vector v{};
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      v[a] += g.inverse[a][b] * s[b + 4];
  State out{};
  for (int a = 0; a < 4; ++a)
    out[a] = -v[a];
  for (int k = 1; k < 4; ++k)
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        out[k + 4] -= .5 * g.dg[k][a][b] * v[a] * v[b];
  finite(out);
  return out;
}
double core_null_residual(const geometry::CoreMetric &g, const State &s) {
  double sum = 0, scale = 0;
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b) {
      const double value = g.inverse[a][b] * s[a + 4] * s[b + 4];
      sum += value;
      scale += std::abs(value);
    }
  return std::isfinite(scale) && scale > 0
             ? std::abs(sum) / scale
             : std::numeric_limits<double>::infinity();
}
double core_normal_frequency(const geometry::CoreMetric &g, const State &s) {
  return -s[4] / std::sqrt(-g.metric[0][0]);
}
} // namespace gravastar::optics
