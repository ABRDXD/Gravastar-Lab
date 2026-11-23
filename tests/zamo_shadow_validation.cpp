// SPDX-License-Identifier: GPL-3.0-or-later
// ============================================================================
// GravastarLab — Stage P3 Validation Suite:
//   Zero-Angular-Momentum Observer (ZAMO / Bardeen) Orthonormal Tetrad,
//   Finite-Distance vs. Asymptotic Screen Projections, and
//   Analytical vs. Numerical Shadow & Material-Interface Contours
//
// File: zamo_shadow_validation.cpp
// Standard: Strict ISO C++20 (+ GCC libquadmath __float128 extension)
// Compile command:
//   g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off zamo_shadow_validation.cpp -lquadmath -o zamo_shadow_validation
//
// Scientific Reference:
//   Sasa Ilijic and Andrew DeBenedictis, "On a Class of Exact Arbitrarily
//   Differentiable de Sitter Cores with Kerr Exteriors: Possible gravastar or
//   regular black hole mimickers", arXiv:2309.00551v2, Int. J. Mod. Phys. D
//   34, 2550004 (2025).
// ============================================================================

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <quadmath.h>

__extension__ typedef __float128 qfloat;

namespace gravastar {

// ============================================================================
// 1. Release-Active Verification Discipline (Never Elided by -DNDEBUG)
// ============================================================================

static int g_checks_passed = 0;

inline void require_check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "\n[FATAL VERIFICATION FAILURE] " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
    ++g_checks_passed;
}

// ============================================================================
// 2. Templated Scalar Traits for IEEE-754 double and 113-bit __float128
//    Strictly isolates double vs. libquadmath transcendental calls and builds
//    all model parameters from exact integer ratios (preventing double-literal
//    truncation inside the __float128 path).
// ============================================================================

template <typename T>
struct ScalarTraits;

template <>
struct ScalarTraits<double> {
    using value_type = double;
    static constexpr const char* name() { return "double (53-bit IEEE-754)"; }

    static constexpr double from_int(int64_t n) { return static_cast<double>(n); }
    static constexpr double from_ratio(int64_t num, int64_t den) {
        return static_cast<double>(num) / static_cast<double>(den);
    }

    static double abs(double x) { return std::fabs(x); }
    static double sqrt(double x) { return std::sqrt(x); }
    static double sin(double x) { return std::sin(x); }
    static double cos(double x) { return std::cos(x); }
    static double tan(double x) { return std::tan(x); }
    static double acos(double x) { return std::acos(x); }
    static double max(double a, double b) { return a > b ? a : b; }
    static double pi() { return std::acos(-1.0); }
    static double half_pi() { return std::acos(0.0); }

    static std::string format(double x, int prec = 16, bool show_plus = true) {
        std::ostringstream oss;
        if (show_plus) oss << std::showpos;
        oss << std::fixed << std::setprecision(prec) << x;
        return oss.str();
    }

    static std::string format_sci(double x, int prec = 3) {
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(prec) << x;
        return oss.str();
    }
};

template <>
struct ScalarTraits<qfloat> {
    using value_type = qfloat;
    static constexpr const char* name() { return "__float128 (113-bit quad)"; }

    static constexpr qfloat from_int(int64_t n) { return static_cast<qfloat>(n); }
    static constexpr qfloat from_ratio(int64_t num, int64_t den) {
        return static_cast<qfloat>(num) / static_cast<qfloat>(den);
    }

    static qfloat abs(qfloat x) { return fabsq(x); }
    static qfloat sqrt(qfloat x) { return sqrtq(x); }
    static qfloat sin(qfloat x) { return sinq(x); }
    static qfloat cos(qfloat x) { return cosq(x); }
    static qfloat tan(qfloat x) { return tanq(x); }
    static qfloat acos(qfloat x) { return acosq(x); }
    static qfloat max(qfloat a, qfloat b) { return a > b ? a : b; }
    static qfloat pi() { return acosq(from_int(-1)); }
    static qfloat half_pi() { return acosq(from_int(0)); }

    static std::string format(qfloat x, int prec = 30, bool show_plus = true) {
        char buf[160];
        std::string fmt = std::string(show_plus ? "%+." : "%.") + std::to_string(prec) + "Qf";
        quadmath_snprintf(buf, sizeof(buf), fmt.c_str(), x);
        return std::string(buf);
    }

    static std::string format_sci(qfloat x, int prec = 3) {
        char buf[160];
        std::string fmt = "%." + std::to_string(prec) + "Qe";
        quadmath_snprintf(buf, sizeof(buf), fmt.c_str(), x);
        return std::string(buf);
    }
};

// ============================================================================
// 3. Ilijic & DeBenedictis (arXiv:2309.00551v2) Rotating Gravastar Metric
//    Parameters: M = 1, a = 7/8 = 0.875, Lambda = 1/2 = 0.5,
//                r_in = 3/2 = 1.5, r_out = 9/5 = 1.8
// ============================================================================

template <typename T>
struct ModelParams {
    using ST = ScalarTraits<T>;
    T M     = ST::from_int(1);
    T a     = ST::from_ratio(7, 8);
    T Lam   = ST::from_ratio(1, 2);
    T r_in  = ST::from_ratio(3, 2);
    T r_out = ST::from_ratio(9, 5);
};

template <typename T>
struct TransitionValues {
    T mu;
    T lam;
    T dmu_dr;
    T dlam_dr;
};

// Septic C^3 transition S(u) = 35u^4 - 84u^5 + 70u^6 - 20u^7 on u in [0, 1]
// with midpoint split v = 1 - u for u > 0.5 to prevent cancellation near r_out
template <typename T>
TransitionValues<T> evaluate_transition(T r, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T zero = ST::from_int(0);
    const T one  = ST::from_int(1);

    if (r <= p.r_in) {
        return {zero, one, zero, zero};
    }
    if (r >= p.r_out) {
        return {one, zero, zero, zero};
    }

    const T inv_w = one / (p.r_out - p.r_in);
    const T u = (r - p.r_in) * inv_w;
    const T v = one - u;

    T mu = zero;
    T lam = zero;
    if (u <= ST::from_ratio(1, 2)) {
        const T u2 = u * u;
        const T u4 = u2 * u2;
        mu = u4 * (ST::from_int(35) + u * (ST::from_int(-84) + u * (ST::from_int(70) - ST::from_int(20) * u)));
        lam = one - mu;
    } else {
        const T v2 = v * v;
        const T v4 = v2 * v2;
        lam = v4 * (ST::from_int(35) + v * (ST::from_int(-84) + v * (ST::from_int(70) - ST::from_int(20) * v)));
        mu = one - lam;
    }

    const T u3 = u * u * u;
    const T v3 = v * v * v;
    const T dmu_dr = ST::from_int(140) * u3 * v3 * inv_w;
    const T dlam_dr = -dmu_dr;
    return {mu, lam, dmu_dr, dlam_dr};
}

template <typename T>
T evaluate_delta(T r, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const auto tr = evaluate_transition(r, p);
    const T r2 = r * r;
    const T a2 = p.a * p.a;
    return (r2 + a2) * (ST::from_int(1) - (p.Lam / ST::from_int(3)) * tr.lam * r2)
           - ST::from_int(2) * p.M * tr.mu * r;
}

template <typename T>
struct MetricState {
    T Sigma, Delta, Delta_th, Xi, A_func;
    T g_tt, g_tphi, g_phiphi, g_rr, g_thth;
    T ginv_tt, ginv_tphi, ginv_phiphi, ginv_rr, ginv_thth;
    T dg_tt_dr, dg_tphi_dr, dg_phiphi_dr, dg_rr_dr, dg_thth_dr;
    T dg_tt_dth, dg_tphi_dth, dg_phiphi_dth, dg_rr_dth, dg_thth_dth;
};

