// SPDX-License-Identifier: MIT
#include "gravastar/geometry/angular.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <iostream>
#include <numbers>

namespace g = gravastar::geometry;
namespace u = gravastar::tools;
using J = u::Json;
template <class T> T require(std::variant<T, g::Error> result) {
  if (auto error = std::get_if<g::Error>(&result))
    throw std::runtime_error(error->message);
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
g::Matrix transform(const g::Matrix &a, const g::Matrix &jac) {
  g::Matrix out{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      for (int k = 0; k < 4; ++k)
        for (int l = 0; l < 4; ++l)
          out[i][j] += jac[k][i] * a[k][l] * jac[l][j];
  return out;
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
    double metric_error = 0, inverse_error = 0, scalar_error = 0, k_error = 0,
           ricci_error = 0, path_error = 0, limit_error = 0, join_error = 0;
    std::size_t points = 0;
    for (auto family : {g::Family::toy, g::Family::kerr, g::Family::de_sitter,
                        g::Family::flat})
      for (double radius : {1.5, 1.50000001, 1.65, 1.79999999, 1.8, 3.0})
        for (double rho : {0.025, 0.3, 0.9, 1.5})
          for (double phi : {0.0, .73, 2.1})
            for (bool south : {false, true}) {
              ++points;
              const double x = rho * std::cos(phi), y = rho * std::sin(phi),
                           q = rho * rho;
              g::Parameters p{family};
              auto a = require(g::evaluate_angular_curvature(p, radius, x, y));
              auto first = require(g::evaluate_angular(p, radius, x, y));
              path_error =
                  std::max({path_error, difference(first.metric, a.metric),
                            difference(first.inverse, a.inverse),
                            difference(first.dg, a.dg)});
              const double theta = 2 * std::atan(rho), sign = south ? -1 : 1;
              auto bl = require(g::evaluate(
                  p, radius, south ? std::numbers::pi - theta : theta));
              g::Matrix jac{}, identity{}, product{};
              jac[0][0] = jac[1][1] = 1;
              jac[2][2] = sign * 2 * std::cos(phi) / (1 + q);
              jac[2][3] = sign * 2 * std::sin(phi) / (1 + q);
              jac[3][2] = -std::sin(phi) / rho;
              jac[3][3] = std::cos(phi) / rho;
              metric_error =
                  std::max(metric_error,
                           difference(a.metric, transform(bl.metric, jac)));
              ricci_error = std::max(
                  ricci_error, difference(a.ricci, transform(bl.ricci, jac)));
              scalar_error = std::max(
                  scalar_error, residual(a.ricci_scalar, bl.ricci_scalar));
              k_error =
                  std::max(k_error, residual(a.kretschmann, bl.kretschmann));
              for (int i = 0; i < 4; ++i) {
                identity[i][i] = 1;
                for (int j = 0; j < 4; ++j)
                  for (int k = 0; k < 4; ++k)
                    product[i][j] += a.metric[i][k] * a.inverse[k][j];
              }
              inverse_error =
                  std::max(inverse_error, difference(product, identity));
            }
    for (auto family : {g::Family::flat, g::Family::kerr, g::Family::de_sitter})
      for (double spin : {0.0, .875, -.875})
        for (double radius : {1.65, 1.8, 3.0}) {
          g::Parameters p{family, 1, spin};
          auto a = require(g::evaluate_angular_curvature(p, radius, 0, 0));
          const double r2 = radius * radius, a2 = spin * spin;
          const double expected =
              family == g::Family::kerr
                  ? 48 *
                        (r2 * r2 * r2 - 15 * r2 * r2 * a2 + 15 * r2 * a2 * a2 -
                         a2 * a2 * a2) /
                        std::pow(r2 + a2, 6)
              : family == g::Family::de_sitter
                  ? 8 * p.cosmological_constant * p.cosmological_constant / 3
                  : 0;
          const double scalar =
              family == g::Family::de_sitter ? 4 * p.cosmological_constant : 0;
          limit_error =
              std::max({limit_error, residual(a.kretschmann, expected),
                        residual(a.ricci_scalar, scalar)});
          for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
              limit_error =
                  std::max(limit_error, residual(a.ricci[i][j],
                                                 family == g::Family::de_sitter
                                                     ? p.cosmological_constant *
                                                           a.metric[i][j]
                                                     : 0));
        }
    for (double radius : {1.5, 1.8})
      for (double x : {0.0, 1e-9, .4}) {
        auto shell = require(
            g::evaluate_angular_curvature({}, radius, x, 0, g::Region::shell));
        auto side = require(g::evaluate_angular_curvature(
            {}, radius, x, 0,
            radius == 1.5 ? g::Region::core : g::Region::exterior));
        join_error = std::max(
            {join_error, difference(shell.metric, side.metric),
             difference(shell.dg, side.dg), difference(shell.ddg, side.ddg)});
      }
    check("metric vs transformed Boyer-Lindquist", metric_error, 2e-11);
    check("inverse identity", inverse_error, 2e-11);
    check("Ricci tensor coordinate transform", ricci_error, 2e-8);
    check("Ricci scalar coordinate invariance", scalar_error, 2e-9);
    check("Kretschmann coordinate invariance", k_error, 2e-8);
    check("first-order and curvature API agreement", path_error, 2e-14);
    check("exact pole flat Kerr de Sitter invariants", limit_error, 2e-10);
    check("unclamped shell C0-C2 endpoint jets including poles", join_error,
          2e-12);
    int errors = 0;
    for (const auto &result :
         {g::evaluate_angular({}, 0, 0, 0), g::evaluate_angular({}, 1.65, 3, 0),
          g::evaluate_angular({}, 1.65,
                              std::numeric_limits<double>::quiet_NaN(), 0),
          g::evaluate_angular({}, 1.65, 0, 0, g::Region::core),
          g::evaluate_angular({g::Family::kerr, 1, 0}, 2, 0, 0)})
      errors += std::holds_alternative<g::Error>(result);
    check("invalid inputs and Schwarzschild horizon rejected", 5 - errors, 0);
    auto after = u::manifest(root);
    if (before.at("computational_sha256") != after.at("computational_sha256"))
      throw std::runtime_error("sources changed during test");
    if (argc == 3)
      u::atomic_json(
          u::result_path(root, argv[2]),
          {{"schema_version", 1},
           {"passed", true},
           {"checks", checks},
           {"coordinate_points", points},
           {"computational_source_sha256", after.at("computational_sha256")},
           {"compiler", __VERSION__},
           {"complete_P2_acceptance", false},
           {"scope", "Regular angular geometry; production rays unchanged."}});
    std::cout << checks.dump(2) << "\nPASS " << checks.size()
              << " angular geometry checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "angular geometry: " << e.what() << '\n';
    return 1;
  }
}
