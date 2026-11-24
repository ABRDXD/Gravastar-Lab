// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/optics/ray.hpp"
#include <array>
#include <cstdint>

namespace gravastar::optics {
// Phenomenological celestial background for escaped rays: a shell of static
// emitters (u proportional to d/dt) at the escape radius, painted with a
// deterministic smooth star field. Star positions and brightness are artistic
// content; only the frequency shift is physics. It is a separate channel and
// never adds to the disk's linear intensity.
struct StarField {
  std::uint64_t seed{1};
  int count{1000};      // stars on the sphere
  double width{.02};    // Gaussian angular width (rad)
  double range{100};    // brightest / faintest emitted intensity
};
// Emitted bolometric intensity in unit direction n (|n| = 1 required).
// Smooth in n: I(n) = sum_i b_i exp(-(1 - n.s_i) / width^2).
double star_intensity(const StarField &, const std::array<double, 3> &n);
// Unit direction of a point's angular position: BL (theta, phi) or the
// north/south stereographic patch formula. Not defined in the core patch.
std::array<double, 3> sky_direction(const Point &);
struct BackgroundSample {
  double redshift{}, emitted_frequency{}, linear_intensity{};
  std::array<double, 3> direction{};
};
// Background seen along a ray that escaped at `escape`. The emitter is the
// static observer there, nu_e = -p_t / sqrt(-g_tt); requires g_tt < 0.
// Observed bolometric intensity (nu_obs / nu_e)^4 I_e.
BackgroundSample background_emission(const Parameters &, const Point &escape,
                                     double observer_frequency,
                                     const StarField &);
} // namespace gravastar::optics
