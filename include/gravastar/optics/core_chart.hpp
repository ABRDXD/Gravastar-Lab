// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/geometry/core.hpp"
#include "gravastar/optics/chart.hpp"

namespace gravastar::optics {
// Static core coordinates (T,X,Y,Z). T=t/Xi, Phi=phi-k*a*T.
// Only the exact constant-curvature region is eligible. Inverse transformations
// require positive oblate r: the focal disk is covered solely by the core
// patch.
State to_core(const Parameters &, const State &, Chart angular_chart);
State from_core(const Parameters &, const State &, Chart angular_chart);
Vector vector_to_core(const Parameters &, const Vector &, const State &angular,
                      Chart angular_chart);
Vector vector_from_core(const Parameters &, const Vector &, const State &core,
                        Chart angular_chart);
Frame frame_to_core(const Parameters &, const Frame &, const State &angular,
                    Chart angular_chart);
Frame frame_from_core(const Parameters &, const Frame &, const State &core,
                      Chart angular_chart);
double core_oblate_radius(const Parameters &, const State &);
double stationary_momentum(const Parameters &, const State &, Chart);
geometry::CoreMetric core_metric(const Parameters &, const State &);
State core_rhs(const Parameters &, const State &);
double core_null_residual(const geometry::CoreMetric &, const State &);
double core_normal_frequency(const geometry::CoreMetric &, const State &);
} // namespace gravastar::optics
