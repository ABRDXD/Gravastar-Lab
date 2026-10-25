// SPDX-License-Identifier: MIT
// Preflight of the proposed angular chart. This does not change production
// rays.
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/reference/geometry.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <iostream>
#include <numbers>

namespace g = gravastar::geometry;
namespace r = gravastar::reference;
namespace u = gravastar::tools;
using J = u::Json;
template <class T> struct Candidate {
  r::Matrix<T> metric{}, inverse{};
  T determinant;
};
template <class T>
Candidate<T> candidate(T radius, T x, T y, const r::Parameters<T> &p,
                       r::Family family) {
  auto [mu, lambda] = r::transition(radius, p, family, r::Branch::automatic);
  T q = x * x + y * y, C = T(4) / ((T(1) + q) * (T(1) + q)), s2 = C * q,
    c2 = (T(1) - q) * (T(1) - q) / ((T(1) + q) * (T(1) + q)),
    k = p.L * lambda / T(3), m = p.m * mu, a2 = p.a * p.a,
    A = radius * radius + a2, sigma = radius * radius + a2 * c2,
    X = T(1) + k * a2, theta = T(1) + k * a2 * c2,
    F = T(1) - k * radius * radius, delta = A * F - T(2) * m * radius,
    B = F / (theta * X) + T(2) * m * radius / (sigma * X * X),
    H = C * sigma / theta, K = C * C * a2 * B,
    D = C * p.a * (delta - theta * A) / (sigma * X * X),
    alpha = (theta * a2 * s2 - delta) / (sigma * X * X),
    Z = -C * delta * theta / (X * X * X * X);
  std::array<T, 2> v{-y, x};
  Candidate<T> out;
  out.metric[0][0] = alpha;
  out.metric[1][1] = sigma / delta;
  out.inverse[0][0] = (H + K * q) / Z;
  out.inverse[1][1] = delta / sigma;
  out.determinant = -sigma * sigma * C * C / (X * X * X * X);
  for (int i = 0; i < 2; ++i) {
    out.metric[0][i + 2] = out.metric[i + 2][0] = D * v[i];
    out.inverse[0][i + 2] = out.inverse[i + 2][0] = -D * v[i] / Z;
    for (int j = 0; j < 2; ++j) {
      out.metric[i + 2][j + 2] = (i == j ? H : T(0)) + K * v[i] * v[j];
      out.inverse[i + 2][j + 2] = (i == j ? T(1) / H : T(0)) +
                                  (D * D - alpha * K) * v[i] * v[j] / (H * Z);
    }
  }
  return out;
}
template <class T>
r::Matrix<T> transform(const r::Matrix<T> &metric,
                       const r::Matrix<T> &jacobian) {
  r::Matrix<T> out{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
          out[i][j] += jacobian[a][i] * metric[a][b] * jacobian[b][j];
  return out;
}
template <class T> T difference(const r::Matrix<T> &a, const r::Matrix<T> &b) {
  using std::abs;
  T maximum(0);
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      maximum =
          std::max(maximum, abs(a[i][j] - b[i][j]) / (T(1) + abs(b[i][j])));
  return maximum;
}
template <class T> r::Matrix<T> bl_jacobian(T rho, T phi, bool south) {
  auto [s, c] = r::sincos(phi);
  r::Matrix<T> jacobian{};
  jacobian[0][0] = jacobian[1][1] = T(1);
  const T sign(south ? -1 : 1);
  jacobian[2][2] = sign * T(2) * c / (T(1) + rho * rho);
  jacobian[2][3] = sign * T(2) * s / (T(1) + rho * rho);
  jacobian[3][2] = -s / rho;
  jacobian[3][3] = c / rho;
  return jacobian;
}
template <int Precision> std::array<double, 3> reference_checks() {
  using T = r::Decimal<Precision>;
  r::Parameters<T> p;
  T metric_error(0), inverse_error(0), determinant_error(0);
  const std::array<r::Family, 4> families{
      r::Family::toy, r::Family::kerr, r::Family::de_sitter, r::Family::flat};
  for (auto family : families)
    for (const char *rv : {"1.5", "1.50000001", "1.65", "1.79999999", "1.8"})
      for (const char *tv : {"0.00000001", "0.4", "1.3"})
        for (bool south : {false, true}) {
          T radius(rv), angle(tv), phi(".73");
          auto [st, ct] = r::sincos(angle / T(2));
          auto [sp, cp] = r::sincos(phi);
          T rho = st / ct, theta = south ? r::pi<T>() - angle : angle;
          auto proposed = candidate(radius, rho * cp, rho * sp, p, family);
          auto original = r::metric(radius, theta, p, family);
          auto jacobian = bl_jacobian(rho, phi, south);
          auto converted = transform(original, jacobian);
          metric_error =
              std::max(metric_error, difference(proposed.metric, converted));
          inverse_error =
              std::max(inverse_error,
                       difference(proposed.inverse, r::inverse(converted)));
          // det(g_old) times det(d old / d new)^2, evaluated independently.
          auto [s, c] = r::sincos(theta);
          auto [mu, lambda] =
              r::transition(radius, p, family, r::Branch::automatic);
          (void)mu;
          T sigma = radius * radius + p.a * p.a * c * c,
            xi = T(1) + p.L * lambda * p.a * p.a / T(3),
            old_det = -sigma * sigma * s * s / (xi * xi * xi * xi),
            angular_jacobian = T(2) / (rho * (T(1) + rho * rho)),
            expected = old_det * angular_jacobian * angular_jacobian;
          determinant_error =
              std::max(determinant_error, abs(proposed.determinant - expected) /
                                              (T(1) + abs(expected)));
        }
  return {metric_error.number(), inverse_error.number(),
          determinant_error.number()};
}
int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    auto before = u::manifest(root);
    J checks = J::array();
    auto check = [&](const char *name, double measured, double ceiling) {
      bool pass = std::isfinite(measured) && measured <= ceiling;
      checks.push_back({{"name", name},
                        {"measured", measured},
                        {"ceiling", ceiling},
                        {"passed", pass}});
      if (!pass)
        throw std::runtime_error(std::string(name) + ": " +
                                 std::to_string(measured));
    };
    double production_error = 0, inverse_error = 0, pole_error = 0,
           patch_error = 0, momentum_error = 0, one_form_error = 0,
           hamiltonian_error = 0, round_trip_error = 0;
    std::size_t points = 0;
    const r::Parameters<double> p{1, .875, .5, 1.5, 1.8};
    for (auto family : {g::Family::toy, g::Family::kerr, g::Family::de_sitter,
                        g::Family::flat})
      for (double radius : {1.5, 1.50000001, 1.65, 1.79999999, 1.8})
        for (double angle : {.05, .3, 1.1, 1.5})
          for (double phi : {0.0, .73, 2.1})
            for (bool south : {false, true}) {
              ++points;
              const double rho = std::tan(angle / 2), x = rho * std::cos(phi),
                           y = rho * std::sin(phi), q = x * x + y * y;
              const auto f = static_cast<r::Family>(family);
              auto proposed = candidate(radius, x, y, p, f);
              auto result =
                  g::evaluate({family, 1, .875, .5, 1.5, 1.8}, radius,
                              south ? std::numbers::pi - angle : angle);
              if (auto error = std::get_if<g::Error>(&result))
                throw std::runtime_error(error->message);
              r::Matrix<double> jacobian{};
              jacobian[0][0] = jacobian[1][1] = 1;
              const double sign = south ? -1 : 1;
              jacobian[2][2] = sign * 2 * std::cos(phi) / (1 + q);
              jacobian[2][3] = sign * 2 * std::sin(phi) / (1 + q);
              jacobian[3][2] = -std::sin(phi) / rho;
              jacobian[3][3] = std::cos(phi) / rho;
              production_error = std::max(
                  production_error,
                  difference(proposed.metric,
                             transform(std::get<g::Evaluation>(result).metric,
                                       jacobian)));
              r::Matrix<double> identity{}, product{};
              for (int i = 0; i < 4; ++i) {
                identity[i][i] = 1;
                for (int j = 0; j < 4; ++j)
                  for (int k = 0; k < 4; ++k)
                    product[i][j] +=
                        proposed.metric[i][k] * proposed.inverse[k][j];
              }
              inverse_error =
                  std::max(inverse_error, difference(product, identity));
              auto other = candidate(radius, x / q, y / q, p, f);
              r::Matrix<double> patch{};
              patch[0][0] = patch[1][1] = 1;
              patch[2][2] = (1 - 2 * x * x / q) / q;
              patch[2][3] = patch[3][2] = -2 * x * y / (q * q);
              patch[3][3] = (1 - 2 * y * y / q) / q;
              patch_error = std::max(
                  patch_error,
                  difference(transform(other.metric, patch), proposed.metric));
              std::array<double, 4> momentum{-1, .2, .3, -.4},
                  transformed = momentum, tangent{.7, -.2, .5, .9},
                  tangent_other{};
              const double dot = x * momentum[2] + y * momentum[3];
              transformed[2] = q * momentum[2] - 2 * x * dot;
              transformed[3] = q * momentum[3] - 2 * y * dot;
              const double back_dot =
                  (x * transformed[2] + y * transformed[3]) / q;
              round_trip_error =
                  std::max({round_trip_error,
                            std::abs(transformed[2] / q - 2 * x / q * back_dot -
                                     momentum[2]),
                            std::abs(transformed[3] / q - 2 * y / q * back_dot -
                                     momentum[3])});
              double before_form = 0, after_form = 0, before_h = 0, after_h = 0;
              for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j)
                  tangent_other[i] += patch[i][j] * tangent[j];
                before_form += momentum[i] * tangent[i];
                after_form += transformed[i] * tangent_other[i];
                for (int j = 0; j < 4; ++j) {
                  before_h +=
                      proposed.inverse[i][j] * momentum[i] * momentum[j];
                  after_h +=
                      other.inverse[i][j] * transformed[i] * transformed[j];
                }
              }
              one_form_error =
                  std::max(one_form_error, std::abs(before_form - after_form));
              hamiltonian_error =
                  std::max(hamiltonian_error, std::abs(before_h - after_h) /
                                                  (1 + std::abs(before_h)));
              momentum_error = std::max(
                  momentum_error,
                  std::abs(-y * momentum[2] + x * momentum[3] +
                           y / q * transformed[2] - x / q * transformed[3]));
            }
    for (auto family : {r::Family::toy, r::Family::kerr, r::Family::de_sitter,
                        r::Family::flat})
      for (double radius : {1.55, 1.65, 1.8}) {
        auto pole = candidate(radius, 0.0, 0.0, p, family);
        for (double phi : {0.0, .73, 2.1}) {
          auto nearby = candidate(radius, 1e-9 * std::cos(phi),
                                  1e-9 * std::sin(phi), p, family);
          pole_error =
              std::max(pole_error, difference(nearby.metric, pole.metric));
        }
        if (!std::isfinite(pole.determinant) || pole.determinant >= 0 ||
            pole.metric[1][1] <= 0 || pole.metric[2][2] <= 0 ||
            pole.metric[3][3] <= 0)
          throw std::runtime_error("pole signature or determinant");
      }
    check("double metric vs transformed canonical geometry", production_error,
          2e-12);
    check("double analytic inverse identity", inverse_error, 2e-12);
    check("double pole continuity at angular radius 1e-9", pole_error, 2e-8);
    check("north/south patch metric consistency", patch_error, 2e-12);
    check("axial Killing momentum under patch transition", momentum_error,
          2e-13);
    check("canonical one-form under patch transition", one_form_error, 2e-13);
    check("Hamiltonian under patch transition", hamiltonian_error, 2e-12);
    check("covariant momentum patch round trip", round_trip_error, 2e-13);
    auto precise = reference_checks<90>(), refined = reference_checks<110>();
    for (int i = 0; i < 3; ++i) {
      const char *names[]{"90-place independently expanded metric transform",
                          "90-place inverse vs pivoted elimination",
                          "90-place transformed determinant"};
      check(names[i], precise[i], 1e-60);
      const char *fine_names[]{"110-place expanded metric transform",
                               "110-place inverse vs pivoted elimination",
                               "110-place transformed determinant"};
      check(fine_names[i], refined[i], 1e-80);
    }
    auto after = u::manifest(root);
    if (before.at("computational_sha256") != after.at("computational_sha256"))
      throw std::runtime_error("sources changed during axis preflight");
    if (argc == 3)
      u::atomic_json(
          u::result_path(root, argv[2]),
          {{"schema_version", 1},
           {"passed", true},
           {"checks", checks},
           {"double_points", points},
           {"reference_points_per_precision", 120},
           {"computational_source_sha256", after.at("computational_sha256")},
           {"compiler", __VERSION__},
           {"complete_P2_acceptance", false},
           {"scope", "Metric/inverse/chart-transition algebra preflight only. "
                     "No production angular "
                     "derivative evaluator, curvature validation, "
                     "axis-crossing ray or central continuation. "
                     "Candidate and independent reference share the previously "
                     "tested radial profile helper."}});
    std::cout << checks.dump(2) << "\nPASS " << checks.size()
              << " angular chart preflight checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "axis preflight failed: " << e.what() << '\n';
    return 1;
  }
}
