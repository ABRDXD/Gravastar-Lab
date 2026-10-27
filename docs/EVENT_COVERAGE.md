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
