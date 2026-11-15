// SPDX-License-Identifier: GPL-3.0-or-later
// Independent Native C++20 Geodesic Cross-Validation Suite for GravastarLab Stage P2
//
// Scientific Reference:
//   Sasa Ilijic and Andrew DeBenedictis, "On a Class of Exact Arbitrarily
//   Differentiable de Sitter Cores with Kerr Exteriors: Possible gravastar or
//   regular black hole mimickers", arXiv:2309.00551v2 [gr-qc] (2024),
//   Int. J. Mod. Phys. D 34, 2550004 (2025).
//   https://arxiv.org/abs/2309.00551
//
// Repository Reference:
//   https://github.com/ABRDXD/Gravastar-Lab (branch: backup/astra-work)
//
// Build & Run (strict C++20, zero external dependencies):
//   g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off tests/geodesic_validation.cpp -o build/geodesic_validation && ./build/geodesic_validation

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <type_traits>

namespace gravastar::validation {

__extension__ using float128_t = __float128;

// =============================================================================
// 1. Scalar Math Abstraction (Supports IEEE `double` and 113-bit `__float128`)
// =============================================================================

inline double abs_s(double x) noexcept { return std::abs(x); }
inline float128_t abs_s(float128_t x) noexcept { return x < 0 ? -x : x; }

inline double max_s(double a, double b) noexcept { return a > b ? a : b; }
inline float128_t max_s(float128_t a, float128_t b) noexcept { return a > b ? a : b; }

inline double min_s(double a, double b) noexcept { return a < b ? a : b; }
inline float128_t min_s(float128_t a, float128_t b) noexcept { return a < b ? a : b; }

inline double sqrt_s(double x) noexcept { return std::sqrt(x); }
inline float128_t sqrt_s(float128_t x) noexcept {
    if (x <= 0) return 0;
    float128_t g = static_cast<float128_t>(std::sqrt(static_cast<double>(x)));
    for (int i = 0; i < 6; ++i) {
        g = static_cast<float128_t>(0.5) * (g + x / g);
    }
    return g;
}

inline constexpr float128_t pi_q() noexcept {
    // 3.141592653589793238462643383279502884197Q constructed from exact integer limbs
    return static_cast<float128_t>(3)
         + static_cast<float128_t>(141592653589793238ULL) / static_cast<float128_t>(1000000000000000000ULL)
         + static_cast<float128_t>(462643383279502884ULL)
             / (static_cast<float128_t>(1000000000000000000ULL) * static_cast<float128_t>(1000000000000000000ULL));
}

inline void sincos_s(double theta, double& s_out, double& c_out) noexcept {
    s_out = std::sin(theta);
    c_out = std::cos(theta);
}

inline void sincos_s(float128_t theta, float128_t& s_out, float128_t& c_out) noexcept {
    // Range-reduce theta by 4 (sin(4x), cos(4x)) and sum Taylor series to 113-bit precision
    const float128_t x = theta / static_cast<float128_t>(4);
    const float128_t x2 = x * x;
    float128_t s = x;
    float128_t c = 1;
    float128_t s_term = x;
    float128_t c_term = 1;
    for (int k = 1; k <= 24; ++k) {
        c_term = -c_term * x2 / static_cast<float128_t>((2 * k - 1) * (2 * k));
        s_term = -s_term * x2 / static_cast<float128_t>((2 * k) * (2 * k + 1));
        c += c_term;
        s += s_term;
    }
    // Double-angle twice: x -> 2x -> 4x
    float128_t s2 = static_cast<float128_t>(2) * s * c;
    float128_t c2 = c * c - s * s;
    s_out = static_cast<float128_t>(2) * s2 * c2;
    c_out = c2 * c2 - s2 * s2;
}

inline std::string format_q(float128_t val, int digits = 30) {
    std::ostringstream oss;
    if (val < 0) {
        oss << '-';
        val = -val;
    } else {
        oss << '+';
    }
    std::uint64_t int_part = static_cast<std::uint64_t>(val);
    oss << int_part << '.';
    float128_t frac = val - static_cast<float128_t>(int_part);
    for (int i = 0; i < digits; ++i) {
        frac *= static_cast<float128_t>(10);
        int d = static_cast<int>(frac);
        if (d < 0) d = 0;
        if (d > 9) d = 9;
        oss << d;
        frac -= static_cast<float128_t>(d);
    }
    return oss.str();
}

// =============================================================================
// 2. Gravastar Model Parameters & Septic C^3 Transition (arXiv:2309.00551v2)
// =============================================================================

template <typename T>
struct Parameters {
    T M      = static_cast<T>(1);
    T a      = static_cast<T>(7) / static_cast<T>(8);   // 0.875
    T Lambda = static_cast<T>(1) / static_cast<T>(2);   // 0.5
    T r_in   = static_cast<T>(3) / static_cast<T>(2);   // 1.5
    T r_out  = static_cast<T>(9) / static_cast<T>(5);   // 1.8
};

template <typename T>
struct TransitionEval {
    T mu{};
    T dmu_dr{};
    T lam{};
    T dlam_dr{};
};

template <typename T>
inline void smoothstep_c3(T u, T& s_val, T& ds_du) noexcept {
    // S(u)  = 35 u^4 - 84 u^5 + 70 u^6 - 20 u^7 = u^4 (35 - 84 u + 70 u^2 - 20 u^3)
    // S'(u) = 140 u^3 (1 - u)^3
    const T u2 = u * u;
    s_val = u2 * u2 * (static_cast<T>(35) + u * (static_cast<T>(-84)
            + u * (static_cast<T>(70) - static_cast<T>(20) * u)));
    const T om = static_cast<T>(1) - u;
    ds_du = static_cast<T>(140) * (u * u2) * (om * om * om);
}

template <typename T>
inline TransitionEval<T> evaluate_transition(T r, const Parameters<T>& p) noexcept {
    if (r <= p.r_in) {
        return {static_cast<T>(0), static_cast<T>(0), static_cast<T>(1), static_cast<T>(0)};
    }
    if (r >= p.r_out) {
        return {static_cast<T>(1), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)};
    }
    const T width = p.r_out - p.r_in;
    if (r - p.r_in <= width / static_cast<T>(2)) {
        const T u = (r - p.r_in) / width;
        T mu{}, dmu_du{};
        smoothstep_c3(u, mu, dmu_du);
        const T dmu = dmu_du / width;
        return {mu, dmu, static_cast<T>(1) - mu, -dmu};
    }
    const T v = (p.r_out - r) / width;
    T lam{}, dlam_dv{};
    smoothstep_c3(v, lam, dlam_dv);
    const T dlam = -dlam_dv / width;
    return {static_cast<T>(1) - lam, -dlam, lam, dlam};
}

