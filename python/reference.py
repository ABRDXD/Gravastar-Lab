# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent expanded metric, Decimal differences, and lower-index curvature.

No C++ imports, generated expressions or nonstandard packages. Decimal trigonometry
uses convergent Taylor series on the benchmark's [0, pi] angle interval.
"""
from decimal import Decimal as D, getcontext
from itertools import product

N = range(4)
ZERO = D(0)


def trig(x, cosine=False):
    term = D(1) if cosine else x
    total = term
    k = 1
    while True:
        j = 2*k if cosine else 2*k+1
        term *= -x*x / (j*(j-1))
        previous = total
        total += term
        if total == previous:
            return total
        k += 1
        if k > 200:
            raise ArithmeticError('trigonometric series did not converge')


def matrix():
    return [[ZERO for _ in N] for _ in N]


def metric(parameters, r, theta, region='auto'):
    M, a, L, ri, ro = map(D, parameters)
    r, theta = D(r), D(theta)
    if region == 'auto':
        region = 'core' if r <= ri else 'exterior' if r >= ro else 'shell'
    if region == 'core':
        m = ZERO
    elif region == 'exterior':
        m, L = M, ZERO
    else:
        # Paper's x in [-1,1], independently evaluated; deliberately not C++'s u form.
        x = (2*r-ri-ro)/(ro-ri)
        mu = D(1)/2 + (35*x-35*x**3+21*x**5-5*x**7)/32
        m, L = M*mu, L*(1-mu)
    s, c = trig(theta), trig(theta, True)
    rho = r*r+a*a*c*c
    delta = r*r-2*m*r+a*a-L*r*r*(r*r+a*a)/3
    angular = 1+L*a*a*c*c/3
    xi = 1+L*a*a/3
    g = matrix()
    # Expanded rather than the production rank-one/block expression.
    g[0][0] = (-r*r+2*m*r-a*a*c*c + L*(r**4+a*a*r*r+a**4*c*c*s*s)/3)/(rho*xi*xi)
    g[0][3] = g[3][0] = -a*s*s*(2*m*r+L*(r*r+a*a)*rho/3)/(rho*xi*xi)
    g[3][3] = s*s*((r*r+a*a)*rho+2*m*r*a*a*s*s+L*a*a*(r*r+a*a)*rho/3)/(rho*xi*xi)
    g[1][1], g[2][2] = rho/delta, rho/angular
    return g


def inverse(g):
    # Pivoted general Gauss-Jordan inversion, independent of the C++ block inverse.
    aug = [row[:] + [D(i == j) for j in N] for i, row in enumerate(g)]
    for k in N:
        pivot = max(range(k, 4), key=lambda i: abs(aug[i][k]))
        aug[k], aug[pivot] = aug[pivot], aug[k]
        scale = aug[k][k]
        if scale == 0:
            raise ArithmeticError('singular reference metric')
        aug[k] = [v/scale for v in aug[k]]
        for i in N:
            if i != k:
                q = aug[i][k]
                aug[i] = [a-q*b for a, b in zip(aug[i], aug[k])]
    return [row[4:] for row in aug]


def derivatives(parameters, r, theta, h, region='auto'):
    """Fourth-order centered first/second and tensor-product mixed differences."""
    r, theta, h = D(r), D(theta), D(h)
    values = {}
    def at(i, j):
        if (i, j) not in values:
            values[i, j] = metric(parameters, r+i*h, theta+j*h, region)
        return values[i, j]
    g = at(0, 0)
    first = [matrix() for _ in N]
    second = [[matrix() for _ in N] for _ in N]
    w1 = {-2: D(1)/12, -1: -D(2)/3, 1: D(2)/3, 2: -D(1)/12}
    w2 = {-2: -D(1)/12, -1: D(4)/3, 0: -D(5)/2, 1: D(4)/3, 2: -D(1)/12}
    for a, b in product(N, repeat=2):
        for k in (1, 2):
            first[k][a][b] = sum(w*at(i if k == 1 else 0, i if k == 2 else 0)[a][b]
                                  for i, w in w1.items())/h
            second[k][k][a][b] = sum(w*at(i if k == 1 else 0, i if k == 2 else 0)[a][b]
                                      for i, w in w2.items())/(h*h)
        mixed = sum(w*v*at(i, j)[a][b] for i, w in w1.items() for j, v in w1.items())/(h*h)
        second[1][2][a][b] = second[2][1][a][b] = mixed
    return g, first, second


def curvature(g, dg, ddg):
    """R_abcd from metric Hessians and first-kind Christoffel products.

    This avoids the production implementation's differentiated inverse/connection.
    """
    inv = inverse(g)
    lower_gamma = [[[sum((dg[b][a][c], dg[c][a][b], -dg[a][b][c]))/2
                     for c in N] for b in N] for a in N]
    upper_gamma = [[[sum(inv[a][e]*lower_gamma[e][b][c] for e in N)
                     for c in N] for b in N] for a in N]
    R = [[matrix() for _ in N] for _ in N]
    for a, b, c, d in product(N, repeat=4):
        v = (ddg[b][c][a][d]+ddg[a][d][b][c]-ddg[b][d][a][c]-ddg[a][c][b][d])/2
        v += sum(lower_gamma[e][b][c]*upper_gamma[e][a][d]
                 -lower_gamma[e][b][d]*upper_gamma[e][a][c] for e in N)
        R[a][b][c][d] = v
    ricci = [[sum(inv[a][c]*R[a][b][c][d] for a, c in product(N, repeat=2)) for d in N] for b in N]
    scalar = sum(inv[b][d]*ricci[b][d] for b, d in product(N, repeat=2))
    raised = R
    for axis in N:
        next_tensor = [[matrix() for _ in N] for _ in N]
        for indices in product(N, repeat=4):
            a, b, c, d = indices
            value = ZERO
            for e in N:
                source = list(indices)
                source[axis] = e
                i, j, k, l = source
                value += inv[indices[axis]][e]*raised[i][j][k][l]
            next_tensor[a][b][c][d] = value
        raised = next_tensor
    K = sum(R[a][b][c][d]*raised[a][b][c][d] for a, b, c, d in product(N, repeat=4))
    return {'ricci': ricci, 'ricci_scalar': scalar, 'kretschmann': K, 'riemann_lower': R}
