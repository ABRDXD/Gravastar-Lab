// SPDX-License-Identifier: GPL-3.0-or-later
//
// Celestial background channel against closed forms.
// - A one-star field is b exp(-(1 - n.s)/w^2); its sphere integral is
//   2 pi w^2 b (1 - exp(-2/w^2)).
// - Static emitters: nu_e = -p_t / sqrt(-g_tt) with the analytic Kerr
//   g_tt = -(1 - 2 r / Sigma). For Schwarzschild with a static camera the
//   shift is sqrt((1 - 2/r_e) / (1 - 2/r_o)) for every ray.
#include "gravastar/optics/background.hpp"
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
namespace {
u::Json checks = u::Json::array();
void check(const std::string &name, double measured, double ceiling) {
  const bool pass = std::isfinite(measured) && measured <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", measured},
                    {"ceiling", ceiling},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << measured << " > " << ceiling << '\n';
}
void require(const std::string &name, bool pass) {
  check(name, pass ? 0 : 1, 0);
}
std::array<double, 3> direction(double theta, double phi) {
  return {std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi),
          std::cos(theta)};
}

void star_field() {
  o::StarField one{7, 1, .05, 100};
  // Locate the single star by its maximum on a fine grid, then refine.
  double best = -1, bt = 0, bp = 0;
  for (int i = 0; i <= 400; ++i)
    for (int j = 0; j < 800; ++j) {
      const double t = std::numbers::pi * i / 400, p = 2 * std::numbers::pi * j / 800;
      const double v = o::star_intensity(one, direction(t, p));
      if (v > best) {
        best = v;
        bt = t;
        bp = p;
      }
    }
  for (double step = .01; step > 1e-9; step /= 2)
    for (bool improved = true; improved;) {
      improved = false;
      for (auto [dt, dp] : {std::pair{step, 0.}, {-step, 0.}, {0., step}, {0., -step}}) {
        const double v = o::star_intensity(one, direction(bt + dt, bp + dp));
        if (v > best) {
          best = v;
          bt += dt;
          bp += dp;
          improved = true;
        }
      }
    }
  require("one-star peak brightness within [1/range, 1]",
          best >= 1.0 / 100 - 1e-15 && best <= 1 + 1e-15);
  // Sphere integral in polar coordinates centred on the star.
  const auto s = direction(bt, bp);
  const std::array<double, 3> a = std::abs(s[2]) < .9 ? std::array{0., 0., 1.}
                                                      : std::array{1., 0., 0.};
  std::array<double, 3> e1{s[1] * a[2] - s[2] * a[1], s[2] * a[0] - s[0] * a[2],
                           s[0] * a[1] - s[1] * a[0]};
  const double l = std::hypot(e1[0], e1[1], e1[2]);
  for (auto &x : e1)
    x /= l;
  const std::array<double, 3> e2{s[1] * e1[2] - s[2] * e1[1],
                                 s[2] * e1[0] - s[0] * e1[2],
                                 s[0] * e1[1] - s[1] * e1[0]};
  double integral = 0;
  const int nt = 40000, np = 64; // midpoint error ~ (dtheta/w)^2/24
  for (int i = 0; i < nt; ++i) {
    const double t = (i + .5) * std::numbers::pi / nt;
    for (int j = 0; j < np; ++j) {
      const double p = 2 * std::numbers::pi * (j + .5) / np;
      std::array<double, 3> n{};
      for (int k = 0; k < 3; ++k)
        n[k] = std::cos(t) * s[k] +
               std::sin(t) * (std::cos(p) * e1[k] + std::sin(p) * e2[k]);
      integral += o::star_intensity(one, n) * std::sin(t) *
                  (std::numbers::pi / nt) * (2 * std::numbers::pi / np);
    }
  }
  const double w2 = .05 * .05;
  const double exact = 2 * std::numbers::pi * w2 * best * (1 - std::exp(-2 / w2));
  check("one-star sphere integral vs 2 pi w^2 b", std::abs(integral - exact) / exact,
        1e-6);

  o::StarField field, other = field;
  other.seed = 2;
  double minimum = 1e300, difference = 0;
  bool deterministic = true;
  for (int i = 0; i < 2000; ++i) {
    const auto n = direction(std::acos(1 - 2 * (i + .5) / 2000), 2.39996 * i);
    const double v = o::star_intensity(field, n);
    minimum = std::min(minimum, v);
    deterministic = deterministic && v == o::star_intensity(field, n);
    difference = std::max(difference, std::abs(v - o::star_intensity(other, n)));
  }
  require("default field deterministic", deterministic);
  require("default field is nonnegative", minimum >= 0);
  require("a different seed gives a different sky", difference > 1e-3);
}