template <typename T>
struct MetricGeometry {
    using Mat4 = std::array<std::array<T, 4>, 4>;
    Mat4 g{};
    Mat4 ginv{};
    Mat4 dg_dr{};
    Mat4 dg_dtheta{};
    T delta{};
    T sigma{};
    T delta_theta{};
    T xi{};
    T mu{};
    T lam{};
};

template <typename T>
inline MetricGeometry<T> evaluate_geometry(T r, T theta, const Parameters<T>& p) noexcept {
    const auto tr = evaluate_transition(r, p);
    const T three = static_cast<T>(3);
    const T two = static_cast<T>(2);
    const T one = static_cast<T>(1);

    const T leff = p.Lambda * tr.lam;
    const T dleff = p.Lambda * tr.dlam_dr;

    const T r2 = r * r;
    const T a = p.a;
    const T a2 = a * a;
    T s{}, c{};
    sincos_s(theta, s, c);
    const T s2 = s * s;
    const T c2 = c * c;
    const T ds2_dth = two * s * c;
    const T dc2_dth = -two * s * c;

    const T A = r2 + a2;
    const T dA_dr = two * r;

    const T sigma = r2 + a2 * c2;
    const T dsigma_dr = two * r;
    const T dsigma_dth = a2 * dc2_dth;

    const T delta = A * (one - leff * r2 / three) - two * p.M * tr.mu * r;
    const T ddelta_dr = dA_dr * (one - leff * r2 / three)
                      - A * (dleff * r2 + two * leff * r) / three
                      - two * p.M * (tr.dmu_dr * r + tr.mu);

    const T T_func = one + leff * a2 * c2 / three;
    const T dT_dr = dleff * a2 * c2 / three;
    const T dT_dth = leff * a2 * dc2_dth / three;

    const T X = one + leff * a2 / three;
    const T dX_dr = dleff * a2 / three;

    const T denom = sigma * X * X;
    const T ddenom_dr = dsigma_dr * X * X + two * sigma * X * dX_dr;
    const T ddenom_dth = dsigma_dth * X * X;
    const T denom2 = denom * denom;

    const T n00 = T_func * a2 * s2 - delta;
    const T dn00_dr = dT_dr * a2 * s2 - ddelta_dr;
    const T dn00_dth = dT_dth * a2 * s2 + T_func * a2 * ds2_dth;

    const T n03 = a * s2 * (delta - T_func * A);
    const T dn03_dr = a * s2 * (ddelta_dr - dT_dr * A - T_func * dA_dr);
    const T dn03_dth = a * ds2_dth * (delta - T_func * A) - a * s2 * dT_dth * A;

    const T n33 = s2 * (T_func * A * A - delta * a2 * s2);
    const T dn33_dr = s2 * (dT_dr * A * A + two * T_func * A * dA_dr - ddelta_dr * a2 * s2);
    const T dn33_dth = ds2_dth * (T_func * A * A - delta * a2 * s2)
                     + s2 * (dT_dth * A * A - delta * a2 * ds2_dth);

    MetricGeometry<T> out{};
    out.delta = delta;
    out.sigma = sigma;
    out.delta_theta = T_func;
    out.xi = X;
    out.mu = tr.mu;
    out.lam = tr.lam;

    out.g[0][0] = n00 / denom;
    out.g[0][3] = out.g[3][0] = n03 / denom;
    out.g[3][3] = n33 / denom;
    out.g[1][1] = sigma / delta;
    out.g[2][2] = sigma / T_func;

    out.dg_dr[0][0] = (dn00_dr * denom - n00 * ddenom_dr) / denom2;
    out.dg_dtheta[0][0] = (dn00_dth * denom - n00 * ddenom_dth) / denom2;

    out.dg_dr[0][3] = out.dg_dr[3][0] = (dn03_dr * denom - n03 * ddenom_dr) / denom2;
    out.dg_dtheta[0][3] = out.dg_dtheta[3][0] = (dn03_dth * denom - n03 * ddenom_dth) / denom2;

    out.dg_dr[3][3] = (dn33_dr * denom - n33 * ddenom_dr) / denom2;
    out.dg_dtheta[3][3] = (dn33_dth * denom - n33 * ddenom_dth) / denom2;

    out.dg_dr[1][1] = (dsigma_dr * delta - sigma * ddelta_dr) / (delta * delta);
    out.dg_dtheta[1][1] = dsigma_dth / delta;

    out.dg_dr[2][2] = (dsigma_dr * T_func - sigma * dT_dr) / (T_func * T_func);
    out.dg_dtheta[2][2] = (dsigma_dth * T_func - sigma * dT_dth) / (T_func * T_func);

    const T pref = (X * X) / sigma;
    out.ginv[0][0] = -pref * ((A * A) / delta - (a2 * s2) / T_func);
    out.ginv[0][3] = out.ginv[3][0] = -a * pref * (A / delta - one / T_func);
    out.ginv[3][3] = pref * (one / (T_func * s2) - a2 / delta);
    out.ginv[1][1] = delta / sigma;
    out.ginv[2][2] = T_func / sigma;

    return out;
}

