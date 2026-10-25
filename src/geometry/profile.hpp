// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/geometry/geometry.hpp"
#include <utility>

namespace gravastar::geometry::detail {
// Shared radial model for both coordinate charts. Scalar supplies value() and
// arithmetic; derivatives propagate through its algebra without differencing.
template <class Scalar> Scalar smoothstep(const Scalar &u) {
  const Scalar u2 = u * u;
  return u2 * u2 *
         (Scalar(35) + u * (Scalar(-84) + u * (Scalar(70) - Scalar(20) * u)));
}
template <class Scalar>
std::pair<Scalar, Scalar> transition(const Parameters &p, const Scalar &r,
                                     Region region) {
  if (p.family == Family::kerr)
    return {Scalar(1), Scalar(0)};
  if (p.family == Family::de_sitter)
    return {Scalar(0), Scalar(1)};
  if (p.family == Family::flat)
    return {Scalar(0), Scalar(0)};
  if (region == Region::core ||
      (region == Region::automatic && r.value() <= p.r_in))
    return {Scalar(0), Scalar(1)};
  if (region == Region::exterior ||
      (region == Region::automatic && r.value() >= p.r_out))
    return {Scalar(1), Scalar(0)};
  const double width = p.r_out - p.r_in;
  // Evaluate the small tail directly to preserve accuracy at both joins.
  if (r.value() - p.r_in <= width * 0.5) {
    const Scalar mu = smoothstep((r - Scalar(p.r_in)) / Scalar(width));
    return {mu, Scalar(1) - mu};
  }
  const Scalar lambda = smoothstep((Scalar(p.r_out) - r) / Scalar(width));
  return {Scalar(1) - lambda, lambda};
}
} // namespace gravastar::geometry::detail
