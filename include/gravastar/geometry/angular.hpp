// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/geometry/geometry.hpp"

namespace gravastar::geometry {
// Coordinate order (t,r,x,y), x+iy=tan(theta/2)*exp(i*phi) in the north
// patch; replace theta by pi-theta in the south patch. Reflection symmetry
// makes the metric identical in both patches; the caller retains patch
// identity.
struct AngularMetric {
  Matrix metric{}, inverse{};
  Tensor3 dg{};
  double determinant{}, mu{}, lambda{}, sigma{}, delta{}, delta_theta{}, xi{};
};
struct AngularCurvature : AngularMetric {
  Tensor4 ddg{};
  Matrix ricci{};
  double ricci_scalar{}, kretschmann{};
};
using AngularResult = std::variant<AngularMetric, Error>;
using AngularCurvatureResult = std::variant<AngularCurvature, Error>;

// Includes x=y=0. Bounded to x*x+y*y<=4; switch patches in the overlap
// before exceeding this bound. Retains r>0 and the existing Delta margin.
// Explicit shell regions evaluate the polynomial at both joins, unclamped.
// This is a geometry API, not yet a change to the production ray tracer.
AngularResult evaluate_angular(const Parameters &p, double r, double x,
                               double y, Region region = Region::automatic);
AngularCurvatureResult
evaluate_angular_curvature(const Parameters &p, double r, double x, double y,
                           Region region = Region::automatic);
} // namespace gravastar::geometry