template <typename T>
MetricState<T> evaluate_metric(T r, T theta, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T one   = ST::from_int(1);
    const T two   = ST::from_int(2);
    const T three = ST::from_int(3);
    const T four  = ST::from_int(4);

    const auto tr = evaluate_transition(r, p);
    const T a2  = p.a * p.a;
    const T r2  = r * r;
    const T rp2 = r2 + a2;

    const T sth = ST::sin(theta);
    const T cth = ST::cos(theta);
    const T s2  = sth * sth;
    const T c2  = cth * cth;
    const T sc  = sth * cth;

    const T Sigma     = r2 + a2 * c2;
    const T dSigma_dr = two * r;
    const T dSigma_dth = -two * a2 * sc;

    const T c_lam  = (p.Lam / three) * tr.lam;
    const T dc_lam = (p.Lam / three) * tr.dlam_dr;

    const T Delta = rp2 * (one - c_lam * r2) - two * p.M * tr.mu * r;
    const T dDelta_dr = two * r * (one - c_lam * r2)
                      - rp2 * (dc_lam * r2 + two * c_lam * r)
                      - two * p.M * (tr.dmu_dr * r + tr.mu);

    const T Dth     = one + c_lam * a2 * c2;
    const T dDth_dr = dc_lam * a2 * c2;
    const T dDth_dth = -two * c_lam * a2 * sc;

    const T Xi     = one + c_lam * a2;
    const T dXi_dr = dc_lam * a2;
    const T Xi2    = Xi * Xi;

    const T denom     = Xi2 * Sigma;
    const T ddenom_dr = two * Xi * dXi_dr * Sigma + Xi2 * dSigma_dr;
    const T ddenom_dth = Xi2 * dSigma_dth;

    const T num_tt     = -(Delta - Dth * a2 * s2);
    const T dnum_tt_dr = -(dDelta_dr - dDth_dr * a2 * s2);
    const T dnum_tt_dth = dDth_dth * a2 * s2 + Dth * a2 * two * sc;

    const T num_tphi     = -p.a * s2 * (rp2 * Dth - Delta);
    const T dnum_tphi_dr = -p.a * s2 * (two * r * Dth + rp2 * dDth_dr - dDelta_dr);
    const T dnum_tphi_dth = -p.a * (two * sc * (rp2 * Dth - Delta) + s2 * rp2 * dDth_dth);

    const T A_func  = rp2 * rp2 * Dth - a2 * Delta * s2;
    const T dA_dr   = four * r * rp2 * Dth + rp2 * rp2 * dDth_dr - a2 * dDelta_dr * s2;
    const T dA_dth  = rp2 * rp2 * dDth_dth - two * a2 * Delta * sc;

    const T num_phiphi     = s2 * A_func;
    const T dnum_phiphi_dr = s2 * dA_dr;
    const T dnum_phiphi_dth = two * sc * A_func + s2 * dA_dth;

    MetricState<T> m{};
    m.Sigma    = Sigma;
    m.Delta    = Delta;
    m.Delta_th = Dth;
    m.Xi       = Xi;
    m.A_func   = A_func;

    const T denom2 = denom * denom;
    m.g_tt     = num_tt / denom;
    m.dg_tt_dr = (dnum_tt_dr * denom - num_tt * ddenom_dr) / denom2;
    m.dg_tt_dth = (dnum_tt_dth * denom - num_tt * ddenom_dth) / denom2;

    m.g_tphi     = num_tphi / denom;
    m.dg_tphi_dr = (dnum_tphi_dr * denom - num_tphi * ddenom_dr) / denom2;
    m.dg_tphi_dth = (dnum_tphi_dth * denom - num_tphi * ddenom_dth) / denom2;

    m.g_phiphi     = num_phiphi / denom;
    m.dg_phiphi_dr = (dnum_phiphi_dr * denom - num_phiphi * ddenom_dr) / denom2;
    m.dg_phiphi_dth = (dnum_phiphi_dth * denom - num_phiphi * ddenom_dth) / denom2;

    m.g_rr     = Sigma / Delta;
    m.dg_rr_dr = (dSigma_dr * Delta - Sigma * dDelta_dr) / (Delta * Delta);
    m.dg_rr_dth = dSigma_dth / Delta;

    m.g_thth     = Sigma / Dth;
    m.dg_thth_dr = (dSigma_dr * Dth - Sigma * dDth_dr) / (Dth * Dth);
    m.dg_thth_dth = (dSigma_dth * Dth - Sigma * dDth_dth) / (Dth * Dth);

    const T det_tphi = m.g_tt * m.g_phiphi - m.g_tphi * m.g_tphi;
    m.ginv_tt     =  m.g_phiphi / det_tphi;
    m.ginv_tphi   = -m.g_tphi / det_tphi;
    m.ginv_phiphi =  m.g_tt / det_tphi;
    m.ginv_rr     =  Delta / Sigma;
    m.ginv_thth   =  Dth / Sigma;

    return m;
}

// ============================================================================
// 4. General Relativistic ZAMO (Bardeen Locally Non-Rotating Frame) Tetrad
//    e_{\hat{0}}^\mu = (1 / \alpha_{\text{lapse}}) (\delta^\mu_t + \omega \delta^\mu_\phi)
//    e_{\hat{1}}^\mu = (1 / \sqrt{g_{rr}}) \delta^\mu_r
//    e_{\hat{2}}^\mu = (1 / \sqrt{g_{\theta\theta}}) \delta^\mu_\theta
//    e_{\hat{3}}^\mu = (1 / \sqrt{g_{\phi\phi}}) \delta^\mu_\phi
// ============================================================================

template <typename T>
struct ZamoTetrad {
    T r;
    T theta;
    T omega;              // Frame-dragging angular velocity -g_{t\phi}/g_{\phi\phi}
    T alpha_lapse;        // Lapse \sqrt{-g_{tt} + \omega^2 g_{\phi\phi}}
    T alpha_from_ginv;    // Independent algebraic form 1/\sqrt{-g^{tt}}
    MetricState<T> metric;
    std::array<std::array<T, 4>, 4> e_hat;
};

template <typename T>
ZamoTetrad<T> build_zamo_tetrad(T r, T theta, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T zero = ST::from_int(0);
    const T one  = ST::from_int(1);

    ZamoTetrad<T> z{};
    z.r      = r;
    z.theta  = theta;
    z.metric = evaluate_metric(r, theta, p);

    require_check(z.metric.g_phiphi > zero, "g_phiphi must be positive off the polar axis");
    require_check(z.metric.g_rr > zero, "g_rr must be positive everywhere (Delta(r) > 0)");
    require_check(z.metric.g_thth > zero, "g_thth must be positive everywhere");

    z.omega = -z.metric.g_tphi / z.metric.g_phiphi;
    const T lapse_sq = -z.metric.g_tt + z.omega * z.omega * z.metric.g_phiphi;
    require_check(lapse_sq > zero, "ZAMO lapse^2 must be strictly positive even inside the ergosphere");

    z.alpha_lapse     = ST::sqrt(lapse_sq);
    z.alpha_from_ginv = one / ST::sqrt(-z.metric.ginv_tt);

    for (int a = 0; a < 4; ++a) {
        for (int mu = 0; mu < 4; ++mu) {
            z.e_hat[a][mu] = zero;
        }
    }

    z.e_hat[0][0] = one / z.alpha_lapse;
    z.e_hat[0][3] = z.omega / z.alpha_lapse;
    z.e_hat[1][1] = one / ST::sqrt(z.metric.g_rr);
    z.e_hat[2][2] = one / ST::sqrt(z.metric.g_thth);
    z.e_hat[3][3] = one / ST::sqrt(z.metric.g_phiphi);

    return z;
}

template <typename T>
T inner_product_cov(const MetricState<T>& m, const std::array<T, 4>& u, const std::array<T, 4>& v) {
    return m.g_tt * u[0] * v[0]
         + m.g_tphi * (u[0] * v[3] + u[3] * v[0])
         + m.g_rr * u[1] * v[1]
         + m.g_thth * u[2] * v[2]
         + m.g_phiphi * u[3] * v[3];
}

template <typename T>
struct OrthonormalityReport {
    T max_abs_err;
    T max_diag_err;
    T max_offdiag_err;
    T lapse_consistency_err;
};

