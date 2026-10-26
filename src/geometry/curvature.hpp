// SPDX-License-Identifier: MIT
#pragma once
#include "gravastar/geometry/geometry.hpp"

namespace gravastar::geometry::detail {
struct Curvature {
  Matrix ricci{};
  double ricci_scalar{}, kretschmann{};
};
Curvature curvature(const Matrix &metric, const Matrix &inverse,
                    const Tensor3 &dg, const Tensor4 &ddg);
} // namespace gravastar::geometry::detail
