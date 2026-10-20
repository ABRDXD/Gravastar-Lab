# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact rational polynomial root counting on bounded intervals.

Coefficients ascend in power. Sturm counts include repeated roots once;
interval endpoints must not themselves be roots. Used on shell coordinate u.
"""
from fractions import Fraction as F


def trim(p):
    p = list(p)
    while len(p) > 1 and p[-1] == 0:
        p.pop()
    return p


def add(a, b):
    return trim([(a[i] if i < len(a) else F(0))+(b[i] if i < len(b) else F(0))
                 for i in range(max(len(a), len(b)))])


def scale(p, k):
    return trim([v*k for v in p])


def multiply(a, b):
    out = [F(0)]*(len(a)+len(b)-1)
    for i, x in enumerate(a):
        for j, y in enumerate(b):
            out[i+j] += x*y
    return trim(out)


def derivative(p):
    return trim([i*p[i] for i in range(1, len(p))] or [F(0)])


def value(p, x):
    result = F(0)
    for c in reversed(p):
        result = result*x+c
    return result


def remainder(a, b):
    a = trim(a)
    while len(a) >= len(b) and any(a):
        k, f = len(a)-len(b), a[-1]/b[-1]
        for j in range(len(b)):
            a[j+k] -= f*b[j]
        a = trim(a)
    return a


def sturm(p):
    seq = [trim(p), derivative(p)]
    if not any(seq[-1]):
        return seq[:1]
    while True:
        rest = scale(remainder(seq[-2], seq[-1]), -1)
        if not any(rest):
            return seq
        # Positive normalization controls rational growth without changing signs.
        rest = scale(rest, 1/abs(rest[-1]))
        seq.append(rest)


def variations(seq, x):
    signs = [1 if v > 0 else -1 for p in seq if (v := value(p, x))]
    return sum(a != b for a, b in zip(signs, signs[1:]))


def count_roots(p, lo=F(0), hi=F(1)):
    if not value(p, lo) or not value(p, hi):
        raise ValueError("root at interval endpoint must be handled explicitly")
    s = sturm(p)
    return variations(s, lo)-variations(s, hi)


def isolate_roots(p, lo=F(0), hi=F(1), width=F(1, 10**12)):
    """All distinct interior roots, including even-multiplicity tangencies."""
    if not value(p, lo) or not value(p, hi):
        raise ValueError("endpoint root")
    seq = sturm(p)
    pending = [(lo, hi, variations(seq, lo)-variations(seq, hi))]
    out = []
    while pending:
        l, h, n = pending.pop()
        if not n:
            continue
        if n == 1 and h-l <= width:
            out.append((l, h))
            continue
        mid = (l+h)/2
        # Avoid evaluating Sturm variations on a root by choosing another rational.
        denom = 3
        while value(p, mid) == 0:
            mid = l+(h-l)/denom
            denom += 1
        left = variations(seq, l)-variations(seq, mid)
        pending.extend([(l, mid, left), (mid, h, n-left)])
    return sorted(out)


def shell_delta(params):
    p = {k: F(str(v)) for k, v in params.items()}
    r = [p["r_in"], p["r_out"]-p["r_in"]]
    r2 = multiply(r, r)
    b = add(r2, [p["a"]**2])
    mu = [F(v) for v in [0, 0, 0, 0, 35, -84, 70, -20]]
    lam = add([F(1)], scale(mu, -1))
    return add(multiply(b, add([F(1)], scale(multiply(r2, lam), -p["Lambda"]/3))),
               scale(multiply(r, mu), -2*p["M"]))


def bernstein_coefficients(p):
    """Power to Bernstein on [0,1]; convex-hull bounds certify positivity."""
    from math import comb
    n = len(p)-1
    return [sum((p[j]*F(comb(k, j), comb(n, j)) for j in range(k+1)), F(0))
            for k in range(n+1)]


def bernstein_positive_bound(p, max_depth=24):
    """Exact lower bound via de Casteljau subdivisions, or explicit failure."""
    pending = [(bernstein_coefficients(p), 0)]
    bounds = []
    while pending:
        b, depth = pending.pop()
        if min(b) > 0:
            bounds.append(min(b))
            continue
        if depth >= max_depth:
            return None
        rows = [b]
        while len(rows[-1]) > 1:
            rows.append([(a+c)/2 for a, c in zip(rows[-1], rows[-1][1:])])
        pending.extend([([row[0] for row in rows], depth+1),
                        ([row[-1] for row in reversed(rows)], depth+1)])
    return min(bounds)
