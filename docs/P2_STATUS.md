# P2 checkpoint: bounded native reference optics

## Audit fixes on 2026-10-24

The audit reproduced and fixed a JSON/PPM output collision, null-constraint
underreporting and bypass at refined shell events, and an empty-polynomial
segfault in the Bernstein checker. See `AUDIT_2026-10-24.md` for exact cases and
regression evidence. These are correctness fixes; no new optics capability was
added. Historical maximum-null-residual fields excluded nonterminal refined
crossings and must not be treated as current acceptance evidence. The audit
reports use the separate `results/p2_audit_*` prefix. Complete P2 remains open.
The corrected suites pass GCC/Clang Release 17/17 each and targeted ASan/UBSan
3/3. Native reports pass 51 optical and 38 CLI/image checks. The coarse 16x12
classifications remain unchanged; the 128x96 pilots were not rerun in this audit.

## Regular angular geometry on 2026-10-23

The geometry layer now evaluates the north/south stereographic metric at the
exact poles, with a first-derivative API for future Hamiltonian integration and
a separate Hessian/curvature API. Both charts share the canonical radial
profiles; their curvature uses the same contraction implementation. The old P1
API is preserved, and its frozen reports have not been overwritten. Production
rays still use the original BL chart, so the unresolved sampling outcomes below
remain valid and complete P2 acceptance is still false.

`angular_geometry` passes nine aggregate checks over 576 coordinate comparisons,
27 exact-pole analytic limits, six shell endpoint comparisons and input failures.
`angular_reference` adds eleven checks over 112 points at each of 90/110 decimal
places, including exact poles, angular coordinates of order 1e-8, both joins,
core, shell and exterior. Its expanded BL expressions, finite differences and
covariant curvature construction are independent of production jets/contraction.

| Observed scaled residual | Maximum |
| --- | ---: |
| Metric vs independent expanded reference | 1.01e-14 |
| First derivatives vs high-precision differences | 2.05e-14 |
| Hessian vs high-precision differences | 5.24e-14 |
| Independent shell Ricci tensor, including pole | 4.18e-14 |
| Independent shell Kretschmann, including pole | 1.84e-14 |
| Exact-pole flat/Kerr/de Sitter invariant identities | 2.00e-15 |
| 90-to-110-place derivative change | 2.88e-58 |
| Shell derivative change, h=1e-8 to 5e-9 | 6.93e-28 |

Halving the coarse reference step gives error ratios 0.06246 (first derivative)
and 0.06251 (Hessian), consistent with fourth-order finite differences. Nonzero
precision/step changes are required. An initial reference arithmetic failure
and its correction are recorded in `AXIS_CHART_DESIGN.md`; no tolerance was
loosened. These are sampled residuals, not rigorous global bounds.

The new first-order kernel avoids unnecessary Hessian and curvature work. On
this machine, median full-angular/first-order cost ratios are 21.3 with GCC and
36.0 with Clang. The original full BL evaluator costs 30.7/37.3 times as much as
the new first-order kernel, respectively, but its outputs and coordinates differ.
There is no end-to-end ray speedup claim. Each report retains seven timing
rounds, all 256 physical samples, checksums, CPU/compiler and source/executable
hashes. First/full angular shared outputs agree exactly on that dataset.

Reports: `results/p2_angular_geometry.json`, `p2_angular_reference.json`, and
`p2_angular_benchmark_{gcc,clang}.json`. The complete GCC Release suite passes
17/17 in 36.76 s; Clang Release passes 17/17 in 39.87 s. Test times include
overlapping verification work and are not benchmark timings. Debug passes the
two new angular tests and existing geometry unit test, 3/3 in 67.56 s. Optimized
ASan/UBSan passes that same subset 3/3 in 132.50 s with default leak detection,
no environment overrides/suppressions and no diagnostic errors. No complete
17-test Debug/sanitizer run is claimed. The four logs, transcribed matrix
`results/p2_angular_verification.json`, and inventory
`results/p2_angular_source_manifest.json` identify this checkpoint separately
from earlier sampling evidence.

