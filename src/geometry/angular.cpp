// SPDX-License-Identifier: MIT
#include "gravastar/geometry/angular.hpp"
#include "curvature.hpp"
#include "gravastar/math/spatial_jet.hpp"
#include "profile.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace gravastar::geometry {
namespace {
template <class A> bool finite(const A &a) {
  for (const auto &v : a) {
    if constexpr (std::is_same_v<std::decay_t<decltype(v)>, double>) {
      if (!std::isfinite(v))
        return false;
    } else if (!finite(v))
      return false;
  }
  return true;
}
template <int Order>
using Output = std::conditional_t<Order == 1, AngularMetric, AngularCurvature>;
template <int Order>
std::variant<Output<Order>, Error> evaluate_impl(const Parameters &p,
                                                 double radius, double xx,
                                                 double yy, Region region) {
  if (family_name(p.family) == "invalid")
    return Error{"invalid_parameter", "unknown geometry family"};
  for (double v : {p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out})
    if (!std::isfinite(v))
      return Error{"invalid_parameter", "all parameters must be finite"};
  if (p.mass < 0 || p.cosmological_constant < 0 || p.r_in <= 0 ||
      p.r_out <= p.r_in)
    return Error{"invalid_parameter",
                 "require M >= 0, Lambda >= 0, 0 < r_in < r_out"};
  if (!std::isfinite(radius) || radius <= 0 || !std::isfinite(xx) ||
      !std::isfinite(yy) || xx * xx + yy * yy > 4)
    return Error{"coordinate_domain", "require finite r > 0 and x*x+y*y <= 4"};
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

  using S = math::SpatialJet<Order>;
  const S r = S::variable(radius, 0), x = S::variable(xx, 1),
          y = S::variable(yy, 2);
  const auto [mu, lambda] = detail::transition(p, r, region);
  const S q = x * x + y * y, C = S(4) / ((S(1) + q) * (S(1) + q)), s2 = C * q,
          c2 = (S(1) - q) * (S(1) - q) / ((S(1) + q) * (S(1) + q)),
          k = S(p.cosmological_constant / 3) * lambda, m = S(p.mass) * mu,
          a2(p.spin * p.spin), A = r * r + a2, sigma = r * r + a2 * c2,
          X = S(1) + k * a2, T = S(1) + k * a2 * c2, F = S(1) - k * r * r,
          delta = A * F - S(2) * m * r;
  const double scale =
      std::max({1.0, std::abs(A.value()), std::abs((A * k * r * r).value()),
                std::abs((S(2) * m * r).value())});
  for (double v : {sigma.value(), delta.value(), T.value(), X.value(), scale})
    if (!std::isfinite(v))
      return Error{"numerical_failure", "nonfinite intermediate geometry"};
  if (sigma.value() <= 0 || T.value() <= 0 || X.value() <= 0 ||
      std::abs(delta.value()) <=
          64 * std::numeric_limits<double>::epsilon() * scale)
    return Error{"chart_singularity",
                 "metric too close to Delta=0 chart surface"};

  // Rank-one angular block. No division by q, even at either pole.
  const S B = F / (T * X) + S(2) * m * r / (sigma * X * X), H = C * sigma / T,
          K = C * C * a2 * B,
          D = C * S(p.spin) * (delta - T * A) / (sigma * X * X),
          alpha = (T * a2 * s2 - delta) / (sigma * X * X);
  const std::array<S, 2> v{-y, x};
  std::array<std::array<S, 4>, 4> metric{};
  metric[0][0] = alpha;
  metric[1][1] = sigma / delta;
  for (int i = 0; i < 2; ++i) {
    metric[0][i + 2] = metric[i + 2][0] = D * v[i];
    for (int j = 0; j < 2; ++j)
      metric[i + 2][j + 2] = (i == j ? H : S(0)) + K * v[i] * v[j];
  }
  Output<Order> out;
  out.mu = mu.value();
  out.lambda = lambda.value();
  out.sigma = sigma.value();
  out.delta = delta.value();
  out.delta_theta = T.value();
  out.xi = X.value();
  const double xi2 = out.xi * out.xi, xi4 = xi2 * xi2;
  out.determinant = -out.sigma * out.sigma * C.value() * C.value() / xi4;
  // Simplified Schur complement avoids subtracting near-equal terms.
  const double z = -C.value() * out.delta * out.delta_theta / xi4;
  out.inverse[0][0] = (H.value() + K.value() * q.value()) / z;
  out.inverse[1][1] = out.delta / out.sigma;
  for (int i = 0; i < 2; ++i) {
    out.inverse[0][i + 2] = out.inverse[i + 2][0] =
        -D.value() * v[i].value() / z;
    for (int j = 0; j < 2; ++j)
      out.inverse[i + 2][j + 2] =
          (i == j ? 1 / H.value() : 0) +
          (D.value() * D.value() - alpha.value() * K.value()) * v[i].value() *
              v[j].value() / (H.value() * z);
  }
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      out.metric[i][j] = metric[i][j].value();
      for (int a = 0; a < 3; ++a) {
        out.dg[a + 1][i][j] = metric[i][j].d[a];
        if constexpr (Order == 2)
          for (int b = 0; b < 3; ++b)
            out.ddg[a + 1][b + 1][i][j] = metric[i][j].h[a][b];
      }
    }
  if (!finite(out.metric) || !finite(out.inverse) || !finite(out.dg) ||
      !std::isfinite(out.determinant) || out.determinant == 0)
    return Error{"numerical_failure",
                 "nonfinite or underflowed angular metric/derivatives"};
  if constexpr (Order == 2) {
    if (!finite(out.ddg))
      return Error{"numerical_failure", "nonfinite angular Hessian"};
    const auto curvature =
        detail::curvature(out.metric, out.inverse, out.dg, out.ddg);
    out.ricci = curvature.ricci;
    out.ricci_scalar = curvature.ricci_scalar;
    out.kretschmann = curvature.kretschmann;
    if (!finite(out.ricci) || !std::isfinite(out.ricci_scalar) ||
        !std::isfinite(out.kretschmann))
      return Error{"numerical_failure", "nonfinite angular curvature"};
  }
  return out;
}
} // namespace
AngularResult evaluate_angular(const Parameters &p, double r, double x,
                               double y, Region region) {
  return evaluate_impl<1>(p, r, x, y, region);
}
AngularCurvatureResult evaluate_angular_curvature(const Parameters &p, double r,
                                                  double x, double y,
                                                  Region region) {
  return evaluate_impl<2>(p, r, x, y, region);
}
} // namespace gravastar::geometry
