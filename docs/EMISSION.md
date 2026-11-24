# Emission models and image convergence (Phase C step 4, 2026-11-24)

All emission stays phenomenological and is labelled as such. Linear data is
stored per ray; display colour (PPM) is a separate diagnostic.

## Disk motion

`disk.motion` is `"ZAMO"` (the original choice) or `"keplerian_prograde"`.
The Keplerian option moves disk matter on the equatorial circular geodesic
rotating toward +phi, with angular speed the larger root of

```text
d_r g_tt + 2 W d_r g_tphi + W^2 d_r g_phiphi = 0
```

taken from the production metric derivatives. This is prescribed motion, not
a self-consistent disk. A scene is rejected unless every sampled radius of
its annulus (64 samples) has a timelike orbit whose angular momentum grows
outward (stable). The emissivity is unchanged: `(r_inner/r)^3`, bolometric,
`I_obs = g^4 I_emit`.

`tests/emission_tests.cpp` (67 checks) compares with the Kerr closed forms of
Bardeen, Press & Teukolsky (1972) for a in {-0.875, 0, 0.5, 0.875}:

| Check | Measured |
| --- | ---: |
| Omega = 1/(r^{3/2}+a), r in [3,6] | 4.8e-16 relative |
| E = -u_t, L = u_phi | 9.3e-16, 1.7e-15 relative |
| Toy exterior (r > 1.8) equals Kerr | exact |
| Disk-hit frequency, redshift and g^4 intensity, 9 camera rays | 1.0e-15 |
| ISCO bounds the supported annulus, both rotation senses | pass |
| Counter-rotating orbits inside the photon orbit (r < 3.888) refused | pass |

On the 16x12 toy scene the Keplerian disk spans g = 0.35 to 1.40 (Doppler
boost on the approaching side); the ZAMO disk spans 0.58 to 0.95.

## Background channel

An optional `background` block (`"model": "static_star_sphere_v1"`, `seed`,
`stars`, `width`, `range`) paints a sphere of static emitters (u ∝ ∂_t) at the
escape radius with a seeded, smooth field of Gaussian stars,
`I(n) = sum b_i exp(-(1 - n.s_i)/width^2)`. The star field is artistic content;
only the frequency shift is physics: `nu_e = -p_t / sqrt(-g_tt)`,
`I_obs = (nu_obs/nu_e)^4 I(n)`. The direction is the escape point's angular
position, not an asymptotic direction.

It is a separate channel. Escaped rays carry `background_intensity`,
`background_redshift` and `background_direction`; their `linear_intensity`
stays zero and alpha stays zero. Sampling averages the channel per pixel:
disk and capture samples hide it, and one escaped sample without it makes it
unknown.

`tests/background_tests.cpp` (9 checks): a one-star sphere integral against
2 pi w^2 b (1e-7 relative, set by the quadrature), the exact Schwarzschild
static-to-static shift, the Kerr shift and g^4 intensity against the analytic
g_tt (7.9e-16), and the same sample from BL and stereographic escape points
(2.2e-16). On the 16x12 scenes the background shift is g = 1.02 to 1.05: the
camera at r=8 sits deeper than the emitters at r=10.

Scenes: `optics_keplerian_v1`, `optics_keplerian_stars_v1` (toy, core
continuation, budget 80) and the Kerr twins `optics_keplerian_kerr_v1`,
`optics_keplerian_kerr_stars_v1` (capture proxy r=1.55).

## Convergence in integration tolerance

`gravastar_convergence` traces one scene at decreasing tolerances on a fixed
pixel set. `gravastar_trace --tolerance T` sets absolute and relative
tolerance to T and event tolerance to 10 T. Levels are compared with the
tightest one (1e-13). 64x48 rays per level:

| Scene | Tolerance | Rays failing their null check | Disk L1 | Disk max per pixel | Disk g max | Background L1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| toy stars | 1e-8 | 465 | — | — | — | — |
| toy stars | 1e-9 | 13 | — | — | — | — |
| toy stars | 1e-10 (scene default) | 0 | 5.3e-10 | 3.2e-7 | 1.0e-7 | 6.2e-7 |
| toy stars | 1e-11 | 0 | 5.1e-11 | 2.1e-8 | 1.1e-8 | 5.5e-8 |
| toy stars | 1e-12 | 0 | 4.9e-12 | 1.4e-9 | 9.3e-10 | 8.6e-9 |
| Kerr stars | 1e-8 | 0 | 1.5e-8 | 5.3e-7 | 6.1e-8 | 2.5e-7 |
| Kerr stars | 1e-10 (scene default) | 0 | 1.1e-10 | 5.6e-10 | 1.2e-10 | 4.0e-9 |
| Kerr stars | 1e-12 | 0 | 9.3e-13 | 3.4e-11 | 4.2e-12 | 7.2e-11 |

No passing level changes any ray's classification, and none has a near-
tangent ray. Errors fall about 10x per decade of tolerance. At 1e-9 and
looser, toy rays exceed the scene's null ceiling (1e-6) and the tracer
reports `numerical_failure` instead of data; with that ceiling the toy
scenes need a tolerance of 1e-10 or tighter. Reports:
`results/p2_tolerance_convergence_{toy,kerr}.json`.

## Convergence in samples per pixel

`gravastar_sampling` (16x12 base pixels, 1, 4, 16, 64 samples each):

| Scene | Refinement | Disk L1 | Class TV | Background L1 |
| --- | --- | ---: | ---: | ---: |
| toy stars | 1→2 | 0.201 | 0.079 | 1.63 |
| toy stars | 2→4 | 0.062 | 0.032 | 0.73 |
| toy stars | 4→8 | 0.024 | 0.010 | 0.29 |
| Kerr stars | 1→2 | 0.173 | 0.052 | 2.11 |
| Kerr stars | 2→4 | 0.050 | 0.020 | 1.22 |
| Kerr stars | 4→8 | 0.019 | 0.006 | 0.82 |

Both scenes resolve every sample and pass the pre-registered disk criteria
(`benchmarks/sampling_v1.json`). **The background channel is not converged**
at 64 samples per pixel: its 0.02-rad stars are about twice the finest sample
spacing (~0.009 rad), and lensing near the shadow sharpens them further. A
local probe with 0.1-rad stars converged like the disk (0.29, 0.095, 0.030);
that probe was not kept as a scene. Reports:
`results/p2_emission_sampling_{toy,kerr}.json`.

**Conclusion.** At the scene tolerances the image error is set by sampling,
not integration: tolerance changes are 1e-7 or smaller, sample changes are
1e-2 (disk) and of order 1 (background). Sampled pixel-area averages are not
a solid-angle flux or a global error bound.

## Not covered

No hotspots, volumetric transfer, opacity, returning radiation or time
dependence (PLAN.md lists these as separate extensions). No polarization.
The background uses the escape radius, not an asymptotic sky. The star field
has no physical spectrum. GYOTO comparison of emission is not done.
