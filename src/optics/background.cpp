// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/background.hpp"
#include "gravastar/optics/chart.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace gravastar::optics {
namespace {
std::uint64_t splitmix(std::uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
double unit(std::uint64_t x) { return double(x >> 11) * 0x1.0p-53; }
} // namespace

StarCatalogue make_catalogue(const StarField &f) {
  if (f.count < 1 || !(f.width > 0) || !(f.range >= 1))
    throw std::invalid_argument("invalid star field");
  StarCatalogue out;
  out.field = f;
  out.stars.reserve(f.count);
  const double golden = std::numbers::pi * (3 - std::sqrt(5.0));
  for (int i = 0; i < f.count; ++i) {
    // Fibonacci lattice with hashed jitter of about one lattice spacing.
    const std::uint64_t h = splitmix(f.seed * 0x100000001b3ULL + i);
    const double z0 = 1 - (2 * (i + .5)) / f.count;
    const double spacing = 2.0 / f.count;
    const double z = std::clamp(z0 + (unit(h) - .5) * spacing, -1.0, 1.0);
    const double phi = golden * i + (unit(splitmix(h)) - .5) * golden;
    out.stars.push_back({std::sqrt(std::max(0.0, 1 - z * z)), std::cos(phi),
                         std::sin(phi), z,
                         std::pow(f.range, -unit(splitmix(h ^ 0x5bd1e995)))});
  }
  return out;
}

double star_intensity(const StarCatalogue &catalogue,
                      const std::array<double, 3> &n) {
  const double length = std::hypot(n[0], n[1], n[2]);
  if (!(std::abs(length - 1) < 1e-9))
    throw std::invalid_argument("star field direction must be a unit vector");
  const double w = catalogue.field.width, w2 = w * w, cutoff = 40 * w2;
  double sum = 0;
  // Same expression order as the original per-call construction, so values
  // are bit-identical to it.
  for (const auto &s : catalogue.stars) {
    const double dot = n[0] * s.rho * s.cos_phi + n[1] * s.rho * s.sin_phi +
                       n[2] * s.z;
    const double x = 1 - dot;
    if (x > cutoff) // exp(-40)
      continue;
    sum += s.brightness * std::exp(-x / w2);
  }
  return sum;
}

double star_intensity(const StarField &f, const std::array<double, 3> &n) {
  return star_intensity(make_catalogue(f), n);
}

std::array<double, 3> sky_direction(const Point &point) {
  const auto &y = point.state;
  if (point.chart == Chart::boyer_lindquist)
    return {std::sin(y[2]) * std::cos(y[3]), std::sin(y[2]) * std::sin(y[3]),
            std::cos(y[2])};
  if (point.chart == Chart::core_cartesian)
    throw std::invalid_argument("sky direction is undefined in the core patch");
  const double q = y[2] * y[2] + y[3] * y[3];
  const double pole = point.chart == Chart::north ? 1 : -1;
  return {2 * y[2] / (1 + q), 2 * y[3] / (1 + q), pole * (1 - q) / (1 + q)};
}

BackgroundSample background_emission(const Parameters &p, const Point &escape,
                                     double nu_observer,
                                     const StarCatalogue &stars) {
  if (!std::isfinite(nu_observer) || nu_observer <= 0)
    throw std::invalid_argument("invalid observer frequency");
  // d/dt is the same vector in BL and in the angular patches, so g_tt is.
  const double gtt = escape.chart == Chart::boyer_lindquist
                         ? metric(p, escape.state).metric[0][0]
                         : angular_metric(p, escape.state).metric[0][0];
  if (!(gtt < 0))
    throw std::runtime_error("no static emitter at the escape point");
  const double nu = -escape.state[4] / std::sqrt(-gtt);
  if (!(nu > 0))
    throw std::runtime_error("nonpositive background emitter frequency");
  BackgroundSample out;
  out.direction = sky_direction(escape);
  out.emitted_frequency = nu;
  out.redshift = nu_observer / nu;
  out.linear_intensity =
      std::pow(out.redshift, 4) * star_intensity(stars, out.direction);
  if (!std::isfinite(out.linear_intensity))
    throw std::runtime_error("nonfinite background intensity");
  return out;
}
} // namespace gravastar::optics
