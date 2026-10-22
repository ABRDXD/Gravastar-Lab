// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/ray.hpp"
#include "gravastar/reference/geometry.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace r = gravastar::reference;
namespace u = gravastar::tools;
using J = u::Json;
J checks = J::array();
void check(const std::string &name, double measured, double threshold) {
  bool pass = std::isfinite(measured) && measured <= threshold;
  checks.push_back({{"name", name},
                    {"measured", measured},
                    {"threshold", threshold},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << std::setprecision(17) << measured << " > "
              << threshold << '\n';
}
void require(const std::string &name, bool pass) {
  check(name, pass ? 0 : 1, 0);
}
double difference(const o::State &a, const o::State &b) {
  double e = 0;
  for (int i = 0; i < 8; ++i)
    e = std::max(e, std::abs(a[i] - b[i]) / (1 + std::abs(b[i])));
  return e;
}
std::array<double, 3> cartesian(const o::State &s) {
  return {s[1] * std::sin(s[2]) * std::cos(s[3]),
          s[1] * std::sin(s[2]) * std::sin(s[3]), s[1] * std::cos(s[2])};
}
std::array<double, 3> cartesian_velocity(const o::State &s, const o::State &d) {
  const double r = s[1], t = s[2], p = s[3];
  return {
      d[1] * std::sin(t) * std::cos(p) + r * d[2] * std::cos(t) * std::cos(p) -
          r * d[3] * std::sin(t) * std::sin(p),
      d[1] * std::sin(t) * std::sin(p) + r * d[2] * std::cos(t) * std::sin(p) +
          r * d[3] * std::sin(t) * std::cos(p),
      d[1] * std::cos(t) - r * d[2] * std::sin(t)};
}
o::State rk4(const g::Parameters &p, o::State y, double duration, double h) {
  const int count = static_cast<int>(std::ceil(duration / h));
  h = duration / count;
  for (int n = 0; n < count; ++n) {
    auto a = o::rhs(p, y);
    o::State z{};
    for (int i = 0; i < 8; ++i)
      z[i] = y[i] + h * a[i] / 2;
    auto b = o::rhs(p, z);
    for (int i = 0; i < 8; ++i)
      z[i] = y[i] + h * b[i] / 2;
    auto c = o::rhs(p, z);
    for (int i = 0; i < 8; ++i)
      z[i] = y[i] + h * c[i];
    auto d = o::rhs(p, z);
    for (int i = 0; i < 8; ++i)
      y[i] += h * (a[i] + 2 * b[i] + 2 * c[i] + d[i]) / 6;
  }
  return y;
}
double carter(const g::Parameters &p, const o::State &s) {
  return s[6] * s[6] + std::pow(std::cos(s[2]), 2) *
                           (s[7] * s[7] / std::pow(std::sin(s[2]), 2) -
                            p.spin * p.spin * s[4] * s[4]);
}
void frames_and_reference() {
  g::Parameters p;
  double frame_error = 0, launch_error = 0, freq_error = 0;
  for (double radius : {.8, 1.5, 1.65, 1.8, 6.})
    for (double theta : {.1, 1.1, 1.5707963267948966}) {
      o::State at{};
      at[1] = radius;
      at[2] = theta;
      auto m = o::metric(p, at);
      double z = -m.metric[0][3] / m.metric[3][3];
      double width = std::sqrt(m.metric[0][3] * m.metric[0][3] -
                               m.metric[0][0] * m.metric[3][3]) /
                     m.metric[3][3];
      for (double omega : {z, z + .2 * width}) {
        auto f = o::circular_frame(m, omega);
        for (int a = 0; a < 4; ++a)
          for (int b = 0; b < 4; ++b) {
            double product = 0;
            for (int i = 0; i < 4; ++i)
              for (int j = 0; j < 4; ++j)
                product += m.metric[i][j] * f.basis[a][i] * f.basis[b][j];
            frame_error = std::max(
                frame_error,
                std::abs(product - (a == b ? (a == 0 ? -1. : 1.) : 0.)));
          }
        auto y = o::launch(p, {0, radius, theta, .2}, {-1, .2, -.3}, omega);
        launch_error = std::max(launch_error, o::null_residual(m, y));
        freq_error =
            std::max(freq_error, std::abs(o::frequency(y, f.basis[0]) - 1));
      }
    }
  check("circular tetrad orthonormality", frame_error, 2e-13);
  check("future photon launch nullness", launch_error, 2e-14);
  check("observer launch frequency", freq_error, 2e-14);
  using T = r::Decimal<90>;
  auto decimal = [](double x) {
    std::ostringstream s;
    s << std::setprecision(17) << x;
    return T(s.str());
  };
  double error = 0;
  for (double radius : {.8, 1.51, 1.65, 1.79, 4.}) {
    auto y = o::launch(p, {0, radius, 1.1, .3}, {-1, .1, -.2});
    auto native = o::rhs(p, y);
    r::Parameters<T> rp;
    auto d = r::derivatives(decimal(radius), decimal(y[2]), rp);
    auto inv = r::inverse(d.g);
    std::array<T, 4> v{};
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        v[a] += inv[a][b] * decimal(y[b + 4]);
    std::array<T, 8> ref{};
    for (int i = 0; i < 4; ++i)
      ref[i] = -v[i];
    for (int k : {1, 2})
      for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
          ref[k + 4] -= d.dg[k][a][b] * v[a] * v[b] / T(2);
    for (int i = 0; i < 8; ++i)
      error = std::max(
          error,
          (abs(decimal(native[i]) - ref[i]) / (T(1) + abs(ref[i]))).number());
  }
  check("Hamiltonian RHS vs independent 90-place derivatives", error, 2e-11);
}
void analytic_rays() {
  g::Parameters flat;
  flat.family = g::Family::flat;
  flat.spin = 0;
  o::Settings c;
  c.maximum_affine = 2;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  auto y = o::launch(flat, {0, 8, 1.1, .2}, {-1, .2, .3});
  auto line = o::trace(flat, y, c);
  require("flat ray reaches affine budget",
          line.outcome == o::Outcome::affine_limit);
  auto start = cartesian(y), v = cartesian_velocity(y, o::rhs(flat, y));
  double error = 0;
  for (const auto &point : line.trajectory) {
    auto x = cartesian(point.state);
    for (int i = 0; i < 3; ++i)
      error = std::max(error, std::abs(x[i] - start[i] - point.affine * v[i]));
  }
  check("flat Cartesian straight line", error, 2e-10);
  check("flat coordinate time orientation", std::abs(line.final.state[0] + 2),
        2e-12);
  g::Parameters bh;
  bh.family = g::Family::kerr;
  bh.spin = 0;
  c.maximum_affine = 20;
  c.capture_radius = 2.1;
  auto radial = o::launch(bh, {0, 8, 1.1, 0}, {-1, 0, 0});
  auto hit = o::trace(bh, radial, c);
  require("Schwarzschild exterior capture proxy",
          hit.outcome == o::Outcome::capture_surface);
  check("capture event location", std::abs(hit.final.state[1] - 2.1), 2e-9);
  double exact_time =
      -((8 - hit.final.state[1]) + 2 * std::log(6 / (hit.final.state[1] - 2)));
  check("Schwarzschild radial analytic time",
        std::abs(hit.final.state[0] - exact_time), 2e-8);
  check("Schwarzschild radial affine coordinate",
        std::abs(hit.final.state[1] - (8 + radial[4] * hit.final.affine)),
        2e-9);
  auto emission = o::disk_emission(bh, hit.final.state, 1, 2.1);
  check("Schwarzschild gravitational frequency shift",
        std::abs(emission.redshift -
                 std::sqrt((1 - 2 / hit.final.state[1]) / (1 - 2. / 8))),
        2e-10);
  auto outgoing = o::trace(bh, o::launch(bh, {0, 8, 1.1, 0}, {1, 0, 0}), c);
  require("radial escape", outgoing.outcome == o::Outcome::escaped);
  check("escape event location", std::abs(outgoing.final.state[1] - 10), 2e-9);
  c.maximum_affine = 5;
  auto orbit = o::trace(
      bh, o::launch(bh, {0, 3, std::numbers::pi / 2, 0}, {0, 0, 1}), c);
  require("circular null orbit remains unresolved at budget",
          orbit.outcome == o::Outcome::affine_limit);
  check("Schwarzschild photon orbit radius", std::abs(orbit.final.state[1] - 3),
        2e-9);
  check("Schwarzschild photon orbit azimuth",
        std::abs(orbit.final.state[3] - 5. / 3), 2e-9);
  check("Schwarzschild photon orbit time",
        std::abs(orbit.final.state[0] + 5 * std::sqrt(3.)), 2e-9);
  bh.spin = .875;
  c.maximum_affine = 3;
  c.capture_radius = 1.6;
  auto initial = o::launch(bh, {0, 6, 1.1, .1}, {-1, .1, -.5});
  auto kerr = o::trace(bh, initial, c);
  require("Kerr reference ray reaches budget",
          kerr.outcome == o::Outcome::affine_limit);
  double qe = 0;
  for (const auto &q : kerr.trajectory)
    qe = std::max(qe, std::abs(carter(bh, q.state) - carter(bh, initial)) /
                          (1 + std::abs(carter(bh, initial))));
  check("exterior Kerr Carter conservation", qe, 2e-9);
}
void boundaries_and_disk() {
  g::Parameters flat;
  flat.family = g::Family::flat;
  flat.spin = 0;
  o::Settings c;
  c.maximum_affine = 20;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  auto central = o::trace(flat, o::launch(flat, {0, 8, 1.1, 0}, {-1, 0, 0}), c);
  require("central boundary is a domain limit",
          central.outcome == o::Outcome::domain_limit);
  check("central boundary refined", std::abs(central.final.state[1] - .25),
        2e-9);
  auto axis = o::trace(flat, o::launch(flat, {0, 8, .1, 0}, {0, -1, 0}), c);
  require("axis boundary is a domain limit",
          axis.outcome == o::Outcome::domain_limit);
  check("axis boundary refined", std::abs(axis.final.state[2] - .05), 2e-9);
  auto initial = o::launch(flat, {0, 8, 1.1, 0}, {-1, .1, .2});
  c.maximum_attempts = 1;
  require("step exhaustion distinct",
          o::trace(flat, initial, c).outcome == o::Outcome::step_limit);
  c.maximum_attempts = 100000;
  auto zero = initial;
  for (int i = 4; i < 8; ++i)
    zero[i] = 0;
  require("zero photon rejected",
          o::trace(flat, zero, c).outcome == o::Outcome::invalid_input);
  auto past = initial;
  for (int i = 4; i < 8; ++i)
    past[i] = -past[i];
  require("past momentum convention rejected",
          o::trace(flat, past, c).outcome == o::Outcome::invalid_input);
  c.capture_radius = 2.;
  require("flat artificial capture rejected",
          o::trace(flat, initial, c).outcome == o::Outcome::invalid_input);
  c.capture_radius.reset();
  c.relative_tolerance = 0;
  require("invalid tolerance rejected",
          o::trace(flat, initial, c).outcome == o::Outcome::invalid_input);
  c.relative_tolerance = 1e-12;
  bool rejected = false;
  try {
    (void)o::launch(flat, {0, 8, 1, 0}, {0, 0, 0});
  } catch (const std::exception &) {
    rejected = true;
  }
  require("zero sight rejected", rejected);
  rejected = false;
  try {
    (void)o::launch(flat, {0, 8, 1, 0}, {1, 0, 0}, 1.);
  } catch (const std::exception &) {
    rejected = true;
  }
  require("spacelike circular observer rejected", rejected);
  double t = std::numbers::pi / 3, x = 8 * std::sin(t), z = 8 * std::cos(t),
         dx = 4 - x, dz = -z;
  auto disk_ray = o::launch(flat, {0, 8, t, 0},
                            {dx * std::sin(t) + dz * std::cos(t),
                             dx * std::cos(t) - dz * std::sin(t), 0});
  c.disk = true;
  c.event_tolerance = 1e-10;
  auto disk = o::trace(flat, disk_ray, c);
  require("transverse disk event", disk.outcome == o::Outcome::emitted);
  check("flat disk analytic hit radius", std::abs(disk.final.state[1] - 4),
        2e-9);
  check("flat disk analytic travel time",
        std::abs(disk.final.affine - std::hypot(dx, dz)), 2e-9);
  check("disk event plane",
        std::abs(disk.final.state[2] - std::numbers::pi / 2), 2e-10);
  auto e = o::disk_emission(flat, disk.final.state, 1, 3);
  check("flat disk redshift", std::abs(e.redshift - 1), 2e-13);
  check("linear bolometric transfer",
        std::abs(e.linear_intensity - std::pow(.75, 3)), 2e-9);
}
void toy_convergence() {
  g::Parameters p;
  o::Settings c;
  c.maximum_affine = 30;
  c.absolute_tolerance = c.relative_tolerance = 1e-11;
  auto y = o::launch(p, {0, 4, std::numbers::pi / 2, 0}, {-1, 0, -.35});
  auto through = o::trace(p, y, c);
  std::cout << "toy outcome " << o::outcome_name(through.outcome) << " joins "
            << through.crossings.size() << " detail " << through.detail << '\n';
  require("toy shell/core traversal escapes",
          through.outcome == o::Outcome::escaped);
  std::size_t inner = 0, outer = 0;
  double minr = 4;
  for (const auto &x : through.crossings) {
    inner += x.surface == "inner_shell_join";
    outer += x.surface == "outer_shell_join";
  }
  for (const auto &x : through.trajectory)
    minr = std::min(minr, x.state[1]);
  require("both joins crossed inward and outward",
          inner == 2 && outer == 2 && minr < p.r_in);
  check("toy maximum null residual", through.maximum_null_error, 2e-8);
  check("structural energy conservation", through.energy_error, 0);
  check("structural angular momentum conservation",
        through.angular_momentum_error, 0);
  c.maximum_affine = 4;
  auto target = o::launch(p, {0, 2.5, std::numbers::pi / 2, 0}, {-1, 0, -.35});
  auto rk_coarse = rk4(p, target, 4, .002), rk_fine = rk4(p, target, 4, .001);
  auto tight = o::trace(p, target, c);
  require("convergence target remains inside chart",
          tight.outcome == o::Outcome::affine_limit);
  check("RK4 refinement endpoint", difference(rk_coarse, rk_fine), 1e-7);
  check("Dormand-Prince vs independently stepped RK4",
        difference(tight.final.state, rk_fine), 1e-7);
  c.absolute_tolerance = c.relative_tolerance = 1e-6;
  c.null_tolerance =
      1e-3; // Deliberately loose run, never an accepted image preset.
  c.maximum_step = .4;
  auto coarse = o::trace(p, target, c);
  c.absolute_tolerance = c.relative_tolerance = 1e-8;
  auto medium = o::trace(p, target, c);
  double ec = difference(coarse.final.state, tight.final.state),
         em = difference(medium.final.state, tight.final.state);
  std::cout << "toy refinement " << ec << " -> " << em << '\n';
  require("tolerance refinement reduces endpoint error",
          coarse.outcome == o::Outcome::affine_limit &&
              medium.outcome == o::Outcome::affine_limit && em < ec / 5);
  check("medium tolerance endpoint error", em, 2e-6);
  o::Settings event_settings;
  event_settings.disk = true;
  const double tangent = std::tan(25 * std::numbers::pi / 180);
  auto event_initial = o::launch(p, {0, 8, std::numbers::pi / 3, 0},
                                 {-1, -tangent / 12, tangent / 4});
  auto event_retry = o::trace(p, event_initial, event_settings);
  require("image event substep rejection recovers",
          event_retry.outcome == o::Outcome::escaped ||
              event_retry.outcome == o::Outcome::emitted ||
              event_retry.outcome == o::Outcome::domain_limit);
  check("event retry null constraint", event_retry.maximum_null_error, 1e-6);
}
int main(int argc, char **argv) {
  try {
    frames_and_reference();
    analytic_rays();
    boundaries_and_disk();
    toy_convergence();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    require("uncaught exception", false);
  }
  bool pass = true;
  for (const auto &c : checks)
    pass = pass && c["passed"].get<bool>();
  J report{{"schema_version", 1},
           {"passed", pass},
           {"checks", checks},
           {"compiler", __VERSION__},
           {"cplusplus", __cplusplus},
           {"scope", "bounded native P2 foundation; complete interior optics "
                     "and GYOTO not accepted"}};
  if (argc == 3 && std::string(argv[1]) == "--report") {
    auto root = std::filesystem::path(GRAVASTAR_SOURCE_ROOT);
    auto output = u::result_path(root, argv[2]);
    auto manifest = u::manifest(root);
    report["computational_source_sha256"] = manifest["computational_sha256"];
    if (!pass)
      output.replace_filename(output.stem().string() + ".failed.json");
    u::atomic_json(output, report);
  } else if (argc != 1) {
    std::cerr << "usage: gravastar_optics_tests [--report results/FILE.json]\n";
    return 2;
  }
  std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
            << " optics checks\n";
  return pass ? 0 : 1;
}
