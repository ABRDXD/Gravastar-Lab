// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/core.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>

namespace g = gravastar::geometry;
namespace u = gravastar::tools;
using J = u::Json;
template <class T> T require(std::variant<T, g::Error> result) {
  if (auto e = std::get_if<g::Error>(&result))
    throw std::runtime_error(e->code + ": " + e->message);
  return std::get<T>(result);
}
double residual(double a, double b) {
  return std::abs(a - b) / (1 + std::abs(b));
}
template <class A> double difference(const A &a, const A &b) {
  double out = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if constexpr (std::is_same_v<typename A::value_type, double>)
      out = std::max(out, residual(a[i], b[i]));
    else
      out = std::max(out, difference(a[i], b[i]));
  }
  return out;
}
// Standalone scalar reference; it does not use production derivative algebra.
g::Matrix reference_metric(double k, const std::array<double, 3> &x) {
  g::Matrix out{};
  const double f = 1 - k * (x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
  out[0][0] = -f;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      out[i + 1][j + 1] = (i == j ? 1 : 0) + k * x[i] * x[j] / f;
  return out;
}
double determinant(g::Matrix m) {
  double result = 1;
  for (int k = 0; k < 4; ++k) {
    int pivot = k;
    for (int i = k + 1; i < 4; ++i)
      if (std::abs(m[i][k]) > std::abs(m[pivot][k]))
        pivot = i;
    if (m[pivot][k] == 0)
      return 0;
    if (pivot != k) {
      std::swap(m[pivot], m[k]);
      result = -result;
    }
    result *= m[k][k];
    for (int i = k + 1; i < 4; ++i) {
      const double factor = m[i][k] / m[k][k];
      for (int j = k + 1; j < 4; ++j)
        m[i][j] -= factor * m[k][j];
    }
  }
  return result;
}
int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    auto before = u::manifest(root);
    J checks = J::array();
    auto check = [&](const char *name, double measured, double ceiling) {
      const bool pass = std::isfinite(measured) && measured <= ceiling;
      checks.push_back({{"name", name},
                        {"measured", measured},
                        {"ceiling", ceiling},
                        {"passed", pass}});
      if (!pass)
        throw std::runtime_error(std::string(name) + ": " +
                                 std::to_string(measured));
    };
    double metric_error = 0, inverse_error = 0, determinant_error = 0,
           first_error = 0, second_error = 0, curvature_error = 0,
           api_error = 0, center_error = 0;
    std::size_t points = 0;
    const std::array positions{
        std::array{0., 0., 0.},   std::array{1e-12, -1e-12, 1e-12},
        std::array{.1, -.2, .3},  std::array{.5, 0., 0.},
        std::array{0., .5, 0.},   std::array{1.2, .1, .4},
        std::array{0., 0., 1.49}, std::array{-.7, .4, -.6}};
    for (auto family : {g::Family::toy, g::Family::de_sitter, g::Family::flat})
      for (double spin : {0., -.875, .875})
        for (const auto &x : positions) {
          ++points;
          g::Parameters p{family};
          p.spin = spin;
          auto a = require(g::evaluate_core_curvature(p, x[0], x[1], x[2]));
          auto first = require(g::evaluate_core(p, x[0], x[1], x[2]));
          api_error = std::max({api_error, difference(first.metric, a.metric),
                                difference(first.inverse, a.inverse),
                                difference(first.dg, a.dg)});
          const double k =
              family == g::Family::flat ? 0 : p.cosmological_constant / 3;
          const double f = 1 - k * (x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
          metric_error = std::max(metric_error,
                                  difference(a.metric, reference_metric(k, x)));
          determinant_error =
              std::max(determinant_error, std::abs(determinant(a.metric) + 1));
          g::Matrix identity{}, product{};
          for (int i = 0; i < 4; ++i) {
            identity[i][i] = 1;
            for (int j = 0; j < 4; ++j)
              for (int l = 0; l < 4; ++l)
                product[i][j] += a.metric[i][l] * a.inverse[l][j];
          }
          inverse_error =
              std::max(inverse_error, difference(product, identity));
          g::Tensor3 dg{};
          g::Tensor4 ddg{};
          for (int l = 0; l < 3; ++l) {
            dg[l + 1][0][0] = 2 * k * x[l];
            for (int m = 0; m < 3; ++m)
              ddg[l + 1][m + 1][0][0] = l == m ? 2 * k : 0;
            for (int i = 0; i < 3; ++i)
              for (int j = 0; j < 3; ++j) {
                const double dl = (i == l ? x[j] : 0) + (j == l ? x[i] : 0);
                dg[l + 1][i + 1][j + 1] =
                    k * dl / f + 2 * k * k * x[i] * x[j] * x[l] / (f * f);
                for (int m = 0; m < 3; ++m) {
                  const double dm = (i == m ? x[j] : 0) + (j == m ? x[i] : 0);
                  const int d2 =
                      (i == l && j == m ? 1 : 0) + (i == m && j == l ? 1 : 0);
                  ddg[l + 1][m + 1][i + 1][j + 1] =
                      k * d2 / f +
                      2 * k * k *
                          (dl * x[m] + dm * x[l] + (l == m ? x[i] * x[j] : 0)) /
                          (f * f) +
                      8 * k * k * k * x[i] * x[j] * x[l] * x[m] / (f * f * f);
                }
              }
          }
          first_error = std::max(first_error, difference(a.dg, dg));
          second_error = std::max(second_error, difference(a.ddg, ddg));
          curvature_error =
              std::max({curvature_error, residual(a.ricci_scalar, 12 * k),
                        residual(a.kretschmann, 24 * k * k)});
          for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
              curvature_error =
                  std::max(curvature_error,
                           residual(a.ricci[i][j], 3 * k * a.metric[i][j]));
          if (x == positions[0]) {
            g::Matrix minkowski{};
            minkowski[0][0] = -1;
            for (int i = 1; i < 4; ++i)
              minkowski[i][i] = 1;
            center_error =
                std::max({center_error, difference(a.metric, minkowski),
                          difference(a.dg, g::Tensor3{})});
          }
        }
    check("static Cartesian metric vs standalone formula", metric_error, 2e-14);
    check("analytic inverse identity", inverse_error, 3e-14);
    check("constant determinant vs pivoted elimination", determinant_error,
          3e-14);
    check("first derivatives vs closed formulas", first_error, 3e-14);
    check("Hessian vs closed formulas", second_error, 5e-14);
    check("de Sitter and flat analytic curvature identities", curvature_error,
          2e-12);
    check("requested derivative orders agree", api_error, 0);
    check("regular Cartesian center metric and vanishing first derivatives",
          center_error, 0);

    g::Parameters reference{g::Family::de_sitter};
    const std::array point{1.2, -.4, .3};
    const auto jets = require(
        g::evaluate_core_curvature(reference, point[0], point[1], point[2]));
    auto fd_errors = [&](double h) {
      auto value = [&](int offset) {
        auto x = point;
        x[0] += offset * h;
        return reference_metric(reference.cosmological_constant / 3, x)[1][1];
      };
      const double fm2 = value(-2), fm = value(-1), f0 = value(0),
                   fp = value(1), fp2 = value(2);
      return std::array{
          std::abs((fm2 - 8 * fm + 8 * fp - fp2) / (12 * h) - jets.dg[1][1][1]),
          std::abs((-fm2 + 16 * fm - 30 * f0 + 16 * fp - fp2) / (12 * h * h) -
                   jets.ddg[1][1][1][1])};
    };
    const auto coarse = fd_errors(.04), fine = fd_errors(.02);
    check("independent finite-difference first derivative", fine[0], 2e-6);
    check("independent finite-difference Hessian", fine[1], 5e-6);
    check("first derivative fourth-order refinement ratio", fine[0] / coarse[0],
          .08);
    check("Hessian fourth-order refinement ratio", fine[1] / coarse[1], .08);

    int errors = 0;
    auto invalid = reference;
    invalid.cosmological_constant = -1;
    auto unknown = reference;
    unknown.family = static_cast<g::Family>(99);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto &result :
         {g::evaluate_core({g::Family::kerr}, 0, 0, 0),
          g::evaluate_core(unknown, 0, 0, 0),
          g::evaluate_core(invalid, 0, 0, 0),
          g::evaluate_core(reference, nan, 0, 0),
          g::evaluate_core(reference, std::sqrt(6.), 0, 0),
          g::evaluate_core(reference, 3, 0, 0),
          g::evaluate_core({}, 0, 0, 1.5001), g::evaluate_core({}, 2, 0, 0)})
      errors += std::holds_alternative<g::Error>(result);
    check(
        "invalid families parameters horizons and outside-core points rejected",
        8 - errors, 0);
    auto disk = require(g::evaluate_core({}, .5, .2, 0));
    check("oblate focal disk remains a regular geometry point",
          std::abs(disk.determinant + 1), 0);
    auto boundary = require(g::evaluate_core({}, 0, 0, 1.5));
    check("closed exact-core boundary is admitted",
          std::abs(boundary.determinant + 1), 0);

    const auto after = u::manifest(root);
    if (before["computational_sha256"] != after["computational_sha256"])
      throw std::runtime_error("sources changed during core geometry test");
    if (argc == 3)
      u::atomic_json(
          u::result_path(root, argv[2]),
          {{"schema_version", 1},
           {"passed", true},
           {"checks", checks},
           {"coordinate_points", points},
           {"compiler", __VERSION__},
           {"computational_source_sha256", after["computational_sha256"]},
           {"finite_difference_steps", {.04, .02}},
           {"complete_P2_acceptance", false},
           {"scope",
            "Static Cartesian de Sitter core geometry including center and "
            "focal disk; no horizon extension or tracer acceptance implied."}});
    std::cout << checks.dump(2) << "\nPASS " << checks.size()
              << " core geometry checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "core geometry: " << e.what() << '\n';
    return 1;
  }
}
