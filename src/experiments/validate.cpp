// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/reference/geometry.hpp"
#include "gravastar/reference/polynomial.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <chrono>
#include <iostream>
#include <limits>
#include <numbers>
#include <set>
#include <sys/utsname.h>

namespace ref = gravastar::reference;
namespace prod = gravastar::geometry;
namespace util = gravastar::tools;
using J = util::Json;
using T = ref::Decimal<90>;
using P = ref::Parameters<T>;
using F = ref::Family;
using B = ref::Branch;
T decimal(double x) {
  std::ostringstream o;
  o << std::setprecision(17) << x;
  return T(o.str());
}
double err(double a, const T &b) {
  return (abs(decimal(a) - b) / (T(1) + abs(b))).number();
}
double err(const T &a, const T &b) {
  return (abs(a - b) / (T(1) + abs(b))).number();
}
template <class A, class C, std::size_t N>
double err(const std::array<A, N> &a, const std::array<C, N> &b) {
  double e = 0;
  for (std::size_t i = 0; i < N; ++i)
    e = std::max(e, err(a[i], b[i]));
  return e;
}
template <class X> J numbers(const X &x) { return x.number(); }
template <class X, std::size_t N> J numbers(const std::array<X, N> &a) {
  J out = J::array();
  for (const auto &x : a)
    out.push_back(numbers(x));
  return out;
}
struct Checks {
  J rows = J::array();
  void truth(const std::string &name, bool pass, J details = J::object()) {
    details["name"] = name;
    details["passed"] = pass;
    rows.push_back(details);
  }
  void upper(const std::string &name, double e, double limit) {
    truth(name, std::isfinite(e) && e <= limit,
          {{"measured", e}, {"threshold", limit}, {"comparison", "<="}});
  }
  bool passed() const {
    for (const auto &r : rows)
      if (!r.at("passed").get<bool>())
        return false;
    return !rows.empty();
  }
};
prod::Evaluation evaluate(const P &p, T r, T th, F family = F::toy,
                          prod::Region region = prod::Region::automatic) {
  prod::Parameters q{static_cast<prod::Family>(family),
                     p.m.number(),
                     p.a.number(),
                     p.L.number(),
                     p.ri.number(),
                     p.ro.number()};
  auto value = prod::evaluate(q, r.number(), th.number(), region);
  if (auto *e = std::get_if<prod::Error>(&value))
    throw std::runtime_error(e->code + ": " + e->message);
  return std::get<prod::Evaluation>(value);
}