## Spatial sampling continuation on 2026-10-23

The user resumed work after the saved stop checkpoint `fb348c4`. Native image
resolution scaling and the sampling experiment are implemented. The arithmetic
passes 64 analytic/manufactured checks, and the end-to-end suite covers positive
scaled renders, independent flat-camera formulas, aggregation, provenance and
failed-child preservation. GCC Release passes all 14 CTest targets in 45.33 s.
Clang Release also passes 14/14 (37.34 s); the new sampling subset passes
Debug 2/2 (47.31 s) and optimized ASan/UBSan 2/2 (23.34 s), with leak detection
enabled and no overrides. These overlapping test times are not performance
benchmarks. Both spatial pilots are complete.

Kerr's 128x96 pilot is complete: 108 unresolved chart-boundary rays, 184/192
original pixels in the common resolved mask, final relative intensity L1
0.01846384698 and mean classification total variation 0.00732421875. It passes
the common-mask pilot criteria and fails whole-image acceptance because of the
unresolved rays. `results/p2_kerr_sampling.json` retains the full aggregate
evidence. A proposed regular angular chart is derived in `AXIS_CHART_DESIGN.md`;
its algebra now has executable preflight checks, described below.

The toy 128x96 run has 5,407 emitted rays, 6,456 escaped, 424 chart-boundary
stops and one affine-budget stop. Its common mask covers 167/192 original
pixels (86.98%), below the frozen 90% criterion. Its last relative intensity
L1 is 0.02541480113 and mean classification total variation is 0.01806640625;
both differences decrease, but the coverage failure keeps the overall pilot
and whole-image acceptance false. `results/p2_toy_sampling.json` records these
results without filling missing intensities. No numerical-failure rays occurred
in either four-level pilot. The matrix is in `results/p2_sampling_verification.json`.

See `SAMPLING.md` for the fixed common-mask comparison, explicit missing data,
frozen pilot criteria and reproduction commands. The 32x24 pilot has already
exposed eight Kerr chart-boundary rays missed by the 16x12 grid; lack of holes
on one coarse grid does not establish complete chart coverage. Earlier reports
and inventories remain historical evidence for their respective source trees.

## Angular-chart algebra preflight

The proposed regular angular metric and inverse pass 14 aggregate checks over
480 double points and 120 points at each of 90 and 110 decimal places. The
samples include both patches, both shell joins and their nearby shell points.
The independent expanded metric transform agrees to 6.52e-74 at 90 places and
7.56e-94 at 110 places. Double metric agreement is 1.86e-14, inverse identity
1.14e-13, and momentum/one-form/Hamiltonian patch invariance below 7.42e-16.
Pole continuity is measured separately at finite angular radius 1e-9, not
mistaken for roundoff error. `results/p2_axis_chart_preflight.json` contains
thresholds, measured values and the exact scope.

The candidate and high-precision reference share the previously tested radial
profile helper. Coordinate expressions and inverse algorithms are compared
independently; this is not a new independent radial-profile validation. No
production angular derivative evaluator, curvature calculation, axis-crossing
ray or performance advantage was accepted at that historical checkpoint. The
geometry-layer continuation above supersedes those geometry limitations; the
production tracer remains unchanged.

The complete final GCC Release suite passes 15/15 in 32.65 s; Clang passes
15/15 in 31.01 s. The axis preflight also passes Debug (12.34 s test time)
and optimized ASan/UBSan with leak detection enabled (24.85 s). Final logs are
`results/p2_continuation_{gcc,clang}_ctest.txt` and
`results/p2_axis_{gcc,debug,sanitizers}_ctest.txt`; the transcribed matrix is
`results/p2_continuation_verification.json`. The final source inventory is
`results/p2_continuation_source_manifest.json`. This does not retroactively
change the computational digests of the earlier sampling runs.

## Verified milestone before this continuation

P2 is in progress on `codex/p2-reference-optics`, based on the frozen P1
commit `609debe`. This checkpoint implements a C++20 observer frame, adaptive
Hamiltonian null tracer, refined surface events, prescribed disk emission,
and a headless ray/image command. The contract is in `OPTICS.md`.

