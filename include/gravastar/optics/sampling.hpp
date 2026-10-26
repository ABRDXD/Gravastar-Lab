// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace gravastar::optics::sampling {
// Values are averages over uniform image-plane area, not solid-angle flux.
enum class Class { emitted, escaped, captured, unresolved };
struct Sample {
  Class classification;
  std::optional<double> intensity;
  std::optional<double> alpha;
};
struct Pixel {
  std::array<double, 4> fractions{};
  std::optional<double> intensity, alpha;
  // Known contribution divided by ALL samples, never a resolved-only mean.
  // This is not a complete radiance estimate when any sample is unresolved.
  double known_contribution{};
};
struct Grid {
  int width{}, height{}, scale{};
  std::vector<Pixel> pixels;
};
inline Grid aggregate(int width, int height, int scale,
                      const std::vector<Sample> &samples) {
  if (width < 1 || height < 1 || scale < 1 || scale > 8 ||
      width > 128 / scale || height > 128 / scale ||
      samples.size() !=
          static_cast<std::size_t>(width * height * scale * scale))
    throw std::invalid_argument("invalid image sample grid");
  Grid out{width, height, scale, std::vector<Pixel>(width * height)};
  const double weight = 1.0 / (scale * scale);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      auto &pixel = out.pixels[y * width + x];
      double alpha = 0;
      bool resolved = true;
      for (int sy = 0; sy < scale; ++sy)
        for (int sx = 0; sx < scale; ++sx) {
          const auto &s =
              samples[(y * scale + sy) * width * scale + x * scale + sx];
          const auto c = static_cast<std::size_t>(s.classification);
          if (c >= pixel.fractions.size())
            throw std::invalid_argument("unknown sample classification");
          pixel.fractions[c] += weight;
          if (s.classification == Class::unresolved) {
            if (s.intensity || s.alpha)
              throw std::invalid_argument(
                  "unresolved sample has invented radiance");
            resolved = false;
          } else {
            if (!s.intensity || !s.alpha || !std::isfinite(*s.intensity) ||
                *s.intensity < 0 || !std::isfinite(*s.alpha) || *s.alpha < 0 ||
                *s.alpha > 1)
              throw std::invalid_argument("invalid resolved sample");
            if ((s.classification == Class::escaped &&
                 (*s.intensity != 0 || *s.alpha != 0)) ||
                (s.classification == Class::captured &&
                 (*s.intensity != 0 || *s.alpha != 1)) ||
                (s.classification == Class::emitted && *s.alpha != 1))
              throw std::invalid_argument(
                  "sample contradicts emission convention");
            pixel.known_contribution += weight * *s.intensity;
            alpha += weight * *s.alpha;
          }
        }
      if (resolved) {
        pixel.intensity = pixel.known_contribution;
        pixel.alpha = alpha;
      }
    }
  return out;
}
struct Difference {
  std::size_t common_pixels{};
  std::optional<double> mean_absolute_intensity, maximum_absolute_intensity;
  std::optional<double> relative_l1_intensity;
  double mean_classification_tv{}, maximum_classification_tv{};
  double mean_unresolved_fraction_change{};
};
// Every comparison uses the SAME mask: resolved at ALL supplied levels.
// An empty mask or zero reference radiance cannot establish convergence.
inline std::vector<Difference> compare(const std::vector<Grid> &levels) {
  if (levels.size() < 2)
    throw std::invalid_argument("at least two sampling levels are required");
  const auto n = levels.front().pixels.size();
  if (n == 0 || levels.front().width < 1 || levels.front().height < 1)
    throw std::invalid_argument("empty sampling image");
  std::vector<bool> common(n, true);
  int previous_scale = 0;
  for (const auto &g : levels) {
    if (g.width != levels.front().width || g.height != levels.front().height ||
        g.pixels.size() != n || g.scale <= previous_scale || g.scale > 8 ||
        g.width > 128 / g.scale || g.height > 128 / g.scale ||
        n != static_cast<std::size_t>(g.width * g.height))
      throw std::invalid_argument("incompatible or unordered sampling levels");
    previous_scale = g.scale;
    for (std::size_t i = 0; i < n; ++i) {
      const auto &p = g.pixels[i];
      double total = 0;
      for (double f : p.fractions) {
        if (!std::isfinite(f) || f < 0 || f > 1)
          throw std::invalid_argument("invalid classification fraction");
        total += f;
      }
      if (std::abs(total - 1) > 1e-12 || !std::isfinite(p.known_contribution) ||
          p.known_contribution < 0 ||
          p.intensity.has_value() != p.alpha.has_value() ||
          p.intensity.has_value() != (p.fractions[3] == 0) ||
          (p.intensity &&
           (!std::isfinite(*p.intensity) || *p.intensity < 0 ||
            *p.intensity != p.known_contribution || !std::isfinite(*p.alpha) ||
            *p.alpha < 0 || *p.alpha > 1)))
        throw std::invalid_argument("inconsistent aggregate pixel");
      common[i] = common[i] && g.pixels[i].intensity.has_value();
    }
  }
  std::vector<Difference> out;
  for (std::size_t level = 1; level < levels.size(); ++level) {
    Difference d;
    long double reference_sum = 0, absolute_sum = 0;
    double maximum = 0;
    for (std::size_t i = 0; i < n; ++i) {
      const auto &a = levels[level - 1].pixels[i], &b = levels[level].pixels[i];
      double tv = 0;
      for (std::size_t c = 0; c < 4; ++c)
        tv += .5 * std::abs(a.fractions[c] - b.fractions[c]);
      d.mean_classification_tv += tv / n;
      d.maximum_classification_tv = std::max(d.maximum_classification_tv, tv);
      d.mean_unresolved_fraction_change +=
          std::abs(a.fractions[3] - b.fractions[3]) / n;
      if (common[i]) {
        ++d.common_pixels;
        const double delta = std::abs(*a.intensity - *b.intensity);
        absolute_sum += delta;
        reference_sum += std::abs(*b.intensity);
        maximum = std::max(maximum, delta);
      }
    }
    if (d.common_pixels) {
      d.mean_absolute_intensity = absolute_sum / d.common_pixels;
      d.maximum_absolute_intensity = maximum;
    }
    if (d.common_pixels && reference_sum > 0)
      d.relative_l1_intensity = absolute_sum / reference_sum;
    out.push_back(d);
  }
  return out;
}
} // namespace gravastar::optics::sampling