// =============================================================================
// 3. Conserved Quantities (E = -p_0, L_z = p_3) & 2nd-Order Geodesic ODEs
// =============================================================================

template <typename T>
struct ConservedQuantities {
    T E{};
    T Lz{};
    T H{};
    T Q{};
    std::array<T, 4> p_cov{};
};

template <typename T>
inline ConservedQuantities<T> compute_conserved_quantities(
    T r, T theta, const std::array<T, 4>& u, const Parameters<T>& p
) noexcept {
    // Mathematical Derivation:
    //   For H(x, p) = (1/2) g^{mu nu}(r, theta) p_mu p_nu = 0:
    //     dp_0 / dlambda = -dH/dt   = (1/2)(d_t g_{ab}) u^a u^b   = 0  => E = -p_0 = const
    //     dp_3 / dlambda = -dH/dphi = (1/2)(d_phi g_{ab}) u^a u^b = 0  => L_z = p_3 = const
    const auto geom = evaluate_geometry(r, theta, p);
    const auto& g = geom.g;

    const T p0 = g[0][0] * u[0] + g[0][3] * u[3];
    const T p1 = g[1][1] * u[1];
    const T p2 = g[2][2] * u[2];
    const T p3 = g[3][0] * u[0] + g[3][3] * u[3];

    const T E = -p0;
    const T Lz = p3;
    const T H = (p0 * u[0] + p1 * u[1] + p2 * u[2] + p3 * u[3]) / static_cast<T>(2);

    T s{}, c{};
    sincos_s(theta, s, c);
    const T s2 = s * s;
    const T xi2 = geom.xi * geom.xi;
    const T term = Lz - p.a * E * s2;
    const T K_sep = geom.delta_theta * (p2 * p2) + (xi2 / (geom.delta_theta * s2)) * (term * term);
    const T l_minus_ae = Lz - p.a * E;
    const T Q = K_sep - xi2 * (l_minus_ae * l_minus_ae);

    return {E, Lz, H, Q, {p0, p1, p2, p3}};
}

template <typename T>
inline std::array<T, 4> null_geodesic_second_derivatives(
    T r, T theta, const std::array<T, 4>& u, const Parameters<T>& p
) noexcept {
    // Evaluates d^2 x^mu / dlambda^2 = -Gamma^mu_{ab} u^a u^b:
    //   d^2 r / dlambda^2     = (1 / g_rr) [ (1/2)(d_r g_{ab}) u^a u^b - (d_r g_rr)(u^r)^2 - (d_th g_rr) u^r u^th ]
    //   d^2 theta / dlambda^2 = (1 / g_thth) [ (1/2)(d_th g_{ab}) u^a u^b - (d_r g_thth) u^r u^th - (d_th g_thth)(u^th)^2 ]
    const auto geom = evaluate_geometry(r, theta, p);
    const auto& g = geom.g;
    const auto& ginv = geom.ginv;
    const auto& dgr = geom.dg_dr;
    const auto& dgth = geom.dg_dtheta;

    const T ut = u[0], ur = u[1], uth = u[2], uphi = u[3];
    const T two = static_cast<T>(2);
    const T half = static_cast<T>(1) / static_cast<T>(2);

    const T quad_r = dgr[0][0] * ut * ut
                   + two * dgr[0][3] * ut * uphi
                   + dgr[3][3] * uphi * uphi
                   + dgr[1][1] * ur * ur
                   + dgr[2][2] * uth * uth;

    const T quad_th = dgth[0][0] * ut * ut
                    + two * dgth[0][3] * ut * uphi
                    + dgth[3][3] * uphi * uphi
                    + dgth[1][1] * ur * ur
                    + dgth[2][2] * uth * uth;

    const T ddr = (half * quad_r - dgr[1][1] * ur * ur - dgth[1][1] * ur * uth) / g[1][1];
    const T ddth = (half * quad_th - dgr[2][2] * ur * uth - dgth[2][2] * uth * uth) / g[2][2];

    const T rhs0 = -((dgr[0][0] * ur + dgth[0][0] * uth) * ut
                   + (dgr[0][3] * ur + dgth[0][3] * uth) * uphi);
    const T rhs3 = -((dgr[3][0] * ur + dgth[3][0] * uth) * ut
                   + (dgr[3][3] * ur + dgth[3][3] * uth) * uphi);

    const T ddt = ginv[0][0] * rhs0 + ginv[0][3] * rhs3;
    const T ddphi = ginv[3][0] * rhs0 + ginv[3][3] * rhs3;
    return {ddt, ddr, ddth, ddphi};
}

