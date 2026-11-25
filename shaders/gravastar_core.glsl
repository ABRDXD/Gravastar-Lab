// SPDX-License-Identifier: GPL-3.0-or-later
// Static Cartesian core chart (T,X,Y,Z) of the exact de Sitter core: GPU twin
// of core_rhs, evaluate_core's domain and the core exit surface in
// src/optics/core_chart.cpp, src/geometry/core.cpp and src/optics/ray.cpp.
// Derivation and domain rules: docs/CORE_CONTINUATION.md. Transitions into
// and out of this chart are made on the host with the CPU tracer's own
// to_core/from_core (src/rendering/GpuTracer.cpp). Compile with
// -DGRAVASTAR_FP64 for double precision, otherwise float (REAL comes from
// the stereographic file).
#ifndef GRAVASTAR_CORE_GLSL
#define GRAVASTAR_CORE_GLSL

#include "gravastar_stereographic.glsl"

bool core_finite(REAL x) { return !(isnan(x) || isinf(x)); }

// core_valid: evaluate_core's domain, the exact toy core ellipsoid (r <= r_in)
// and F > 64 epsilon_double.
bool core_valid(StereoModel m, REAL X, REAL Y, REAL Z) {
    REAL k = m.Lambda / REAL(3), a2 = m.a * m.a, xi = REAL(1) + k * a2;
    REAL F = REAL(1) - k * (X * X + Y * Y + Z * Z), inner2 = m.r_in * m.r_in;
    REAL ellipsoid = xi * (X * X + Y * Y) / (inner2 + a2) + Z * Z / inner2;
    return F > REAL(64) * REAL(2.220446049250313e-16) && ellipsoid <= REAL(1);
}

// d(state)/ds in the Cartesian core, production convention (dx/ds = -dH/dp,
// dp/ds = +dH/dx), from H = (-P_T^2/F + P.P - k W^2)/2, F = 1 - k R^2,
// W = X.P. Fails, like evaluate_core, outside the exact toy core ellipsoid
// (r <= r_in) or where F <= 64 epsilon_double.
bool core_rhs(StereoModel m, REAL y[8], out REAL d[8], out REAL H) {
    REAL k = m.Lambda / REAL(3);
    REAL X = y[1], Y = y[2], Z = y[3];
    REAL F = REAL(1) - k * (X * X + Y * Y + Z * Z);
    REAL PT = y[4], PX = y[5], PY = y[6], PZ = y[7];
    REAL W = X * PX + Y * PY + Z * PZ;
    H = (-PT * PT / F + PX * PX + PY * PY + PZ * PZ - k * W * W) / REAL(2);
    REAL c = k * PT * PT / (F * F);
    d[0] = PT / F;
    d[1] = -PX + k * X * W;
    d[2] = -PY + k * Y * W;
    d[3] = -PZ + k * Z * W;
    d[4] = REAL(0);
    d[5] = -c * X - k * W * PX;
    d[6] = -c * Y - k * W * PY;
    d[7] = -c * Z - k * W * PZ;
    // One exit and no return inside the loop, so the loop stays unrollable.
    bool ok = core_valid(m, X, Y, Z) && core_finite(H);
    for (int i = 0; i < 8; ++i)
        ok = ok && core_finite(d[i]);
    return ok;
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

#endif
