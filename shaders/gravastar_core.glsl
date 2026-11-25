// SPDX-License-Identifier: GPL-3.0-or-later
// Static Cartesian core chart (T,X,Y,Z) of the exact de Sitter core, GPU twin
// of src/optics/core_chart.cpp and geometry::evaluate_core. Derivation and
// domain rules: docs/CORE_CONTINUATION.md. Compile with -DGRAVASTAR_FP64 for
// double precision, otherwise float (REAL comes from the stereographic file).
#ifndef GRAVASTAR_CORE_GLSL
#define GRAVASTAR_CORE_GLSL

#include "gravastar_stereographic.glsl"

// sin and cos without GLSL's float-only trigonometry: reduce by the nearest
// multiple n of pi/2, then Taylor series on |r| <= pi/4 to degree 17 (sin)
// and 18 (cos), truncation below 1e-19. GLSL floating literals are single
// precision, so pi/2 is the sum of three float-exact parts (residual 1e-23);
// n times each part is exact for |n| < 2^29, far above the core phase k*a*T.
void sin_cos(REAL x, out REAL s, out REAL c) {
    REAL p1 = REAL(1.5707963705062866), p2 = REAL(-4.371138828673793e-08),
         p3 = REAL(-1.7151245100058819e-15);
    REAL n = floor(x / (p1 + p2 + p3) + REAL(0.5));
    REAL r = ((x - n * p1) - n * p2) - n * p3, z = r * r;
    REAL sp = REAL(1) - z / REAL(272);
    sp = REAL(1) - z / REAL(210) * sp;
    sp = REAL(1) - z / REAL(156) * sp;
    sp = REAL(1) - z / REAL(110) * sp;
    sp = REAL(1) - z / REAL(72) * sp;
    sp = REAL(1) - z / REAL(42) * sp;
    sp = REAL(1) - z / REAL(20) * sp;
    sp = r * (REAL(1) - z / REAL(6) * sp);
    REAL cp = REAL(1) - z / REAL(306);
    cp = REAL(1) - z / REAL(240) * cp;
    cp = REAL(1) - z / REAL(182) * cp;
    cp = REAL(1) - z / REAL(132) * cp;
    cp = REAL(1) - z / REAL(90) * cp;
    cp = REAL(1) - z / REAL(56) * cp;
    cp = REAL(1) - z / REAL(30) * cp;
    cp = REAL(1) - z / REAL(12) * cp;
    cp = REAL(1) - z / REAL(2) * cp;
    int quadrant = int(n) & 3; // two's complement: -1 & 3 == 3
    s = quadrant == 0 ? sp : quadrant == 1 ? cp : quadrant == 2 ? -sp : -cp;
    c = quadrant == 0 ? cp : quadrant == 1 ? -sp : quadrant == 2 ? -cp : sp;
}

bool core_finite(REAL x) { return !(isnan(x) || isinf(x)); }

// d(state)/ds in the Cartesian core, production convention (dx/ds = -dH/dp,
// dp/ds = +dH/dx), from H = (-P_T^2/F + P.P - k W^2)/2, F = 1 - k R^2,
// W = X.P. Fails, like evaluate_core, outside the exact toy core ellipsoid
// (r <= r_in) or where F <= 64 epsilon_double.
bool core_rhs(StereoModel m, REAL y[8], out REAL d[8], out REAL H) {
    for (int i = 0; i < 8; ++i) d[i] = REAL(0);
    H = REAL(0);
    const REAL k = m.Lambda / REAL(3), a2 = m.a * m.a, xi = REAL(1) + k * a2;
    const REAL X = y[1], Y = y[2], Z = y[3];
    const REAL F = REAL(1) - k * (X * X + Y * Y + Z * Z);
    const REAL inner2 = m.r_in * m.r_in;
    const REAL ellipsoid = xi * (X * X + Y * Y) / (inner2 + a2) + Z * Z / inner2;
    if (!(F > REAL(64) * REAL(2.220446049250313e-16)) || !(ellipsoid <= REAL(1)))
        return false;
    const REAL PT = y[4], PX = y[5], PY = y[6], PZ = y[7];
    const REAL W = X * PX + Y * PY + Z * PZ;
    H = (-PT * PT / F + PX * PX + PY * PY + PZ * PZ - k * W * W) / REAL(2);
    const REAL c = k * PT * PT / (F * F);
    d[0] = PT / F;
    d[1] = -PX + k * X * W;
    d[2] = -PY + k * Y * W;
    d[3] = -PZ + k * Z * W;
    d[4] = REAL(0);
    d[5] = -c * X - k * W * PX;
    d[6] = -c * Y - k * W * PY;
    d[7] = -c * Z - k * W * PZ;
    for (int i = 0; i < 8; ++i)
        if (!core_finite(d[i]))
            return false;
    return core_finite(H);
}