template <typename T>
inline std::array<T, 4> initial_null_four_velocity(
    T r0, T theta0, T E, T Lz, T u_theta0, bool ingoing, const Parameters<T>& p
) noexcept {
    const auto geom = evaluate_geometry(r0, theta0, p);
    const auto& g = geom.g;
    const auto& ginv = geom.ginv;

    const T p0 = -E;
    const T p3 = Lz;
    const T ut = ginv[0][0] * p0 + ginv[0][3] * p3;
    const T uphi = ginv[3][0] * p0 + ginv[3][3] * p3;
    const T uth = u_theta0;

    T rhs_null = -(g[0][0] * ut * ut
                 + static_cast<T>(2) * g[0][3] * ut * uphi
                 + g[3][3] * uphi * uphi
                 + g[2][2] * uth * uth);
    if (rhs_null < 0) rhs_null = 0;
    const T ur_mag = sqrt_s(rhs_null / g[1][1]);
    const T ur = ingoing ? -ur_mag : ur_mag;
    return {ut, ur, uth, uphi};
}

// =============================================================================
// 4. Standalone 2nd-Order Christoffel & 1st-Order Hamiltonian RK4 Integrators
// =============================================================================

template <typename T>
struct GeodesicResult {
    int steps{};
    T lambda_final{};
    std::array<T, 4> x_final{};
    std::array<T, 4> u_final{};
    T r_min{};
    bool entered_shell{};
    bool entered_core{};
    bool scattered_to_infinity{};
    T max_abs_dE{};
    T max_abs_dLz{};
    T max_abs_H{};
    T max_abs_dQ{};
};

template <typename T>
inline T adaptive_step_scale(T r, T b, T dlambda, const Parameters<T>& p) noexcept {
    const T seven_twentieths = static_cast<T>(7) / static_cast<T>(20);
    const T three_twentieths = static_cast<T>(3) / static_cast<T>(20);
    const T one_tenth = static_cast<T>(1) / static_cast<T>(10);
    const T one_fifth = static_cast<T>(1) / static_cast<T>(5);

    const T b_scale = static_cast<T>(1) / (static_cast<T>(1) + seven_twentieths * abs_s(b));
    if (r >= p.r_in - three_twentieths && r <= p.r_out + three_twentieths) {
        return dlambda * one_tenth * b_scale;
    }
    const T r_scale = min_s(static_cast<T>(1), max_s(one_fifth, r / static_cast<T>(4)));
    return dlambda * r_scale * b_scale;
}