void redshift() {
  o::StarField field;
  // Schwarzschild: the camera's ZAMO is static, so every escaping ray has the
  // same shift.
  g::Parameters s;
  s.family = g::Family::kerr;
  s.mass = 1;
  s.spin = 0;
  o::Settings c;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  c.outer_radius = 20;
  c.maximum_affine = 200;
  c.capture_radius = 2.2;
  double schwarzschild = 0, kerr_error = 0, chart_error = 0;
  std::size_t escaped = 0;
  for (double spin : {0., .875})
    for (double up : {-.6, -.2, .2, .6})
      for (double right : {-.5, .4}) {
        auto p = s;
        p.spin = spin;
        const auto launch =
            o::launch(p, {0, 8, 1.0471975511965976, 0}, {-1, up, right});
        const auto t = o::trace(p, launch, c);
        if (t.outcome != o::Outcome::escaped)
          continue;
        ++escaped;
        const double nu_obs =
            o::frequency(launch, o::circular_frame(o::metric(p, launch)).basis[0]);
        const auto b = o::background_emission(p, t.final, nu_obs, field);
        const double r = t.final.state[1], th = t.final.state[2];
        const double sigma = r * r + spin * spin * std::cos(th) * std::cos(th);
        const double nu_e = -t.final.state[4] / std::sqrt(1 - 2 * r / sigma);
        const double want = nu_obs / nu_e;
        kerr_error = std::max(
            {kerr_error, std::abs(b.redshift - want) / want,
             std::abs(b.linear_intensity -
                      std::pow(want, 4) * o::star_intensity(field, b.direction)) /
                 std::max(1e-300, b.linear_intensity)});
        // Exact for a static camera (p_t is conserved exactly here).
        const double expected = std::sqrt((1 - 2 / r) / (1 - 2 / 8.));
        if (spin == 0)
          schwarzschild =
              std::max(schwarzschild, std::abs(b.redshift - expected) / expected);
        // Same escape point in the patch covering its hemisphere.
        const auto patch = t.final.state[2] <= std::numbers::pi / 2
                               ? o::Chart::north
                               : o::Chart::south;
        const o::Point other{t.final.affine,
                             o::transform_state(t.final.state,
                                                o::Chart::boyer_lindquist, patch),
                             0, patch};
        const auto bn = o::background_emission(p, other, nu_obs, field);
        for (int k = 0; k < 3; ++k)
          chart_error = std::max(chart_error,
                                 std::abs(bn.direction[k] - b.direction[k]));
        chart_error = std::max(chart_error, std::abs(bn.redshift - b.redshift));
      }
  require("at least eight escaping rays", escaped >= 8);
  check("Schwarzschild static-to-static shift sqrt((1-2/r_e)/(1-2/8))",
        schwarzschild, 1e-14);
  check("Kerr background shift and g^4 intensity vs analytic g_tt", kerr_error,
        1e-13);
  check("BL and stereographic-patch escape points give the same sky sample",
        chart_error, 1e-13);
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    star_field();
    redshift();
    bool pass = true;
    for (const auto &entry : checks)
      pass = pass && entry["passed"].get<bool>();
    if (argc == 3) {
      const auto root = std::filesystem::path(GRAVASTAR_SOURCE_ROOT);
      auto output = u::result_path(root, argv[2]);
      if (!pass)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(output, {{"schema_version", 1},
                              {"passed", pass},
                              {"checks", checks},
                              {"compiler", __VERSION__},
                              {"computational_source_sha256",
                               u::manifest(root)["computational_sha256"]}});
    }
    std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
              << " background checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "background: " << error.what() << '\n';
    return 1;
  }
}