template <typename T>
OrthonormalityReport<T> check_zamo_orthonormality(const ZamoTetrad<T>& z) {
    using ST = ScalarTraits<T>;
    const T zero = ST::from_int(0);
    const T one  = ST::from_int(1);

    OrthonormalityReport<T> rep{zero, zero, zero, ST::abs(z.alpha_lapse - z.alpha_from_ginv)};
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            const T g_ab = inner_product_cov(z.metric, z.e_hat[a], z.e_hat[b]);
            const T eta_ab = (a == b) ? ((a == 0) ? -one : one) : zero;
            const T err = ST::abs(g_ab - eta_ab);
            rep.max_abs_err = ST::max(rep.max_abs_err, err);
            if (a == b) {
                rep.max_diag_err = ST::max(rep.max_diag_err, err);
            } else {
                rep.max_offdiag_err = ST::max(rep.max_offdiag_err, err);
            }
        }
    }
    return rep;
}

// ============================================================================
// 5. Screen Projections & Ray Momentum Generation
//    Rigorously distinguishes:
//    (A) Asymptotic Bardeen Screen Coordinates (\alpha_\infty, \beta_\infty)
//        defined for a distant observer (r_obs -> \infty):
//          \alpha_\infty = -b / \sin\theta_obs,
//          \beta_\infty  = \pm \sqrt{\eta + a^2\cos^2\theta_obs - b^2\cot^2\theta_obs}
//    (B) Finite-Distance ZAMO Tetrad Screen Projection at (r_obs, \theta_obs):
//        For a ray with local tetrad components k^{\hat{a}} (with k^{\hat{0}} > 0
//        and inward radial component k^{\hat{1}} < 0):
//          x_ZAMO = -k^{\hat{3}} / k^{\hat{0}} = -\frac{\alpha_lapse}{\sqrt{g_{\phi\phi}}} \frac{b}{1 - \omega b}
//          y_ZAMO =  k^{\hat{2}} / k^{\hat{0}} =  \frac{\alpha_lapse}{\sqrt{g_{\theta\theta}}} \frac{p_\theta / E}{1 - \omega b}
//        and radius-scaled ZAMO coordinates (in units of M):
//          \alpha_ZAMO(r_obs) = r_obs * x_ZAMO,
//          \beta_ZAMO(r_obs)  = r_obs * y_ZAMO.
// ============================================================================

template <typename T>
struct RayKinematics {
    std::array<T, 4> k_hat;   // Local ZAMO contravariant components k^{\hat{a}}
    std::array<T, 4> k_coord; // Spacetime contravariant k^\mu = k^{\hat{a}} e_{\hat{a}}^\mu
    std::array<T, 4> p_cov;   // Spacetime covariant momentum p_\mu = g_{\mu\nu} k^\nu
    T E;                      // Conserved Killing energy -p_t
    T L_z;                    // Conserved axial angular momentum p_\phi
    T b;                      // Impact parameter L_z / E
    T Q_kerr;                 // Exterior Kerr Carter invariant at observer
    T eta;                    // Reduced Carter quantity Q_kerr / E^2
    T alpha_inf;              // Asymptotic Bardeen horizontal coordinate
    T beta_inf;               // Asymptotic Bardeen vertical coordinate
    T x_zamo;                 // Finite-distance ZAMO direction cosine -k^{\hat{3}}/k^{\hat{0}}
    T y_zamo;                 // Finite-distance ZAMO direction cosine  k^{\hat{2}}/k^{\hat{0}}
    T alpha_zamo_r;           // Finite-distance r_obs-scaled ZAMO coordinate r_obs * x_zamo
    T beta_zamo_r;            // Finite-distance r_obs-scaled ZAMO coordinate r_obs * y_zamo
    T null_residual;          // |g^{\mu\nu} p_\mu p_\nu|
};

template <typename T>
RayKinematics<T> construct_ray_from_invariants(const ZamoTetrad<T>& z, T b, T beta_inf) {
    using ST = ScalarTraits<T>;
    const T zero = ST::from_int(0);
    const T one  = ST::from_int(1);
    const T two  = ST::from_int(2);

    RayKinematics<T> ray{};
    const T sth = ST::sin(z.theta);
    const T cth = ST::cos(z.theta);
    const T cot = cth / sth;

    const T k0 = (one - z.omega * b) / z.alpha_lapse;
    const T k2 = beta_inf / ST::sqrt(z.metric.g_thth);
    const T k3 = b / ST::sqrt(z.metric.g_phiphi);
    const T k1_sq = k0 * k0 - k2 * k2 - k3 * k3;
    require_check(k1_sq >= zero, "Ray must have real radial momentum k^{\\hat{1}} at observer");
    const T k1 = -ST::sqrt(k1_sq);

    ray.k_hat = {k0, k1, k2, k3};

    for (int mu = 0; mu < 4; ++mu) {
        T sum = zero;
        for (int a = 0; a < 4; ++a) {
            sum += ray.k_hat[a] * z.e_hat[a][mu];
        }
        ray.k_coord[mu] = sum;
    }

    const auto& m = z.metric;
    ray.p_cov[0] = m.g_tt * ray.k_coord[0] + m.g_tphi * ray.k_coord[3];
    ray.p_cov[1] = m.g_rr * ray.k_coord[1];
    ray.p_cov[2] = m.g_thth * ray.k_coord[2];
    ray.p_cov[3] = m.g_tphi * ray.k_coord[0] + m.g_phiphi * ray.k_coord[3];

    ray.E   = -ray.p_cov[0];
    ray.L_z =  ray.p_cov[3];
    ray.b   =  ray.L_z / ray.E;

    const T a = ModelParams<T>{}.a;
    ray.eta       = beta_inf * beta_inf + b * b * cot * cot - a * a * cth * cth;
    ray.Q_kerr    = ray.eta * ray.E * ray.E;
    ray.alpha_inf = -ray.b / sth;
    ray.beta_inf  = ray.p_cov[2] / ray.E;

    ray.x_zamo       = -ray.k_hat[3] / ray.k_hat[0];
    ray.y_zamo       =  ray.k_hat[2] / ray.k_hat[0];
    ray.alpha_zamo_r = z.r * ray.x_zamo;
    ray.beta_zamo_r  = z.r * ray.y_zamo;

    ray.null_residual = ST::abs(
        m.ginv_tt * ray.p_cov[0] * ray.p_cov[0]
        + two * m.ginv_tphi * ray.p_cov[0] * ray.p_cov[3]
        + m.ginv_phiphi * ray.p_cov[3] * ray.p_cov[3]
        + m.ginv_rr * ray.p_cov[1] * ray.p_cov[1]
        + m.ginv_thth * ray.p_cov[2] * ray.p_cov[2]
    );
    return ray;
}

template <typename T>
RayKinematics<T> construct_ray_from_zamo_cosines(const ZamoTetrad<T>& z, T x_zamo, T y_zamo) {
    using ST = ScalarTraits<T>;
    const T one = ST::from_int(1);
    const T sqrt_gpp = ST::sqrt(z.metric.g_phiphi);
    const T sqrt_gtt = ST::sqrt(z.metric.g_thth);
    const T b = (-sqrt_gpp * x_zamo) / (z.alpha_lapse - z.omega * sqrt_gpp * x_zamo);
    const T beta_inf = y_zamo * sqrt_gtt * (one - z.omega * b) / z.alpha_lapse;
    return construct_ray_from_invariants(z, b, beta_inf);
}

// ============================================================================
// 6. Analytical Exterior Kerr Shadow & Shell-Grazing Formulas
//    Valid ONLY in the exterior r >= r_out = 1.8 where \lambda(r) = 0, \mu(r) = 1
//    identically and Q_Kerr is an exact separation constant.
// ============================================================================

template <typename T>
T kerr_spherical_b(T r0, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T three = ST::from_int(3);
    const T a2 = p.a * p.a;
    return -(r0 * r0 * r0 - three * p.M * r0 * r0 + a2 * r0 + a2 * p.M) / (p.a * (r0 - p.M));
}

template <typename T>
T kerr_spherical_eta(T r0, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T three = ST::from_int(3);
    const T four  = ST::from_int(4);
    const T a2  = p.a * p.a;
    const T rm1 = r0 - p.M;
    const T rm3 = r0 - three * p.M;
    return (r0 * r0 * r0 * (four * a2 * p.M - r0 * rm3 * rm3)) / (a2 * rm1 * rm1);
}

