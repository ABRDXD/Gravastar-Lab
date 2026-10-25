// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/core_chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
u::Json checks = u::Json::array();
void check(const std::string &name, double measured, double ceiling) {
  const bool pass = std::isfinite(measured) && measured <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", measured},
                    {"threshold", ceiling},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << measured << " > " << ceiling << '\n';
}
double difference(const o::State &a, const o::State &b) {
  double e = 0;
  for (int k = 0; k < 8; ++k)
    e = std::max(e, std::abs(a[k] - b[k]) / (1 + std::abs(b[k])));
  return e;
}
double hamiltonian(const g::Matrix &inverse, const o::State &s) {
  double h = 0;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      h += .5 * inverse[i][j] * s[i + 4] * s[j + 4];
  return h;
}
void transitions() {
  double state = 0, pullback = 0, pairing = 0, vectors = 0, kill = 0,
         h_error = 0, position = 0, frames = 0, frequency = 0;
  for (auto family : {g::Family::flat, g::Family::de_sitter, g::Family::toy})
    for (double spin : {-.875, 0., .875})
      for (double radius : {.4, .75, 1., 1.4})
        for (double time : {-10., 0., 10.})
          for (auto chart : {o::Chart::north, o::Chart::south})
            for (const auto uv : {std::array{0., 0.}, std::array{.3, -.4},
                                  std::array{1., .2}}) {
              g::Parameters p{family};
              p.spin = spin;
              const o::State a{time, radius, uv[0], uv[1], -1.2, .3, -.2, .7};
              const auto b = o::to_core(p, a, chart),
                         back = o::from_core(p, b, chart);
              state = std::max(state, difference(a, back));
              const auto angular = o::angular_metric(p, a);
              const auto core = o::core_metric(p, b);
              std::array<o::Vector, 4> basis{};
              for (int i = 0; i < 4; ++i) {
                o::Vector unit{};
                unit[i] = 1;
                basis[i] = o::vector_to_core(p, unit, a, chart);
                const auto inverse = o::vector_from_core(p, basis[i], b, chart);
                for (int j = 0; j < 4; ++j)
                  vectors = std::max(vectors, std::abs(inverse[j] - unit[j]));
                pairing = std::max(
                    pairing, std::abs(o::frequency(b, basis[i]) + a[i + 4]));
              }
              for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) {
                  double cov = 0;
                  for (int k = 0; k < 4; ++k)
                    for (int l = 0; l < 4; ++l)
                      cov += core.metric[k][l] * basis[i][k] * basis[j][l];
                  pullback = std::max(pullback,
                                      std::abs(cov - angular.metric[i][j]) /
                                          (1 + std::abs(angular.metric[i][j])));
                }
              const double lz = b[1] * b[6] - b[2] * b[5];
              const double k = family == g::Family::flat
                                   ? 0
                                   : p.cosmological_constant / 3,
                           xi = 1 + k * spin * spin;
              kill =
                  std::max({kill, std::abs(lz - (-a[3] * a[6] + a[2] * a[7])),
                            std::abs((b[4] - k * spin * lz) / xi - a[4])});
              h_error = std::max(
                  h_error, std::abs(hamiltonian(core.inverse, b) -
                                    hamiltonian(angular.inverse, a)) /
                               (1 + std::abs(hamiltonian(angular.inverse, a))));
              const double rho = std::hypot(uv[0], uv[1]),
                           theta = chart == o::Chart::north
                                       ? 2 * std::atan(rho)
                                       : std::numbers::pi - 2 * std::atan(rho);
              const double phi = rho == 0 ? 0 : std::atan2(uv[1], uv[0]);
              const double phase = phi - k * spin * time / xi,
                           transverse =
                               std::sqrt((radius * radius + spin * spin) / xi) *
                               std::sin(theta);
              const o::Vector independent{
                  time / xi, transverse * std::cos(phase),
                  transverse * std::sin(phase), radius * std::cos(theta)};
              for (int i = 0; i < 4; ++i)
                position = std::max(position, std::abs(b[i] - independent[i]));
              if (rho == 0)
                continue; // BL observer frame itself excludes poles.
              const auto bl =
                  o::transform_state(a, chart, o::Chart::boyer_lindquist);
              const auto gm = o::metric(p, bl);
              const double zamo = -gm.metric[0][3] / gm.metric[3][3];
              auto frame = o::circular_frame(gm, zamo + .05);
              const auto af = o::transform_frame(
                  frame, bl, o::Chart::boyer_lindquist, chart);
              const auto cf = o::frame_to_core(p, af, a, chart),
                         restored = o::frame_from_core(p, cf, b, chart);
              for (int i = 0; i < 4; ++i) {
                frequency =
                    std::max(frequency, std::abs(o::frequency(a, af.basis[i]) -
                                                 o::frequency(b, cf.basis[i])));
                for (int j = 0; j < 4; ++j) {
                  frames = std::max(
                      frames, std::abs(restored.basis[i][j] - af.basis[i][j]));
                  double dot = 0;
                  for (int k2 = 0; k2 < 4; ++k2)
                    for (int l = 0; l < 4; ++l)
                      dot +=
                          core.metric[k2][l] * cf.basis[i][k2] * cf.basis[j][l];
                  frames = std::max(
                      frames, std::abs(dot - (i == j ? (i == 0 ? -1 : 1) : 0)));
                }
              }
              if (cf.omega != frame.omega || restored.omega != frame.omega)
                throw std::runtime_error(
                    "original BL frame-speed label changed");
            }
  check("angular-core state and covector round trips", state, 3e-13);
  check("Cartesian metric pullback against angular metric", pullback, 3e-13);
  check("canonical covector/vector pairing", pairing, 3e-13);
  check("contravariant vector round trips", vectors, 3e-13);
  check("global stationary and axial Killing momenta", kill, 3e-13);
  check("Hamiltonian invariance for arbitrary covectors", h_error, 3e-13);
  check("positions against independent BL trigonometric map", position, 3e-13);
  check("transformed observer frames and round trips", frames, 3e-13);
  check("observer frequency under core transformation", frequency, 3e-13);
}
void domains() {
  g::Parameters p;
  int rejected = 0;
  for (const auto s :
       {o::State{0, 0, 0, 0, -1, 0, 0, 0}, o::State{0, .3, .2, 0, -1, 0, 0, 0}})
    try {
      (void)o::from_core(p, s, o::Chart::north);
    } catch (const std::exception &) {
      ++rejected;
    }
  try {
    (void)o::to_core(p, {0, 1.6, .3, .4, -1, 0, 0, 0}, o::Chart::north);
  } catch (const std::exception &) {
    ++rejected;
  }
  p.family = g::Family::kerr;
  try {
    (void)o::to_core(p, {0, 2., .3, .4, -1, 0, 0, 0}, o::Chart::north);
  } catch (const std::exception &) {
    ++rejected;
  }
  check("inverse focal disk and wrong regions rejected",
        std::abs(rejected - 4.), 0);
}
int main(int argc, char **argv) {
  try {
    transitions();
    domains();
    bool pass = true;
    for (const auto &c : checks)
      pass = pass && c["passed"].get<bool>();
    if (argc == 3 && std::string(argv[1]) == "--report") {
      const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
      auto output = u::result_path(root, argv[2]);
      if (!pass)
        output.replace_filename(output.stem().string() + ".failed.json");
      u::atomic_json(output, {{"passed", pass},
                              {"checks", checks},
                              {"compiler", __VERSION__},
                              {"computational_source_sha256",
                               u::manifest(root)["computational_sha256"]}});
    } else if (argc != 1)
      return 2;
    std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
              << " core chart checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
