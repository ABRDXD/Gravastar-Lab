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

    const std::array<int, 4> offsets{-2, -1, 1, 2};
    const std::array<Real, 4> w1{
        static_cast<Real>(1) / static_cast<Real>(12),
        -static_cast<Real>(2) / static_cast<Real>(3),
        static_cast<Real>(2) / static_cast<Real>(3),
        -static_cast<Real>(1) / static_cast<Real>(12),
    };
    const std::array<int, 5> offsets2{-2, -1, 0, 1, 2};
    const std::array<Real, 5> w2{
        -static_cast<Real>(1) / static_cast<Real>(12),
        static_cast<Real>(4) / static_cast<Real>(3),
        -static_cast<Real>(5) / static_cast<Real>(2),
        static_cast<Real>(4) / static_cast<Real>(3),
        -static_cast<Real>(1) / static_cast<Real>(12),
    };
    const std::array<int, 6> offsets3{-3, -2, -1, 1, 2, 3};
    const std::array<Real, 6> w3{
        static_cast<Real>(1) / static_cast<Real>(8),
        -static_cast<Real>(1),
        static_cast<Real>(13) / static_cast<Real>(8),
        -static_cast<Real>(13) / static_cast<Real>(8),
        static_cast<Real>(1),
        -static_cast<Real>(1) / static_cast<Real>(8),
    };

    std::array<MatrixQ, 7> r_samples{};
    for (int idx = -3; idx <= 3; ++idx)
        r_samples[idx + 3] = metric_q(p, r + static_cast<Real>(idx) * h, theta, region);

    std::array<MatrixQ, 5> th_samples{};
    for (int idx = -2; idx <= 2; ++idx)
        th_samples[idx + 2] = metric_q(p, r, theta + static_cast<Real>(idx) * h, region);

    std::array<std::array<MatrixQ, 4>, 4> mixed_samples{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            mixed_samples[i][j] = metric_q(p, r + static_cast<Real>(offsets[i]) * h,
                                           theta + static_cast<Real>(offsets[j]) * h, region);

    const Real h2 = h * h;
    const Real h3 = h2 * h;
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            Real d1r = 0, d1th = 0;
            for (int m = 0; m < 4; ++m) {
                d1r += w1[m] * r_samples[offsets[m] + 3][a][b];
                d1th += w1[m] * th_samples[offsets[m] + 2][a][b];
            }
            out.first[1][a][b] = d1r / h;
            out.first[2][a][b] = d1th / h;

            Real d2r = 0, d2th = 0;
            for (int m = 0; m < 5; ++m) {
                d2r += w2[m] * r_samples[offsets2[m] + 3][a][b];
                d2th += w2[m] * th_samples[offsets2[m] + 2][a][b];
            }
            out.second[1][1][a][b] = d2r / h2;
            out.second[2][2][a][b] = d2th / h2;

            Real dmix = 0;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    dmix += w1[i] * w1[j] * mixed_samples[i][j][a][b];
            out.second[1][2][a][b] = out.second[2][1][a][b] = dmix / h2;

            Real d3r = 0;
            for (int m = 0; m < 6; ++m)
                d3r += w3[m] * r_samples[offsets3[m] + 3][a][b];
            out.drrr[a][b] = d3r / h3;
        }
    }
    if (out.g[1][1] > 0) {
        Real inv_two_sqrt_grr = static_cast<Real>(0.5) / sqrt_q(out.g[1][1]);
        for (int a : {0, 2, 3})
            for (int b : {0, 2, 3})
                out.extrinsic_curvature[a][b] = out.first[1][a][b] * inv_two_sqrt_grr;
    }
    return out;
}

struct CurvatureQ {
    MatrixQ ricci{};
    Real ricci_scalar{};
    Real kretschmann{};
    Tensor4Q riemann_lower{};
};

// R_abcd from metric Hessians and first-kind Christoffel products, avoiding
// the production implementation's differentiated inverse/connection.
inline CurvatureQ curvature_q(const MatrixQ& g, const Tensor3Q& dg, const Tensor4Q& ddg) {
    MatrixQ inv = inverse_q(g);
    Tensor3Q lower_gamma{};
    Tensor3Q upper_gamma{};
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c)
                lower_gamma[a][b][c] =
                    (dg[b][a][c] + dg[c][a][b] - dg[a][b][c]) / static_cast<Real>(2);

    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c) {
                Real sum = 0;
                for (int e = 0; e < 4; ++e) sum += inv[a][e] * lower_gamma[e][b][c];
                upper_gamma[a][b][c] = sum;
            }

    CurvatureQ out{};
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c)
                for (int d = 0; d < 4; ++d) {
                    Real v = (ddg[b][c][a][d] + ddg[a][d][b][c]
                              - ddg[b][d][a][c] - ddg[a][c][b][d])
                           / static_cast<Real>(2);
                    for (int e = 0; e < 4; ++e)
                        v += lower_gamma[e][b][c] * upper_gamma[e][a][d]
                           - lower_gamma[e][b][d] * upper_gamma[e][a][c];
                    out.riemann_lower[a][b][c][d] = v;
                }

    for (int b = 0; b < 4; ++b)
        for (int d = 0; d < 4; ++d) {
            Real sum = 0;
            for (int a = 0; a < 4; ++a)
                for (int c = 0; c < 4; ++c)
                    sum += inv[a][c] * out.riemann_lower[a][b][c][d];
            out.ricci[b][d] = sum;
            out.ricci_scalar += inv[b][d] * sum;
        }

    Tensor4Q raised = out.riemann_lower;
    for (int axis = 0; axis < 4; ++axis) {
        Tensor4Q next{};
        for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 4; ++b)
                for (int c = 0; c < 4; ++c)
                    for (int d = 0; d < 4; ++d) {
                        std::array<int, 4> indices{a, b, c, d};
                        Real value = 0;
                        for (int e = 0; e < 4; ++e) {
                            auto src = indices;
                            src[axis] = e;
                            value += inv[indices[axis]][e] * raised[src[0]][src[1]][src[2]][src[3]];
                        }
                        next[a][b][c][d] = value;
                    }
        raised = next;
    }

    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c)
                for (int d = 0; d < 4; ++d)
                    out.kretschmann += out.riemann_lower[a][b][c][d] * raised[a][b][c][d];
    return out;
}

inline double normalized_error(double actual, Real expected) {
    Real diff = abs_q(static_cast<Real>(actual) - expected);
    Real denom = static_cast<Real>(1) + abs_q(expected);
    return static_cast<double>(diff / denom);
}

inline double normalized_error_matrix(const Matrix& actual, const MatrixQ& expected) {
    double max_err = 0.0;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            max_err = std::max(max_err, normalized_error(actual[i][j], expected[i][j]));
    return max_err;
}

}  // namespace gravastar::geometry::reference
