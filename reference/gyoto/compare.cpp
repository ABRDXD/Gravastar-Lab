// SPDX-License-Identifier: GPL-3.0-or-later
// Optional GYOTO comparison. The linked executable includes GPL GYOTO 2.0.2
// (pinned in reference/gyoto/CMakeLists.txt); see docs/GYOTO_FEASIBILITY.md.
//
// 1. Kerr metric and Christoffel symbols: GYOTO KerrBL vs native geometry.
// 2. GYOTO's generic RK4 on an exact flat null line.
// 3. Kerr null rays: native adaptive tracer vs GYOTO's own fixed-step
//    integrators (generic geodesic equation with GYOTO's Christoffels, and
//    the KerrBL-specific constants-of-motion scheme). Compared where each
//    ray crosses a fixed radius: affine parameter, t, theta, phi.
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/optics/ray.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <GyotoError.h>
#include <GyotoKerrBL.h>
#include <GyotoMinkowski.h>
#include <GyotoPhoton.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <optional>

namespace g = gravastar::geometry;
namespace o = gravastar::optics;
namespace u = gravastar::tools;
namespace {
u::Json checks = u::Json::array();
void check(const std::string &name, double error, double ceiling) {
  const bool pass = std::isfinite(error) && error <= ceiling;
  checks.push_back({{"name", name}, {"measured", error},
                    {"ceiling", ceiling}, {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << error << " > " << ceiling << '\n';
}
void require(const std::string &name, bool pass) { check(name, pass ? 0 : 1, 0); }
double relative_error(double a, double b) {
  if (!std::isfinite(a) || !std::isfinite(b))
    throw std::runtime_error("non-finite reference component");
  return std::abs(a - b) / std::max({1., std::abs(a), std::abs(b)});
}

void tensors() {
  double metric_error = 0, connection_error = 0;
  for (double spin : {-.875, 0., .875}) {
    Gyoto::Metric::KerrBL external;
    external.spin(spin);
    g::Parameters p;
    p.family = g::Family::kerr;
    p.mass = 1;
    p.spin = spin;
    for (double r : {2.5, 5., 12.})
      for (double theta : {.3, .9, std::numbers::pi / 2, 2.7}) {
        const double x[4]{0, r, theta, .2};
        double metric[4][4]{}, connection[4][4][4]{};
        external.gmunu(metric, x);
        if (external.christoffel(connection, x))
          throw std::runtime_error("GYOTO Kerr Christoffel evaluation failed");
        const auto result = g::evaluate(p, r, theta);
        if (const auto *error = std::get_if<g::Error>(&result))
          throw std::runtime_error(error->message);
        const auto &native = std::get<g::Evaluation>(result);
        for (int a = 0; a < 4; ++a)
          for (int b = 0; b < 4; ++b) {
            metric_error = std::max(
                metric_error, relative_error(metric[a][b], native.metric[a][b]));
            for (int c = 0; c < 4; ++c) {
              double gamma = 0;
              for (int d = 0; d < 4; ++d)
                gamma += .5 * native.inverse[a][d] *
                         (native.dg[b][d][c] + native.dg[c][d][b] -
                          native.dg[d][b][c]);
              connection_error = std::max(
                  connection_error, relative_error(connection[a][b][c], gamma));
            }
          }
      }
  }
  check("GYOTO Kerr metric components vs native (36 points)", metric_error,
        3e-13);
  check("GYOTO Kerr Christoffel symbols vs native (36 points)",
        connection_error, 3e-12);
}

void flat_line() {
  Gyoto::Metric::Minkowski flat;
  flat.spherical(false);
  Gyoto::Photon photon; // mass 0
  // GYOTO states hold x^mu and dx^mu/dlambda; its generic diff requires
  // dt/dlambda > 0. Null: -1 + 0.36 + 0.64 = 0.
  Gyoto::state_t state{0, 2, 3, 4, 1, .6, 0, .8};
  const auto initial = state;
  constexpr int count = 100;
  constexpr double step = .01;
  for (int i = 0; i < count; ++i) {
    Gyoto::state_t next(8);
    if (flat.Gyoto::Metric::Generic::myrk4(&photon, state, step, next))
      throw std::runtime_error("GYOTO flat RK4 failed");
    state = std::move(next);
  }
  double error = 0;
  for (int i = 0; i < 8; ++i) {
    const double expected =
        initial[i] + (i < 4 ? count * step * initial[i + 4] : 0);
    error = std::max(error, relative_error(state[i], expected));
  }
  check("GYOTO generic RK4 vs exact flat null line", error, 1e-13);
}

// Position at a radius crossing, parameterized by the native past-directed
// affine parameter s (GYOTO's lambda = -s).
struct Crossing {
  double s, t, theta, phi;
};

struct GyotoRun {
  std::optional<Crossing> crossing;
  std::size_t points{};
  double maximum_norm{};
};

// GYOTO fixed-step integration from the native launch state. Stored point
// k has lambda = (k - i0) * h. The crossing is located on the cubic Hermite
// interpolant built from stored positions and velocities.
GyotoRun gyoto_ray(Gyoto::SmartPointer<Gyoto::Metric::Generic> metric,
                   const o::State &launch, double h, double target,
                   bool outward, double t_limit) {
  double gup[4][4]{};
  metric->gmunu_up(gup, launch.data());
  double coord[8]{launch[0], launch[1], launch[2], launch[3]};
  for (int mu = 0; mu < 4; ++mu)
    for (int nu = 0; nu < 4; ++nu)
      coord[4 + mu] += gup[mu][nu] * launch[4 + nu];
  Gyoto::Photon photon;
  photon.metric(metric);
  photon.integrator("Legacy");
  photon.adaptive(false);
  photon.delta(h);
  photon.maxiter(100000000);
  photon.setInitCoord(coord, -1);
  photon.xFill(t_limit);

  GyotoRun out;
  const std::size_t i0 = photon.getI0(), imin = photon.getImin();
  out.points = i0 - imin + 1;
  Gyoto::state_t a(8), b(8);
  photon.getCoord(i0, a);
  for (std::size_t k = i0; k-- > imin;) {
    photon.getCoord(k, b);
    out.maximum_norm = std::max(
        out.maximum_norm,
        std::abs(metric->ScalarProd(b.data(), b.data() + 4, b.data() + 4)) /
            (b[4] * b[4]));
    const bool crossed = outward ? (a[1] < target && b[1] >= target)
                                 : (a[1] > target && b[1] <= target);
    if (crossed) {
      // s increases from a to b by h; dX/ds = -dX/dlambda.
      const double s0 = double(i0 - (k + 1)) * h;
      auto hermite = [&](int i, double tau) {
        const double x = tau / h, x2 = x * x, x3 = x2 * x;
        return (2 * x3 - 3 * x2 + 1) * a[i] + (x3 - 2 * x2 + x) * h * -a[i + 4] +
               (-2 * x3 + 3 * x2) * b[i] + (x3 - x2) * h * -b[i + 4];
      };
      double lo = 0, hi = h;
      for (int it = 0; it < 200 && hi - lo > 1e-17; ++it) {
        const double mid = (lo + hi) / 2;
        ((hermite(1, mid) < target) == (a[1] < target) ? lo : hi) = mid;
      }
      const double tau = (lo + hi) / 2;
      out.crossing = Crossing{s0 + tau, hermite(0, tau), hermite(2, tau),
                              hermite(3, tau)};
      return out;
    }
    a = b;
  }
  return out;
}

Gyoto::SmartPointer<Gyoto::Metric::Generic> kerr_metric(double spin,
                                                        bool generic) {
  Gyoto::SmartPointer<Gyoto::Metric::KerrBL> kerr(new Gyoto::Metric::KerrBL());
  kerr->spin(spin);
  kerr->genericIntegrator(generic);
  return Gyoto::SmartPointer<Gyoto::Metric::Generic>(kerr);
}

// Gravastar adapter: the production toy geometry exposed to GYOTO as a
// spherical-coordinate metric. GYOTO's own integrator, geodesic equation and
// storage then trace rays through it. This shares the metric with the native
// tracer, so it checks integration and event handling, not the metric (the
// metric has its own independent references in tests/).
class ToyGravastar : public Gyoto::Metric::Generic {
public:
  explicit ToyGravastar(const g::Parameters &p)
      : Gyoto::Metric::Generic(GYOTO_COORDKIND_SPHERICAL, "GravastarLabToy"),
        p_(p) {}
  ToyGravastar *clone() const override { return new ToyGravastar(*this); }
  using Gyoto::Metric::Generic::christoffel;
  using Gyoto::Metric::Generic::gmunu;
  using Gyoto::Metric::Generic::gmunu_up;
  void gmunu(double out[4][4], const double x[4]) const override {
    const auto e = evaluate(x);
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        out[a][b] = e.metric[a][b];
  }
  void gmunu_up(double out[4][4], const double x[4]) const override {
    const auto e = evaluate(x);
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        out[a][b] = e.inverse[a][b];
  }
  int christoffel(double dst[4][4][4], const double x[4]) const override {
    const auto e = evaluate(x);
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        for (int c = 0; c < 4; ++c) {
          double gamma = 0;
          for (int d = 0; d < 4; ++d)
            gamma += .5 * e.inverse[a][d] *
                     (e.dg[b][d][c] + e.dg[c][d][b] - e.dg[d][b][c]);
          dst[a][b][c] = gamma;
        }
    return 0;
  }

private:
  g::Evaluation evaluate(const double x[4]) const {
    auto result = g::evaluate(p_, x[1], x[2]);
    if (const auto *error = std::get_if<g::Error>(&result))
      GYOTO_ERROR(error->message.c_str());
    return std::get<g::Evaluation>(result);
  }
  g::Parameters p_;
};

struct Ray {
  const char *label;
  double spin, theta, p_theta, p_phi;
  bool captured; // compared at an inward r = 2.5 crossing instead of r = 20
};

o::State native_launch(const g::Parameters &p, const Ray &ray) {
  o::State y{0, 10, ray.theta, 0, -1, 0, ray.p_theta, ray.p_phi};
  const auto &gi = o::metric(p, y).inverse;
  double rest = gi[0][0] * y[4] * y[4] + 2 * gi[0][3] * y[4] * y[7] +
                gi[2][2] * y[6] * y[6] + gi[3][3] * y[7] * y[7];
  if (!(rest < 0))
    throw std::runtime_error("launch is not an inward null ray");
  // Future-directed outgoing p_r: the past-directed trace moves inward.
  y[5] = std::sqrt(-rest / gi[1][1]);
  return y;
}

u::Json rays() {
  const Ray fixtures[] = {
      {"equatorial prograde b=4.5", .875, std::numbers::pi / 2, 0, 4.5, false},
      {"equatorial retrograde b=-8", .875, std::numbers::pi / 2, 0, -8, false},
      {"near-critical prograde b=2.96", .875, std::numbers::pi / 2, 0, 2.96,
       false},
      {"off-equatorial north", .875, 1.0, 5, 3, false},
      {"off-equatorial south, retrograde", .875, 2.2, -5, -4, false},
      {"off-equatorial north, spin -0.875", -.875, 1.0, 5, 3, false},
      {"off-equatorial captured", .875, 1.0, 3, 2, true},
      {"captured, equatorial b=1", .875, std::numbers::pi / 2, 0, 1, true}};
  u::Json rows = u::Json::array();
  for (const auto &ray : fixtures) {
    const std::string label = ray.label;
    g::Parameters p;
    p.family = g::Family::kerr;
    p.mass = 1;
    p.spin = ray.spin;
    const auto launch = native_launch(p, ray);
    const double target = ray.captured ? 2.5 : 20;

    o::Settings c;
    c.absolute_tolerance = c.relative_tolerance = 1e-12;
    c.maximum_step = .05;
    c.event_tolerance = 1e-12;
    c.null_tolerance = 1e-8;
    c.outer_radius = 20;
    c.maximum_affine = 400;
    c.capture_radius = ray.captured ? 2.5 : 1.55;
    const auto native = o::trace(p, launch, c);
    const auto want =
        ray.captured ? o::Outcome::capture_surface : o::Outcome::escaped;
    require(label + " native outcome", native.outcome == want);
    const Crossing mine{native.final.affine, native.final.state[0],
                        native.final.state[2], native.final.state[3]};
    const double t_limit = mine.t - 10;

    const auto coarse = gyoto_ray(kerr_metric(ray.spin, true), launch, 2e-3,
                                  target, !ray.captured, t_limit);
    const auto fine = gyoto_ray(kerr_metric(ray.spin, true), launch, 1e-3,
                                target, !ray.captured, t_limit);
    const auto specific = gyoto_ray(kerr_metric(ray.spin, false), launch, 1e-3,
                                    target, !ray.captured, t_limit);
    require(label + " GYOTO generic crosses the target radius",
            coarse.crossing && fine.crossing);
    require(label + " GYOTO KerrBL-specific crosses the target radius",
            specific.crossing.has_value());
    if (!coarse.crossing || !fine.crossing || !specific.crossing)
      continue;
    auto difference = [](const Crossing &x, const Crossing &y) {
      return std::max({std::abs(x.s - y.s), std::abs(x.t - y.t),
                       std::abs(x.theta - y.theta), std::abs(x.phi - y.phi)});
    };
    const double refinement = difference(*coarse.crossing, *fine.crossing);
    const double native_error = difference(mine, *fine.crossing);
    const double specific_error = difference(*specific.crossing, *fine.crossing);
    check(label + " GYOTO generic step-halving change", refinement, 5e-10);
    check(label + " GYOTO generic null drift (norm/tdot^2)", fine.maximum_norm,
          1e-9);
    check(label + " native vs GYOTO generic at crossing", native_error, 5e-10);
    check(label + " GYOTO KerrBL-specific vs generic at crossing",
          specific_error, 5e-10);
    auto json = [](const Crossing &x) {
      return u::Json{{"s", x.s}, {"t", x.t}, {"theta", x.theta}, {"phi", x.phi}};
    };
    rows.push_back({{"label", label},
                    {"spin", ray.spin},
                    {"theta", ray.theta},
                    {"p_theta", ray.p_theta},
                    {"p_phi", ray.p_phi},
                    {"target_radius", target},
                    {"native_outcome", o::outcome_name(native.outcome)},
                    {"native", json(mine)},
                    {"gyoto_generic_h2e-3", json(*coarse.crossing)},
                    {"gyoto_generic_h1e-3", json(*fine.crossing)},
                    {"gyoto_specific_h1e-3", json(*specific.crossing)},
                    {"gyoto_step_halving_change", refinement},
                    {"native_minus_gyoto", native_error},
                    {"specific_minus_generic", specific_error},
                    {"gyoto_points", fine.points}});
  }
  return rows;
}

struct ToyRay {
  const char *label;
  double theta, p_theta, p_phi;
};
u::Json toy_rays() {
  g::Parameters p; // toy preset
  const ToyRay fixtures[] = {
      {"equatorial b=1.8, turns in core", std::numbers::pi / 2, 0, 1.8},
      {"equatorial b=-3.6, turns in core", std::numbers::pi / 2, 0, -3.6},
      {"equatorial b=2.9537, buried Kerr photon orbit", std::numbers::pi / 2,
       0, 2.9537},
      {"off-equatorial, shell only", 1.0, 3, 2},
      {"off-equatorial, core r_min 0.875", 1.0, 1.0, .5},
      {"off-equatorial prograde, core r_min 0.48", 1.0, .6, .5},
      {"off-equatorial retrograde, core r_min 0.47", 1.0, .8, -.3}};
  u::Json rows = u::Json::array();
  for (const auto &ray : fixtures) {
    const std::string label = std::string("toy ") + ray.label;
    const auto launch =
        native_launch(p, {ray.label, 0, ray.theta, ray.p_theta, ray.p_phi, false});
    o::Settings c;
    c.absolute_tolerance = c.relative_tolerance = 1e-12;
    c.maximum_step = .02;
    c.event_tolerance = 1e-12;
    c.null_tolerance = 1e-8;
    c.outer_radius = 20;
    c.maximum_affine = 400;
    const auto native = o::trace(p, launch, c);
    require(label + " native escapes", native.outcome == o::Outcome::escaped);
    const Crossing mine{native.final.affine, native.final.state[0],
                        native.final.state[2], native.final.state[3]};
    Gyoto::SmartPointer<Gyoto::Metric::Generic> metric(new ToyGravastar(p));
    const auto coarse =
        gyoto_ray(metric, launch, 5e-4, 20, true, mine.t - 10);
    const auto fine =
        gyoto_ray(metric, launch, 2.5e-4, 20, true, mine.t - 10);
    require(label + " GYOTO crosses r=20", coarse.crossing && fine.crossing);
    if (!coarse.crossing || !fine.crossing)
      continue;
    auto difference = [](const Crossing &x, const Crossing &y) {
      return std::max({std::abs(x.s - y.s), std::abs(x.t - y.t),
                       std::abs(x.theta - y.theta), std::abs(x.phi - y.phi)});
    };
    const double refinement = difference(*coarse.crossing, *fine.crossing);
    const double native_error = difference(mine, *fine.crossing);
    // RK4: the h=2.5e-4 error is about 1/15 of this change.
    check(label + " GYOTO step-halving change", refinement, 1e-7);
    check(label + " native vs GYOTO at r=20", native_error, 1e-8);
    check(label + " GYOTO null drift (norm/tdot^2)", fine.maximum_norm, 1e-9);
    rows.push_back({{"label", label},
                    {"theta", ray.theta},
                    {"p_theta", ray.p_theta},
                    {"p_phi", ray.p_phi},
                    {"native_s", mine.s},
                    {"native_phi", mine.phi},
                    {"gyoto_step_halving_change", refinement},
                    {"native_minus_gyoto", native_error},
                    {"gyoto_null_drift", fine.maximum_norm},
                    {"gyoto_points", fine.points}});
  }
  return rows;
}

u::Json run() {
  tensors();
  flat_line();
  auto rows = rays();
  auto toy = toy_rays();
  bool passed = true;
  for (const auto &entry : checks)
    passed = passed && entry.at("passed").get<bool>();
  return {{"schema_version", 1},
          {"passed", passed},
          {"checks", checks},
          {"kerr_rays", rows},
          {"toy_rays", toy},
          {"complete_P2_acceptance", false},
          {"scope",
           "GYOTO 2.0.2 core built as C++20 from pinned sources: Kerr "
           "tensors, flat RK4, and Kerr null rays against GYOTO's own "
           "fixed-step Legacy integrators. No toy-gravastar adapter yet."},
          {"gyoto_revision", GRAVASTAR_GYOTO_REVISION},
          {"eigen_revision", GRAVASTAR_EIGEN_REVISION},
          {"boost_version", "1.84.0"},
          {"boost_archive_sha256", GRAVASTAR_BOOST_SHA256},
          {"compiler", __VERSION__},
          {"cplusplus", __cplusplus},
          {"computational_source_sha256",
           u::manifest(GRAVASTAR_SOURCE_ROOT)["computational_sha256"]}};
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--report"))
      throw std::invalid_argument(
          "usage: gravastar_gyoto_compare [--report results/NAME.json]");
    auto report = run();
    if (argc == 3)
      u::atomic_json(u::result_path(GRAVASTAR_SOURCE_ROOT, argv[2]), report);
    std::cout << report["kerr_rays"].dump(2) << '\n'
              << (report.at("passed").get<bool>() ? "PASS" : "FAIL")
              << " GYOTO reference: " << checks.size() << " checks\n";
    return report.at("passed").get<bool>() ? 0 : 1;
  } catch (const Gyoto::Error &error) {
    std::cerr << "GYOTO error: " << error.get_message() << '\n';
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
  }
  return 1;
}
