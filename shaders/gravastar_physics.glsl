// SPDX-License-Identifier: GPL-3.0-or-later
// Gravastar Spacetime Physics Kernel in GLSL
// Reference: Ilijić & DeBenedictis, arXiv:2309.00551v2
#ifndef GRAVASTAR_PHYSICS_GLSL
#define GRAVASTAR_PHYSICS_GLSL

struct MetricEvaluation {
    float g00, g03, g11, g22, g33;
    float inv00, inv03, inv11, inv22, inv33;
    float dg_dr00, dg_dr03, dg_dr11, dg_dr22, dg_dr33;
    float dg_dth00, dg_dth03, dg_dth11, dg_dth22, dg_dth33;
    float delta, sigma, T, X;
    bool valid;
};

float smoothstep_c3(float u) {
    float u2 = u * u;
    return u2 * u2 * (35.0 + u * (-84.0 + u * (70.0 - 20.0 * u)));
}

float dsmoothstep_c3(float u) {
    float u3 = u * u * u;
    return u3 * (140.0 + u * (-420.0 + u * (420.0 - 140.0 * u)));
}

MetricEvaluation evaluate_gravastar_metric(
    float r, float theta,
    float M, float a, float Lambda,
    float r_in, float r_out)
{
    MetricEvaluation res;
    res.valid = false;

    const float PI = 3.14159265358979323846;
    if (r <= 1e-4 || theta <= 1e-4 || theta >= (PI - 1e-4)) {
        return res;
    }

    float width = r_out - r_in;
    float mu = 0.0;
    float lam = 0.0;
    float dmu_dr = 0.0;
    float dlam_dr = 0.0;

    if (r <= r_in) {
        mu = 0.0; lam = 1.0;
        dmu_dr = 0.0; dlam_dr = 0.0;
    } else if (r >= r_out) {
        mu = 1.0; lam = 0.0;
        dmu_dr = 0.0; dlam_dr = 0.0;
    } else {
        if (r - r_in <= width * 0.5) {
            float u = (r - r_in) / width;
            mu = smoothstep_c3(u);
            lam = 1.0 - mu;
            dmu_dr = dsmoothstep_c3(u) / width;
            dlam_dr = -dmu_dr;
        } else {
            float u = (r_out - r) / width;
            lam = smoothstep_c3(u);
            mu = 1.0 - lam;
            dlam_dr = -dsmoothstep_c3(u) / width;
            dmu_dr = -dlam_dr;
        }
    }

    float eff_lam = Lambda * lam;
    float deff_lam_dr = Lambda * dlam_dr;

    float r2 = r * r;
    float a2 = a * a;
    float s = sin(theta);
    float c = cos(theta);
    float s2 = s * s;
    float c2 = c * c;
    float sin_2th = sin(2.0 * theta);

    float A = r2 + a2;
    float sigma = r2 + a2 * c2;
    float delta = A * (1.0 - eff_lam * r2 / 3.0) - 2.0 * M * mu * r;
    float T = 1.0 + eff_lam * a2 * c2 / 3.0;
    float X = 1.0 + eff_lam * a2 / 3.0;

    if (delta <= 1e-6 || sigma <= 1e-6 || T <= 1e-6 || X <= 1e-6) {
        return res; // Horizon / singularity
    }

    res.delta = delta;
    res.sigma = sigma;
    res.T = T;
    res.X = X;

    float denom = sigma * X * X;

    // Metric components
    res.g00 = (T * a2 * s2 - delta) / denom;
    res.g03 = a * s2 * (delta - T * A) / denom;
    res.g33 = s2 * (T * A * A - delta * a2 * s2) / denom;
    res.g11 = sigma / delta;
    res.g22 = sigma / T;

    // Inverse metric
    float pref = (X * X) / sigma;
    res.inv00 = -pref * (A * A / delta - a2 * s2 / T);
    res.inv03 = -a * pref * (A / delta - 1.0 / T);
    res.inv33 = pref * (1.0 / (T * s2) - a2 / delta);
    res.inv11 = delta / sigma;
    res.inv22 = T / sigma;

    // --- Derivatives with respect to r ---
    float dsigma_dr = 2.0 * r;
    float dA_dr = 2.0 * r;
    float ddelta_dr = dA_dr * (1.0 - eff_lam * r2 / 3.0)
                    + A * (-deff_lam_dr * r2 / 3.0 - eff_lam * 2.0 * r / 3.0)
                    - 2.0 * M * (dmu_dr * r + mu);
    float dT_dr = deff_lam_dr * a2 * c2 / 3.0;
    float dX_dr = deff_lam_dr * a2 / 3.0;
    float ddenom_dr = dsigma_dr * X * X + sigma * 2.0 * X * dX_dr;

    float N00 = T * a2 * s2 - delta;
    float dN00_dr = dT_dr * a2 * s2 - ddelta_dr;
    res.dg_dr00 = (dN00_dr * denom - N00 * ddenom_dr) / (denom * denom);

    float N03 = a * s2 * (delta - T * A);
    float dN03_dr = a * s2 * (ddelta_dr - dT_dr * A - T * dA_dr);
    res.dg_dr03 = (dN03_dr * denom - N03 * ddenom_dr) / (denom * denom);

    float N33 = s2 * (T * A * A - delta * a2 * s2);
    float dN33_dr = s2 * (dT_dr * A * A + T * 2.0 * A * dA_dr - ddelta_dr * a2 * s2);
    res.dg_dr33 = (dN33_dr * denom - N33 * ddenom_dr) / (denom * denom);

    res.dg_dr11 = (dsigma_dr * delta - sigma * ddelta_dr) / (delta * delta);
    res.dg_dr22 = (dsigma_dr * T - sigma * dT_dr) / (T * T);

    // --- Derivatives with respect to theta ---
    float dsigma_dth = -a2 * sin_2th;
    float dT_dth = -eff_lam * a2 * sin_2th / 3.0;
    float ddenom_dth = dsigma_dth * X * X;

    float dN00_dth = dT_dth * a2 * s2 + T * a2 * sin_2th;
    res.dg_dth00 = (dN00_dth * denom - N00 * ddenom_dth) / (denom * denom);

    float dN03_dth = a * sin_2th * (delta - T * A) - a * s2 * (dT_dth * A);
    res.dg_dth03 = (dN03_dth * denom - N03 * ddenom_dth) / (denom * denom);

    float dN33_dth = sin_2th * (T * A * A - delta * a2 * s2)
                   + s2 * (dT_dth * A * A - delta * a2 * sin_2th);
    res.dg_dth33 = (dN33_dth * denom - N33 * ddenom_dth) / (denom * denom);

    res.dg_dth11 = dsigma_dth / delta;
    res.dg_dth22 = (dsigma_dth * T - sigma * dT_dth) / (T * T);

    res.valid = true;
    return res;
}