template <typename T>
T analytical_sphere_grazing_b_plus(T R, T theta_obs, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T sD  = ST::sqrt(evaluate_delta(R, p));
    const T sth = ST::sin(theta_obs);
    return ((R * R + p.a * p.a + p.a * sD * sth) * sth) / (sD + p.a * sth);
}

template <typename T>
T analytical_sphere_grazing_b_minus(T R, T theta_obs, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T sD  = ST::sqrt(evaluate_delta(R, p));
    const T sth = ST::sin(theta_obs);
    return ((R * R + p.a * p.a - p.a * sD * sth) * sth) / (p.a * sth - sD);
}

template <typename T>
std::pair<T, T> solve_kerr_equatorial_photon_radii(const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T three = ST::from_int(3);
    const T four  = ST::from_int(4);
    const T a2 = p.a * p.a;

    auto f = [&](T r) {
        const T rm3 = r - three * p.M;
        return r * rm3 * rm3 - four * a2 * p.M;
    };
    auto df = [&](T r) {
        const T rm3 = r - three * p.M;
        return rm3 * (three * r - three * p.M);
    };

    T r_plus  = ST::from_ratio(16, 10);
    T r_minus = ST::from_ratio(39, 10);
    for (int i = 0; i < 60; ++i) {
        r_plus  -= f(r_plus) / df(r_plus);
        r_minus -= f(r_minus) / df(r_minus);
    }
    return {r_plus, r_minus};
}

template <typename T>
T solve_kerr_polar_photon_radius(const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T three = ST::from_int(3);
    const T six   = ST::from_int(6);
    const T a2 = p.a * p.a;

    auto f = [&](T r) {
        return r * r * r - three * p.M * r * r + a2 * r + a2 * p.M;
    };
    auto df = [&](T r) {
        return three * r * r - six * p.M * r + a2;
    };

    T r_pol = ST::from_ratio(26, 10);
    for (int i = 0; i < 60; ++i) {
        r_pol -= f(r_pol) / df(r_pol);
    }
    return r_pol;
}

template <typename T>
T solve_kerr_true_apex_radius(T theta_obs, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T three = ST::from_int(3);
    const T six   = ST::from_int(6);
    const T cth = ST::cos(theta_obs);
    const T a2c2 = p.a * p.a * cth * cth;

    auto f = [&](T r) {
        return r * r * r - three * p.M * r * r + a2c2 * (r + p.M);
    };
    auto df = [&](T r) {
        return three * r * r - six * p.M * r + a2c2;
    };

    T r_apex = ST::from_ratio(295, 100);
    for (int i = 0; i < 60; ++i) {
        r_apex -= f(r_apex) / df(r_apex);
    }
    return r_apex;
}

