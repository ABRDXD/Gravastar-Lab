// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/math/jet.hpp"
#include "profile.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>
#include <utility>

namespace gravastar::geometry {
namespace {
using math::Jet;
using JetMatrix = std::array<std::array<Jet, 4>, 4>;

template <typename Array> bool finite_array(const Array &array) {
  for (const auto &entry : array) {
    if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, double>) {
      if (!std::isfinite(entry))
        return false;
    } else {
      if (!finite_array(entry))
        return false;
    }
  }
  return true;
}

void curvature(Evaluation &result) {
  const auto &g = result.metric;
  const auto &inv = result.inverse;
  const auto &dg = result.dg;
  const auto &ddg = result.ddg;
  Tensor3 dinv{};
  Tensor3 gamma{};
  Tensor4 dgamma{};
  Tensor4 riemann{};
  for (int k = 0; k < 4; ++k)
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        for (int i = 0; i < 4; ++i)
          for (int j = 0; j < 4; ++j)
            dinv[k][a][b] -= inv[a][i] * dg[k][i][j] * inv[j][b];
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int e = 0; e < 4; ++e) {
          const double first = dg[b][e][c] + dg[c][e][b] - dg[e][b][c];
          gamma[a][b][c] += 0.5 * inv[a][e] * first;
          for (int k = 0; k < 4; ++k) {
            const double second =
                ddg[k][b][e][c] + ddg[k][c][e][b] - ddg[k][e][b][c];
            dgamma[k][a][b][c] +=
                0.5 * (dinv[k][a][e] * first + inv[a][e] * second);
          }
        }
  // R^a_bcd = d_c Gamma^a_db - d_d Gamma^a_cb
  //         + Gamma^a_ce Gamma^e_db - Gamma^a_de Gamma^e_cb.
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int d = 0; d < 4; ++d) {
          double value = dgamma[c][a][d][b] - dgamma[d][a][c][b];
          for (int e = 0; e < 4; ++e)
            value += gamma[a][c][e] * gamma[e][d][b] -
                     gamma[a][d][e] * gamma[e][c][b];
          riemann[a][b][c][d] = value;
          if (a == c)
            result.ricci[b][d] += value;
        }
  for (int b = 0; b < 4; ++b)
    for (int d = 0; d < 4; ++d)
      result.ricci_scalar += inv[b][d] * result.ricci[b][d];

  Tensor4 lower{};
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int d = 0; d < 4; ++d)
          for (int e = 0; e < 4; ++e)
            lower[a][b][c][d] += g[a][e] * riemann[e][b][c][d];
  Tensor4 raised = lower;
  for (int axis = 0; axis < 4; ++axis) {
    Tensor4 next{};
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        for (int c = 0; c < 4; ++c)
          for (int d = 0; d < 4; ++d) {
            std::array<int, 4> index{a, b, c, d};
            const int row = index[axis];
            for (int e = 0; e < 4; ++e) {
              index[axis] = e;
              next[a][b][c][d] +=
                  inv[row][e] * raised[index[0]][index[1]][index[2]][index[3]];
            }
          }
    raised = next;
  }
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int d = 0; d < 4; ++d)
          result.kretschmann += lower[a][b][c][d] * raised[a][b][c][d];
}
} // namespace

std::string_view family_name(Family family) {
  switch (family) {
  case Family::toy:
    return "toy";
  case Family::kerr:
    return "kerr";
  case Family::de_sitter:
    return "de_sitter";
  case Family::flat:
    return "flat";
  }
  return "invalid";
}

std::variant<Family, std::string> parse_family(std::string_view text) {
  if (text == "toy")
    return Family::toy;
  if (text == "kerr")
    return Family::kerr;
  if (text == "de_sitter")
    return Family::de_sitter;
  if (text == "flat")
    return Family::flat;
  return std::string("family must be toy, kerr, de_sitter, or flat");
}

