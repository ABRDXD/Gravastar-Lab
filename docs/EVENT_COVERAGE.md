# Turning-point event coverage

Verified 2026-10-27. The previous detector searched only a trial's
endpoint surface values. A shallow excursion across a boundary and back can
leave both endpoints on the same side even when the integration error is small.

The new detector retains the first and last Dormand-Prince derivatives. For
each active surface it evaluates the derivative along the ray. Strictly
opposite endpoint rates bracket a stationary point; reintegration and bisection
locate that point within the configured affine event tolerance. The event search
is then split there, allowing both signed crossings to be found. Both intervals
are refined from the original trial state, with the existing local-error and
null checks. Events remain ordered by affine parameter and are published only
after the earliest terminal/transition candidate is valid.

This handles a resolved single turning excursion. It does not certify arbitrary
hidden extrema, all exact tangencies, or penetrations below integration and
event resolution. It introduces no absorbing surface or altered null dynamics.
Additional work at turning points consumes RHS calls; correctness takes priority
over a performance claim. The ordinary Dormand-Prince stages already provide the
endpoint derivatives.

The four focused existing core, angular, original-reference and core-CLI CTests
pass. New native tests pass 63 analytic event checks and 73 near-critical
Schwarzschild checks. Both suites are registered with CTest. The flat fixtures
check a shallow excursion, an adjacent miss, two nonterminal roots and clipping
of the discarded angular trial tail at Cartesian-core entry. The Schwarzschild
fixtures compare six impact parameters on both sides of 3*sqrt(3)*M, at relative
offsets 1e-3, 1e-5 and 1e-7, against independently stepped long-double equations.
GCC and Clang Release each pass 24/24 CTests. Targeted optimized ASan/UBSan
passes 6/6: both new suites, core optics, angular optics, the original optics
reference and core CLI. No diagnostics, suppressions or environment overrides
were used; Linux default leak detection remained enabled. These are sampled
correctness checks, not full P2 acceptance or a GYOTO comparison.

## Analytic regression and preserved baseline

The exact Cartesian line is `(X,Y,Z)=(-.005+s,.75-1e-8,0)` in Minkowski space,
with future-directed covector `P=(-1,-1,0,0)`. A step of length .01 begins and
ends outside the radial surface r=.75, while its middle lies inside. Its first
crossing occurs at `s=.005-sqrt(.75^2-(.75-1e-8)^2)`, approximately
0.004877525513. Both BL and stereographic patches must stop at this first
boundary. A ray with impact .75+1e-8 is a nearby miss and must continue.

Linking the new fixture against the preserved `ray.cpp` from `eae1dc9` reproduces
the missed boundary in all three charts, missing both nonterminal join roots
and the shallow core entry. That baseline probe exits with failure. The new
detector passes all 63 checks; the maximum first-hit timing error across the
three charts is 1.05e-11. A small-step run independently resolves the same root.
The zero-mass, zero-Lambda toy limit supplies exact Minkowski diagnostic shell
joins; both crossings must be retained in order. A core-enabled version verifies
that the discarded angular trial tail cannot publish a spurious second entry.

## Independent near-critical reference

For Schwarzschild M=E=1, equatorial null rays obey

```text
(dr/ds)^2 = 1 - b^2/r^2 + 2*b^2/r^3
r'' = b^2*(r-3)/r^4
t' = -1/(1-2/r),  (-phi)' = b/r^2
```

The reference integrates `(r,r',t,-phi)` with independent long-double RK4;
evolving radial velocity avoids choosing a square-root sign at periapsis.
It uses neither production geometry nor the production Dormand-Prince solver.
At b=3*sqrt(3), the radial potential has a repeated root at r=3. Six fixtures
use b=3*sqrt(3)*(1±epsilon) for epsilon=1e-3,1e-5,1e-7. They launch at r=10,
terminate at the exterior capture proxy r=2.2 or escape surface r=20, and have
affine budget150. The exact critical value is excluded from fate classification.