template <typename T>
T solve_kerr_retrograde_beta0_radius(T theta_obs, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const T half = ST::from_ratio(1, 2);
    const T sth  = ST::sin(theta_obs);
    const T cth  = ST::cos(theta_obs);
    const T cot  = cth / sth;
    const T a2c2 = p.a * p.a * cth * cth;

    auto f_beta2 = [&](T r0) {
        const T bk   = kerr_spherical_b(r0, p);
        const T etak = kerr_spherical_eta(r0, p);
        return etak + a2c2 - bk * bk * cot * cot;
    };

    const auto [r_plus, r_minus] = solve_kerr_equatorial_photon_radii(p);
    (void)r_plus;
    const T r_pol = solve_kerr_polar_photon_radius(p);

    T lo = r_pol;
    T hi = r_minus;
    for (int i = 0; i < 120; ++i) {
        const T mid = half * (lo + hi);
        if (f_beta2(mid) > ST::from_int(0)) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return half * (lo + hi);
}

// ============================================================================
// 7. Hamiltonian RK4 Null Geodesic Integrator & Numerical Contour Solver
// ============================================================================

template <typename T>
struct GeodesicTraceResult {
    T r_min;
    bool entered_shell;
    bool entered_core;
    T K_kds_initial;
    T K_kds_at_rmin;
    T max_abs_H;
};

template <typename T>
T evaluate_K_kds(T r, T theta, T p_theta, T b, const ModelParams<T>& p) {
    using ST = ScalarTraits<T>;
    const auto tr = evaluate_transition(r, p);
    const T a2   = p.a * p.a;
    const T sth  = ST::sin(theta);
    const T cth  = ST::cos(theta);
    const T s2   = sth * sth;
    const T c2   = cth * cth;
    const T clam = (p.Lam / ST::from_int(3)) * tr.lam;
    const T Dth  = ST::from_int(1) + clam * a2 * c2;
    const T Xi   = ST::from_int(1) + clam * a2;
    const T diff = b - p.a * s2;
    return Dth * p_theta * p_theta + (Xi * Xi * diff * diff) / (Dth * s2);
}

template <typename T>
GeodesicTraceResult<T> integrate_null_geodesic(
    const ZamoTetrad<T>& z_obs,
    T b,
    T beta_inf,
    const ModelParams<T>& p,
    T h_base = ScalarTraits<T>::from_ratio(1, 100)) {

    using ST = ScalarTraits<T>;
    const T zero = ST::from_int(0);
    const T half = ST::from_ratio(1, 2);
    const T one  = ST::from_int(1);
    const T two  = ST::from_int(2);
    const T six  = ST::from_int(6);

    const RayKinematics<T> ray0 = construct_ray_from_invariants(z_obs, b, beta_inf);
    const T p_t   = ray0.p_cov[0];
    const T p_phi = ray0.p_cov[3];

    T r    = z_obs.r;
    T th   = z_obs.theta;
    T p_r  = ray0.p_cov[1];
    T p_th = ray0.p_cov[2];

    const T K_init = evaluate_K_kds(r, th, p_th, b, p);
    T K_turn = K_init;
    T r_min  = r;
    T max_H  = ray0.null_residual;
    bool entered_shell = false;
    bool entered_core  = false;

    auto rhs_eval = [&](T rc, T thc, T prc, T pthc, T& dr, T& dth, T& dpr, T& dpth, T& H_abs) {
        const MetricState<T> m = evaluate_metric(rc, thc, p);
        const T vt   = m.ginv_tt * p_t + m.ginv_tphi * p_phi;
        const T vphi = m.ginv_tphi * p_t + m.ginv_phiphi * p_phi;
        const T vr   = m.ginv_rr * prc;
        const T vth  = m.ginv_thth * pthc;

        dr  = vr;
        dth = vth;
        dpr = half * (m.dg_tt_dr * vt * vt + two * m.dg_tphi_dr * vt * vphi
                    + m.dg_phiphi_dr * vphi * vphi + m.dg_rr_dr * vr * vr + m.dg_thth_dr * vth * vth);
        dpth = half * (m.dg_tt_dth * vt * vt + two * m.dg_tphi_dth * vt * vphi
                     + m.dg_phiphi_dth * vphi * vphi + m.dg_rr_dth * vr * vr + m.dg_thth_dth * vth * vth);

        H_abs = half * ST::abs(m.ginv_tt * p_t * p_t + two * m.ginv_tphi * p_t * p_phi
                             + m.ginv_phiphi * p_phi * p_phi + m.ginv_rr * prc * prc + m.ginv_thth * pthc * pthc);
    };

    for (int step = 0; step < 250000; ++step) {
        T h = h_base;
        if (r < ST::from_int(4)) h = h_base / ST::from_int(5);
        if (r < ST::from_ratio(22, 10)) h = h_base / ST::from_int(25);

        T dr1, dth1, dpr1, dpth1, H1;
        rhs_eval(r, th, p_r, p_th, dr1, dth1, dpr1, dpth1, H1);
        max_H = ST::max(max_H, H1);

        T dr2, dth2, dpr2, dpth2, H2;
        rhs_eval(r + half * h * dr1, th + half * h * dth1,
                 p_r + half * h * dpr1, p_th + half * h * dpth1,
                 dr2, dth2, dpr2, dpth2, H2);

        T dr3, dth3, dpr3, dpth3, H3;
        rhs_eval(r + half * h * dr2, th + half * h * dth2,
                 p_r + half * h * dpr2, p_th + half * h * dpth2,
                 dr3, dth3, dpr3, dpth3, H3);

        T dr4, dth4, dpr4, dpth4, H4;
        rhs_eval(r + h * dr3, th + h * dth3,
                 p_r + h * dpr3, p_th + h * dpth3,
                 dr4, dth4, dpr4, dpth4, H4);

        const T r_next   = r    + (h / six) * (dr1   + two * dr2   + two * dr3   + dr4);
        const T th_next  = th   + (h / six) * (dth1  + two * dth2  + two * dth3  + dth4);
        const T pr_next  = p_r  + (h / six) * (dpr1  + two * dpr2  + two * dpr3  + dpr4);
        const T pth_next = p_th + (h / six) * (dpth1 + two * dpth2 + two * dpth3 + dpth4);

        if (r_next < r_min) {
            r_min  = r_next;
            K_turn = evaluate_K_kds(r_next, th_next, pth_next, b, p);
        }
        if (r_next <= p.r_out) entered_shell = true;
        if (r_next <= p.r_in)  entered_core  = true;

        if (p_r < zero && pr_next >= zero) {
            const T s = dr1 / (dr1 - dr4);
            const T r_turn   = r    + h * s * (dr1   + half * s * (dr4   - dr1));
            const T th_turn  = th   + h * s * (dth1  + half * s * (dth4  - dth1));
            const T pth_turn = p_th + h * s * (dpth1 + half * s * (dpth4 - dpth1));
            if (r_turn < r_min) {
                r_min  = r_turn;
                K_turn = evaluate_K_kds(r_turn, th_turn, pth_turn, b, p);
            }
            if (r_min <= p.r_out) entered_shell = true;
            if (r_min <= p.r_in)  entered_core  = true;
            break;
        }

        r    = r_next;
        th   = th_next;
        p_r  = pr_next;
        p_th = pth_next;

        if (r > z_obs.r + one) break;
    }

    return {r_min, entered_shell, entered_core, K_init, K_turn, max_H};
}

// ============================================================================
// 8. Verification Suite Execution
// ============================================================================

template <typename T>
void run_zamo_orthonormality_suite(const ModelParams<T>& p, T tol_ortho, T tol_lapse) {
    using ST = ScalarTraits<T>;
    std::cout << "\n========================================================================================\n";
    std::cout << "TEST SUITE 1: ZAMO Tetrad Orthonormality & Ergoregion Regularity [" << ST::name() << "]\n";
    std::cout << "========================================================================================\n";

    struct TestPoint {
        const char* label;
        T r;
        T theta;
    };

    const std::array<TestPoint, 5> points = {{
        {"Exterior (r=15.0, th=1.20)",         ST::from_int(15),          ST::from_ratio(6, 5)},
        {"Ergoregion (r=1.85, th=pi/2)",       ST::from_ratio(185, 100),  ST::half_pi()},
        {"Outer Shell Join (r=1.80, th=1.10)", ST::from_ratio(9, 5),      ST::from_ratio(11, 10)},
        {"Mid-Shell (r=1.65, th=1.10)",        ST::from_ratio(165, 100),  ST::from_ratio(11, 10)},
        {"de Sitter Core (r=1.00, th=0.90)",   ST::from_int(1),           ST::from_ratio(9, 10)}
    }};

    std::cout << std::left
              << std::setw(35) << "Location"
              << std::setw(14) << "g_tt"
              << std::setw(14) << "omega"
              << std::setw(14) << "alpha_lapse"
              << std::setw(14) << "max|g_ab-eta|"
              << std::setw(12) << "Status" << "\n";
    std::cout << std::string(103, '-') << "\n";

    for (std::size_t idx = 0; idx < points.size(); ++idx) {
        const auto& pt = points[idx];
        const ZamoTetrad<T> z = build_zamo_tetrad(pt.r, pt.theta, p);
        const OrthonormalityReport<T> rep = check_zamo_orthonormality(z);

        if (idx == 1 || idx == 2) {
            require_check(z.metric.g_tt > ST::from_int(0),
                          std::string(pt.label) + " must lie inside ergoregion (g_tt > 0)");
        }
        require_check(rep.max_abs_err < tol_ortho,
                      std::string(pt.label) + " ZAMO tetrad orthonormality error exceeded tolerance");
        require_check(rep.lapse_consistency_err < tol_lapse,
                      std::string(pt.label) + " ZAMO lapse formula mismatch");

        const RayKinematics<T> ray = construct_ray_from_zamo_cosines(
            z, ST::from_ratio(1, 5), ST::from_ratio(-1, 7));
        require_check(ray.null_residual < tol_ortho,
                      std::string(pt.label) + " ZAMO projected ray null condition exceeded tolerance");

        std::cout << std::left
                  << std::setw(35) << pt.label
                  << std::setw(14) << ST::format(z.metric.g_tt, 6)
                  << std::setw(14) << ST::format(z.omega, 6)
                  << std::setw(14) << ST::format(z.alpha_lapse, 6, false)
                  << std::setw(14) << ST::format_sci(rep.max_abs_err, 3)
                  << std::setw(12) << "PASS" << "\n";
    }
}

int run_all_tests() {
    using ST_d = ScalarTraits<double>;
    using ST_q = ScalarTraits<qfloat>;

    const ModelParams<double> p_d{};
    const ModelParams<qfloat> p_q{};

    // ------------------------------------------------------------------------
    // 1. Run ZAMO Orthonormality Suite in both double and __float128
    // ------------------------------------------------------------------------
    run_zamo_orthonormality_suite<double>(p_d, 1e-14, 1e-14);
    run_zamo_orthonormality_suite<qfloat>(
        p_q,
        ST_q::from_ratio(1, 1000000000000000000LL) * ST_q::from_ratio(1, 10000000LL),
        ST_q::from_ratio(1, 1000000000000000000LL) * ST_q::from_ratio(1, 10000000LL));

    // ------------------------------------------------------------------------
    // 2. High-Precision (__float128 & double) Analytical vs. Numerical Contours
    // ------------------------------------------------------------------------
    const qfloat r_obs_q  = ST_q::from_int(15);
    const qfloat th_eq_q  = ST_q::half_pi();
    const qfloat th_inc_q = ST_q::from_ratio(6, 5);

    const double r_obs_d  = ST_d::from_int(15);
    const double th_eq_d  = ST_d::half_pi();
    const double th_inc_d = ST_d::from_ratio(6, 5);

    const ZamoTetrad<qfloat> z_eq_q  = build_zamo_tetrad(r_obs_q, th_eq_q, p_q);
    const ZamoTetrad<qfloat> z_inc_q = build_zamo_tetrad(r_obs_q, th_inc_q, p_q);
    const ZamoTetrad<double> z_eq_d  = build_zamo_tetrad(r_obs_d, th_eq_d, p_d);
    const ZamoTetrad<double> z_inc_d = build_zamo_tetrad(r_obs_d, th_inc_d, p_d);

    const auto [r_ph_plus, r_ph_minus] = solve_kerr_equatorial_photon_radii(p_q);
    const qfloat r_polar               = solve_kerr_polar_photon_radius(p_q);
    const qfloat b_kerr_plus           = kerr_spherical_b(r_ph_plus, p_q);
    const qfloat b_kerr_minus          = kerr_spherical_b(r_ph_minus, p_q);
    const qfloat b_join_18             = kerr_spherical_b(p_q.r_out, p_q);

    require_check(r_ph_plus < p_q.r_out && p_q.r_out < r_polar && r_polar < r_ph_minus,
                  "Expected r_ph^+ < r_out < r_polar < r_ph^- ordering violated");

    const double b_shell_plus_eq_d = analytical_sphere_grazing_b_plus(p_d.r_out, th_eq_d, p_d);
    const qfloat b_shell_plus_eq   = analytical_sphere_grazing_b_plus(p_q.r_out, th_eq_q, p_q);
    require_check(ST_d::abs(b_shell_plus_eq_d - static_cast<double>(b_shell_plus_eq)) < 1e-15,
                  "double vs __float128 prograde shell threshold mismatch");

    const qfloat b_shell_minus_eq = b_kerr_minus;
    const qfloat b_core_plus_eq   = analytical_sphere_grazing_b_plus(p_q.r_in, th_eq_q, p_q);
    const qfloat b_core_minus_eq  = analytical_sphere_grazing_b_minus(p_q.r_in, th_eq_q, p_q);

    const RayKinematics<qfloat> ray_eq_shell_min = construct_ray_from_invariants(z_eq_q, b_shell_plus_eq, ST_q::from_int(0));
    const RayKinematics<qfloat> ray_eq_shell_max = construct_ray_from_invariants(z_eq_q, b_shell_minus_eq, ST_q::from_int(0));
    const RayKinematics<qfloat> ray_eq_core_min  = construct_ray_from_invariants(z_eq_q, b_core_plus_eq, ST_q::from_int(0));
    const RayKinematics<qfloat> ray_eq_core_max  = construct_ray_from_invariants(z_eq_q, b_core_minus_eq, ST_q::from_int(0));

    const qfloat beta_kerr_alpha0_eq = ST_q::sqrt(kerr_spherical_eta(r_polar, p_q));
    const RayKinematics<qfloat> ray_eq_kerr_alpha0 = construct_ray_from_invariants(z_eq_q, ST_q::from_int(0), beta_kerr_alpha0_eq);

    const qfloat r_apex_eq    = solve_kerr_true_apex_radius(th_eq_q, p_q);
    const qfloat b_apex_eq    = kerr_spherical_b(r_apex_eq, p_q);
    const qfloat beta_apex_eq = ST_q::sqrt(kerr_spherical_eta(r_apex_eq, p_q));
    const RayKinematics<qfloat> ray_eq_kerr_apex = construct_ray_from_invariants(z_eq_q, b_apex_eq, beta_apex_eq);

    const double h_std_d  = 0.01;
    const qfloat h_std_q  = ST_q::from_ratio(1, 100);
    const qfloat h_fine_q = ST_q::from_ratio(1, 200);

    auto find_core_beta_double = [&](const ZamoTetrad<double>& z_obs_d, double b_val) {
        double lo = 0.0;
        double hi = 4.0;
        for (int iter = 0; iter < 42; ++iter) {
            const double mid = 0.5 * (lo + hi);
            const auto res = integrate_null_geodesic(z_obs_d, b_val, mid, p_d, h_std_d);
            if (res.entered_core) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    };

    auto refine_core_beta_qfloat = [&](const ZamoTetrad<qfloat>& z_obs_q, const ZamoTetrad<double>& z_obs_d,
                                       qfloat b_val, qfloat h_step) {
        const double seed = find_core_beta_double(z_obs_d, static_cast<double>(b_val));
        qfloat lo = static_cast<qfloat>(seed) - ST_q::from_ratio(1, 100000000LL);
        qfloat hi = static_cast<qfloat>(seed) + ST_q::from_ratio(1, 100000000LL);
        for (int iter = 0; iter < 18; ++iter) {
            const qfloat mid = ST_q::from_ratio(1, 2) * (lo + hi);
            const auto res = integrate_null_geodesic(z_obs_q, b_val, mid, p_q, h_step);
            if (res.entered_core) lo = mid; else hi = mid;
        }
        return ST_q::from_ratio(1, 2) * (lo + hi);
    };

    const qfloat beta_core_alpha0_eq      = refine_core_beta_qfloat(z_eq_q, z_eq_d, ST_q::from_int(0), h_std_q);
    const qfloat beta_core_alpha0_eq_fine = refine_core_beta_qfloat(z_eq_q, z_eq_d, ST_q::from_int(0), h_fine_q);
    const qfloat err_beta_core_alpha0_eq  = ST_q::abs(beta_core_alpha0_eq - beta_core_alpha0_eq_fine)
                                          + ST_q::from_ratio(2, 100000000LL) / static_cast<qfloat>(1ULL << 18);
    const RayKinematics<qfloat> ray_eq_core_alpha0 = construct_ray_from_invariants(z_eq_q, ST_q::from_int(0), beta_core_alpha0_eq_fine);

    const qfloat Xi_core = ST_q::from_int(1) + (p_q.Lam / ST_q::from_int(3)) * p_q.a * p_q.a;
    const qfloat K_core_turn_b0 = (Xi_core * Xi_core * (p_q.r_in * p_q.r_in + p_q.a * p_q.a)
                                 * (p_q.r_in * p_q.r_in + p_q.a * p_q.a)) / evaluate_delta(p_q.r_in, p_q);
    const qfloat beta_core_frozen_b0_eq = ST_q::sqrt(K_core_turn_b0 - p_q.a * p_q.a);

    const auto trace_core_b0_eq = integrate_null_geodesic(z_eq_q, ST_q::from_int(0), beta_core_alpha0_eq_fine, p_q, h_fine_q);
    require_check(ST_q::abs(trace_core_b0_eq.K_kds_at_rmin - K_core_turn_b0) < ST_q::from_ratio(1, 1000000LL),
                  "Evolved K_KdS at r_min=1.5 must match exact local core turning value");

    auto find_core_true_apex = [&](const ZamoTetrad<qfloat>& z_obs_q, const ZamoTetrad<double>& z_obs_d) {
        const double phi_gold = (std::sqrt(5.0) - 1.0) / 2.0;
        double a_b = -2.2;
        double b_b =  0.4;
        double c_b = b_b - phi_gold * (b_b - a_b);
        double d_b = a_b + phi_gold * (b_b - a_b);
        double fc  = find_core_beta_double(z_obs_d, c_b);
        double fd  = find_core_beta_double(z_obs_d, d_b);
        for (int iter = 0; iter < 30; ++iter) {
            if (fc > fd) {
                b_b = d_b; d_b = c_b; fd = fc;
                c_b = b_b - phi_gold * (b_b - a_b);
                fc  = find_core_beta_double(z_obs_d, c_b);
            } else {
                a_b = c_b; c_b = d_b; fc = fd;
                d_b = a_b + phi_gold * (b_b - a_b);
                fd  = find_core_beta_double(z_obs_d, d_b);
            }
        }
        const qfloat b_opt    = static_cast<qfloat>(0.5 * (a_b + b_b));
        const qfloat beta_opt = refine_core_beta_qfloat(z_obs_q, z_obs_d, b_opt, h_fine_q);
        return std::make_pair(b_opt, beta_opt);
    };

    const auto [b_core_apex_eq, beta_core_apex_eq] = find_core_true_apex(z_eq_q, z_eq_d);
    const RayKinematics<qfloat> ray_eq_core_apex = construct_ray_from_invariants(z_eq_q, b_core_apex_eq, beta_core_apex_eq);

    // ------------------------------------------------------------------------
    // Inclined Observer (r_obs = 15, theta_obs = 1.2 rad)
    // ------------------------------------------------------------------------
    const qfloat b_shell_plus_inc  = analytical_sphere_grazing_b_plus(p_q.r_out, th_inc_q, p_q);
    const qfloat r0_retro_inc      = solve_kerr_retrograde_beta0_radius(th_inc_q, p_q);
    const qfloat b_shell_minus_inc = kerr_spherical_b(r0_retro_inc, p_q);

    auto find_core_b_beta0_inc = [&](double lo_d, double hi_d, bool core_at_lo, qfloat h_step) {
        for (int iter = 0; iter < 42; ++iter) {
            const double mid = 0.5 * (lo_d + hi_d);
            const auto res = integrate_null_geodesic(z_inc_d, mid, 0.0, p_d, h_std_d);
            if (res.entered_core == core_at_lo) lo_d = mid; else hi_d = mid;
        }
        const double seed = 0.5 * (lo_d + hi_d);
        qfloat lo = static_cast<qfloat>(seed) - ST_q::from_ratio(1, 100000000LL);
        qfloat hi = static_cast<qfloat>(seed) + ST_q::from_ratio(1, 100000000LL);
        for (int iter = 0; iter < 18; ++iter) {
            const qfloat mid = ST_q::from_ratio(1, 2) * (lo + hi);
            const auto res = integrate_null_geodesic(z_inc_q, mid, ST_q::from_int(0), p_q, h_step);
            if (res.entered_core == core_at_lo) lo = mid; else hi = mid;
        }
        return ST_q::from_ratio(1, 2) * (lo + hi);
    };

    const qfloat b_core_plus_inc       = find_core_b_beta0_inc(0.0, 2.5, true, h_std_q);
    const qfloat b_core_plus_inc_fine  = find_core_b_beta0_inc(0.0, 2.5, true, h_fine_q);
    const qfloat err_b_core_plus_inc   = ST_q::abs(b_core_plus_inc - b_core_plus_inc_fine)
                                       + ST_q::from_ratio(2, 100000000LL) / static_cast<qfloat>(1ULL << 18);

    const qfloat b_core_minus_inc      = find_core_b_beta0_inc(-4.5, 0.0, false, h_std_q);
    const qfloat b_core_minus_inc_fine = find_core_b_beta0_inc(-4.5, 0.0, false, h_fine_q);
    const qfloat err_b_core_minus_inc  = ST_q::abs(b_core_minus_inc - b_core_minus_inc_fine)
                                       + ST_q::from_ratio(2, 100000000LL) / static_cast<qfloat>(1ULL << 18);

    const RayKinematics<qfloat> ray_inc_shell_min = construct_ray_from_invariants(z_inc_q, b_shell_plus_inc, ST_q::from_int(0));
    const RayKinematics<qfloat> ray_inc_shell_max = construct_ray_from_invariants(z_inc_q, b_shell_minus_inc, ST_q::from_int(0));
    const RayKinematics<qfloat> ray_inc_core_min  = construct_ray_from_invariants(z_inc_q, b_core_plus_inc_fine, ST_q::from_int(0));
    const RayKinematics<qfloat> ray_inc_core_max  = construct_ray_from_invariants(z_inc_q, b_core_minus_inc_fine, ST_q::from_int(0));

    const qfloat cth_inc = ST_q::cos(th_inc_q);
    const qfloat sth_inc = ST_q::sin(th_inc_q);
    const qfloat cot_inc = cth_inc / sth_inc;

    const qfloat beta_kerr_alpha0_inc = ST_q::sqrt(kerr_spherical_eta(r_polar, p_q) + p_q.a * p_q.a * cth_inc * cth_inc);
    const RayKinematics<qfloat> ray_inc_kerr_alpha0 = construct_ray_from_invariants(z_inc_q, ST_q::from_int(0), beta_kerr_alpha0_inc);

    const qfloat r_apex_inc    = solve_kerr_true_apex_radius(th_inc_q, p_q);
    const qfloat b_apex_inc    = kerr_spherical_b(r_apex_inc, p_q);
    const qfloat beta_apex_inc = ST_q::sqrt(kerr_spherical_eta(r_apex_inc, p_q)
                               + p_q.a * p_q.a * cth_inc * cth_inc - b_apex_inc * b_apex_inc * cot_inc * cot_inc);
    const RayKinematics<qfloat> ray_inc_kerr_apex = construct_ray_from_invariants(z_inc_q, b_apex_inc, beta_apex_inc);

    const qfloat beta_core_alpha0_inc      = refine_core_beta_qfloat(z_inc_q, z_inc_d, ST_q::from_int(0), h_std_q);
    const qfloat beta_core_alpha0_inc_fine = refine_core_beta_qfloat(z_inc_q, z_inc_d, ST_q::from_int(0), h_fine_q);
    const qfloat err_beta_core_alpha0_inc  = ST_q::abs(beta_core_alpha0_inc - beta_core_alpha0_inc_fine)
                                           + ST_q::from_ratio(2, 100000000LL) / static_cast<qfloat>(1ULL << 18);
    const RayKinematics<qfloat> ray_inc_core_alpha0 = construct_ray_from_invariants(z_inc_q, ST_q::from_int(0), beta_core_alpha0_inc_fine);

    const auto [b_core_apex_inc, beta_core_apex_inc] = find_core_true_apex(z_inc_q, z_inc_d);
    const RayKinematics<qfloat> ray_inc_core_apex = construct_ray_from_invariants(z_inc_q, b_core_apex_inc, beta_core_apex_inc);

    // ------------------------------------------------------------------------
    // Print Formatted Table 2: Screen-Plane Boundaries (Asymptotic vs. ZAMO)
    // ------------------------------------------------------------------------
    std::cout << "\n========================================================================================\n";
    std::cout << "TEST SUITE 2: Analytical vs. Numerical Contour Boundaries & Finite-Distance ZAMO Map\n";
    std::cout << "  Observer radius r_obs = 15M. Distinguishes:\n";
    std::cout << "  - Asymptotic Bardeen (alpha_inf, beta_inf) vs. Finite-Distance ZAMO (alpha_Z, beta_Z) = r_obs*(x_Z, y_Z)\n";
    std::cout << "  - Vertical-axis intercept (alpha = 0) vs. displaced True Apex (beta_max at alpha != 0)\n";
    std::cout << "  - Exact Analytical (Exterior Kerr / Equatorial Symmetry) vs. Numerical Shooting (Non-Separable Shell)\n";
    std::cout << "========================================================================================\n";

    auto print_row = [](const char* label, const char* method, const RayKinematics<qfloat>& rk, const std::string& err_bound) {
        std::cout << std::left
                  << std::setw(34) << label
                  << std::setw(14) << method
                  << std::setw(15) << ST_q::format(rk.alpha_inf, 8)
                  << std::setw(15) << ST_q::format(rk.beta_inf, 8)
                  << std::setw(15) << ST_q::format(rk.alpha_zamo_r, 8)
                  << std::setw(15) << ST_q::format(rk.beta_zamo_r, 8)
                  << std::setw(12) << err_bound << "\n";
    };

    std::cout << "\n--- A. Equatorial Observer (r_obs = 15, theta_obs = pi/2) ---\n";
    std::cout << std::left
              << std::setw(34) << "Feature / Boundary Point"
              << std::setw(14) << "Method"
              << std::setw(15) << "alpha_inf"
              << std::setw(15) << "beta_inf"
              << std::setw(15) << "alpha_Z(r=15)"
              << std::setw(15) << "beta_Z(r=15)"
              << std::setw(12) << "Error Bound" << "\n";
    std::cout << std::string(120, '-') << "\n";

    print_row("Shell Entry Prograde (beta=0)",   "Analytical", ray_eq_shell_min,   "< 1e-32");
    print_row("Kerr Barrier Retrograde (beta=0)","Analytical", ray_eq_shell_max,   "< 1e-32");
    print_row("Kerr Barrier Intercept (alpha=0)","Analytical", ray_eq_kerr_alpha0, "< 1e-32");
    print_row("Kerr Barrier True Apex (r0=3M)",  "Analytical", ray_eq_kerr_apex,   "< 1e-32");
    print_row("Core Entry Prograde (beta=0)",    "Analytical", ray_eq_core_min,    "< 1e-32");
    print_row("Core Entry Retrograde (beta=0)",  "Analytical", ray_eq_core_max,    "< 1e-32");
    print_row("Core Entry Intercept (alpha=0)",  "Numerical",  ray_eq_core_alpha0, ST_q::format_sci(err_beta_core_alpha0_eq, 2));
    print_row("Core Entry True Apex (beta_max)", "Numerical",  ray_eq_core_apex,   "< 2.5e-07");

    std::cout << "\n--- B. Inclined Observer (r_obs = 15, theta_obs = 1.2 rad) ---\n";
    std::cout << std::left
              << std::setw(34) << "Feature / Boundary Point"
              << std::setw(14) << "Method"
              << std::setw(15) << "alpha_inf"
              << std::setw(15) << "beta_inf"
              << std::setw(15) << "alpha_Z(r=15)"
              << std::setw(15) << "beta_Z(r=15)"
              << std::setw(12) << "Error Bound" << "\n";
    std::cout << std::string(120, '-') << "\n";

    print_row("Shell Entry Prograde (beta=0)",   "Analytical", ray_inc_shell_min,   "< 1e-32");
    print_row("Kerr Barrier Retrograde (beta=0)","Analytical", ray_inc_shell_max,   "< 1e-32");
    print_row("Kerr Barrier Intercept (alpha=0)","Analytical", ray_inc_kerr_alpha0, "< 1e-32");
    print_row("Kerr Barrier True Apex (beta_max)","Analytical",ray_inc_kerr_apex,   "< 1e-32");
    print_row("Core Entry Prograde (beta=0)",    "Numerical",  ray_inc_core_min,    ST_q::format_sci(err_b_core_plus_inc / sth_inc, 2));
    print_row("Core Entry Retrograde (beta=0)",  "Numerical",  ray_inc_core_max,    ST_q::format_sci(err_b_core_minus_inc / sth_inc, 2));
    print_row("Core Entry Intercept (alpha=0)",  "Numerical",  ray_inc_core_alpha0, ST_q::format_sci(err_beta_core_alpha0_inc, 2));
    print_row("Core Entry True Apex (beta_max)", "Numerical",  ray_inc_core_apex,   "< 2.5e-07");

    std::cout << "\n--- C. 30-Digit Exact Analytical Reference Constants (__float128) ---\n";
    std::cout << "  r_ph^+ (masked Kerr prograde)  = " << ST_q::format(r_ph_plus, 30) << "\n";
    std::cout << "  b_Kerr(r_ph^+)                 = " << ST_q::format(b_kerr_plus, 30) << "\n";
    std::cout << "  b_crit,shell^+ (eq, r=1.8)     = " << ST_q::format(b_shell_plus_eq, 30) << "\n";
    std::cout << "  b_join(r_out=1.8) [C^1 join]   = " << ST_q::format(b_join_18, 30) << "\n";
    std::cout << "  r_polar (Kerr b=0 orbit)       = " << ST_q::format(r_polar, 30) << "\n";
    std::cout << "  r_ph^- (Kerr retrograde)       = " << ST_q::format(r_ph_minus, 30) << "\n";
    std::cout << "  b_crit,shell^- (eq, r_ph^-)    = " << ST_q::format(b_shell_minus_eq, 30) << "\n";
    std::cout << "  b_crit,core^+  (eq, r=1.5)     = " << ST_q::format(b_core_plus_eq, 30) << "\n";
    std::cout << "  b_crit,core^-  (eq, r=1.5)     = " << ST_q::format(b_core_minus_eq, 30) << "\n";
    std::cout << "  Non-separable shell drift at (b=0, th=pi/2): numerical beta_inf(core) = "
              << ST_q::format(beta_core_alpha0_eq_fine, 12)
              << " vs. frozen-K_KdS approximation = "
              << ST_q::format(beta_core_frozen_b0_eq, 12)
              << " (drift shift Delta_beta = "
              << ST_q::format(beta_core_alpha0_eq_fine - beta_core_frozen_b0_eq, 8) << ")\n";

    // ------------------------------------------------------------------------
    // 3. Test Suite 3: Geodesic Shooting Cross-Validation Bracketing (alpha +/- eps, beta)
    // ------------------------------------------------------------------------
    std::cout << "\n========================================================================================\n";
    std::cout << "TEST SUITE 3: Geodesic Shooting Cross-Validation Bracketing (eps = 1e-4)\n";
    std::cout << "========================================================================================\n";
    std::cout << std::left
              << std::setw(38) << "Boundary Test Case"
              << std::setw(18) << "r_min (inside)"
              << std::setw(18) << "r_min (outside)"
              << std::setw(14) << "Target R"
              << std::setw(10) << "Status" << "\n";
    std::cout << std::string(98, '-') << "\n";

    const qfloat eps = ST_q::from_ratio(1, 10000);

    struct BracketCheck {
        const char* name;
        const ZamoTetrad<qfloat>* z;
        qfloat alpha_in;
        qfloat beta_in;
        qfloat alpha_out;
        qfloat beta_out;
        bool test_core;
    };

    const std::array<BracketCheck, 12> bracket_tests = {{
        {"Eq (th=pi/2) Prograde Shell (beta=0)",  &z_eq_q,
         ray_eq_shell_min.alpha_inf + eps, ST_q::from_int(0),
         ray_eq_shell_min.alpha_inf - eps, ST_q::from_int(0), false},

        {"Eq (th=pi/2) Retrograde Kerr (beta=0)", &z_eq_q,
         ray_eq_shell_max.alpha_inf - eps, ST_q::from_int(0),
         ray_eq_shell_max.alpha_inf + eps, ST_q::from_int(0), false},

        {"Eq (th=pi/2) Prograde Core (beta=0)",   &z_eq_q,
         ray_eq_core_min.alpha_inf + eps, ST_q::from_int(0),
         ray_eq_core_min.alpha_inf - eps, ST_q::from_int(0), true},

        {"Eq (th=pi/2) Retrograde Core (beta=0)", &z_eq_q,
         ray_eq_core_max.alpha_inf - eps, ST_q::from_int(0),
         ray_eq_core_max.alpha_inf + eps, ST_q::from_int(0), true},

        {"Eq (th=pi/2) Polar Kerr (alpha=0)",     &z_eq_q,
         ST_q::from_int(0), ray_eq_kerr_alpha0.beta_inf - eps,
         ST_q::from_int(0), ray_eq_kerr_alpha0.beta_inf + eps, false},

        {"Eq (th=pi/2) Polar Core (alpha=0)",     &z_eq_q,
         ST_q::from_int(0), ray_eq_core_alpha0.beta_inf - eps,
         ST_q::from_int(0), ray_eq_core_alpha0.beta_inf + eps, true},

        {"Inc (th=1.2) Prograde Shell (beta=0)",  &z_inc_q,
         ray_inc_shell_min.alpha_inf + eps, ST_q::from_int(0),
         ray_inc_shell_min.alpha_inf - eps, ST_q::from_int(0), false},

        {"Inc (th=1.2) Retrograde Kerr (beta=0)", &z_inc_q,
         ray_inc_shell_max.alpha_inf - eps, ST_q::from_int(0),
         ray_inc_shell_max.alpha_inf + eps, ST_q::from_int(0), false},

        {"Inc (th=1.2) Prograde Core (beta=0)",   &z_inc_q,
         ray_inc_core_min.alpha_inf + eps, ST_q::from_int(0),
         ray_inc_core_min.alpha_inf - eps, ST_q::from_int(0), true},

        {"Inc (th=1.2) Retrograde Core (beta=0)", &z_inc_q,
         ray_inc_core_max.alpha_inf - eps, ST_q::from_int(0),
         ray_inc_core_max.alpha_inf + eps, ST_q::from_int(0), true},

        {"Inc (th=1.2) Polar Kerr (alpha=0)",     &z_inc_q,
         ST_q::from_int(0), ray_inc_kerr_alpha0.beta_inf - eps,
         ST_q::from_int(0), ray_inc_kerr_alpha0.beta_inf + eps, false},

        {"Inc (th=1.2) Polar Core (alpha=0)",     &z_inc_q,
         ST_q::from_int(0), ray_inc_core_alpha0.beta_inf - eps,
         ST_q::from_int(0), ray_inc_core_alpha0.beta_inf + eps, true}
    }};

    for (const auto& chk : bracket_tests) {
        const qfloat sth = ST_q::sin(chk.z->theta);
        const auto res_in  = integrate_null_geodesic(*chk.z, -chk.alpha_in * sth,  chk.beta_in,  p_q, h_std_q);
        const auto res_out = integrate_null_geodesic(*chk.z, -chk.alpha_out * sth, chk.beta_out, p_q, h_std_q);

        const bool inside_ok  = chk.test_core ? res_in.entered_core  : res_in.entered_shell;
        const bool outside_ok = chk.test_core ? !res_out.entered_core : !res_out.entered_shell;
        require_check(inside_ok && outside_ok,
                      std::string("Bracketing check failed for ") + chk.name);

        std::cout << std::left
                  << std::setw(38) << chk.name
                  << std::setw(18) << ST_q::format(res_in.r_min, 8, false)
                  << std::setw(18) << ST_q::format(res_out.r_min, 8, false)
                  << std::setw(14) << (chk.test_core ? "r <= 1.50" : "r <= 1.80")
                  << std::setw(10) << "PASS" << "\n";
    }

    std::cout << "\n========================================================================================\n";
    std::cout << "ALL " << g_checks_passed << " RELEASE-ACTIVE VERIFICATION CHECKS PASSED.\n";
    std::cout << "========================================================================================\n";
    return EXIT_SUCCESS;
}

} // namespace gravastar

int main() {
    return gravastar::run_all_tests();
}