The GCC release and independent Clang release runs each passed all 12 CTest
targets. The suite includes **49 analytic/reference optics checks** and **31
CLI/image checks**, including deterministic runtime failure preservation.
GCC's complete run took 37.93 seconds; Clang's took 29.56 seconds with other
verification overlapping. These are test wall times, not rendering performance
claims. Optimized ASan/UBSan passed all three optics CTest targets in 244.62
seconds with leak detection enabled, no suppressions, and no sanitizer/runtime
error diagnostics. Debug passed the same three optics targets in 571.37 seconds.
The bounded native milestone is verified; complete P2 acceptance remains open.

A failed event-refinement substep now rejects and shortens the entire trial
step; a regression ray covers the pixel that exposed this issue. Invalid input
preserves the previous report. Numerical failure writes a separate failed JSON
artifact and preserves both the accepted JSON and its diagnostic PPM.

## Numerical evidence

All errors below are observed sample maxima, not rigorous global error bounds.
Image and trajectory comparisons use `abs(a-b)/(1+abs(b))`; flat line and
Schwarzschild time checks use absolute error. Null residual uses the normalized
Hamiltonian contraction defined in `OPTICS.md`.

| Reference check | Measured error |
| --- | ---: |
| Circular observer tetrad orthonormality | 6.67e-16 |
| Hamiltonian RHS vs independent 90-place metric derivatives | 4.40e-16 |
| Flat Cartesian straight trajectory | 2.98e-13 |
| Schwarzschild radial coordinate time | 6.38e-13 |
| Exterior Kerr Carter constant | 8.18e-14 |
| Toy shell/core traversal maximum null residual | 1.54e-10 |
| Dormand-Prince endpoint vs independently stepped RK4 | 3.65e-10 |
| RK4 endpoint change from step 0.002 to 0.001 | 9.23e-10 |

The toy reference ray crosses both shell joins inward and outward and escapes.
It turns within the valid positive-radius chart; this does not demonstrate
continuation through the excluded center or axes. The separate tolerance sweep
reduces endpoint error from 7.34e-6 to 1.44e-7 when tolerances tighten from
1e-6 to 1e-8, compared with a 1e-11 trajectory at the same affine endpoint.
That deliberately loose pilot uses a 1e-3 null ceiling; accepted image presets
use 1e-6. Conserved `p_t,p_phi` have exactly zero RHS, so their zero drift is a
structural check rather than independent evidence of accuracy.

## Paired image diagnostic

Both presets use M=1, a=0.875, a ZAMO camera at r=8 and theta=pi/3, vertical
field of view 50 degrees, and an equatorial ZAMO annulus from r=3 to r=6.
The toy's shell radii remain project-selected at [1.5,1.8]. The Kerr image
uses an exterior capture proxy at r=1.55, outside r_plus approximately 1.4841.
Each image has 16x12 pixel-center rays. These are coarse numerical diagnostics.

| Result | Toy gravastar | Kerr |
| --- | ---: | ---: |
| Emitted | 87 | 76 |
| Escaped | 101 | 8 |
| Capture proxy | 0 | 108 |
| Unresolved chart boundary | 4 | 0 |
| Classification changes under integration refinement | 0 | 0 |
| Maximum endpoint-state difference | 1.42e-7 | 2.87e-8 |
| Maximum linear-intensity difference | 3.36e-9 | 4.68e-11 |
| Maximum frequency-shift difference | 1.12e-8 | 6.22e-11 |
| Maximum accepted-state null residual | 1.11e-7 | 1.00e-10 |

The baseline uses atol=rtol=1e-10, maximum step 0.1 and event tolerance 1e-9.
The refined run uses 1e-11, 0.05 and 1e-10 respectively. Acceptance ceilings
are 2e-5 for state/intensity differences, 5e-6 for frequency-shift differences,
1e-6 for null residual, and zero classification changes. The four unresolved
toy pixels have null intensity/alpha in JSON and magenta diagnostic color;
they are not absorption. Escape means crossing the finite outer boundary
r=10, not a proof of reaching infinity. No spatial or sample convergence is
claimed from comparing identical pixel centers.

