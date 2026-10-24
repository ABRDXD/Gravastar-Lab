# Regular angular ray integration

The native C++20 tracer now offers opt-in north/south stereographic patches.
Use `benchmarks/optics_angular_v1.json` or `optics_angular_kerr_v1.json` with the
existing trace/sampling executables. Original presets retain the full
Boyer-Lindquist (BL) baseline. Angular continuation covers either pole but does
not extend r=0, radial horizons, or the specified radial tracing boundaries.
The camera is still specified and launched in nonsingular BL coordinates.

## Shared integrator and transitions

`trace_angular` shares Dormand-Prince 5(4), scaled error control, event bisection,
substep retries, shell joins and outcome handling with `trace`. Its RHS uses
`evaluate_angular`, which computes only metric, inverse and first derivatives.
All three spatial momentum derivatives are integrated. Stationary p_t remains
structurally constant, while axial momentum -y*p_x+x*p_y is measured explicitly.

Every `Point`, including refined crossings, carries a `Chart`. JSON points add
`chart`; each ray's `initial_state` remains the BL camera launch and is explicitly
tagged with `initial_chart`. Consumers must use these tags before comparing or
interpreting state components. North/south states use (t,r,x,y,p_t,p_r,p_x,p_y).

At an accepted endpoint with q=x*x+y*y >= `angular_switch_q`, the tracer uses
the overlap transformation derived in `AXIS_CHART_DESIGN.md`. The default
threshold is 1.5, constrained to 1<q<=2. Switching maps q to 1/q, providing
hysteresis. Trial stages must remain within the geometry's q<=4 bound; stages
outside it reject and halve the trial. Switching is an exact coordinate change
at an accepted endpoint, not a physical event requiring bisection. The null
ceiling is checked before and after the transition. Maxima include both forms
and published events. The normalized null residual is coordinate-dependent;
its numerator is the invariant Hamiltonian contraction.

`transform_state`, `transform_vector` and `transform_frame` apply covariant and
contravariant Jacobians, preserving canonical pairing and tetrad products.
They reject singular BL/opposite-pole conversions. Frame conversion is available
and tested; parallel transport of polarization or camera frames is not added.

Disk events use q=1. Their roots and annulus checks use the same refinement path
as BL events. Disk hits convert to BL at the regular equator for the existing
ZAMO frequency/emissivity calculation. The original polar-margin stops are
omitted in angular mode. The configuration retains that legacy field, while
radial, capture and affine limits still apply. Endpoint-bracketed event finding
does not certify multiple hidden or grazing crossings within one trial.

## Independent ray checks

The 50-check `angular_optics` test includes all four metric families, both
patches, state and frame transformations, flat Cartesian lines through either
pole and exact-pole launches. A Schwarzschild polar photon orbit at r=3 is
compared with its analytic Cartesian position and time over affine length 24,
crossing multiple patches with thresholds 1.2, 1.5 and 1.9.

Rotating Kerr pole crossings use both spin signs and independent separated
geodesic equations. For E=-p_t and L_z=0, define

```text
A=r^2+a^2, Delta=A-2Mr, Sigma=r^2+a^2 cos(theta)^2
P=EA, R=P^2-Delta*(a^2 E^2+Q), Theta=Q+a^2 E^2 cos(theta)^2
dr/ds=-sqrt(R)/Sigma, dtheta/ds=-sqrt(Theta)/Sigma
dt/ds=-(A*P/Delta-a^2*E*sin(theta)^2)/Sigma
dphi/ds=-(a*P/Delta-a*E)/Sigma
```

The fixture stays on the inward radial branch, with decreasing signed theta
through the north pole. It requires R and Theta positive throughout. A separate
fixed-step RK4 routine integrates these scalar equations at 500 and 1000 steps;
it calls neither production geometry nor the production DP tableau. The
reference's signed theta is mapped directly to Cartesian coordinates, avoiding
a singular BL conversion at the pole. The regular Carter invariant is
`(1+q)^2*(p_x^2+p_y^2)/4 - L_z^2 - a^2*E^2*((1-q)/(1+q))^2`.
These are bounded numerical checks, not a global integrability certificate.

Shell traversal, capture, disk timing, null diagnostics and invalid switch
settings have regression coverage. The CLI adds paired toy/Kerr 16x12 images,
tighter step/event tolerances and a threshold change from 1.5 to 1.2. Previously
resolved BL classifications and intensities must agree; radial failures remain
explicit. Whole-image acceptance still requires the spatial criteria and the
remaining independent reference work.

## Performance comparison

`gravastar_ray_benchmark --output results/FILE.json` compares 48 fixed launches
in flat, Kerr and toy geometries over affine length 1.5. It validates paired
endpoints before timing, warms both implementations, alternates their order over
seven rounds, and consumes every final state component. Angular conversion,
integration, adaptive error control and shell event handling are included;
camera launch, emission, file I/O and trajectory storage are excluded.

Both paths use absolute/relative tolerance 1e-11 and retain the same physical
limits. Coordinate norms can select different adaptive steps, so reports include
RHS/accept/reject counts, endpoint agreement, null and Killing drift, all timings,
checksums, configurations, dataset/source/executable hashes and CPU/compiler.
The ratio combines omitted curvature work and changed coordinate arithmetic;
it does not isolate the rank-one inverse, predict all-image timing, or measure
an equivalent BL continuation at its singular poles. No timing acceptance
threshold, CPU affinity or frequency control is used.

## Remaining boundaries

The central de Sitter continuation, actual pinned GYOTO executable comparison,
grazing/near-critical event coverage, and whole-image scientific acceptance are
still separate work. No stability, horizon continuation or GPU claim follows
from this milestone. Historical BL artifacts retain their original source hashes.
