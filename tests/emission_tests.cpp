// SPDX-License-Identifier: GPL-3.0-or-later
//
// Disk emitter prescriptions against independent closed forms.
// Kerr prograde circular equatorial geodesics (M = 1, signed spin a, +phi
// rotation; Bardeen, Press & Teukolsky 1972):
//   Omega = 1 / (r^{3/2} + a)
//   E = (r^{3/2} - 2 r^{1/2} + a) / (r^{3/4} sqrt(r^{3/2} - 3 r^{1/2} + 2 a))
//   L = (r^2 - 2 a r^{1/2} + a^2) / (r^{3/4} sqrt(r^{3/2} - 3 r^{1/2} + 2 a))
// and the prograde ISCO from Z1, Z2. None of this uses production geometry.
#include "gravastar/optics/ray.hpp"
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
g::Parameters kerr(double a) {
  g::Parameters p;
  p.family = g::Family::kerr;
  p.mass = 1;
  p.spin = a;
  return p;
}
// ISCO of the +phi circular orbit: co-rotating for a > 0, counter-rotating
// (BPT lower sign with |a|) for a < 0.
double isco(double a) {
  const double s = std::abs(a);
  const double z1 = 1 + std::cbrt(1 - s * s) * (std::cbrt(1 + s) + std::cbrt(1 - s));
  const double z2 = std::sqrt(3 * s * s + z1 * z1);
  const double root = std::sqrt((3 - z1) * (3 + z1 + 2 * z2));
  return 3 + z2 + (a < 0 ? root : -root);
}
// Photon orbit of the +phi direction (Bardeen 1972), signed a.
double photon_orbit(double a) {
  return 2 * (1 + std::cos(2 * std::acos(-a) / 3));
}
o::State equator(double r) { return {0, r, std::numbers::pi / 2, 0}; }

void circular_orbits() {
  double omega_error = 0, energy_error = 0, momentum_error = 0, toy_error = 0;
  for (double a : {-.875, 0., .5, .875}) {
    // Counter-rotating orbits exist only outside the photon orbit (3.888 for
    // a = -0.875): inside it the prescription must refuse.
    for (double r = 3; a < 0 && r < photon_orbit(a); r += .25)
      require("Kerr a=" + u::Json(a).dump() + " r=" + u::Json(r).dump() +
                  " inside the photon orbit has no timelike orbit",
              !o::keplerian_omega(o::metric(kerr(a), equator(r))));
    for (double r = a < 0 ? 4 : 3; r <= 6.0001; r += .25) {
      const auto e = o::metric(kerr(a), equator(r));
      const auto omega = o::keplerian_omega(e);
      require("Kerr a=" + u::Json(a).dump() + " r=" + u::Json(r).dump() +
                  " has a prograde circular orbit",
              omega.has_value());
      if (!omega)
        continue;
      const double s = std::sqrt(r), r32 = r * s, r34 = std::sqrt(r32);
      const double root = std::sqrt(r32 - 3 * s + 2 * a);
      omega_error = std::max(omega_error,
                             std::abs(*omega - 1 / (r32 + a)) * (r32 + a));
      const auto u_up = o::circular_frame(e, *omega).basis[0];
      const double ut = e.metric[0][0] * u_up[0] + e.metric[0][3] * u_up[3];
      const double up = e.metric[3][0] * u_up[0] + e.metric[3][3] * u_up[3];
      const double E = (r32 - 2 * s + a) / (r34 * root);
      const double L = (r * r - 2 * a * s + a * a) / (r34 * root);
      energy_error = std::max(energy_error, std::abs(-ut - E) / E);
      momentum_error = std::max(momentum_error, std::abs(up - L) / std::abs(L));
      if (a == .875) {
        // The toy preset's exterior beyond r_out = 1.8 is exact Kerr.
        const auto toy = o::keplerian_omega(o::metric(g::Parameters{}, equator(r)));
        toy_error = std::max(toy_error, toy ? std::abs(*toy - *omega) : 1.);
      }
    }
  }
  check("Kerr Keplerian angular speed vs closed form (relative)", omega_error,
        1e-14);
  check("Kerr circular-orbit energy -u_t vs closed form", energy_error, 1e-14);
  check("Kerr circular-orbit angular momentum u_phi vs closed form",
        momentum_error, 1e-14);
  check("toy exterior Keplerian speed equals Kerr", toy_error, 1e-15);

  // Flat space: no gravity, so the "circular geodesic" is static.
  g::Parameters flat;
  flat.family = g::Family::flat;
  flat.mass = flat.spin = flat.cosmological_constant = 0;
  const auto omega = o::keplerian_omega(o::metric(flat, equator(4)));
  require("flat Keplerian speed is zero", omega && *omega == 0);
}

