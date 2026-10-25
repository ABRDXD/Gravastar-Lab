// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/angular.hpp"
#include "gravastar/reference/angular.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <iostream>

namespace g = gravastar::geometry;
namespace r = gravastar::reference;
namespace u = gravastar::tools;
using J = u::Json;
template <class A, class B> double difference(const A &a, const B &b) {
  double maximum = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if constexpr (std::is_same_v<typename A::value_type, double>)
      maximum = std::max(maximum, std::abs(a[i] - b[i].number()) /
                                      (1 + std::abs(b[i].number())));
    else
      maximum = std::max(maximum, difference(a[i], b[i]));
  }
  return maximum;
}
template <class A, class B>
r::Decimal<110> precise_difference(const A &a, const B &b) {
  using T = r::Decimal<110>;
  T maximum(0);
  for (std::size_t i = 0; i < a.size(); ++i) {
    if constexpr (requires { a[i].str(); }) {
      T av(a[i].str()), bv(b[i].str());
      maximum = std::max(maximum, abs(av - bv) / (T(1) + abs(bv)));
    } else
      maximum = std::max(maximum, precise_difference(a[i], b[i]));
  }
  return maximum;
}
int main(int argc, char **argv) {
  try {
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    const std::filesystem::path root = GRAVASTAR_SOURCE_ROOT;
    const auto before = u::manifest(root);
    using A = r::Decimal<90>;
    using B = r::Decimal<110>;
    r::Parameters<A> pa;
    r::Parameters<B> pb;
    J checks = J::array();
    auto check = [&](const char *name, double measured, double ceiling) {
      const bool pass = std::isfinite(measured) && measured <= ceiling;
      checks.push_back({{"name", name},
                        {"measured", measured},
                        {"ceiling", ceiling},
                        {"passed", pass}});
      if (!pass)
        throw std::runtime_error(checks.back().dump());
    };
    double metric_error = 0, first_error = 0, second_error = 0, ricci_error = 0,
           scalar_error = 0, k_error = 0;
    B precision_error(0), step_error(0);
    std::size_t points = 0, curvature_points = 0;
    for (auto family : {g::Family::toy, g::Family::kerr, g::Family::de_sitter,
                        g::Family::flat})
      for (const char *radius :
           {"1.2", "1.5", "1.50000001", "1.65", "1.79999999", "1.8", "2.3"})
        for (const auto &xy :
             std::array<std::array<const char *, 2>, 4>{{{"0", "0"},
                                                         {"1e-8", "-2e-8"},
                                                         {".31", "-.27"},
                                                         {"1.2", ".4"}}}) {
          const double rd = std::stod(radius), x = std::stod(xy[0]),
                       y = std::stod(xy[1]);
          const bool shell = family == g::Family::toy && rd >= 1.5 && rd <= 1.8;
          const auto region = shell ? g::Region::shell : g::Region::automatic;
          const auto branch = shell ? r::Branch::shell : r::Branch::automatic;
          const auto f = static_cast<r::Family>(family);
          auto result =
              g::evaluate_angular_curvature({family}, rd, x, y, region);
          if (auto error = std::get_if<g::Error>(&result))
            throw std::runtime_error(error->message);
          const auto &production = std::get<g::AngularCurvature>(result);
          auto ref = r::angular_derivatives<A>({A(radius), A(xy[0]), A(xy[1])},
                                               pa, f, A("1e-8"), branch);
          auto refined = r::angular_derivatives<B>(
              {B(radius), B(xy[0]), B(xy[1])}, pb, f, B("1e-8"), branch);
          ++points;
          metric_error =
              std::max(metric_error, difference(production.metric, ref.g));
          first_error =
              std::max(first_error, difference(production.dg, ref.dg));
          second_error =
              std::max(second_error, difference(production.ddg, ref.ddg));
          precision_error =
              std::max({precision_error, precise_difference(ref.dg, refined.dg),
                        precise_difference(ref.ddg, refined.ddg)});
          if (family == g::Family::toy && rd == 1.65) {
            auto smaller = r::angular_derivatives<A>(
                {A(radius), A(xy[0]), A(xy[1])}, pa, f, A("5e-9"), branch);
            step_error =
                std::max({step_error, precise_difference(ref.dg, smaller.dg),
                          precise_difference(ref.ddg, smaller.ddg)});
            auto curve = r::curvature(ref);
            ++curvature_points;
            ricci_error = std::max(ricci_error,
                                   difference(production.ricci, curve.ricci));
            scalar_error = std::max(
                scalar_error,
                std::abs(production.ricci_scalar - curve.scalar.number()) /
                    (1 + std::abs(curve.scalar.number())));
            k_error = std::max(
                k_error,
                std::abs(production.kretschmann - curve.kretschmann.number()) /
                    (1 + std::abs(curve.kretschmann.number())));
          }
        }
    const std::array<A, 3> point{A("1.65"), A(".31"), A("-.27")};
    auto coarse = r::angular_derivatives(point, pa, r::Family::toy, A("1e-3"),
                                         r::Branch::shell);
    auto middle = r::angular_derivatives(point, pa, r::Family::toy, A("5e-4"),
                                         r::Branch::shell);
    auto fine = r::angular_derivatives(point, pa, r::Family::toy, A("1e-8"),
                                       r::Branch::shell);
    const double first_coarse = precise_difference(coarse.dg, fine.dg).number(),
                 first_middle = precise_difference(middle.dg, fine.dg).number(),
                 second_coarse =
                     precise_difference(coarse.ddg, fine.ddg).number(),
                 second_middle =
                     precise_difference(middle.ddg, fine.ddg).number();
    check("metric vs independent expanded reference", metric_error, 2e-11);
    check("first derivatives vs 90-place finite differences", first_error,
          2e-10);
    check("Hessian vs 90-place finite differences", second_error, 2e-9);
    check("90 to 110 decimal-place derivative refinement",
          precision_error.number(), 1e-55);
    check("1e-8 to 5e-9 step refinement in shell", step_error.number(), 1e-25);
    check("independent shell Ricci tensor including exact pole", ricci_error,
          2e-9);
    check("independent shell Ricci scalar including exact pole", scalar_error,
          2e-9);
    check("independent shell Kretschmann including exact pole", k_error, 2e-9);
    check("fourth-order first derivative refinement ratio",
          first_middle / first_coarse, .08);
    check("fourth-order Hessian refinement ratio",
          second_middle / second_coarse, .08);
    check("nonzero reference truncation and refinement observed",
          first_coarse > 0 && second_coarse > 0 && step_error > B(0) &&
                  precision_error > B(0)
              ? 0
              : 1,
          0);
    const auto after = u::manifest(root);
    if (before.at("computational_sha256") != after.at("computational_sha256"))
      throw std::runtime_error("sources changed during reference test");
    if (argc == 3)
      u::atomic_json(
          u::result_path(root, argv[2]),
          {{"schema_version", 1},
           {"passed", true},
           {"checks", checks},
           {"points_per_precision", points},
           {"independent_curvature_points", curvature_points},
           {"computational_source_sha256", after.at("computational_sha256")},
           {"compiler", __VERSION__},
           {"complete_P2_acceptance", false},
           {"coarse_refinement",
            {{"steps", {1e-3, 5e-4, 1e-8}},
             {"first_coarse_error", first_coarse},
             {"first_middle_error", first_middle},
             {"hessian_coarse_error", second_coarse},
             {"hessian_middle_error", second_middle}}},
           {"scope", "Independent expanded BL metric transformed to angular "
                     "coordinates, 5-point finite differences at 90/110 "
                     "decimal places. Explicit shell branch extended locally "
                     "at endpoints for one-sided jets. No production jets or "
                     "curvature assembly reused. Production rays unchanged."}});
    std::cout << checks.dump(2) << "\nPASS " << checks.size()
              << " independent angular reference checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "angular reference: " << e.what() << '\n';
    return 1;
  }
}