Production tolerances tighten from 2e-11 to 2e-13, maximum step from .1 to .05,
and event tolerance from 1e-10 to 1e-12. Independent RK4 steps halve from .002
to .001. Tests also check the independent cubic periapsis root, increasing
winding/dwell near r=3, null/Killing diagnostics and expected capture/scatter
classification. Dwell and periapsis minima are sampled trajectory diagnostics.

| Maximum observed difference | Value |
| --- | ---: |
| Production affine endpoint vs refined reference | 3.38e-8 |
| Production coordinate time vs refined reference | 1.02e-7 |
| Production unwrapped azimuth vs refined reference | 1.95e-8 |
| Reference affine change on halving its step | 4.67e-9 |
| Reference azimuth change on halving its step | 2.70e-9 |

The 73 native checks retain their declared ceilings and complete fixture data
in `results/p2_critical_validation.json`. These observations do not establish
all near-critical Kerr/toy rays, exact trapping, or asymptotic error bounds.

## Broader ray families (2026-11-24, Phase C step 1)

`tests/ray_family_tests.cpp` (CTest `ray_families`, 456 checks, ~26 s) traces
three toy-preset families with the production tracer and compares them with
references that share no production code. The report is
`results/p2_ray_family_validation.json` (computational digest
`4fdf403c19af8dfd9bd1a3f72d3f45123b9ee1eba15c06de0b48c12249c775c4`).

**Off-equatorial reference.** `tests/toy_ray_reference.hpp` integrates the
closed-form inverse of the MODEL.md metric in long double:

```text
2 Sigma H = D p_r^2 + T p_theta^2 - X^2 (B p_t + a p_phi)^2 / D
          + X^2 (a^2 sin^2 p_t^2 + 2 a p_t p_phi + p_phi^2/sin^2) / T
```

with complex-step derivatives and fixed-step RK4. Theta is signed, so
p_phi=0 rays cross either pole with no chart change. In the shell T and X
depend on r, so no Carter constant is assumed. The shell's profile has large
high derivatives; RK4 at h=1e-3 lost four digits there until steps within
0.05 of the shell were cut by 32. After that, halving h changes every compared
quantity by at most 4.2e-10. At 300 random points the reference Hamiltonian
and the production inverse metric agree to 7.5e-16 (relative).

**Pole and core in one ray.** Eight rays launched at r=10, p_t=-1, chosen by
a scan of (theta, p_theta, p_phi):

| Ray | theta, p_theta, p_phi | Reference r_min | Axis crossings at r |
| --- | --- | ---: | --- |
| pole inside Cartesian core patch | 1.2, 0.8, 0 | 0.555 | 0.672 |
| pole in core, angular chart | 0.8, 1.0, 0 | 0.618 | 1.112 |
| pole in core, no Cartesian patch | 1.0, 1.2, 0 | 0.866 | 1.115 |
| pole outside shell, then core | 0.5, 1.2, 0 | 0.733 | 1.911 |
| two poles, shell only | 0.3, 4.0, 0 | 1.668 | 5.973, 2.261 |
| rotating, core without Cartesian patch | 1.0, 1.0, 0.5 | 0.875 | none |
| prograde rotating, Cartesian core | 1.0, 0.6, 0.5 | 0.479 | none |
| retrograde rotating, Cartesian core | 1.0, 0.8, -0.3 | 0.466 | none |

Each runs through `trace_angular` with core continuation at four settings.
All give the reference's crossing sequence (shell joins, core entry, core
exit) and escape. Largest differences from the reference:

| Settings | Crossing affine | Escape affine, t, p_r, direction |
| --- | ---: | ---: |
| tol 1e-10, max step 0.1 | 8.0e-8 | 7.4e-7 |
| tol 1e-12, max step 0.05 | 8.7e-10 | 7.5e-9 |
| same, max step 0.05/8 | 2.0e-9 | 8.8e-9 |
| same, max step 0.05*8 | 4.0e-10 | 8.2e-9 |
| BL chart, no core patch (rotating rays) | combined: 1.1e-8 | |

The step-size runs are sampled evidence that no crossing hides inside a long
step for these rays. Error control still bounds the real step, so the 8x
larger maximum changes the step count by only ~15%.