J radial(const P &p, Checks &checks) {
  using ref::Rational;
  auto rational = [](T x) { return Rational::decimal(x.str()); };
  auto m = rational(p.m), a = rational(p.a), L = rational(p.L),
       ri = rational(p.ri), ro = rational(p.ro);
  auto poly = ref::shell_delta(m, a, L, ri, ro);
  int count = ref::count_roots(poly);
  auto bound = ref::positive_bounds(poly);
  checks.truth("exact shell radial root count", count == 0, {{"roots", count}});
  checks.truth("exact shell positive Bernstein certificate", bound.has_value());
  auto core = Rational(1) - L * ri * ri / Rational(3),
       exterior = ro * ro - Rational(2) * m * ro + a * a;
  checks.truth("core and exterior analytic positivity",
               core > Rational(0) && ro > m && exterior > Rational(0));
  J critical = J::array();
  for (const auto &[lo, hi] : ref::isolate_roots(ref::derivative(poly))) {
    auto mid = (lo + hi) / Rational(2);
    critical.push_back({{"u_bracket", {lo.str(), hi.str()}},
                        {"radius", (ri + (ro - ri) * mid).number()},
                        {"Delta", ref::value(poly, mid).number()}});
  }
  J result{
      {"shell_roots", count},
      {"stationary_points", critical},
      {"core_factor_lower_bound", core.str()},
      {"exterior_delta_minimum", exterior.str()},
      {"scope",
       "fixed preset; radial null surfaces, not a global event-horizon proof"}};
  if (bound) {
    result["shell_delta_lower_bound_exact"] = bound->lower.str();
    result["shell_delta_lower_bound"] = bound->lower.number();
    result["shell_delta_upper_bound"] = bound->upper.number();
    result["subintervals"] = bound->leaves;
    result["maximum_depth"] = bound->depth;
  }
  return result;
}
J domain(const P &p, const J &cfg, Checks &checks) {
  auto box = cfg.at("domain");
  int nr = box.at("radial_samples"), nt = box.at("angular_samples");
  T lo(box.at("r_min").get<std::string>()),
      hi(box.at("r_max").get<std::string>()),
      tmin(box.at("theta_min").get<std::string>());
  if (nr < 2 || nt < 2 || nr > 1000 || nt > 1000 || lo <= T(0) || hi <= lo ||
      tmin <= T(0) || tmin >= ref::pi<T>() / T(2))
    throw std::invalid_argument("invalid bounded grid");
  std::set<T> radii;
  for (int i = 0; i < nr; ++i)
    radii.insert(lo + (hi - lo) * T(i) / T(nr - 1));
  for (int i = 0; i <= 80; ++i)
    radii.insert(p.ri + (p.ro - p.ri) * T(i) / T(80));
  for (T join : {p.ri, p.ro})
    for (int sign : {-1, 0, 1})
      radii.insert(join + T(sign) * T("1e-7"));
  double mineig = 1e300, condition = 0, minphi = 1e300, minlapse = 1e300,
         identity = 0, det = 0;
  double minR = 1e300, maxR = -1e300, minK = 1e300, maxK = -1e300;
  std::size_t samples = 0, failures = 0, ergoregion = 0;
  for (const T &r : radii)
    for (int j = 0; j < nt; ++j) {
      T theta = tmin + (ref::pi<T>() - T(2) * tmin) * T(j) / T(nt - 1);
      auto e = evaluate(p, r, theta);
      auto &g = e.metric;
      double tt = g[0][0], tp = g[0][3], pp = g[3][3];
      double positive = (tt + pp + std::hypot(tt - pp, 2 * tp)) / 2;
      double negative = (tt * pp - tp * tp) / positive;
      std::array<double, 4> eig{negative, positive, g[1][1], g[2][2]};
      bool good =
          negative < 0 && positive > 0 && g[1][1] > 0 && g[2][2] > 0 && pp > 0;
      double lapse = (tp * tp - tt * pp) / pp;
      if (!good || lapse <= 0)
        ++failures;
      if (tt > 0)
        ++ergoregion;
      double low = 1e300, high = 0;
      for (double x : eig) {
        low = std::min(low, std::abs(x));
        high = std::max(high, std::abs(x));
      }
      mineig = std::min(mineig, low);
      condition = std::max(condition, high / low);
      minphi = std::min(minphi, pp);
      minlapse = std::min(minlapse, lapse);
      for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b) {
          double v = 0;
          for (int k = 0; k < 4; ++k)
            v += g[a][k] * e.inverse[k][b];
          identity = std::max(identity, std::abs(v - (a == b ? 1 : 0)));
        }
      auto [s, c] = ref::sincos(theta);
      auto [mu, lam] = ref::transition(r, p, F::toy, B::automatic);
      (void)mu;
      T sigma = r * r + p.a * p.a * c * c,
        xi = T(1) + p.L * lam * p.a * p.a / T(3);
      det = std::max(
          det, err(e.determinant, -sigma * sigma * s * s / ref::power(xi, 4)));
      minR = std::min(minR, e.ricci_scalar);
      maxR = std::max(maxR, e.ricci_scalar);
      minK = std::min(minK, e.kretschmann);
      maxK = std::max(maxK, e.kretschmann);
      ++samples;
    }
  checks.truth("sampled Lorentzian inertia, azimuthal causality and ZAMO",
               failures == 0, {{"failures", failures}});
  checks.upper("sampled inverse identity absolute", identity,
               cfg["thresholds"]["identity"]);
  checks.truth(
      "coordinate conditioning margins",
      mineig >=
              cfg["thresholds"]["minimum_eigenvalue_magnitude"].get<double>() &&
          condition <=
              cfg["thresholds"]["maximum_coordinate_condition"].get<double>(),
      {{"minimum_eigenvalue_magnitude", mineig},
       {"maximum_condition", condition}});
  checks.upper("sampled determinant identity", det,
               cfg["thresholds"]["identity"]);
  return {{"samples", samples},
          {"box", box},
          {"minimum_absolute_coordinate_eigenvalue", mineig},
          {"maximum_coordinate_condition", condition},
          {"minimum_g_phiphi", minphi},
          {"minimum_zamo_lapse_squared", minlapse},
          {"ergoregion_sample_count", ergoregion},
          {"ricci_scalar_range", {minR, maxR}},
          {"kretschmann_range", {minK, maxK}}};
}
J independent(const P &p, const J &cfg, Checks &checks) {
  struct Case {
    P p;
    T r, th;
    F family;
  };
  std::vector<Case> cases;
  std::vector<T> radii{
      T(".25"),          T(1), p.ri - T(".0001"), p.ri, p.ri + T(".0001"),
      p.ro - T(".0001"), p.ro, p.ro + T(".0001"), T(3), T(10)};
  for (T u : {T(".1"), T(".25"), T(".5"), T(".75"), T(".9")})
    radii.push_back(p.ri + (p.ro - p.ri) * u);
  for (T r : radii)
    for (T th : {T(".05"), ref::pi<T>() / T(2), ref::pi<T>() - T(".05")})
      cases.push_back({p, r, th, F::toy});
  cases.push_back({p, T(".25"), T("1.1"), F::flat});
  cases.push_back({p, T(1), T("1.1"), F::de_sitter});
  cases.push_back({p, T(3), T("1.1"), F::kerr});
  P zero = p;
  zero.a = T(0);
  cases.push_back({zero, T(3), T("1.1"), F::kerr});
  cases.push_back({zero, (p.ri + p.ro) / T(2), T("1.1"), F::toy});
  P reversed = p;
  reversed.a = -p.a;
  cases.push_back({reversed, (p.ri + p.ro) / T(2), T("1.1"), F::toy});
  std::map<std::string, double> maxima{{"metric", 0}, {"inverse", 0},
                                       {"dg", 0},     {"ddg", 0},
                                       {"drrr", 0},   {"curvature", 0}};
  double analytic = 0;
  J samples = J::array(), worst = J::object();
  for (const auto &c : cases) {
    auto production = evaluate(c.p, c.r, c.th, c.family);
    B branch = c.r == c.p.ri   ? B::core
               : c.r == c.p.ro ? B::exterior
                               : B::automatic;
    auto d = ref::derivatives(
        c.r, c.th, c.p, c.family,
        T(cfg["reference"]["finite_difference_step"].get<std::string>()),
        branch);
    auto cur = ref::curvature(d);
    auto rrr = ref::radial_derivative(c.r, c.th, c.p, 3, c.family, branch);
    auto inv = ref::inverse(d.g);
    std::map<std::string, double> errors{
        {"metric", err(production.metric, d.g)},
        {"inverse", err(production.inverse, inv)},
        {"dg", err(production.dg, d.dg)},
        {"ddg", err(production.ddg, d.ddg)},
        {"drrr", err(production.drrr, rrr)},
        {"curvature",
         std::max({err(production.ricci, cur.ricci),
                   err(production.ricci_scalar, cur.scalar),
                   err(production.kretschmann, cur.kretschmann)})}};
    J location{{"family", static_cast<int>(c.family)},
               {"a", c.p.a.number()},
               {"r", c.r.number()},
               {"theta", c.th.number()}};
    for (const auto &[k, v] : errors)
      if (v > maxima[k]) {
        maxima[k] = v;
        worst[k] = location;
      }
    if (c.family == F::flat) {
      analytic = std::max({analytic, std::abs(production.ricci_scalar),
                           std::abs(production.kretschmann)});
    } else if (c.family == F::de_sitter ||
               (c.family == F::toy && c.r <= c.p.ri)) {
      analytic =
          std::max({analytic, err(production.ricci_scalar, T(4) * c.p.L),
                    err(production.kretschmann, T(8) / T(3) * c.p.L * c.p.L)});
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
          analytic = std::max(analytic,
                              err(production.ricci[i][j], c.p.L * d.g[i][j]));
    } else if (c.family == F::kerr || (c.family == F::toy && c.r >= c.p.ro)) {
      auto co = ref::sincos(c.th).second;
      T b = c.p.a * c.p.a * co * co, r = c.r;
      T K = T(48) * c.p.m * c.p.m *
            (ref::power(r, 6) - T(15) * ref::power(r, 4) * b +
             T(15) * r * r * b * b - b * b * b) /
            ref::power(r * r + b, 6);
      analytic = std::max(analytic, err(production.kretschmann, K));
      for (const auto &row : production.ricci)
        for (double v : row)
          analytic = std::max(analytic, std::abs(v));
    }
    if (c.family == F::toy && c.p.a == T(0)) {
      auto [mu, lam] = ref::transition(c.r, c.p, c.family, B::automatic);
      T f = T(1) - T(2) * c.p.m * mu / c.r - c.p.L * lam * c.r * c.r / T(3);
      analytic = std::max({analytic, err(production.metric[0][0], -f),
                           err(production.metric[1][1], T(1) / f)});
    }
    location["errors"] = errors;
    location["ricci_scalar"] = cur.scalar.number();
    location["kretschmann"] = cur.kretschmann.number();
    samples.push_back(location);
  }
  for (const auto &[k, v] : maxima)
    checks.upper("independent " + k, v, cfg["thresholds"][k]);
  checks.upper("analytic limits", analytic,
               cfg["thresholds"]["analytic_curvature"]);
  return {{"samples", samples},
          {"maxima", maxima},
          {"worst_cases", worst},
          {"analytic_maximum", analytic}};
}
J matching(const P &p, const J &cfg, Checks &checks) {
  ref::Polynomial poly{0, 0, 0, 0, 35, -84, 70, -20};
  bool exact = ref::value(poly, 0) == ref::Rational(0) &&
               ref::value(poly, 1) == ref::Rational(1);
  for (int order = 1; order <= 3; ++order) {
    poly = ref::derivative(poly);
    exact = exact && ref::value(poly, 0) == ref::Rational(0) &&
            ref::value(poly, 1) == ref::Rational(0);
  }
  checks.truth("exact C3 endpoint conditions", exact);
  double endpoint = 0, extrinsic = 0;
  std::array<double, 4> production_joins{};
  double production_extrinsic = 0;
  J approach = J::array();
  for (T join : {p.ri, p.ro}) {
    B branch = join == p.ri ? B::core : B::exterior;
    for (T theta : {T(".1"), T("1.1"), ref::pi<T>() / T(2)}) {
      const auto shell = evaluate(p, join, theta, F::toy, prod::Region::shell);
      const auto pure =
          evaluate(p, join, theta, join == p.ri ? F::de_sitter : F::kerr);
      const std::array<prod::Matrix, 4> shell_jets{shell.metric, shell.dg[1],
                                                   shell.ddg[1][1], shell.drrr};
      const std::array<prod::Matrix, 4> pure_jets{pure.metric, pure.dg[1],
                                                  pure.ddg[1][1], pure.drrr};
      for (int k = 0; k < 4; ++k)
        for (int i = 0; i < 4; ++i)
          for (int j = 0; j < 4; ++j)
            production_joins[k] =
                std::max(production_joins[k],
                         std::abs(shell_jets[k][i][j] - pure_jets[k][i][j]) /
                             (1 + std::abs(pure_jets[k][i][j])));
      for (int i : {0, 2, 3})
        for (int j : {0, 2, 3}) {
          const double ks =
              shell.dg[1][i][j] / (2 * std::sqrt(shell.metric[1][1]));
          const double kp =
              pure.dg[1][i][j] / (2 * std::sqrt(pure.metric[1][1]));
          production_extrinsic = std::max(
              production_extrinsic, std::abs(ks - kp) / (1 + std::abs(kp)));
        }
      for (int order = 0; order < 4; ++order) {
        auto a = ref::radial_derivative(join, theta, p, order, F::toy, branch);
        auto b =
            ref::radial_derivative(join, theta, p, order, F::toy, B::shell);
        endpoint = std::max(endpoint, err(a, b));
      }
      auto a = ref::derivatives(join, theta, p, F::toy, T("1e-9"), branch),
           b = ref::derivatives(join, theta, p, F::toy, T("1e-9"), B::shell);
      for (int i : {0, 2, 3})
        for (int j : {0, 2, 3})
          extrinsic = std::max(
              extrinsic, err(a.dg[1][i][j] / (T(2) * ref::sqrt(a.g[1][1])),
                             b.dg[1][i][j] / (T(2) * ref::sqrt(b.g[1][1]))));
    }
    for (int side : {-1, 1}) {
      std::vector<std::array<double, 4>> rows;
      for (T h : {T("1e-3"), T("1e-4"), T("1e-5")}) {
        T r = join + T(side) * h;
        auto prod = evaluate(p, r, T("1.1"));
        std::array<prod::Matrix, 4> source{prod.metric, prod.dg[1],
                                           prod.ddg[1][1], prod.drrr};
        std::array<double, 4> e{};
        for (int k = 0; k < 4; ++k)
          e[k] = err(source[k],
                     ref::radial_derivative(r, T("1.1"), p, k, F::toy, branch));
        rows.push_back(e);
      }
      bool shell = (branch == B::core && side == 1) ||
                   (branch == B::exterior && side == -1),
           ok = true;
      for (int k = 0; k < 4; ++k)
        ok = ok &&
             (shell ? rows.back()[k] < rows.front()[k] / 20
                    : rows.back()[k] < cfg["thresholds"]["drrr"].get<double>());
      checks.truth("join " + std::to_string(join.number()) + " side " +
                       std::to_string(side) + " C3 approach",
                   ok);
      approach.push_back({{"join", join.number()},
                          {"side", side},
                          {"steps", {.001, .0001, .00001}},
                          {"errors_by_derivative_order", rows}});
    }
  }
  checks.upper("analytic branch matching through order 3", endpoint,
               cfg["thresholds"]["matching"]);
  checks.upper("extrinsic curvature matching", extrinsic,
               cfg["thresholds"]["matching"]);
  const std::array<std::string, 4> thresholds{"metric", "dg", "ddg", "drrr"};
  for (int k = 0; k < 4; ++k)
    checks.upper("production explicit shell C" + std::to_string(k) + " join",
                 production_joins[k], cfg["thresholds"][thresholds[k]]);
  checks.upper("production explicit shell outward-normal extrinsic curvature",
               production_extrinsic, cfg["thresholds"]["dg"]);
  return {{"endpoint_maximum", endpoint},
          {"extrinsic_maximum", extrinsic},
          {"production_shell_endpoint_errors_C0_C3", production_joins},
          {"production_shell_extrinsic_error", production_extrinsic},
          {"boundary_map",
           {{"r_in", "shell vs de_sitter/core"},
            {"r_out", "shell vs kerr/exterior"}}},
          {"approach", approach}};
}
J convergence(const P &p, const J &cfg, Checks &checks) {
  T r = p.ri + T(".4") * (p.ro - p.ri), theta("1.1");
  auto target = evaluate(p, r, theta);
  J rows = J::array();
  std::vector<std::array<double, 3>> errors;
  for (T h : {T(".004"), T(".002"), T(".001")}) {
    auto d = ref::derivatives(r, theta, p, F::toy, h);
    auto c = ref::curvature(d);
    std::array<double, 3> e{err(target.dg, d.dg), err(target.ddg, d.ddg),
                            err(target.kretschmann, c.kretschmann)};
    errors.push_back(e);
    rows.push_back({{"h", h.number()},
                    {"dg", e[0]},
                    {"ddg", e[1]},
                    {"kretschmann", e[2]}});
  }
  J ratios = J::array();
  bool converged = true;
  for (int i = 0; i < 2; ++i) {
    std::array<double, 3> row{};
    for (int k = 0; k < 3; ++k) {
      row[k] = errors[i][k] / errors[i + 1][k];
      converged =
          converged &&
          row[k] > cfg["thresholds"]["convergence_ratio_min"].get<double>();
    }
    ratios.push_back(row);
  }
  checks.truth("fourth-order finite difference convergence", converged);
  auto a = ref::derivatives(r, theta, p, F::toy, T("1e-8")),
       b = ref::derivatives(r, theta, p, F::toy, T("1e-9"));
  double step = std::max({err(a.g, b.g), err(a.dg, b.dg), err(a.ddg, b.ddg)});
  using H = ref::Decimal<110>;
  ref::Parameters<H> hp{H(p.m.str()), H(p.a.str()), H(p.L.str()), H(p.ri.str()),
                        H(p.ro.str())};
  auto high =
      ref::derivatives(H(r.str()), H(theta.str()), hp, F::toy, H("1e-9"));
  double precision = 0;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      precision = std::max(precision, err(b.g[i][j], T(high.g[i][j].str())));
      for (int k = 0; k < 4; ++k) {
        precision =
            std::max(precision, err(b.dg[k][i][j], T(high.dg[k][i][j].str())));
        for (int l = 0; l < 4; ++l)
          precision = std::max(
              precision, err(b.ddg[k][l][i][j], T(high.ddg[k][l][i][j].str())));
      }
    }
  checks.upper("reference step refinement", step,
               cfg["thresholds"]["reference_refinement"]);
  checks.upper("reference 90 to 110 decimal-place refinement", precision,
               cfg["thresholds"]["reference_refinement"]);
  return {{"refinement", rows},
          {"ratios_dg_ddg_K", ratios},
          {"reference_step_error", step},
          {"reference_precision_error", precision}};
}
J energy(const P &p) {
  T minimum(100000), null_error;
  J witness;
  for (T u : {T(".1"), T(".25"), T(".5"), T(".75"), T(".9")}) {
    T r = p.ri + u * (p.ro - p.ri), th = ref::pi<T>() / T(2);
    auto d = ref::derivatives(r, th, p);
    auto c = ref::curvature(d);
    auto &g = d.g;
    T omega = -g[0][3] / g[3][3],
      alpha = ref::sqrt((g[0][3] * g[0][3] - g[0][0] * g[3][3]) / g[3][3]);
    for (int axis : {1, 2, 3})
      for (int sign : {-1, 1}) {
        std::array<T, 4> k{T(1) / alpha, T(0), T(0), omega / alpha};
        k[axis] += T(sign) / ref::sqrt(g[axis][axis]);
        T null, tkk;
        for (int i = 0; i < 4; ++i)
          for (int j = 0; j < 4; ++j) {
            null += g[i][j] * k[i] * k[j];
            tkk += c.ricci[i][j] * k[i] * k[j];
          }
        tkk = tkk / (T(8) * ref::pi<T>());
        if (tkk < minimum) {
          minimum = tkk;
          null_error = abs(null);
          witness = {{"Tkk", tkk.number()},
                     {"r", r.number()},
                     {"theta", th.number()},
                     {"null_vector", numbers(k)},
                     {"null_residual", null_error.number()}};
        }
      }
  }
  return {{"minimum_witness", witness},
          {"finding", minimum < T("-1e-20") ? "NEC violation witnessed"
                                            : "inconclusive finite sample"},
          {"cosmological_term", "included once via T=G/(8*pi)"},
          {"tensor_classification", "not assessed"},
          {"dynamical_stability", "not assessed"},
          {"acceptance_role", "diagnostic only"}};
}
int main(int argc, char **argv) {
  std::filesystem::path root = GRAVASTAR_SOURCE_ROOT,
                        config = "benchmarks/rotating_v1.json",
                        output = "results/rotating_v1.json";
  try {
    for (int i = 1; i < argc; ++i) {
      std::string flag = argv[i];
      if (flag == "--help") {
        std::cout << "gravastar_validate [--root PATH] [--config "
                     "benchmarks/FILE.json] [--output results/FILE.json]\n";
        return 0;
      }
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      if (flag == "--root")
        root = argv[++i];
      else if (flag == "--config")
        config = argv[++i];
      else if (flag == "--output")
        output = argv[++i];
      else
        throw std::invalid_argument("unknown option " + flag);
    }
    root = std::filesystem::canonical(root);
    if (config.is_relative())
      config = root / config;
    auto rel = std::filesystem::canonical(config).lexically_relative(
        root / "benchmarks");
    if (rel.empty() || *rel.begin() == "..")
      throw std::invalid_argument("config must be under benchmarks/");
    output = util::result_path(root, output);
    J cfg = J::parse(util::read_file(config));
    if (cfg.at("schema_version") != 1)
      throw std::invalid_argument("unsupported schema");
    auto values = cfg.at("parameters");
    if (cfg.at("reference").at("decimal_places") != 90 ||
        cfg.at("reference").at("refinement_decimal_places") != 110)
      throw std::invalid_argument(
          "this reference build supports 90/110 decimal places");
    P p{T(values.at("M").get<std::string>()),
        T(values.at("a").get<std::string>()),
        T(values.at("Lambda").get<std::string>()),
        T(values.at("r_in").get<std::string>()),
        T(values.at("r_out").get<std::string>())};
    if (p.m <= T(0) || p.L < T(0) || p.ri <= T(0) || p.ro <= p.ri)
      throw std::invalid_argument("invalid preset");
    auto before = util::manifest(root);
    Checks checks;
    J report{{"schema_version", 2},
             {"configuration", cfg},
             {"input_sha256", util::sha256(util::read_file(config))},
             {"compiler", __VERSION__},
             {"cplusplus", __cplusplus},
             {"reference_decimal_places", 90},
             {"error_norm",
              "max |production-reference|/(1+|reference|); inverse "
              "identity and vacuum Ricci use absolute errors"},
             {"published_figure_reproduction",
              "unresolved: source shell radii unavailable"},
             {"full_interior_optics",
              "not implemented; extension and axis charts unresolved"}};
    struct utsname host{};
    if (uname(&host) == 0)
      report["host"] = {{"system", host.sysname},
                        {"release", host.release},
                        {"machine", host.machine}};
    report["executable_sha256"] = util::sha256(
        util::read_file(std::filesystem::canonical("/proc/self/exe")));
    auto start = std::chrono::steady_clock::now();
    try {
      std::cout << "Exact radial certificate..." << std::endl;
      report["radial_certificate"] = radial(p, checks);
      std::cout << "Domain grid..." << std::endl;
      report["domain"] = domain(p, cfg, checks);
      std::cout << "Independent reference vectors..." << std::endl;
      report["independent"] = independent(p, cfg, checks);
      std::cout << "Matching and convergence..." << std::endl;
      report["matching"] = matching(p, cfg, checks);
      report["convergence"] = convergence(p, cfg, checks);
      std::cout << "Energy diagnostic..." << std::endl;
      report["energy"] = energy(p);
    } catch (const std::exception &e) {
      checks.truth("execution", false, {{"reason", e.what()}});
    }
    auto after = util::manifest(root);
    checks.truth("computational source unchanged during run",
                 before["computational_sha256"] ==
                     after["computational_sha256"]);
    report["computational_source_sha256"] = after["computational_sha256"];
    report["checks"] = checks.rows;
    report["passed"] = checks.passed();
    report["elapsed_seconds"] =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (checks.passed()) {
      util::atomic_json(output, report);
      util::atomic_json(output.parent_path() /
                            (output.stem().string() + "_manifest.json"),
                        after);
    } else {
      auto failed = output;
      failed.replace_filename(output.stem().string() + ".failed.json");
      util::atomic_json(failed, report);
    }
    for (const auto &row : checks.rows)
      if (!row.at("passed").get<bool>())
        std::cerr << row.dump() << '\n';
    std::cout << (checks.passed() ? "PASS " : "FAIL ") << checks.rows.size()
              << " checks; " << output << '\n';
    return checks.passed() ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "validation error: " << e.what() << '\n';
    return 2;
  }
}