template <typename T>
inline GeodesicResult<T> integrate_null_geodesic(
    T r0,
    T theta0,
    T b,
    T u_theta0,
    T E,
    T dlambda,
    int max_steps,
    T r_escape,
    const Parameters<T>& p
) noexcept {
    const T Lz = b * E;
    std::array<T, 4> x = {static_cast<T>(0), r0, theta0, static_cast<T>(0)};
    std::array<T, 4> u = initial_null_four_velocity(r0, theta0, E, Lz, u_theta0, true, p);

    const auto init_cons = compute_conserved_quantities(x[1], x[2], u, p);
    const T E_ref = init_cons.E;
    const T Lz_ref = init_cons.Lz;
    const T Q_ref = init_cons.Q;

    GeodesicResult<T> res{};
    res.r_min = r0;
    res.entered_shell = (r0 <= p.r_out);
    res.entered_core = (r0 <= p.r_in);
    res.max_abs_H = abs_s(init_cons.H);

    const T half = static_cast<T>(1) / static_cast<T>(2);
    const T two = static_cast<T>(2);
    const T sixth = static_cast<T>(1) / static_cast<T>(6);

    for (int step = 1; step <= max_steps; ++step) {
        if (x[1] > r_escape && u[1] > 0) {
            res.scattered_to_infinity = true;
            break;
        }
        const T h = adaptive_step_scale(x[1], b, dlambda, p);

        const auto dx1 = u;
        const auto du1 = null_geodesic_second_derivatives(x[1], x[2], u, p);

        std::array<T, 4> x2{}, u2{};
        for (int i = 0; i < 4; ++i) {
            x2[i] = x[i] + half * h * dx1[i];
            u2[i] = u[i] + half * h * du1[i];
        }
        const auto dx2 = u2;
        const auto du2 = null_geodesic_second_derivatives(x2[1], x2[2], u2, p);

        std::array<T, 4> x3{}, u3{};
        for (int i = 0; i < 4; ++i) {
            x3[i] = x[i] + half * h * dx2[i];
            u3[i] = u[i] + half * h * du2[i];
        }
        const auto dx3 = u3;
        const auto du3 = null_geodesic_second_derivatives(x3[1], x3[2], u3, p);

        std::array<T, 4> x4{}, u4{};
        for (int i = 0; i < 4; ++i) {
            x4[i] = x[i] + h * dx3[i];
            u4[i] = u[i] + h * du3[i];
        }
        const auto dx4 = u4;
        const auto du4 = null_geodesic_second_derivatives(x4[1], x4[2], u4, p);

        for (int i = 0; i < 4; ++i) {
            x[i] += h * sixth * (dx1[i] + two * dx2[i] + two * dx3[i] + dx4[i]);
            u[i] += h * sixth * (du1[i] + two * du2[i] + two * du3[i] + du4[i]);
        }

        res.lambda_final += h;
        res.steps = step;
        if (x[1] < res.r_min) res.r_min = x[1];
        if (x[1] <= p.r_out) res.entered_shell = true;
        if (x[1] <= p.r_in) res.entered_core = true;

        const auto cons = compute_conserved_quantities(x[1], x[2], u, p);
        res.max_abs_dE = max_s(res.max_abs_dE, abs_s(cons.E - E_ref));
        res.max_abs_dLz = max_s(res.max_abs_dLz, abs_s(cons.Lz - Lz_ref));
        res.max_abs_H = max_s(res.max_abs_H, abs_s(cons.H));
        res.max_abs_dQ = max_s(res.max_abs_dQ, abs_s(cons.Q - Q_ref));
    }

    res.x_final = x;
    res.u_final = u;
    return res;
}

template <typename T>
inline void integrate_hamiltonian_rk4(
    T r0,
    T theta0,
    T b,
    T u_theta0,
    T E,
    T dlambda,
    int steps,
    const Parameters<T>& p,
    std::array<T, 4>& x_out,
    std::array<T, 4>& u_out
) noexcept {
    const T Lz = b * E;
    std::array<T, 4> x = {static_cast<T>(0), r0, theta0, static_cast<T>(0)};
    const auto u0 = initial_null_four_velocity(r0, theta0, E, Lz, u_theta0, true, p);
    const auto init_cons = compute_conserved_quantities(r0, theta0, u0, p);
    std::array<T, 4> mom = init_cons.p_cov;

    auto ham_rhs = [&](const std::array<T, 4>& sx, const std::array<T, 4>& sp,
                       std::array<T, 4>& dx, std::array<T, 4>& dp) noexcept {
        const auto geom = evaluate_geometry(sx[1], sx[2], p);
        const auto& ginv = geom.ginv;
        const auto& dgr = geom.dg_dr;
        const auto& dgth = geom.dg_dtheta;

        const T ut = ginv[0][0] * sp[0] + ginv[0][3] * sp[3];
        const T ur = ginv[1][1] * sp[1];
        const T uth = ginv[2][2] * sp[2];
        const T uphi = ginv[3][0] * sp[0] + ginv[3][3] * sp[3];

        const T two = static_cast<T>(2);
        const T half = static_cast<T>(1) / static_cast<T>(2);

        const T dp_r = half * (dgr[0][0] * ut * ut + two * dgr[0][3] * ut * uphi
                             + dgr[3][3] * uphi * uphi + dgr[1][1] * ur * ur + dgr[2][2] * uth * uth);
        const T dp_th = half * (dgth[0][0] * ut * ut + two * dgth[0][3] * ut * uphi
                              + dgth[3][3] * uphi * uphi + dgth[1][1] * ur * ur + dgth[2][2] * uth * uth);

        dx = {ut, ur, uth, uphi};
        dp = {static_cast<T>(0), dp_r, dp_th, static_cast<T>(0)};
    };

    const T half = static_cast<T>(1) / static_cast<T>(2);
    const T two = static_cast<T>(2);
    const T sixth = static_cast<T>(1) / static_cast<T>(6);

    for (int step = 0; step < steps; ++step) {
        std::array<T, 4> dx1{}, dp1{};
        ham_rhs(x, mom, dx1, dp1);
        const T h = adaptive_step_scale(x[1], b, dlambda, p);

        std::array<T, 4> x2{}, p2{};
        for (int i = 0; i < 4; ++i) {
            x2[i] = x[i] + half * h * dx1[i];
            p2[i] = mom[i] + half * h * dp1[i];
        }
        std::array<T, 4> dx2{}, dp2{};
        ham_rhs(x2, p2, dx2, dp2);

        std::array<T, 4> x3{}, p3{};
        for (int i = 0; i < 4; ++i) {
            x3[i] = x[i] + half * h * dx2[i];
            p3[i] = mom[i] + half * h * dp2[i];
        }
        std::array<T, 4> dx3{}, dp3{};
        ham_rhs(x3, p3, dx3, dp3);

        std::array<T, 4> x4{}, p4{};
        for (int i = 0; i < 4; ++i) {
            x4[i] = x[i] + h * dx3[i];
            p4[i] = mom[i] + h * dp3[i];
        }
        std::array<T, 4> dx4{}, dp4{};
        ham_rhs(x4, p4, dx4, dp4);

        for (int i = 0; i < 4; ++i) {
            x[i] += h * sixth * (dx1[i] + two * dx2[i] + two * dx3[i] + dx4[i]);
            mom[i] += h * sixth * (dp1[i] + two * dp2[i] + two * dp3[i] + dp4[i]);
        }
    }

    std::array<T, 4> dp_unused{};
    ham_rhs(x, mom, u_out, dp_unused);
    x_out = x;
}