// Compute Hamilton's Equations of Motion:
// dx^mu / dlambda = g^{mu nu} p_nu
// dp_mu / dlambda = 0.5 * (partial_mu g_{alpha beta}) v^alpha v^beta
void compute_geodesic_rhs(
    in MetricEvaluation m,
    in vec4 x, in vec4 p,
    out vec4 dx, out vec4 dp,
    out float null_constraint)
{
    // dx^mu = g^{mu nu} p_nu
    dx.x = m.inv00 * p.x + m.inv03 * p.w; // t
    dx.y = m.inv11 * p.y;                 // r
    dx.z = m.inv22 * p.z;                 // theta
    dx.w = m.inv03 * p.x + m.inv33 * p.w; // phi

    // Null constraint H = 0.5 * v^mu p_mu
    null_constraint = 0.5 * (dx.x * p.x + dx.y * p.y + dx.z * p.z + dx.w * p.w);

    // dp_0 = 0 (t-symmetry / Energy)
    // dp_3 = 0 (phi-symmetry / Angular momentum)
    dp.x = 0.0;
    dp.w = 0.0;

    // dp_1 (r)
    dp.y = 0.5 * (m.dg_dr00 * dx.x * dx.x
                + 2.0 * m.dg_dr03 * dx.x * dx.w
                + m.dg_dr33 * dx.w * dx.w
                + m.dg_dr11 * dx.y * dx.y
                + m.dg_dr22 * dx.z * dx.z);

    // dp_2 (theta)
    dp.z = 0.5 * (m.dg_dth00 * dx.x * dx.x
                + 2.0 * m.dg_dth03 * dx.x * dx.w
                + m.dg_dth33 * dx.w * dx.w
                + m.dg_dth11 * dx.y * dx.y
                + m.dg_dth22 * dx.z * dx.z);
}

#endif // GRAVASTAR_PHYSICS_GLSL
