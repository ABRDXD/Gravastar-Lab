# Candidate regular angular chart: derivation for review

This is a proposed next P2 implementation, not an accepted chart or a change to
current ray behavior. It derives a regular angular metric for the actual toy
family, retaining its radial transition functions. C++20 algebraic preflight
checks now pass against the canonical geometry and independent 90/110-place
expanded reference. A production derivative evaluator, curvature checks and an
executable chart-switching tracer are still required before using it in ray
results. The central r=0 extension is a separate decision.

## Coordinates and metric

For the north patch, let x=tan(theta/2) cos(phi), y=tan(theta/2) sin(phi).
For the south patch replace theta by pi-theta, retaining the same phi. Define
q=x*x+y*y, C=4/(1+q)^2, s2=C*q and c2=(1-q)^2/(1+q)^2. Then use the existing
radial mu(r), lambda(r), and define

```text
k = Lambda * lambda / 3
m = M * mu
A = r*r + a*a
sigma = r*r + a*a*c2
X = 1 + k*a*a
T = 1 + k*a*a*c2
F = 1 - k*r*r
Delta = A*F - 2*m*r
E = sigma/T
B = F/(T*X) + 2*m*r/(sigma*X*X)
P = E + a*a*s2*B
v = (-y, x)
```

In coordinate order (t,r,x,y), the nonzero components are

```text
g_tt = (T*a*a*s2 - Delta)/(sigma*X*X)
g_rr = sigma/Delta
g_ti = C*a*(Delta - T*A)/(sigma*X*X) * v_i
g_ij = C * (P*delta_ij - C*a*a*B*x_i*x_j), i,j in {x,y}
```

The cancellation needed to make the axis regular is analytic:

```text
g_theta_theta - g_phi_phi/sin(theta)^2
  = -a*a*sin(theta)^2 * [F/(T*X) + 2*m*r/(sigma*X*X)]
```

It follows by expanding sigma=A-a*a*sin(theta)^2 and
T=X-k*a*a*sin(theta)^2. Do not evaluate a numerical difference of the two
angular components and divide by q near the axis. The displayed expression
has no division by q and remains finite at x=y=0 for the accepted radial domain.
Its determinant is -sigma^2*C^2/X^4. At the pole the angular block is C*E
times the identity and g_ti=0. Both angular patches have the same formula,
because this stationary model is symmetric under theta -> pi-theta.

## Coordinate and momentum transitions

On the patch overlap, x'=x/q and y'=y/q. The angular Jacobian is
J=(I-2*u*u^T/q)/q, where u=(x,y). Covariant momenta transform as
p'=J^(-T)*p = q*p - 2*u*(u dot p). The t and r coordinates and momenta
are unchanged. Transform contravariant frame vectors with J. Never just copy
the angular momentum components between patches.

## Inverse without an axial cancellation

The angular block can also be written H*I+K*v*v^T, with H=C*E,
K=C*C*a*a*B and D=C*a*(Delta-T*A)/(sigma*X*X). Let alpha=g_tt. The
two-dimensional time/azimuth determinant simplifies analytically to

```text
Z = alpha*(H+K*q) - D*D*q = -C*Delta*T/X^4
g^tt = (H+K*q)/Z
g^ti = -D*v_i/Z
g^ij = delta_ij/H + (D*D-alpha*K)*v_i*v_j/(H*Z)
g^rr = Delta/sigma
```

Use the simplified Z instead of subtracting large terms near an ergosurface.
These formulas contain no division by q and require only a rank-one angular
update. The inverse identity and agreement with independent pivoted elimination
are covered by `gravastar_axis_chart_tests`. Their runtime advantage must still
be benchmarked; no speedup or accepted axis-crossing ray is claimed.

The axial Killing momentum becomes p_phi=-y*p_x+x*p_y; unlike the original
chart's stored p_phi, it would no longer be conserved by an identically zero
RHS. Its measured drift would therefore provide an additional integration
check. Stationary p_t remains structurally conserved.

## Required validation before adoption

1. Compare off-axis components and first derivatives against the existing
   canonical geometry under the full coordinate Jacobian, including near both
   shell joins and both patches. Use an independent high-precision derivative
   calculation, not only two forms of the same automatic differentiation code.
2. Check the analytic pole limit, determinant, Lorentzian signature and
   continuity as q tends to zero along multiple azimuths.
3. Check coordinate/momentum/frame round trips, canonical one-form invariance,
   Hamiltonian invariance and Killing momentum invariance on patch overlap.
4. Trace flat Cartesian lines through each pole and equator, then Schwarzschild
   and Kerr reference rays; quantify tolerance and patch-threshold convergence.
5. Repeat paired images, including the unresolved axis rays discovered by the
   spatial pilot. Preserve any remaining r=0/domain limits explicitly.

Implement scalar algebra and derivatives in C++20, share the radial transition
construction, and avoid duplicating an independently drifting geometry model.
Benchmark against the original chart where both are valid. A coordinate
extension is a scientific correctness change, not merely an optimization.

## Measured preflight

`results/p2_axis_chart_preflight.json` records 14 aggregate checks, 480 double
sample points and 120 points at each of 90/110 decimal places. Double metric
agreement with the transformed canonical evaluator is 1.86e-14 and inverse
identity residual is 1.14e-13. Independent high-precision metric-transform
residuals are 6.52e-74 and 7.56e-94, respectively. Momentum/one-form/Hamiltonian
patch checks are below 7.42e-16. The radial profile helper is shared with the
previously tested reference and is not independently revalidated by this test.

These are observed algebraic residuals on the sampled preset, not a complete
chart acceptance. The code currently lives only in a C++20 test executable.
It neither supplies production derivatives nor changes existing ray outcomes.