// Core exit surface: the oblate ellipsoid of radius b, value and rate along
// the ray (src/optics/ray.cpp, surface axis -2).
REAL core_exit_value(StereoModel m, REAL b, REAL y[8]) {
    const REAL a2 = m.a * m.a, xi = REAL(1) + m.Lambda / REAL(3) * a2;
    return xi * (y[1] * y[1] + y[2] * y[2]) / (b * b + a2) + y[3] * y[3] / (b * b) - REAL(1);
}
REAL core_exit_rate(StereoModel m, REAL b, REAL y[8], REAL d[8]) {
    const REAL a2 = m.a * m.a, xi = REAL(1) + m.Lambda / REAL(3) * a2;
    return REAL(2) * (xi * (y[1] * d[1] + y[2] * d[2]) / (b * b + a2) + y[3] * d[3] / (b * b));
}

// Position (T,X,Y,Z) and Jacobian d(T,X,Y,Z)/d(t,r,u,v) at an angular-patch
// state; twin of forward() in core_chart.cpp.
void core_forward(StereoModel m, REAL y[8], bool north, out REAL pos[4], out REAL J[4][4]) {
    const REAL k = m.Lambda / REAL(3), a = m.a, a2 = a * a, xi = REAL(1) + k * a2;
    const REAL r = y[1], x = y[2], v = y[3], q = x * x + v * v, den = REAL(1) + q;
    const REAL sign = north ? REAL(1) : REAL(-1);
    const REAL nx = REAL(2) * x / den, ny = REAL(2) * v / den, nz = sign * (REAL(1) - q) / den;
    const REAL A = r * r + a2, h = sqrt(A / xi);
    const REAL T = y[0] / xi;
    REAL si, co;
    sin_cos(k * a * T, si, co);
    pos[0] = T;
    pos[1] = h * (co * nx + si * ny);
    pos[2] = h * (-si * nx + co * ny);
    pos[3] = r * nz;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            J[i][j] = REAL(0);
    const REAL omega = k * a / xi, d2 = den * den;
    J[0][0] = REAL(1) / xi;
    J[1][0] = omega * pos[2];
    J[2][0] = -omega * pos[1];
    J[1][1] = r * pos[1] / A;
    J[2][1] = r * pos[2] / A;
    J[3][1] = nz;
    const REAL dnx0 = REAL(2) / den - REAL(4) * x * x / d2, dnx1 = -REAL(4) * x * v / d2;
    const REAL dny0 = -REAL(4) * x * v / d2, dny1 = REAL(2) / den - REAL(4) * v * v / d2;
    const REAL dnz0 = -REAL(4) * sign * x / d2, dnz1 = -REAL(4) * sign * v / d2;
    J[1][2] = h * (co * dnx0 + si * dny0);
    J[2][2] = h * (-si * dnx0 + co * dny0);
    J[3][2] = r * dnz0;
    J[1][3] = h * (co * dnx1 + si * dny1);
    J[2][3] = h * (-si * dnx1 + co * dny1);
    J[3][3] = r * dnz1;
}

// Gauss-Jordan inverse with partial pivoting, as inverse() in core_chart.cpp.
bool core_invert(REAL a[4][4], out REAL b[4][4]) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            b[i][j] = i == j ? REAL(1) : REAL(0);
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (abs(a[row][col]) > abs(a[pivot][col]))
                pivot = row;
        if (!core_finite(a[pivot][col]) || a[pivot][col] == REAL(0))
            return false;
        for (int j = 0; j < 4; ++j) {
            const REAL ta = a[pivot][j], tb = b[pivot][j];
            a[pivot][j] = a[col][j];
            b[pivot][j] = b[col][j];
            a[col][j] = ta;
            b[col][j] = tb;
        }
        const REAL d = a[col][col];
        for (int j = 0; j < 4; ++j) {
            a[col][j] /= d;
            b[col][j] /= d;
        }
        for (int row = 0; row < 4; ++row)
            if (row != col) {
                const REAL f = a[row][col];
                for (int j = 0; j < 4; ++j) {
                    a[row][j] -= f * a[col][j];
                    b[row][j] -= f * b[col][j];
                }
            }
    }
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!core_finite(b[i][j]))
                return false;
    return true;
}