**Grazes of the shell joins.** Equatorial rays at b = b_c (1 -+ eps),
eps = 1e-4 ... 1e-12, around the three graze values (outer join prograde
3.0180173587, inner join prograde 1.8759486035, inner join retrograde
-3.6442732109). The reference depth delta = r_join - r_t comes from the
equatorial potential; the time inside the join is checked against
chord = 2 sqrt(2 delta / |r''|), with r'' from the reference Hamiltonian.

- Join crossing counts are even in every case: the tracer never leaves a ray
  labelled inside a region it has left.
- Classification is required for |delta| >= 5e-11 (tol 1e-12) and 5e-9
  (tol 1e-10). Measured: the outer join is right down to delta = 5e-12 at
  both settings. At the inner join, fine settings are right to 7.6e-11 and
  wrong at 1.5e-12; coarse settings are right to 7.6e-9 and wrong at 1.5e-10,
  including one false entry.
- The limit is the radial position error accumulated before the turn, not
  the event search. The chord gives it directly: curvature * chord^2 / 8 is
  the depth the trace reached. Its error is 8e-12 (fine) and 1.2e-9 (coarse)
  at the inner join, and below 1e-14 at the outer join, which the ray reaches
  before crossing the shell.
- **Open:** a graze below that resolution is misclassified silently. The
  trace carries no flag saying the classification is uncertain. Phase C step
  2 (explicit outcomes) should add one.

**Buried Kerr photon orbit.** The Kerr prograde equatorial photon orbit of the
exterior parameters is at r = 1.6287816594, b = 2.9537119158: inside the shell.
A scan of the toy equatorial potential finds no local minimum for any
prograde b in [1.8, 3.1], so the toy model has no prograde photon orbit
there. At b = b_ph (1 -+ 1e-3) and (1 -+ 1e-6):

| Offset | Toy outcome, sweep | Toy turning r | Kerr outcome, sweep |
| --- | --- | ---: | --- |
| -1e-3 | escaped, 9.08 rad | 1.74805 | capture proxy (r=1.55), 13.5 rad |
| +1e-3 | escaped, 9.20 rad | 1.74998 | escaped, 17.5 rad |
| -1e-6 | escaped, 9.14 rad | 1.74900 | capture proxy, 29.7 rad |
| +1e-6 | escaped, 9.14 rad | 1.74900 | escaped, 33.7 rad |

Toy turning radii match the independent root to 7e-12 and Kerr's to 9e-12.
The toy azimuth sweep's difference quotient across b_ph is 19.93 at both
widths (change 4.5e-4 relative): smooth, with no critical winding.

**Not covered.** Off-equatorial grazes; grazes of the core patch surfaces;
the retrograde exterior photon orbit at b = -6.79 beyond the existing
±1e-4 check; rays launched inside the core; rays through the focal disk
against this reference (its BL chart ends at r=0; the flat and de Sitter
analytic disk checks remain the evidence there). Sampled fixtures are not
a certificate for arbitrary rays.

## Reports and remaining limits

`results/p2_event_validation.json` records analytic regression checks. The
current matrix and source inventory are `p2_events_verification.json` and
`p2_events_source_manifest.json`. The computational digest is
`ef17eca351bbc5cb3a181a7cbaa4f281482c6f987ec43390d04902805a415908`.
Historical core, angular and MIT inventories retain their original digests.

Refreshed four-grid toy budget-80 and Kerr pilots both resolve every ray and
pass the frozen full-image refinement criteria. At 128x96, toy counts remain
5453 emitted / 6835 escaped; Kerr remains 4665 emitted / 612 escaped / 7011
capture-proxy outcomes. Reports use `p2_events_{toy,kerr}_sampling.json`.
This is a fixed-scene regression result, not general domain acceptance.

No speedup is claimed. Turning-point isolation adds reintegrations and searches
all active surfaces; RHS counters include this work. Arbitrary hidden extrema,
exact tangencies and crossings below numerical resolution remain unresolved.
GYOTO requires the separate C++20/CMake feasibility work in
`GYOTO_FEASIBILITY.md`; no incompatible build workflow was introduced.