## Artifacts and reproduction

`results/p2_optics_validation.json` contains analytic/reference checks and
thresholds. `results/p2_image_validation.json` contains image comparisons,
complete baseline/refined configurations and failure-isolation checks.
`results/p2_toy_image.json` and `results/p2_kerr_image.json` contain every pixel's
launch, endpoint, events, classification, linear intensity, redshift and errors.
Their `.ppm` files are diagnostic displays with the legend in `OPTICS.md`.
`results/p2_selected_ray.json` stores the full trajectory for pixel (9,5),
the event-refinement regression case. Each trace artifact has a source manifest.

```text
cmake --preset headless-release
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release
build/headless-release/gravastar_optics_tests --report results/p2_optics_validation.json
build/headless-release/gravastar_optics_cli_tests build/headless-release/gravastar_trace --report results/p2_image_validation.json
build/headless-release/gravastar_trace image --output results/p2_toy_image.json
build/headless-release/gravastar_trace image --config benchmarks/optics_kerr_v1.json --output results/p2_kerr_image.json
build/headless-release/gravastar_trace ray --x 9 --y 5 --output results/p2_selected_ray.json
build/headless-release/gravastar_tools manifest --output results/p2_source_manifest.json
build/headless-release/gravastar_tools manifest --verify results/p2_source_manifest.json
```

Compile independently with `cmake --preset headless-release -B
build/clang-release -DCMAKE_CXX_COMPILER=clang++`, then build and run CTest in
that directory. The tested compilers are GCC 16.2.1 and Clang 23.1.3, both using
the installed libstdc++; this is independent compiler coverage, not an
independent hardware/platform reproduction. All authored implementation and
tools remain C++20. No package, language runtime, or numerical library was added.

The P1 reports and source manifest describe the frozen P1 tree. The P2 source
manifest identifies this milestone separately. Per-trace inventories record
the documentation present at execution time; documentation-only updates do
not change their computational digest.

CTest transcripts are retained as `results/p2_gcc_ctest.txt`,
`results/p2_clang_ctest.txt`, `results/p2_debug_ctest.txt` and
`results/p2_sanitizers_ctest.txt`. The transcribed test matrix is
`results/p2_verification.json`. The sanitizer
run uses the following direct build-tool invocations with no ASAN/LSAN/UBSAN
environment overrides:

```text
cmake --preset sanitizers -B build/sanitizers-optimized -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/sanitizers-optimized --parallel 2
ctest --test-dir build/sanitizers-optimized -R optics --output-on-failure
```

The earlier P1 sandbox's ptrace limitation did not recur in this unrestricted
session. This establishes leak-check execution for the P2 optics subset only;
the complete P1 sanitizer suite was not rerun with leak detection enabled.
The debug run uses `ctest --preset headless-debug -R optics --output-on-failure`.
Its image test took 557.47 seconds against a 600-second CTest timeout; a slower
host may need a larger test budget. No numerical tolerance was relaxed for
debug or instrumented builds.

## Remaining gates and review tasks

Complete P2 acceptance remains open: central and axial chart continuation,
real GYOTO integration/comparison, whole-image spatial acceptance, and broader
near-critical/grazing-ray checks are not established. The tracer does not
replace an unresolved boundary with artificial absorption or reflection.

Argon can independently reproduce frozen P1, then audit the Hamiltonian sign,
observer-frame orientation, emission convention, event ordering and refinement,
and unresolved-pixel encoding on this branch. A separate branch should contain
any proposed corrections. The next implementation needs angular-chart tracer
transitions for position, covariant momentum and observer frames, actual
pole-crossing rays and patch-threshold convergence, plus a separately derived
central continuation model, followed by a real pinned GYOTO build and comparisons compatible
with the C++20-only build constraint. No untested plugin stub counts as progress
through that gate. GPU/desktop and stability work remain later stages.

Preserve this branch separately from concurrent work on `main`; checkpoints
are reviewable progress, not a claim that the complete P2 gate has passed.
