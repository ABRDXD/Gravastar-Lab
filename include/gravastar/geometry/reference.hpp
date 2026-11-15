// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/geometry/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace gravastar::geometry::reference {

// 113-bit IEEE 754 quadruple precision (~34 decimal digits, eps ~ 1.93e-34).
// Provides an independent high-precision reference for the expanded metric,
// 4th-order centered finite differences, and metric-Hessian curvature without
// using the production Jet automatic differentiation or block inverse.
#if defined(__SIZEOF_FLOAT128__)
__extension__ using Real = __float128;
#else
using Real = long double;
#endif

using MatrixQ = std::array<std::array<Real, 4>, 4>;
using Tensor3Q = std::array<MatrixQ, 4>;
using Tensor4Q = std::array<Tensor3Q, 4>;

inline Real abs_q(Real x) { return x < 0 ? -x : x; }

inline Real sqrt_q(Real x) {
    if (x < 0) throw std::domain_error("sqrt_q of negative value");
    if (x == 0) return 0;
    Real g = static_cast<Real>(std::sqrt(static_cast<long double>(x)));
    for (int i = 0; i < 6; ++i) g = static_cast<Real>(0.5) * (g + x / g);
    return g;
}

inline Real pi_q() {
    Real p = static_cast<Real>(3.141592653589793238462643383279502884L);
    return p;
}

// Convergent Taylor series for sin(x) and cos(x) on [0, pi].
inline std::pair<Real, Real> trig_q(Real x) {
    Real sin_term = x, sin_total = x;
    for (int k = 1; k <= 200; ++k) {
        int j = 2 * k + 1;
        sin_term *= -x * x / static_cast<Real>(j * (j - 1));
        Real prev = sin_total;
        sin_total += sin_term;
        if (sin_total == prev) break;
    }
    Real cos_term = 1, cos_total = 1;
    for (int k = 1; k <= 200; ++k) {
        int j = 2 * k;
        cos_term *= -x * x / static_cast<Real>(j * (j - 1));
        Real prev = cos_total;
        cos_total += cos_term;
        if (cos_total == prev) break;
    }
    return {sin_total, cos_total};
}

struct ParametersQ {
    Family family{Family::toy};
    Real M{1};
    Real a{static_cast<Real>(7) / static_cast<Real>(8)};
    Real Lambda{static_cast<Real>(1) / static_cast<Real>(2)};
    Real r_in{static_cast<Real>(3) / static_cast<Real>(2)};
    Real r_out{static_cast<Real>(9) / static_cast<Real>(5)};
};

inline MatrixQ metric_q(const ParametersQ& p, Real r, Real theta,
                        Region region = Region::automatic) {
    if (region == Region::automatic) {
        if (r <= p.r_in) region = Region::core;
        else if (r >= p.r_out) region = Region::exterior;
        else region = Region::shell;
    }
    Real m = 0, L = 0;
    if (p.family == Family::flat) {
        m = 0;
        L = 0;
    } else if (p.family == Family::kerr || region == Region::exterior) {
        m = p.M;
        L = 0;
    } else if (p.family == Family::de_sitter || region == Region::core) {
        m = 0;
        L = p.Lambda;
    } else {
        // Paper's x in [-1, 1] septic polynomial, distinct from C++'s u in [0, 1] form.
        Real x = (static_cast<Real>(2) * r - p.r_in - p.r_out) / (p.r_out - p.r_in);
        Real x2 = x * x, x3 = x2 * x, x5 = x3 * x2, x7 = x5 * x2;
        Real mu = static_cast<Real>(1) / static_cast<Real>(2)
                + (static_cast<Real>(35) * x - static_cast<Real>(35) * x3
                   + static_cast<Real>(21) * x5 - static_cast<Real>(5) * x7)
                  / static_cast<Real>(32);
        m = p.M * mu;
        L = p.Lambda * (static_cast<Real>(1) - mu);
    }
    auto [s, c] = trig_q(theta);
    Real r2 = r * r, a2 = p.a * p.a, s2 = s * s, c2 = c * c;
    Real rho = r2 + a2 * c2;
    Real delta = r2 - static_cast<Real>(2) * m * r + a2
               - L * r2 * (r2 + a2) / static_cast<Real>(3);
    Real angular = static_cast<Real>(1) + L * a2 * c2 / static_cast<Real>(3);
    Real xi = static_cast<Real>(1) + L * a2 / static_cast<Real>(3);
    Real denom = rho * xi * xi;

    MatrixQ g{};
    // Expanded form rather than production rank-one/block expression.
    g[0][0] = (-r2 + static_cast<Real>(2) * m * r - a2 * c2
               + L * (r2 * r2 + a2 * r2 + a2 * a2 * c2 * s2) / static_cast<Real>(3))
            / denom;
    g[0][3] = g[3][0] =
        -p.a * s2 * (static_cast<Real>(2) * m * r + L * (r2 + a2) * rho / static_cast<Real>(3))
        / denom;
    g[3][3] = s2 * ((r2 + a2) * rho + static_cast<Real>(2) * m * r * a2 * s2
                    + L * a2 * (r2 + a2) * rho / static_cast<Real>(3))
            / denom;
    g[1][1] = rho / delta;
    g[2][2] = rho / angular;
    return g;
}

// General pivoted Gauss-Jordan 4x4 inversion, independent of the C++ 2x2 block inverse.
inline MatrixQ inverse_q(const MatrixQ& g) {
    std::array<std::array<Real, 8>, 4> aug{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) aug[i][j] = g[i][j];
        aug[i][4 + i] = 1;
    }
    for (int k = 0; k < 4; ++k) {
        int pivot = k;
        for (int i = k + 1; i < 4; ++i)
            if (abs_q(aug[i][k]) > abs_q(aug[pivot][k])) pivot = i;
        std::swap(aug[k], aug[pivot]);
        Real scale = aug[k][k];
        if (scale == 0) throw std::runtime_error("singular reference metric");
        for (int j = 0; j < 8; ++j) aug[k][j] /= scale;
        for (int i = 0; i < 4; ++i) {
            if (i == k) continue;
            Real q = aug[i][k];
            for (int j = 0; j < 8; ++j) aug[i][j] -= q * aug[k][j];
        }
    }
    MatrixQ inv{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) inv[i][j] = aug[i][4 + j];
    return inv;
}

struct DerivativesQ {
    MatrixQ g{};
    Tensor3Q first{};
    Tensor4Q second{};
    MatrixQ drrr{};
    MatrixQ extrinsic_curvature{};
};

// 4th-order centered finite differences for first, second, mixed, and 3rd radial derivatives.
inline DerivativesQ derivatives_q(const ParametersQ& p, Real r, Real theta, Real h,
                                  Region region = Region::automatic) {
    DerivativesQ out{};
    out.g = metric_q(p, r, theta, region);


}  // namespace gravastar::geometry::reference
