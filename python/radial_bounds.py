# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact rational Bernstein bounds for Delta; no sampled root absence claim."""
from fractions import Fraction as F
from math import comb


def add(a, b):
    return [(a[i] if i < len(a) else F(0))+(b[i] if i < len(b) else F(0)) for i in range(max(len(a), len(b)))]


def scale(a, v):
    return [c*v for c in a]


def mul(a, b):
    out = [F(0)]*(len(a)+len(b)-1)
    for i, x in enumerate(a):
        for j, y in enumerate(b):
            out[i+j] += x*y
    return out


def certify(parameters):
    M, a, L, ri, ro = map(lambda x: F(str(x)), parameters)
    r = [ri, ro-ri]
    r2 = mul(r, r)
    A = add(r2, [a*a])
    mu = list(map(F, [0, 0, 0, 0, 35, -84, 70, -20]))
    lam = add([F(1)], scale(mu, -1))
    delta = add(mul(A, add([F(1)], scale(mul(lam, r2), -L/3))), scale(mul(mu, r), -2*M))
    n = len(delta)-1
    bernstein = [sum(delta[k]*F(comb(i, k), comb(n, k)) for k in range(i+1)) for i in range(n+1)]
    leaves = []
    def bound(coefficients, depth):
        if min(coefficients) > 0:
            leaves.append((min(coefficients), max(coefficients), depth))
            return
        if depth == 24:
            raise ArithmeticError('could not certify shell Delta positivity')
        rows = [coefficients]
        while len(rows[-1]) > 1:
            rows.append([(a+b)/2 for a, b in zip(rows[-1], rows[-1][1:])])
        bound([row[0] for row in rows], depth+1)
        bound([row[-1] for row in reversed(rows)], depth+1)
    bound(bernstein, 0)
    # Core: A>0 and 1-L*r²/3 >= 1-L*ri²/3 for positive L.
    core_factor = 1-max(L, F(0))*ri*ri/3
    # Exterior: Delta=(r-M)^2+a²-M², increasing for r>=ro if ro>M.
    exterior_min = ro*ro-2*M*ro+a*a
    if core_factor <= 0 or ro <= M or exterior_min <= 0:
        raise ArithmeticError('core or exterior bound did not certify positivity')
    return {
        'method': 'exact rational Bernstein subdivision, core factor bound, exterior monotonicity',
        'shell_delta_lower_bound': float(min(x[0] for x in leaves)),
        'shell_delta_lower_bound_rational': str(min(x[0] for x in leaves)),
        'shell_delta_upper_bound': float(max(x[1] for x in leaves)),
        'subintervals': len(leaves), 'maximum_depth': max(x[2] for x in leaves),
        'core_1_minus_Lr2_over_3_lower_bound': float(core_factor),
        'exterior_delta_minimum': float(exterior_min),
        'scope': 'fixed rational preset, all r>0; radial null surfaces only; not global event horizons'
    }
