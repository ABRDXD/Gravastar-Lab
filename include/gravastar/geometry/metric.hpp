// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <string>
#include <variant>

namespace gravastar {
using Matrix = std::array<std::array<double,4>,4>;
using Tensor3 = std::array<Matrix,4>;
using Tensor4 = std::array<Tensor3,4>;
struct Parameters { double mass, spin, lambda, r_in, r_out; };
enum class Region { automatic, core, shell, exterior };
struct DomainError { std::string code; };
struct Evaluation {
    Matrix g{}, inverse{};
    Tensor3 first{}; // first[k][a][b] = partial_k g_ab
    Tensor4 second{}; // second[k][l][a][b]
    Matrix radial_third{};
    double delta{}, determinant{}, mu{}, lambda_weight{};
};
using MetricResult = std::variant<Evaluation,DomainError>;
class Geometry {
public:
    explicit Geometry(Parameters parameters);
    const Parameters& parameters() const { return parameters_; }
    MetricResult evaluate(double r, double theta, Region region=Region::automatic) const;
private:
    const Parameters parameters_;
};
struct Curvature {
    double scalar{}, kretschmann{};
    Matrix ricci{};
    Tensor4 riemann_lower{}; // R_abcd
};
Curvature curvature(const Evaluation& metric);
} // namespace gravastar