// =============================================================================
// 5. Critical Equatorial Impact Parameters b_crit = L_z / E (113-bit Quad)
// =============================================================================

struct CriticalImpactParameters {
    float128_t delta_r_out{};
    float128_t delta_r_in{};
    float128_t b_crit_prograde_shell{};
    float128_t b_crit_prograde_core{};
    float128_t r_ph_prograde_kerr_masked{};
    float128_t b_prograde_kerr_masked{};
    float128_t r_ph_retrograde_exterior{};
    float128_t b_crit_retrograde_shell{};
    float128_t b_crit_retrograde_core{};
};

inline CriticalImpactParameters calculate_critical_impact_parameters(const Parameters<float128_t>& p) noexcept {
    const float128_t one = 1;
    const float128_t two = 2;
    const float128_t three = 3;
    const float128_t four = 4;

    const float128_t delta_out = p.r_out * p.r_out - two * p.M * p.r_out + p.a * p.a;
    const float128_t sqrt_delta_out = sqrt_s(delta_out);
    const float128_t b_crit_pro_shell = p.a + (p.r_out * p.r_out) / (p.a + sqrt_delta_out);

    const float128_t delta_in = (p.r_in * p.r_in + p.a * p.a) * (one - p.Lambda * p.r_in * p.r_in / three);
    const float128_t sqrt_delta_in = sqrt_s(delta_in);
    const float128_t b_crit_pro_core = p.a + (p.r_in * p.r_in) / (p.a + sqrt_delta_in);
    const float128_t b_crit_ret_core = p.a - (p.r_in * p.r_in) / (sqrt_delta_in - p.a);

    auto solve_kerr_photon_orbit = [&](float128_t r_guess, float128_t& r_ph_out, float128_t& b_ph_out) noexcept {
        float128_t r = r_guess;
        for (int i = 0; i < 30; ++i) {
            const float128_t rm3 = r - three * p.M;
            const float128_t f = r * rm3 * rm3 - four * p.M * p.a * p.a;
            const float128_t df = rm3 * (three * r - three * p.M);
            r -= f / df;
        }
        r_ph_out = r;
        b_ph_out = -(r * r * r - three * p.M * r * r + p.a * p.a * r + p.M * p.a * p.a)
                   / (p.a * (r - p.M));
    };

    float128_t r_ph_pro{}, b_kerr_pro{};
    float128_t r_ph_ret{}, b_crit_ret_shell{};
    solve_kerr_photon_orbit(static_cast<float128_t>(1.62878165935), r_ph_pro, b_kerr_pro);
    solve_kerr_photon_orbit(static_cast<float128_t>(3.88756325539), r_ph_ret, b_crit_ret_shell);

    return {
        delta_out,
        delta_in,
        b_crit_pro_shell,
        b_crit_pro_core,
        r_ph_pro,
        b_kerr_pro,
        r_ph_ret,
        b_crit_ret_shell,
        b_crit_ret_core
    };
}

}  // namespace gravastar::validation