void supported_domain() {
  for (double a : {.875, 0., -.875}) {
    const double r = isco(a);
    const std::string tag = "Kerr a=" + u::Json(a).dump() + " ISCO " +
                            u::Json(r).dump();
    require(tag + ": annulus just outside is supported",
            o::keplerian_annulus_supported(kerr(a), r * 1.01, r + 4));
    require(tag + ": annulus reaching inside is rejected",
            !o::keplerian_annulus_supported(kerr(a), r * 0.95, r + 4));
  }
  require("scene annulus [3, 6] supported for the toy preset",
          o::keplerian_annulus_supported(g::Parameters{}, 3, 6));
  // Wide annuli: with 64 samples the first spacing is far larger than the
  // distance to the ISCO, so a chord test between samples cannot see it.
  // (Ultrareview finding: [5.6, 100] in Schwarzschild was accepted.)
  require("Schwarzschild [5.6, 100] reaching inside ISCO 6 is rejected",
          !o::keplerian_annulus_supported(kerr(0), 5.6, 100));
  require("Schwarzschild [6.01, 100] outside ISCO 6 is supported",
          o::keplerian_annulus_supported(kerr(0), 6.01, 100));
  for (double a : {.875, -.875}) {
    const double r = isco(a);
    require("Kerr a=" + u::Json(a).dump() + " wide annulus from 0.99 ISCO rejected",
            !o::keplerian_annulus_supported(kerr(a), .99 * r, r + 60));
    require("Kerr a=" + u::Json(a).dump() + " wide annulus from 1.001 ISCO supported",
            o::keplerian_annulus_supported(kerr(a), 1.001 * r, r + 60));
  }
}

void redshift() {
  // Kerr equatorial metric, independent of production geometry.
  const double a = .875;
  const auto p = kerr(a);
  o::Settings c;
  c.absolute_tolerance = c.relative_tolerance = 1e-12;
  c.disk = true;
  c.disk_inner = 3;
  c.disk_outer = 6;
  c.capture_radius = 1.55;
  c.maximum_affine = 60;
  std::size_t hits = 0;
  double error = 0, zamo_difference = 0;
  // Sight components (e_r, e_theta, e_phi); +e_theta points toward the disk.
  for (double down : {.45, .55, .65})
    for (double right : {-.2, 0., .2}) {
      const auto launch = o::launch(p, {0, 8, 1.0471975511965976, 0},
                                    {-1, down, right});
      const auto t = o::trace(p, launch, c);
      if (t.outcome != o::Outcome::emitted)
        continue;
      ++hits;
      const auto &y = t.final.state;
      const double r = y[1];
      const double gtt = -(1 - 2 / r), gtp = -2 * a / r,
                   gpp = r * r + a * a + 2 * a * a / r;
      const double omega = 1 / (r * std::sqrt(r) + a);
      const double ut =
          1 / std::sqrt(-(gtt + 2 * omega * gtp + omega * omega * gpp));
      const double nu_emit = -ut * (y[4] + omega * y[7]);
      const auto frame = o::circular_frame(o::metric(p, launch));
      const double nu_obs = o::frequency(launch, frame.basis[0]);
      const auto e = o::disk_emission(p, y, nu_obs, 3,
                                      o::DiskMotion::keplerian_prograde);
      error = std::max({error, std::abs(e.emitted_frequency - nu_emit) / nu_emit,
                        std::abs(e.redshift - nu_obs / nu_emit) /
                            (nu_obs / nu_emit),
                        std::abs(e.linear_intensity -
                                 std::pow(nu_obs / nu_emit, 4) *
                                     std::pow(3 / r, 3)) /
                            e.linear_intensity});
      const auto z = o::disk_emission(p, y, nu_obs, 3);
      zamo_difference =
          std::max(zamo_difference, std::abs(z.redshift - e.redshift));
    }
  require("at least four camera rays hit the disk", hits >= 4);
  check("Keplerian disk frequency, redshift and g^4 intensity vs closed form",
        error, 1e-14);
  require("Keplerian and ZAMO prescriptions differ on the disk",
          zamo_difference > 1e-3);
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--report"))
      throw std::invalid_argument("expected [--report results/FILE.json]");
    circular_orbits();
    supported_domain();
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
              << " emission checks\n";
    return pass ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "emission: " << error.what() << '\n';
    return 1;
  }
}
