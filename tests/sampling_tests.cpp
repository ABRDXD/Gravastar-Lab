// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/sampling.hpp"
#include <iostream>
#include <limits>

namespace s = gravastar::optics::sampling;
int main() {
  try {
    int checks = 0;
    auto check = [&](bool pass, const char *name) {
      ++checks;
      if (!pass) throw std::runtime_error(name);
    };
    auto near = [&](double a, double b) { return std::abs(a - b) < 2e-13; };
    auto reject = [&](auto action) {
      try { action(); } catch (const std::invalid_argument &) { return true; }
      return false;
    };
    auto grid = [](int width, int height, int scale, auto function) {
      std::vector<s::Sample> samples;
      for (int y = 0; y < height * scale; ++y)
        for (int x = 0; x < width * scale; ++x)
          samples.push_back(function((x + .5) / scale, (y + .5) / scale));
      return s::aggregate(width, height, scale, samples);
    };
    auto emitted = [](double value) { return s::Sample{s::Class::emitted, value, 1}; };
    const s::Sample unknown{s::Class::unresolved, {}, {}};
    std::vector<s::Grid> smooth;
    for (int scale : {1, 2, 4, 8}) {
      auto g = grid(2, 3, scale, [&](double x, double y) { return emitted(x*x + y*y); });
      // Exact square integral, independently evaluated per unit base pixel.
      for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 2; ++x)
          check(near(*g.pixels[2*y+x].intensity,
                     x*x+x+y*y+y+2.0/3 - 1.0/(6*scale*scale)),
                "quadratic midpoint integral and second-order error");
      smooth.push_back(g);
      auto linear = grid(2, 3, scale, [&](double x, double y) { return emitted(x+10*y); });
      check(near(*linear.pixels[5].intensity, 26.5), "row/column mapping on rectangular image");
      auto constant = grid(2, 3, scale, [&](double, double) { return emitted(7); });
      check(near(*constant.pixels[4].intensity, 7), "constant radiance preserved");
    }
    auto differences = s::compare(smooth);
    check(differences.size() == 3 && differences[0].common_pixels == 6,
          "three successive comparisons on six pixels");
    for (std::size_t i = 0; i < differences.size(); ++i) {
      check(near(*differences[i].mean_absolute_intensity, .125 / (1 << (2*i))),
            "analytic midpoint refinement error");
      check(differences[i].mean_classification_tv == 0, "smooth classifications unchanged");
    }
    for (int scale : {1, 2, 4, 8}) {
      auto edge = grid(1, 1, scale, [&](double x, double) {
        return x < 1.0/3 ? emitted(1) : s::Sample{s::Class::escaped, 0, 0};
      });
      const double count = scale == 1 ? 0 : scale == 2 ? 1 : scale == 4 ? 1 : 3;
      check(near(*edge.pixels[0].intensity, count / scale) &&
            near(edge.pixels[0].fractions[0], count / scale), "discontinuous edge coverage");
      check(std::abs(*edge.pixels[0].intensity - 1.0/3) <= 1.0/scale,
            "edge integral error within one sampling cell");
    }
    auto mixed = s::aggregate(1, 1, 2, {emitted(4), {s::Class::escaped,0,0},
                                      {s::Class::captured,0,1}, unknown});
    check(!mixed.pixels[0].intensity && !mixed.pixels[0].alpha &&
          mixed.pixels[0].known_contribution == 1 && mixed.pixels[0].fractions[3] == .25,
          "unknown subpixel is not zero or a resolved-only average");
    std::vector<s::Grid> masks;
    for (int scale : {1, 2, 4})
      masks.push_back(grid(2, 1, scale, [&](double x, double) {
        return scale == 4 && x < 1 ? unknown : emitted(scale);
      }));
    auto masked = s::compare(masks);
    check(masked[0].common_pixels == 1 && masked[1].common_pixels == 1 &&
          *masked[0].mean_absolute_intensity == 1 &&
          *masked[1].mean_absolute_intensity == 2,
          "all comparisons share the intersection of every resolved mask");
    check(masked[1].mean_classification_tv == .5 &&
          masked[1].mean_unresolved_fraction_change == .5,
          "unresolved classification changes remain in full-image statistics");
    auto empty = s::compare({s::aggregate(1,1,1,{unknown}),
                            s::aggregate(1,1,2,{unknown,unknown,unknown,unknown})});
    check(empty[0].common_pixels == 0 && !empty[0].mean_absolute_intensity &&
          !empty[0].maximum_absolute_intensity && !empty[0].relative_l1_intensity,
          "empty mask is unavailable, not perfect agreement");
    auto zero = s::compare({s::aggregate(1,1,1,{emitted(0)}),
                           s::aggregate(1,1,2,std::vector<s::Sample>(4,emitted(0)))});
    check(zero[0].mean_absolute_intensity == 0 && !zero[0].relative_l1_intensity,
          "zero reference radiance has no relative error");
    check(reject([&] { (void)s::aggregate(0,1,1,{}); }), "zero dimension rejected");
    check(reject([&] { (void)s::aggregate(128,1,2,{}); }), "oversized grid rejected");
    check(reject([&] { (void)s::aggregate(1,1,2,{emitted(1)}); }), "truncated samples rejected");
    for (auto bad : {s::Sample{s::Class::unresolved,0,{}},
                     s::Sample{s::Class::emitted,{},1},
                     s::Sample{s::Class::escaped,1,0},
                     s::Sample{s::Class::captured,0,0},
                     s::Sample{s::Class::emitted,-1,1},
                     s::Sample{s::Class::emitted,std::numeric_limits<double>::infinity(),1},
                     s::Sample{static_cast<s::Class>(99),0,0}})
      check(reject([&] { (void)s::aggregate(1,1,1,{bad}); }), "malformed sample rejected");
    check(reject([&] { (void)s::compare({smooth[0],smooth[0]}); }), "repeated level rejected");
    auto corrupt = smooth[1];
    corrupt.pixels[0].fractions[0] = std::numeric_limits<double>::quiet_NaN();
    check(reject([&] { (void)s::compare({smooth[0],corrupt}); }), "nonfinite aggregate rejected");
    std::cout << "PASS " << checks << " sampling arithmetic checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "sampling test failed: " << e.what() << '\n';
    return 1;
  }
}