int main() {
    using namespace gravastar::validation;

    const Parameters<double> p_d{};
    const Parameters<float128_t> p_q{};

    std::cout << std::string(78, '=') << "\n";
    std::cout << "GRAVASTARLAB STAGE P2 — NATIVE C++20 GEODESIC CROSS-VALIDATION SUITE\n";
    std::cout << "Parameters: M=1.0, a=0.875 (7/8), Lambda=0.5 (1/2), r_in=1.5, r_out=1.8\n";
    std::cout << std::string(78, '=') << "\n\n";

    // -------------------------------------------------------------------------
    // 1. Conserved Quantities (E = -p_0, L_z = p_3) & Hamiltonian Cross-Check
    // -------------------------------------------------------------------------
    std::cout << "[1] CONSERVED QUANTITIES & 2ND-ORDER VS. HAMILTONIAN RK4 CROSS-CHECK\n";
    struct RaySpec {
        const char* label;
        double r0;
        double th0;
        double b;
        double uth0;
    };
    const std::array<RaySpec, 4> rays = {{
        {"Core-traversing off-equatorial ray", 8.0, 1.15,  1.20,  0.015},
        {"Shell-reflecting prograde ray",      8.0, 1.30,  2.40, -0.010},
        {"Exterior-scattering prograde ray",   8.0, std::numbers::pi / 2.0,  3.20, 0.0},
        {"Shell-reflecting retrograde ray",    8.0, std::numbers::pi / 2.0, -5.00, 0.0},
    }};

    std::cout << std::scientific << std::setprecision(3);
    for (const auto& ray : rays) {
        const auto res = integrate_null_geodesic<double>(
            ray.r0, ray.th0, ray.b, ray.uth0, 1.0, 0.004, 80000, 12.0, p_d
        );
        std::cout << "  * " << ray.label << " (b=" << std::fixed << std::setprecision(2) << ray.b
                  << ", theta0=" << ray.th0 << "):\n"
                  << "      r_min = " << std::setprecision(8) << res.r_min
                  << " | entered_shell=" << (res.entered_shell ? "true" : "false")
                  << " | entered_core=" << (res.entered_core ? "true" : "false")
                  << " | scattered=" << (res.scattered_to_infinity ? "true" : "false") << "\n"
                  << std::scientific << std::setprecision(3)
                  << "      max |dE| = " << res.max_abs_dE
                  << " | max |dL_z| = " << res.max_abs_dLz
                  << " | max |H| = " << res.max_abs_H
                  << " | max |dQ_shell| = " << res.max_abs_dQ << "\n";
        assert(res.max_abs_dE < 1e-10);
        assert(res.max_abs_dLz < 1e-10);
        assert(res.max_abs_H < 1e-9);
    }

    const auto res_2nd = integrate_null_geodesic<double>(
        4.0, 1.2, 1.5, 0.02, 1.0, 0.002, 2000, 100.0, p_d
    );
    std::array<double, 4> x_ham{}, u_ham{};
    integrate_hamiltonian_rk4<double>(
        4.0, 1.2, 1.5, 0.02, 1.0, 0.002, 2000, p_d, x_ham, u_ham
    );
    double max_dx = 0.0, max_du = 0.0;
    for (int i = 0; i < 4; ++i) {
        max_dx = std::max(max_dx, std::abs(res_2nd.x_final[i] - x_ham[i]));
        max_du = std::max(max_du, std::abs(res_2nd.u_final[i] - u_ham[i]));
    }
    std::cout << "  * 2nd-Order Christoffel vs. 1st-Order Hamiltonian RK4 (2000 steps through shell/core):\n"
              << "      max |dx^mu| = " << max_dx << " | max |du^mu| = " << max_du << "\n\n";
    assert(max_dx < 1e-12 && max_du < 1e-12);

    // -------------------------------------------------------------------------
    // 2. 113-Bit Quad-Precision (`__float128`) Radial & Polar Verification
    // -------------------------------------------------------------------------
    std::cout << "[2] 113-BIT QUAD-PRECISION (`__float128`) RADIAL & POLAR VERIFICATION\n";
    const float128_t r0_q = static_cast<float128_t>(22) / static_cast<float128_t>(10);
    const float128_t th0_q = static_cast<float128_t>(5) / static_cast<float128_t>(4);
    const float128_t b_q = static_cast<float128_t>(21) / static_cast<float128_t>(10);
    const float128_t uth0_q = static_cast<float128_t>(1) / static_cast<float128_t>(100);
    const float128_t dlam_q = static_cast<float128_t>(1) / static_cast<float128_t>(10000);

    const auto res_quad = integrate_null_geodesic<float128_t>(
        r0_q, th0_q, b_q, uth0_q, static_cast<float128_t>(1), dlam_q, 5000, static_cast<float128_t>(10), p_q
    );
    std::array<float128_t, 4> x_ham_q{}, u_ham_q{};
    integrate_hamiltonian_rk4<float128_t>(
        r0_q, th0_q, b_q, uth0_q, static_cast<float128_t>(1), dlam_q, 5000, p_q, x_ham_q, u_ham_q
    );
    float128_t max_dx_q = 0, max_du_q = 0;
    for (int i = 0; i < 4; ++i) {
        max_dx_q = max_s(max_dx_q, abs_s(res_quad.x_final[i] - x_ham_q[i]));
        max_du_q = max_s(max_du_q, abs_s(res_quad.u_final[i] - u_ham_q[i]));
    }
    std::cout << "  * __float128 RK4 integration across shell (r0=2.2, theta0=1.25, b=2.1, 5000 steps):\n"
              << "      r_final  = " << format_q(res_quad.x_final[1], 28) << "\n"
              << "      th_final = " << format_q(res_quad.x_final[2], 28) << "\n"
              << "      max |dE| = " << static_cast<double>(res_quad.max_abs_dE)
              << " | max |dL_z| = " << static_cast<double>(res_quad.max_abs_dLz)
              << " | max |H| = " << static_cast<double>(res_quad.max_abs_H) << "\n"
              << "      2nd-Order vs. Hamiltonian __float128 O(h^4) agreement: max |dx^mu| = "
              << static_cast<double>(max_dx_q) << "\n\n";
    assert(res_quad.max_abs_dE < static_cast<float128_t>(1e-20));
    assert(res_quad.max_abs_dLz < static_cast<float128_t>(1e-20));
    assert(res_quad.max_abs_H < static_cast<float128_t>(1e-19));
    assert(max_dx_q < static_cast<float128_t>(1e-17));

    // -------------------------------------------------------------------------
    // 3. Critical Equatorial Impact Parameters b_crit = L_z / E
    // -------------------------------------------------------------------------
    std::cout << "[3] CRITICAL EQUATORIAL IMPACT PARAMETERS b_crit = L_z / E\n";
    const auto crit = calculate_critical_impact_parameters(p_q);
    std::cout << "  * Prograde Shell-Entry Threshold (r <= r_out = 1.8):\n"
              << "      b_crit,shell^+ = " << format_q(crit.b_crit_prograde_shell, 30) << "\n"
              << "      Exact form     : 7/8 + 648 / (35 + 5*sqrt(649))  [Delta(1.8) = 649/1600 = 0.405625]\n"
              << "      (Masks vacuum Kerr prograde photon sphere r_ph^+ = "
              << format_q(crit.r_ph_prograde_kerr_masked, 15) << " < r_out,\n"
              << "       where b_Kerr^+ = " << format_q(crit.b_prograde_kerr_masked, 18) << ")\n"
              << "  * Prograde Core-Entry Threshold (r <= r_in = 1.5):\n"
              << "      b_crit,core^+  = " << format_q(crit.b_crit_prograde_core, 30)
              << "  [Delta(1.5) = 965/512]\n"
              << "  * Retrograde Shell-Entry Threshold (barrier at exterior r_ph^- = "
              << format_q(crit.r_ph_retrograde_exterior, 15) << " > r_out):\n"
              << "      b_crit,shell^- = " << format_q(crit.b_crit_retrograde_shell, 30) << "\n"
              << "  * Retrograde Core-Entry Threshold (r <= r_in = 1.5):\n"
              << "      b_crit,core^-  = " << format_q(crit.b_crit_retrograde_core, 30) << "\n\n";

    const double b_pro_shell = static_cast<double>(crit.b_crit_prograde_shell);
    const double b_ret_shell = static_cast<double>(crit.b_crit_retrograde_shell);
    const double eps = 1e-4;
    const double half_pi = std::numbers::pi / 2.0;

    const auto pro_in = integrate_null_geodesic<double>(
        6.0, half_pi, b_pro_shell - eps, 0.0, 1.0, 0.005, 80000, 12.0, p_d
    );
    const auto pro_out = integrate_null_geodesic<double>(
        6.0, half_pi, b_pro_shell + eps, 0.0, 1.0, 0.005, 80000, 12.0, p_d
    );
    const auto ret_in = integrate_null_geodesic<double>(
        8.0, half_pi, b_ret_shell + eps, 0.0, 1.0, 0.005, 100000, 12.0, p_d
    );
    const auto ret_out = integrate_null_geodesic<double>(
        8.0, half_pi, b_ret_shell - eps, 0.0, 1.0, 0.005, 100000, 12.0, p_d
    );

    std::cout << std::fixed << std::setprecision(8);
    std::cout << "  * Geodesic Shooting Verification around b_crit,shell^+ and b_crit,shell^-:\n"
              << "      b = b_crit,shell^+ - 1e-4 (" << std::showpos << (b_pro_shell - eps) << std::noshowpos
              << "): r_min = " << pro_in.r_min << " <= 1.8 (entered_shell = "
              << (pro_in.entered_shell ? "true" : "false") << ")\n"
              << "      b = b_crit,shell^+ + 1e-4 (" << std::showpos << (b_pro_shell + eps) << std::noshowpos
              << "): r_min = " << pro_out.r_min << " >  1.8 (entered_shell = "
              << (pro_out.entered_shell ? "true" : "false") << ")\n"
              << "      b = b_crit,shell^- + 1e-4 (" << std::showpos << (b_ret_shell + eps) << std::noshowpos
              << "): r_min = " << ret_in.r_min << " <= 1.8 (entered_shell = "
              << (ret_in.entered_shell ? "true" : "false") << ")\n"
              << "      b = b_crit,shell^- - 1e-4 (" << std::showpos << (b_ret_shell - eps) << std::noshowpos
              << "): r_min = " << ret_out.r_min << " >  1.8 (entered_shell = "
              << (ret_out.entered_shell ? "true" : "false") << ")\n\n";

    assert(pro_in.entered_shell && !pro_out.entered_shell);
    assert(ret_in.entered_shell && !ret_out.entered_shell);

    std::cout << "ALL C++20 GEODESIC CROSS-VALIDATION CHECKS PASSED.\n";
    return 0;
}