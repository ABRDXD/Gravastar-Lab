// SPDX-License-Identifier: GPL-3.0-or-later
//
// FP32 GPU fixed-step RK4 equatorial rays (shaders/gravastar_geodesic.comp)
// compared with the independent equatorial turning radius in
// tests/equatorial_reference.hpp.
//
// Ported from backup/astra-work. The old test compared the GPU with that
// branch's fixed-step CPU integrator using a 2e-3 radius tolerance. Here the
// comparison is against the exact root of the equatorial radial potential, so
// the measured number is the GPU kernel's own turning-radius error. The kernel
// reports its smallest sampled radius, not a refined turning point.
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/rendering/vulkan/ComputeContext.hpp"
#include "gravastar/tools/testing.hpp"
#include "equatorial_reference.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <vector>

using namespace gravastar;
using namespace gravastar::rendering::vulkan;
namespace ref = gravastar::test_reference;

namespace {
struct RayInput {
  float x0[4];           // t, r, theta, phi
  float p0[4];           // p_t, p_r, p_theta, p_phi
  float params_geom[4];  // M, a, Lambda, unused
  float params_radii[4]; // r_in, r_out, unused, unused
  float limits[4];       // escape_radius, h0, h_min, h_max
  uint32_t options[4];   // max_steps, unused, unused, unused
};
struct RayOutput {
  float final_x[4];
  float final_p[4];
  float stats[4];     // min_r, max_null_drift, final_lambda, outcome
  uint32_t counts[4]; // steps, crossed_shell_in, crossed_shell_out, status
};
static_assert(sizeof(RayInput) == 96 && sizeof(RayOutput) == 64);

std::vector<uint32_t> load_spirv(const std::string &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    throw std::runtime_error("Failed to open SPIR-V file: " + path);
  const auto size = static_cast<std::size_t>(stream.tellg());
  stream.seekg(0);
  std::vector<uint32_t> spirv(size / sizeof(uint32_t));
  stream.read(reinterpret_cast<char *>(spirv.data()), size);
  return spirv;
}
} // namespace

int main() {
  try {
    geometry::Parameters p; // toy preset: M=1, a=7/8, Lambda=1/2, 1.5, 1.8
    const ref::Model q{p.mass, p.spin, p.cosmological_constant, p.r_in, p.r_out};
    const double r0 = 10, th0 = std::numbers::pi / 2;
    const double impacts[] = {+1.80, +1.95, -3.60, -3.70};
    const auto evaluation = geometry::evaluate(p, r0, th0);
    GRAVASTAR_REQUIRE(std::holds_alternative<geometry::Evaluation>(evaluation),
                      "valid evaluation at the launch point");
    const auto &gi = std::get<geometry::Evaluation>(evaluation).inverse;

    // Kernel settings kept from the old test: h0=0.005, clamp [1e-5, 0.05],
    // 25000 steps, escape at r=25. The kernel steps forward in affine
    // parameter, so the launch momentum is future-directed and ingoing.
    const uint32_t max_steps = 25000;
    std::vector<RayInput> inputs(std::size(impacts));
    for (std::size_t i = 0; i < inputs.size(); ++i) {
      const double b = impacts[i], pt = -1, pphi = b;
      const double quad =
          gi[0][0] * pt * pt + 2 * gi[0][3] * pt * pphi + gi[3][3] * pphi * pphi;
      const double pr = -std::sqrt(-quad / gi[1][1]);
      inputs[i] = RayInput{
          {0, float(r0), float(th0), 0},
          {float(pt), float(pr), 0, float(pphi)},
          {float(p.mass), float(p.spin), float(p.cosmological_constant), 0},
          {float(p.r_in), float(p.r_out), 0, 0},
          {25.0f, 0.005f, 1e-5f, 0.05f},
          {max_steps, 0, 0, 0}};
    }

    ComputeOptions options;
    options.validation = ValidationMode::disabled;
    ComputeContext ctx(options);
    const auto spirv = load_spirv(GRAVASTAR_GEODESIC_SPV);
    const std::span<const std::byte> input_span(
        reinterpret_cast<const std::byte *>(inputs.data()),
        inputs.size() * sizeof(RayInput));
    const auto result =
        ctx.dispatch(spirv, input_span, inputs.size() * sizeof(RayOutput), 1);
    std::vector<RayOutput> outputs(inputs.size());
    GRAVASTAR_REQUIRE(result.output.size() == outputs.size() * sizeof(RayOutput),
                      "GPU output size");
    std::memcpy(outputs.data(), result.output.data(), result.output.size());

    std::cout << "Device: " << ctx.selected_device().name << ", "
              << result.gpu_milliseconds << " ms\n"
              << std::setprecision(10)
              << "b, exact turning radius, GPU sampled min r, |error|, "
                 "exact enters core, GPU enters core, steps, null drift\n";
    double worst = 0;
    for (std::size_t i = 0; i < outputs.size(); ++i) {
      const auto &out = outputs[i];
      const double exact = double(ref::turning_radius(q, impacts[i], r0));
      const double gpu = out.stats[0];
      const double error = std::abs(gpu - exact);
      const bool exact_core = exact < p.r_in, gpu_core = out.counts[1] == 1;
      worst = std::max(worst, error);
      std::cout << impacts[i] << ", " << exact << ", " << gpu << ", " << error
                << ", " << exact_core << ", " << gpu_core << ", "
                << out.counts[0] << ", " << out.stats[1] << '\n';
      GRAVASTAR_REQUIRE(out.counts[3] == 1, "GPU kernel status OK");
      GRAVASTAR_REQUIRE(out.stats[3] == 0.0f || out.stats[3] == 1.0f,
                        "GPU ray escaped (0) or passed the core and escaped (1)");
      GRAVASTAR_REQUIRE(out.counts[0] < max_steps, "GPU ray within step budget");
      GRAVASTAR_REQUIRE(gpu_core == exact_core,
                        "GPU core entry matches the exact turning region");
      GRAVASTAR_REQUIRE(gpu_core == (out.stats[3] == 1.0f),
                        "GPU core flag consistent with its outcome code");
      GRAVASTAR_REQUIRE(error < GRAVASTAR_GPU_TURNING_TOLERANCE,
                        "GPU sampled turning radius vs exact root");
    }
    std::cout << "Worst GPU turning-radius error: " << worst
              << " (ceiling " << GRAVASTAR_GPU_TURNING_TOLERANCE << ")\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "GPU ray comparison failed: " << e.what() << '\n';
    return 1;
  }
}
