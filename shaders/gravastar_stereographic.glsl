// SPDX-License-Identifier: GPL-3.0-or-later
// Closed-form toy-gravastar Hamiltonian and gradient in a stereographic
// patch, with no trigonometric functions (GLSL has none for double). GPU twin
// of tests/stereographic_hamiltonian.hpp; the formulas are derived there.
// Compile with -DGRAVASTAR_FP64 for double precision, otherwise float.
// Integer literals are wrapped in REAL(...) so double builds lose nothing to
// single-precision constant folding.
#ifndef GRAVASTAR_STEREOGRAPHIC_GLSL
#define GRAVASTAR_STEREOGRAPHIC_GLSL

#ifdef GRAVASTAR_FP64
#define REAL double
#else
#define REAL float
#endif

struct StereoModel {
    REAL M, a, Lambda, r_in, r_out;
};

// d(state)/ds in the production convention (dx/ds = -dH/dp, dp/ds = +dH/dx).
// Returns false where the chart is not Lorentzian or a value is not finite.
bool stereographic_rhs(StereoModel m, REAL y[8], out REAL d[8], out REAL H) {
    REAL r = y[1], x = y[2], v = y[3];
    REAL pt = y[4], pr = y[5], px = y[6], py = y[7];
    REAL a = m.a, a2 = a * a;
    REAL mass = m.M, dmass = REAL(0), k = REAL(0), dk = REAL(0);
    if (r <= m.r_in) {
        mass = REAL(0);
        k = m.Lambda / REAL(3);
    } else if (r < m.r_out) {
        REAL w = m.r_out - m.r_in, u = (r - m.r_in) / w;
        REAL u2 = u * u, u3 = u2 * u, u4 = u2 * u2;
        REAL S = u4 * (REAL(35) - REAL(84) * u + REAL(70) * u2 - REAL(20) * u3);
        REAL dS = u3 * (REAL(140) - REAL(420) * u + REAL(420) * u2 - REAL(140) * u3) / w;
        mass = m.M * S;
        dmass = m.M * dS;
        k = m.Lambda * (REAL(1) - S) / REAL(3);
        dk = -m.Lambda * dS / REAL(3);
    }
    REAL q = x * x + v * v, op = REAL(1) + q;
    REAL c = (REAL(1) - q) / op, c2 = c * c, s2 = REAL(4) * q / (op * op);
    REAL dc2 = REAL(2) * c * (REAL(-2) / (op * op));
    REAL ds2 = -dc2;
    REAL B = r * r + a2, sigma = r * r + a2 * c2;
    REAL T = REAL(1) + k * a2 * c2, X = REAL(1) + k * a2;
    REAL D = B * (REAL(1) - k * r * r) - REAL(2) * mass * r;
    if (!(D > REAL(0)) || !(T > REAL(0)) || !(sigma > REAL(0)))
        return false;
    REAL L = x * py - v * px, P2 = px * px + py * py;
    REAL W = B * pt + a * L;
    REAL ang = a2 * s2 * pt * pt + REAL(2) * a * pt * L;
    REAL E1 = op * op / REAL(4) * T * P2;
    REAL E2 = k * a2 * (X + T) * L * L / T;
    REAL E3 = X * X / T * ang;
    REAL E4 = X * X * W * W / D;
    REAL N = D * pr * pr + E1 + E2 + E3 - E4;
    H = N / (REAL(2) * sigma);

    REAL dB = REAL(2) * r, dX = a2 * dk, dT = a2 * c2 * dk;
    REAL dD = dB * (REAL(1) - k * r * r) + B * (-dk * r * r - REAL(2) * k * r) -
              REAL(2) * (dmass * r + mass);
    REAL dN_dr = dD * pr * pr + op * op / REAL(4) * P2 * dT +
                 a2 * L * L * (dk * (X + T) / T + k * (dX + dT) / T - k * (X + T) * dT / (T * T)) +
                 ang * (REAL(2) * X * dX / T - X * X * dT / (T * T)) -
                 ((REAL(2) * X * dX * W * W + X * X * REAL(2) * W * dB * pt) / D - E4 * dD / D);
    REAL dTq = k * a2 * dc2;
    REAL dN_dq = P2 * (op / REAL(2) * T + op * op / REAL(4) * dTq) -
                 k * a2 * L * L * X * dTq / (T * T) +
                 X * X * (a2 * pt * pt * ds2 / T - ang * dTq / (T * T));
    REAL dN_dL = REAL(2) * k * a2 * (X + T) * L / T + X * X / T * REAL(2) * a * pt -
                 X * X * REAL(2) * W * a / D;
    REAL dsigma_dr = REAL(2) * r, dsigma_dq = a2 * dc2;
    REAL inv = REAL(1) / (REAL(2) * sigma);
    REAL dH_dr = dN_dr * inv - H * dsigma_dr / sigma;
    REAL dH_dq = dN_dq * inv - H * dsigma_dq / sigma;
    REAL dH_dL = dN_dL * inv;
    REAL dH_dx = dH_dq * REAL(2) * x + dH_dL * py;
    REAL dH_dy = dH_dq * REAL(2) * v - dH_dL * px;
    REAL dH_dpt = (X * X / T * (REAL(2) * a2 * s2 * pt + REAL(2) * a * L) -
                   X * X * REAL(2) * W * B / D) * inv;
    REAL dH_dpr = REAL(2) * D * pr * inv;
    REAL dH_dpx = op * op / REAL(2) * T * px * inv - dH_dL * v;
    REAL dH_dpy = op * op / REAL(2) * T * py * inv + dH_dL * x;
    d[0] = -dH_dpt;
    d[1] = -dH_dpr;
    d[2] = -dH_dpx;
    d[3] = -dH_dpy;
    d[4] = REAL(0);
    d[5] = dH_dr;
    d[6] = dH_dx;
    d[7] = dH_dy;
    for (int i = 0; i < 8; ++i)
        if (isnan(d[i]) || isinf(d[i]))
            return false;
    return true;
}

// Inversion to the opposite patch; preserves L.
void switch_patch(inout REAL y[8]) {
    REAL q = y[2] * y[2] + y[3] * y[3];
    REAL R = y[2] * y[6] + y[3] * y[7];
    REAL x = y[2], v = y[3];
    y[2] = x / q;
    y[3] = v / q;
    y[6] = q * y[6] - REAL(2) * x * R;
    y[7] = q * y[7] - REAL(2) * v * R;
}

#endif
