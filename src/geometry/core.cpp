// SPDX-License-Identifier: MIT
#include "gravastar/geometry/core.hpp"
#include "curvature.hpp"
#include "gravastar/math/spatial_jet.hpp"
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
using Output = std::conditional_t<Order == 1, CoreMetric, CoreCurvature>;
template <int Order>
std::variant<Output<Order>, Error> evaluate_impl(const Parameters &p, double xx,
                                                 double yy, double zz) {
  if (p.family != Family::toy && p.family != Family::de_sitter &&
      p.family != Family::flat)
    return Error{"region_mismatch",
                 "static Cartesian core requires toy, de_sitter or flat"};
  for (double v : {p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out})
    if (!std::isfinite(v))
      return Error{"invalid_parameter", "all parameters must be finite"};
  if (p.mass < 0 || p.cosmological_constant < 0 || p.r_in <= 0 ||
      p.r_out <= p.r_in)
    return Error{"invalid_parameter",
                 "require M >= 0, Lambda >= 0, 0 < r_in < r_out"};
  if (!std::isfinite(xx) || !std::isfinite(yy) || !std::isfinite(zz))
    return Error{"coordinate_domain", "Cartesian coordinates must be finite"};

  const double k = p.family == Family::flat ? 0 : p.cosmological_constant / 3;
  const double rho2 = xx * xx + yy * yy + zz * zz, lapse = 1 - k * rho2;
  if (!std::isfinite(rho2) || !std::isfinite(lapse))
    return Error{"numerical_failure", "nonfinite Cartesian core geometry"};
  if (lapse <= 64 * std::numeric_limits<double>::epsilon())
    return Error{"chart_singularity",
                 "core point outside the regular static patch"};
  if (p.family == Family::toy) {
    const double a2 = p.spin * p.spin, inner2 = p.r_in * p.r_in;
    const double xi = 1 + k * a2, radial2 = inner2 + a2;
    if (!std::isfinite(xi) || !std::isfinite(radial2) || inner2 == 0)
      return Error{"numerical_failure", "core ellipsoid is not representable"};
    // Avoid an oblate inverse map: its focal disk is a regular part of this
    // Cartesian chart, even though the r>0 coordinates do not cover that disk.
    const double ellipsoid =
        xi * (xx * xx + yy * yy) / radial2 + zz * zz / inner2;
    if (!std::isfinite(ellipsoid))
      return Error{"numerical_failure", "nonfinite core ellipsoid coordinate"};
    if (ellipsoid > 1)
      return Error{"region_mismatch",
                   "Cartesian point is outside the exact toy core"};
  }

  using S = math::SpatialJet<Order>;
  const std::array<S, 3> x{S::variable(xx, 0), S::variable(yy, 1),
                           S::variable(zz, 2)};
  const S f = S(1) - S(k) * (x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
  std::array<std::array<S, 4>, 4> metric{};
  metric[0][0] = -f;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      metric[i + 1][j + 1] = S(i == j ? 1 : 0) + S(k) * x[i] * x[j] / f;

  Output<Order> out;
  out.radius_squared = rho2;
  out.lapse_squared = lapse;
  out.curvature_scale = k;
  out.determinant = -1;
  out.inverse[0][0] = -1 / lapse;
  const std::array coordinates{xx, yy, zz};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      out.inverse[i + 1][j + 1] =
          (i == j ? 1. : 0.) - k * coordinates[i] * coordinates[j];
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
  if (!finite(out.metric) || !finite(out.inverse) || !finite(out.dg))
    return Error{"numerical_failure",
                 "nonfinite Cartesian core metric or derivatives"};
  if constexpr (Order == 2) {
    if (!finite(out.ddg))
      return Error{"numerical_failure", "nonfinite Cartesian core Hessian"};
    const auto curvature =
        detail::curvature(out.metric, out.inverse, out.dg, out.ddg);
    out.ricci = curvature.ricci;
    out.ricci_scalar = curvature.ricci_scalar;
    out.kretschmann = curvature.kretschmann;
    if (!finite(out.ricci) || !std::isfinite(out.ricci_scalar) ||
        !std::isfinite(out.kretschmann))
      return Error{"numerical_failure", "nonfinite Cartesian core curvature"};
  }
  return out;
}
} // namespace
CoreResult evaluate_core(const Parameters &p, double x, double y, double z) {
  return evaluate_impl<1>(p, x, y, z);
}
CoreCurvatureResult evaluate_core_curvature(const Parameters &p, double x,
                                            double y, double z) {
  return evaluate_impl<2>(p, x, y, z);
}
} // namespace gravastar::geometry
