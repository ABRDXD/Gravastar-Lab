// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/chart.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace gravastar::optics {
namespace {
void validate_chart(const State &s, Chart chart) {
  if (!std::all_of(s.begin(), s.end(),
                   [](double v) { return std::isfinite(v); }))
    throw std::invalid_argument("nonfinite chart state");
  if (chart == Chart::boyer_lindquist) {
    if (!(s[2] > 0 && s[2] < std::numbers::pi))
      throw std::invalid_argument("BL transformation excludes the poles");
  } else if (chart == Chart::north || chart == Chart::south) {
    if (!(s[2] * s[2] + s[3] * s[3] <= 4))
      throw std::invalid_argument("angular transformation requires q<=4");
  } else
    throw std::invalid_argument("unknown coordinate chart");
}
struct Transition {
  State state;
  double a{1}, b{}, c{}, d{1}; // angular contravariant Jacobian
};
Transition transition(const State &s, Chart from, Chart to) {
  validate_chart(s, from);
  Transition out{s};
  if (from == to)
    return out;
  if (from == Chart::boyer_lindquist) {
    const double sign = to == Chart::north ? 1 : -1;
    const double angle = to == Chart::north ? s[2] : std::numbers::pi - s[2];
    const double rho = std::tan(angle / 2), dr = sign * (1 + rho * rho) / 2;
    const double co = std::cos(s[3]), si = std::sin(s[3]);
    out.state[2] = rho * co;
    out.state[3] = rho * si;
    out.a = dr * co;
    out.b = -rho * si;
    out.c = dr * si;
    out.d = rho * co;
  } else {
    const double x = s[2], y = s[3], q = x * x + y * y;
    if (!(q > 0))
      throw std::invalid_argument(
          "cannot transform the pole to a singular chart");
    if (to == Chart::boyer_lindquist) {
      const double rho = std::sqrt(q), sign = from == Chart::north ? 1 : -1;
      out.state[2] = from == Chart::north
                         ? 2 * std::atan(rho)
                         : std::numbers::pi - 2 * std::atan(rho);
      out.state[3] = std::atan2(y, x);
      out.a = sign * 2 * x / (rho * (1 + q));
      out.b = sign * 2 * y / (rho * (1 + q));
      out.c = -y / q;
      out.d = x / q;
    } else {
      out.state[2] = x / q;
      out.state[3] = y / q;
      out.a = (1 - 2 * x * x / q) / q;
      out.b = out.c = -2 * x * y / (q * q);
      out.d = (1 - 2 * y * y / q) / q;
    }
  }
  validate_chart(out.state, to);
  return out;
}
} // namespace
State transform_state(const State &s, Chart from, Chart to) {
  auto t = transition(s, from, to);
  const double determinant = t.a * t.d - t.b * t.c;
  t.state[6] = (t.d * s[6] - t.c * s[7]) / determinant;
  t.state[7] = (-t.b * s[6] + t.a * s[7]) / determinant;
  validate_chart(t.state, to);
  return t.state;
}
Vector transform_vector(const Vector &v, const State &s, Chart from, Chart to) {
  const auto t = transition(s, from, to);
  Vector out{v[0], v[1], t.a * v[2] + t.b * v[3], t.c * v[2] + t.d * v[3]};
  for (double x : out)
    if (!std::isfinite(x))
      throw std::invalid_argument("nonfinite transformed vector");
  return out;
}
Frame transform_frame(const Frame &f, const State &s, Chart from, Chart to) {
  Frame out = f;
  for (auto &v : out.basis)
    v = transform_vector(v, s, from, to);
  return out;
}
double axial_momentum(const State &s, Chart chart) {
  if (chart == Chart::core_cartesian)
    return s[1] * s[6] - s[2] * s[5];
  return chart == Chart::boyer_lindquist ? s[7] : -s[3] * s[6] + s[2] * s[7];
}
geometry::AngularMetric angular_metric(const Parameters &p, const State &s) {
  auto result = geometry::evaluate_angular(p, s[1], s[2], s[3]);
  if (auto *e = std::get_if<geometry::Error>(&result))
    throw std::runtime_error(e->code + ": " + e->message);
  auto g = std::get<geometry::AngularMetric>(result);
  if (!(g.metric[1][1] > 0 && g.inverse[0][0] < 0))
    throw std::runtime_error(
        "optics requires the exterior-type Lorentzian chart");
  return g;
}
State angular_rhs(const Parameters &p, const State &s) {
  validate_chart(s, Chart::north); // Metric is identical in the two patches.
  const auto g = angular_metric(p, s);
  Vector v{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      v[i] += g.inverse[i][j] * s[j + 4];
  State out{};
  for (int i = 0; i < 4; ++i)
    out[i] = -v[i];
  for (int k = 1; k < 4; ++k)
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        out[k + 4] -= .5 * g.dg[k][i][j] * v[i] * v[j];
  for (double x : out)
    if (!std::isfinite(x))
      throw std::runtime_error("nonfinite angular Hamiltonian derivative");
  return out;
}
double angular_null_residual(const geometry::AngularMetric &g, const State &s) {
  double sum = 0, scale = 0;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      const double term = g.inverse[i][j] * s[i + 4] * s[j + 4];
      sum += term;
      scale += std::abs(term);
    }
  return std::isfinite(scale) && scale > 0
             ? std::abs(sum) / scale
             : std::numeric_limits<double>::infinity();
}
double angular_normal_frequency(const geometry::AngularMetric &g,
                                const State &s) {
  double nu = 0;
  for (int i = 0; i < 4; ++i)
    nu += g.inverse[0][i] * s[i + 4] / std::sqrt(-g.inverse[0][0]);
  if (!std::isfinite(nu))
    throw std::invalid_argument("nonfinite photon frequency");
  return nu;
}
} // namespace gravastar::optics