Result evaluate(const Parameters &p, double radius, double polar_angle,
                Region region) {
  if (family_name(p.family) == "invalid")
    return Error{"invalid_parameter", "unknown geometry family"};
  for (double value :
       {p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out})
    if (!std::isfinite(value))
      return Error{"invalid_parameter", "all parameters must be finite"};
  if (p.mass < 0 || p.cosmological_constant < 0 || p.r_in <= 0 ||
      p.r_out <= p.r_in)
    return Error{"invalid_parameter",
                 "require M >= 0, Lambda >= 0, 0 < r_in < r_out"};
  if (!std::isfinite(radius) || !std::isfinite(polar_angle) || radius <= 0 ||
      polar_angle <= 0 || polar_angle >= std::numbers::pi)
    return Error{"coordinate_domain",
                 "require finite r > 0 and 0 < theta < pi"};
  if (region != Region::automatic && region != Region::core &&
      region != Region::shell && region != Region::exterior)
    return Error{"invalid_parameter", "unknown region"};
  if (region != Region::automatic && p.family != Family::toy)
    return Error{"region_mismatch",
                 "explicit regions apply only to the toy family"};
  if ((region == Region::core && radius > p.r_in) ||
      (region == Region::exterior && radius < p.r_out) ||
      (region == Region::shell && (radius < p.r_in || radius > p.r_out)))
    return Error{"region_mismatch",
                 "explicit region outside its physical interval"};

  const Jet r = Jet::variable(radius, 0);
  const Jet theta = Jet::variable(polar_angle, 1);
  const auto [mu, lambda] = detail::transition(p, r, region);
  const Jet effective_lambda = Jet(p.cosmological_constant) * lambda;
  const Jet r2 = r * r;
  const Jet a2(p.spin * p.spin);
  const Jet s = math::sin(theta), c = math::cos(theta);
  const Jet s2 = s * s, c2 = c * c;
  const Jet A = r2 + a2;
  const Jet sigma = r2 + a2 * c2;
  const Jet delta =
      A * (Jet(1) - effective_lambda * r2 / Jet(3)) - Jet(2 * p.mass) * mu * r;
  const Jet T = Jet(1) + effective_lambda * a2 * c2 / Jet(3);
  const Jet X = Jet(1) + effective_lambda * a2 / Jet(3);
  const double delta_scale =
      std::max({1.0, std::abs(A.value()),
                std::abs((A * effective_lambda * r2 / Jet(3)).value()),
                std::abs((Jet(2 * p.mass) * mu * r).value())});
  constexpr double margin = 64 * std::numeric_limits<double>::epsilon();
  for (double value :
       {sigma.value(), delta.value(), T.value(), X.value(), delta_scale})
    if (!std::isfinite(value))
      return Error{"numerical_failure", "nonfinite intermediate geometry"};
  if (sigma.value() <= 0 || T.value() <= 0 || X.value() <= 0 ||
      std::abs(delta.value()) <= margin * delta_scale || s2.value() <= margin)
    return Error{"chart_singularity",
                 "metric too close to an axis or Delta=0 chart surface"};
  const Jet denominator = sigma * X * X;
  JetMatrix g{};
  g[0][0] = (T * a2 * s2 - delta) / denominator;
  g[0][3] = g[3][0] = Jet(p.spin) * s2 * (delta - T * A) / denominator;
  g[3][3] = s2 * (T * A * A - delta * a2 * s2) / denominator;
  g[1][1] = sigma / delta;
  g[2][2] = sigma / T;

  Evaluation out;
  out.mu = mu.value();
  out.lambda = lambda.value();
  out.sigma = sigma.value();
  out.delta = delta.value();
  out.delta_theta = T.value();
  out.xi = X.value();
  out.determinant =
      -std::pow(sigma.value(), 2) * s2.value() / std::pow(X.value(), 4);
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      out.metric[i][j] = g[i][j].value();
      out.dg[1][i][j] = g[i][j].derivative(1, 0);
      out.dg[2][i][j] = g[i][j].derivative(0, 1);
      out.ddg[1][1][i][j] = g[i][j].derivative(2, 0);
      out.ddg[1][2][i][j] = out.ddg[2][1][i][j] = g[i][j].derivative(1, 1);
      out.ddg[2][2][i][j] = g[i][j].derivative(0, 2);
      out.drrr[i][j] = g[i][j].derivative(3, 0);
    }
  const double prefactor = X.value() * X.value() / sigma.value();
  out.inverse[0][0] = -prefactor * (A.value() * A.value() / delta.value() -
                                    a2.value() * s2.value() / T.value());
  out.inverse[0][3] = out.inverse[3][0] =
      -p.spin * prefactor * (A.value() / delta.value() - 1 / T.value());
  out.inverse[3][3] =
      prefactor * (1 / (T.value() * s2.value()) - a2.value() / delta.value());
  out.inverse[1][1] = delta.value() / sigma.value();
  out.inverse[2][2] = T.value() / sigma.value();
  if (!finite_array(out.metric) || !finite_array(out.inverse) ||
      !finite_array(out.dg) || !finite_array(out.ddg) ||
      !finite_array(out.drrr) || !std::isfinite(out.determinant) ||
      out.determinant == 0)
    return Error{"numerical_failure",
                 "nonfinite or underflowed metric/derivatives"};
  curvature(out);
  if (!finite_array(out.ricci) || !std::isfinite(out.ricci_scalar) ||
      !std::isfinite(out.kretschmann))
    return Error{"numerical_failure", "nonfinite curvature"};
  return out;
}
} // namespace gravastar::geometry
