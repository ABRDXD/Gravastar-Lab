// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/geometry/geometry.hpp"

namespace gravastar::geometry {
// Static Cartesian coordinates (T,X,Y,Z), regular at the center and at the
// oblate-coordinate focal disk. This patch covers only the exact core model.
struct CoreMetric {
  Matrix metric{}, inverse{};
  Tensor3 dg{};
  double determinant{}, radius_squared{}, lapse_squared{}, curvature_scale{};
};
struct CoreCurvature : CoreMetric {
  Tensor4 ddg{};
  Matrix ricci{};
  double ricci_scalar{}, kretschmann{};
};
using CoreResult = std::variant<CoreMetric, Error>;
using CoreCurvatureResult = std::variant<CoreCurvature, Error>;

// Toy inputs must lie in the closed ellipsoid corresponding to r<=r_in.
// de_sitter/flat reference inputs need only satisfy the static-patch domain.
// Kerr is not supported. F=1-(Lambda/3)*(X^2+Y^2+Z^2)>64 epsilon is required
// (Lambda is zero for Family::flat); no cosmological-horizon crossing is added.
CoreResult evaluate_core(const Parameters &, double x, double y, double z);
CoreCurvatureResult evaluate_core_curvature(const Parameters &, double x,
                                            double y, double z);
} // namespace gravastar::geometry
