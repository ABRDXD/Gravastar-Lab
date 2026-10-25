// SPDX-License-Identifier: MIT
#pragma once
#include "gravastar/geometry/angular.hpp"
#include "gravastar/optics/ray.hpp"

namespace gravastar::optics {
// These transformations preserve t,r and transform angular covariant momenta.
// BL conversions exclude the poles; north/south transitions use q -> 1/q.
// Both input and output stereographic coordinates must satisfy q<=4.
State transform_state(const State &, Chart from, Chart to);
Vector transform_vector(const Vector &, const State &position, Chart from,
                        Chart to);
Frame transform_frame(const Frame &, const State &position, Chart from,
                      Chart to);
double axial_momentum(const State &, Chart);
geometry::AngularMetric angular_metric(const Parameters &, const State &);
State angular_rhs(const Parameters &, const State &);
double angular_null_residual(const geometry::AngularMetric &, const State &);
double angular_normal_frequency(const geometry::AngularMetric &, const State &);
} // namespace gravastar::optics
