// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <variant>

namespace gravastar::geometry {

using Matrix = std::array<std::array<double, 4>, 4>;
using Tensor3 = std::array<Matrix, 4>;
using Tensor4 = std::array<Tensor3, 4>;

enum class Family { toy, kerr, de_sitter, flat };
enum class Region { automatic, core, shell, exterior };
std::string_view family_name(Family family);
std::variant<Family, std::string> parse_family(std::string_view text);

// Geometric units. M >= 0 includes flat/de Sitter reference cases.
// All fields must be finite, Lambda >= 0, and 0 < r_in < r_out.
struct Parameters {
  Family family{Family::toy};
  double mass{1};
  double spin{0.875};
  double cosmological_constant{0.5};
  double r_in{1.5};
  double r_out{1.8};
};

struct Error {
  std::string code;
  std::string message;
};

struct Evaluation {
  Matrix metric{};
  Matrix inverse{};
  Tensor3 dg{};  // dg[k][i][j] = partial_k g_ij, order t,r,theta,phi.
  Tensor4 ddg{}; // ddg[k][l][i][j] = partial_k partial_l g_ij.
  Matrix drrr{};
  Matrix ricci{};
  double ricci_scalar{};
  double kretschmann{};
  double determinant{};
  double mu{}, lambda{}, sigma{}, delta{}, delta_theta{}, xi{};
};

using Result = std::variant<Evaluation, Error>;

// Bounded Boyer-Lindquist chart: r>0, 0<theta<pi; axes and numerically
// singular Delta surfaces are rejected. No central continuation is implied.
// Explicit regions apply only to Family::toy and include their physical closed
// intervals. Region::shell evaluates the polynomial at either join without
// clamping, so one-sided endpoint jets can be tested directly.
Result evaluate(const Parameters &parameters, double r, double theta,
                Region region = Region::automatic);

} // namespace gravastar::geometry
