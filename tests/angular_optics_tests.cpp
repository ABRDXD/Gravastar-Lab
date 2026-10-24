// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/chart.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <cmath>
#include <iostream>
#include <numbers>

namespace o = gravastar::optics;
namespace g = gravastar::geometry;
namespace u = gravastar::tools;
u::Json checks = u::Json::array();
void check(const std::string &name, double error, double ceiling) {
  const bool pass = std::isfinite(error) && error <= ceiling;
  checks.push_back({{"name", name},
                    {"measured", error},
                    {"threshold", ceiling},
                    {"passed", pass}});
  if (!pass)
    std::cerr << name << ": " << error << " > " << ceiling << '\n';
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
void transformations() {
  double roundtrip = 0, frame_error = 0, pairing = 0, killing = 0, null = 0,
         flow = 0;
  g::Parameters p;
  for (auto family : {g::Family::flat, g::Family::kerr, g::Family::de_sitter,
                      g::Family::toy}) {
    p.family = family;
    for (double radius : {1.65, 2.0})
      for (double theta : {1.0, 1.5, 2.0})
        for (double phi : {-.7, .3, 2.1}) {
          auto bl = o::launch(p, {0, radius, theta, phi}, {-.6, .4, .2});
          auto frame = o::circular_frame(o::metric(p, bl));
          const auto derivative = o::rhs(p, bl);
          for (auto patch : {o::Chart::north, o::Chart::south}) {
            const auto other =
                patch == o::Chart::north ? o::Chart::south : o::Chart::north;
            auto a = o::transform_state(bl, o::Chart::boyer_lindquist, patch);
            auto b = o::transform_state(a, patch, other);
            roundtrip = std::max(
                {roundtrip,
                 difference(bl, o::transform_state(a, patch,
                                                   o::Chart::boyer_lindquist)),
                 difference(a, o::transform_state(b, other, patch))});
            killing = std::max(killing,
                               std::abs(o::axial_momentum(a, patch) - bl[7]));
            auto f =
                o::transform_frame(frame, bl, o::Chart::boyer_lindquist, patch);
            auto f2 = o::transform_frame(f, a, patch, other);
            auto m = o::angular_metric(p, b);
            for (int i = 0; i < 4; ++i) {
              pairing =
                  std::max(pairing, std::abs(o::frequency(b, f2.basis[i]) -
                                             o::frequency(bl, frame.basis[i])));
              for (int j = 0; j < 4; ++j) {
                double dot = 0;
                for (int k = 0; k < 4; ++k)
                  for (int l = 0; l < 4; ++l)
                    dot += m.metric[k][l] * f2.basis[i][k] * f2.basis[j][l];
                frame_error =
                    std::max(frame_error,
                             std::abs(dot - (i == j ? (i == 0 ? -1 : 1) : 0)));
              }
            }
            null = std::max(null, o::angular_null_residual(m, b));
            auto velocity = o::transform_vector(
                {derivative[0], derivative[1], derivative[2], derivative[3]},
                bl, o::Chart::boyer_lindquist, patch);
            auto da = o::angular_rhs(p, a);
            for (int i = 0; i < 4; ++i)
              flow = std::max(flow, std::abs(da[i] - velocity[i]) /
                                        (1 + std::abs(velocity[i])));
          }
        }
  }
  check("state and covector chart round trips", roundtrip, 3e-14);
  check("transformed frame orthonormality", frame_error, 5e-13);
  check("canonical covector/vector pairing", pairing, 5e-14);
  check("axial Killing momentum invariance", killing, 3e-14);
  check("transformed launch nullness", null, 3e-14);
  check("Hamiltonian coordinate velocity transform", flow, 3e-14);
  bool rejected = false;
  try {
    (void)o::transform_state({0, 2, 0, 0, -1, 0, 1, 0}, o::Chart::north,
                             o::Chart::south);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require("singular opposite-pole transition rejected", rejected);
}
int main(int argc, char **argv) {
  try {
    transformations();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    require("uncaught exception", false);
  }
  bool pass = true;
  for (const auto &c : checks)
    pass = pass && c["passed"].get<bool>();
  if (argc == 3 && std::string(argv[1]) == "--report") {
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
  } else if (argc != 1)
    return 2;
  std::cout << (pass ? "PASS " : "FAIL ") << checks.size()
            << " angular optics checks\n";
  return pass ? 0 : 1;
}