// Positive oblate radius of a Cartesian point, rationalized near the focal
// disk; negative when not representable (core_oblate_radius throws there).
REAL core_oblate_radius(StereoModel m, REAL y[8]) {
    const REAL a2 = m.a * m.a, xi = REAL(1) + m.Lambda / REAL(3) * a2;
    const REAL b = xi * (y[1] * y[1] + y[2] * y[2]) + y[3] * y[3] - a2;
    const REAL d = sqrt(b * b + REAL(4) * a2 * y[3] * y[3]);
    const REAL r2 = b >= REAL(0) ? (b + d) / REAL(2) : REAL(2) * a2 * y[3] * y[3] / (d - b);
    return core_finite(r2) && r2 >= REAL(0) ? sqrt(r2) : REAL(-1);
}

// Angular patch -> core, P = J^-T p (to_core). False where the CPU throws.
bool core_from_angular(StereoModel m, inout REAL y[8], bool north) {
    REAL d[8], H;
    if (!stereographic_rhs(m, y, d, H) || y[1] > m.r_in)
        return false; // angular_metric and the shell/exterior guard
    REAL pos[4], J[4][4], inv[4][4];
    core_forward(m, y, north, pos, J);
    if (!core_invert(J, inv))
        return false;
    REAL res[8];
    for (int a = 0; a < 4; ++a) {
        res[a] = pos[a];
        REAL P = REAL(0);
        for (int b = 0; b < 4; ++b)
            P += inv[b][a] * y[b + 4];
        res[a + 4] = P;
    }
    if (!core_rhs(m, res, d, H)) // core_metric at the new position; finite
        return false;
    for (int i = 0; i < 8; ++i)
        y[i] = res[i];
    return true;
}

// Core -> angular patch, p = J^T P at the recovered angular position
// (from_core). False where the CPU throws.
bool core_to_angular(StereoModel m, inout REAL y[8], bool north) {
    REAL d[8], H;
    if (!core_rhs(m, y, d, H))
        return false;
    const REAL r = core_oblate_radius(m, y);
    if (!(r > REAL(0)))
        return false; // focal disk: no regular inverse oblate coordinates
    const REAL k = m.Lambda / REAL(3), a2 = m.a * m.a, xi = REAL(1) + k * a2;
    const REAL h = sqrt((r * r + a2) / xi);
    REAL si, co;
    sin_cos(k * m.a * y[0], si, co);
    const REAL sign = north ? REAL(1) : REAL(-1);
    const REAL den = REAL(1) + sign * y[3] / r;
    REAL ang[8];
    ang[0] = xi * y[0];
    ang[1] = r;
    ang[2] = (co * y[1] - si * y[2]) / (h * den);
    ang[3] = (si * y[1] + co * y[2]) / (h * den);
    for (int i = 4; i < 8; ++i) ang[i] = REAL(0);
    for (int i = 0; i < 4; ++i)
        if (!core_finite(ang[i]))
            return false;
    if (!stereographic_rhs(m, ang, d, H) || ang[1] > m.r_in)
        return false; // angular_metric, and forward()'s guards
    REAL pos[4], J[4][4];
    core_forward(m, ang, north, pos, J);
    REAL probe[8];
    for (int i = 0; i < 4; ++i) {
        probe[i] = pos[i];
        probe[i + 4] = REAL(0);
    }
    if (!core_rhs(m, probe, d, H))
        return false; // forward() evaluates core_metric at its position too
    for (int a = 0; a < 4; ++a) {
        REAL p = REAL(0);
        for (int b = 0; b < 4; ++b)
            p += J[b][a] * y[b + 4];
        if (!core_finite(p))
            return false;
        ang[a + 4] = p;
    }
    for (int i = 0; i < 8; ++i)
        y[i] = ang[i];
    return true;
}

#endif
