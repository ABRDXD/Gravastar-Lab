# P2 native reference optics contract

The user authorized starting P2 on 2026-10-21 while the independent P1 audit
remains open. P1 is frozen at `609debe`; this work uses a separate branch.
This first milestone advertises only the existing positive-radius, off-axis
chart. Full central and axial continuation and GYOTO agreement remain gates
for complete P2 acceptance. No new language or numerical dependency is used.

## State, time orientation and observer convention

Use coordinates `(t,r,theta,phi)` and **future-directed** covariant photon
momentum `p_mu`. The evolution parameter `s` increases into the past:

```text
H = 1/2 g^{mu nu} p_mu p_nu
v^mu = g^{mu nu} p_nu
dx^mu/ds = -v^mu
dp_mu/ds = -1/2 (partial_mu g_ab) v^a v^b
```

There is no null projection. The normalized null residual is
`abs(g^{ab} p_a p_b) / sum_ab abs(g^{ab} p_a p_b)`.
Stationarity and axial symmetry give exactly zero RHS for `p_t,p_phi`;
their numerical conservation is structural and cannot alone validate a ray.
No Carter constant is assumed in the transition shell.

A circular observer has `u=ut(1,0,0,Omega)`, normalized to `g(u,u)=-1`.
The default is ZAMO, `Omega=-g_tphi/g_phiphi`. The spatial frame consists of
normalized positive-r and positive-theta coordinate vectors and
`e_phi=(u_phi,0,0,-u_t)/sqrt(g_tphi^2-g_tt*g_phiphi)`, where the u components
in this expression are covariant. Frame construction rejects non-timelike
observers. For a unit outward-looking sight vector n in this frame,
`p^mu=u^mu-n^i e_i^mu`, giving observed frequency one and a past ray along n.

## Integration and events

Use the Dormand–Prince embedded 5(4) pair, with the fifth-order solution and
the maximum component norm `abs(error_i)/(atol+rtol*max(abs(y_i),abs(next_i)))`.
Unwrapped azimuth and coordinate time remain in this error norm. Step growth
uses safety 0.9 and exponent -1/5, bounded to [0.2,5]. Accepted-step counts,
rejections, RHS calls, affine extent and null residual are recorded.
Configurations fix absolute/relative tolerances, step limits and attempt budget.
The [Boost Odeint coefficient listing](https://github.com/boostorg/odeint/blob/boost-1.87.0/include/boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp)
provides an independently published tableau; no Boost implementation is copied
or linked.

Transverse sign-changing event brackets are refined by reintegrating from the
left endpoint and bisecting affine distance, rather than linearly interpolating
coordinates. Select the earliest terminal event. Record shell crossings with
refined states. Grazing contacts and multiple crossings hidden inside one step
are not certified; step/tolerance convergence is required for accepted examples.
The disk convention is a **transverse crossing** of an infinitesimally thin
surface; a ray lying in its plane is not silently treated as an intersection.
If a refinement substep fails the local error test, reject and shorten the
whole trial before recording any event. Full-step acceptance alone does not
bound every shorter step, especially near a transition.

Outcomes distinguish escape, disk emission, a reference Kerr capture surface,
chart-domain limit, affine-budget exhaustion, step-budget exhaustion, invalid
input and numerical failure. A budget or domain limit is never absorption.
Capture surfaces are allowed only outside the outer horizon of a subextremal
Kerr reference; they are proxies, not horizon-crossing solutions. The toy
gravastar has no artificial capture surface.

## Emission and image data

Use a phenomenological, opaque, equatorial annulus with prescribed ZAMO motion,
isotropic emitted bolometric intensity `(r_inner/r)^3`, and no volumetric
transfer or scattering. It is not a self-consistent orbital disk. With
`nu=-p_mu u^mu>0`, the bolometric transfer is `I_observer=(nu_observer/nu_emitter)^4 I_emitter`.
Backward coordinate time is the emission time of this stationary model.

The camera looks radially inward. Image-right is positive e_phi and image-up
is negative e_theta. Pixel-center pinhole rays use a specified vertical field
of view and aspect ratio. JSON stores linear intensity, redshift, classification,
null error and event coordinates. Unresolved pixels have null intensity;
escaped rays have transparent background. A diagnostic PPM may color outcomes
but does not replace the linear data or establish full interior rendering.

`gravastar_trace image --resolution-scale N` multiplies both image dimensions
by an integer from 1 to 8, subject to the 128x128 diagnostic limit. Camera,
field of view, aspect ratio, emission and integration settings stay fixed.
Each original pixel is thereby subdivided into an N-by-N midpoint grid. The
report records the effective dimensions and scale; the input hash describes
the original preset bytes. This is uniform image-plane area sampling, not
solid-angle integration or a measured flux at the observer.

`gravastar_sampling` aggregates these grids and compares their image-plane
averages and outcome fractions. Its missing-data policy, analytic tests,
provenance and frozen pilot criteria are specified in `SAMPLING.md`.

## Independent checks and remaining acceptance

Check tetrad orthonormality, launch nullness and frequency, exact flat Cartesian
lines, Schwarzschild radial null trajectories and redshift, exterior Kerr
Carter conservation, shell/core crossings, and tolerance refinement. Cross-check
the Hamiltonian RHS with the existing independent 90-place metric derivatives.
An independently stepped RK4 trajectory is an integration check and shares
production geometry; it is not a second metric validation.

GYOTO headers, library and executable are absent on this machine. Its
[documented C++ metric API](https://gyoto.obspm.fr/_gyoto_metric_8h_source.html)
requires a real pinned dependency build, ABI/license review and executable
comparisons. No placeholder adapter is counted as integration. Native checks
can proceed while this acceptance item remains explicitly incomplete.
