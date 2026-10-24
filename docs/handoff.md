# Handoff: P2 regular angular geometry

Updated 2026-10-24. The user resumed implementation after `fb348c4`; its stop
instruction is superseded. Active isolated worktree:
`/home/abrsdxd/Projects/GravastarLab-Cpp20`, branch
`codex/p2-reference-optics`. Keep the original checkout and Argon's concurrent
work untouched. Commit and push checkpoints roughly every three minutes.
This branch has not been merged to main.

All authored code and tools remain C++20; CMake is only for build/dependency/
CTest configuration. No dependency was added. The optimization philosophy is
recorded in `AGENTS.md`: creative algorithms, controlled complexity, measured
baselines, verified numerical accuracy and documented trade-offs.

## Latest audit fixes

Implementation resumed after the completed audit. Production state/covector/
vector/frame transformations and a first-derivative angular Hamiltonian RHS
are in `optics/chart.hpp`. Opt-in `trace_angular` now shares the existing
Dormand-Prince stepping, event refinement and rejection machinery. Output points
carry chart identity; accepted endpoints switch patches at configurable q>1,
and diagnostics measure the physical axial Killing momentum. Flat lines cross
both poles (including exact-pole starts), and a Schwarzschild polar photon orbit
crosses multiple patches under three switch thresholds. Rotating overlap rays
agree with the original BL tracer. Angular and existing optical reference CTests
pass. The CLI remains on its existing BL path; event/image adoption and further
Kerr pole-crossing validation are the next checkpoint.

The user requested an audit before further feature work. The resumed audit found
and reproduced three faults: a `.ppm` output name overwrote its own JSON report;
refined nonterminal crossings bypassed null-ceiling enforcement and maximum-error
reporting; and empty Bernstein input segfaulted. Fixes are committed as `9740933`,
`d16fdf7` and `2e213e5`, with failing-before/passing-after regression checks.
`AUDIT_2026-10-24.md` records reproduction details, scope and remaining limitations.

Historical ray reports can understate maximum null residual because they excluded
refined nonterminal crossings. Preserve their original source digests. Current
audit evidence uses `results/p2_audit_*`; the full 128x96 pilots have not been
rerun. The exact geometry and kernel-performance reports below describe the
earlier geometry checkpoint, not this audit's source tree.

The corrected GCC/Clang Release suites each pass 17/17; native reports pass 51
optical checks and 38 CLI/image checks. Current 16x12 classifications match the
historical coarse images and agree under integration refinement. The audit
source digest is
`d0e73493552790f529738e9dee5029b82937ee53d950f14ed4eae85b69909f76`.
Use `results/p2_audit_source_manifest.json` for the current inventory.
The targeted ASan/UBSan suite also passes 3/3 in 264.40 s: optical reference,
full optics CLI/image test and reference arithmetic/polynomials, including all
new regressions. No sanitizer diagnostics, overrides or suppressions were used.
`results/p2_audit_verification.json` records the complete audit matrix. The audit
is complete for this checkpoint; the next implementation remains tracer chart
transitions, not new geometry formulas or a repeat of already-passing tests.

## Completed work

The sampling draft is now integrated and tested. `gravastar_sampling` launches
fixed-scene grids at scales 1,2,4,8, aggregates onto the original 16x12 pixels,
preserves unresolved intensity/alpha as null, and compares every level on one
common resolved mask. Classification statistics cover the whole image.
`SAMPLING.md` specifies formulas, provenance and preselected criteria.

There are 64 analytic/manufactured sampling checks and 31 CLI checks, including
positive scaled renders, independent flat-space camera formulas, raw-report
reconstruction/digests and preservation of accepted results on child failure.
The sampling checkpoint passed GCC/Clang Release 14/14 each, Debug sampling 2/2,
and optimized ASan/UBSan sampling 2/2 with leak detection enabled. The subsequent
axis preflight adds one CTest target; the final matrix is in `P2_STATUS.md`.

Both four-level spatial pilots completed without numerical-failure rays:

| Finest grid: 128x96 | Toy | Kerr |
| --- | ---: | ---: |
| Emitted | 5407 | 4665 |
| Escaped | 6456 | 612 |
| Capture proxy | 0 | 6903 |
| Chart-boundary stops | 424 | 108 |
| Affine-budget stops | 1 | 0 |
| Common resolved base pixels | 167/192 | 184/192 |
| Final relative intensity L1 | 0.02541480113 | 0.01846384698 |
| Final mean classification TV | 0.01806640625 | 0.00732421875 |
| Common-mask pilot criteria | fail: coverage < 90% | pass |
| Whole-image criteria | fail | fail |

The thresholds were committed before measurement in `benchmarks/sampling_v1.json`.
Summary JSON files retain every aggregate pixel, masks, settings and source/
executable hashes. Raw rays remain in unique ignored directories under
`results/local/sampling/`, with paths and hashes recorded in the summaries.
Results from before and after the axis-test addition have distinct computational
digests. Do not relabel historical reports as products of newer source trees.

## Regular angular geometry and next implementation

`AXIS_CHART_DESIGN.md` derives north/south stereographic angular patches for the
actual metric, including a finite pole limit, analytic inverse, covariant
momentum transformations and axial Killing momentum. `tests/axis_chart_tests.cpp`
remains an independent algebra preflight. Its 14 aggregate
checks cover 480 double points and 120 points at each of 90 and 110 decimal
places, plus pole limits and patch transitions. Candidate/reference calculations
share the previously tested radial profile helper; coordinate expressions and
inverse algorithms are independently compared. No speedup is claimed.

The production geometry backend is now implemented in `geometry/angular.hpp`
and `src/geometry/angular.cpp`. `AngularMetric` evaluates metric/inverse/first
derivatives; `AngularCurvature` adds Hessian and curvature without presenting
absent quantities as zero. The first-order scalar instantiation omits Hessian
work. Both charts share radial profiles and curvature contraction. Parameters,
r>0, q<=4 and the Delta singularity margin are validated; r=0 and radial chart
surfaces are still excluded. Existing rays do not call this API yet.

New tests cover 576 double coordinate comparisons, 27 analytic pole limits,
unclamped shell C0-C2 endpoint jets, and 112 independent finite-difference points
at each of 90/110 decimal places. Production first/Hessian errors are below
5.24e-14. Independent shell Ricci/Kretschmann checks include the exact pole.
Reference precision refinement is 2.88e-58 and coarse step refinement gives
the expected approximately 1/16 error ratio. No numerical threshold was loosened.
P1 still covers third radial derivatives and outward-normal extrinsic curvature.

The C++20 `gravastar_angular_benchmark` preserves full angular and original BL
evaluators as baselines. On the same host, GCC/Clang first/full angular kernel
ratios are 21.3/36.0 with matching shared outputs. These are microbenchmarks;
there is no end-to-end speedup yet. Reports include timings, dataset, hashes,
checksums and limitations. All original image/sampling artifacts remain intact.

The final GCC/Clang Release suites each pass 17/17. Debug passes the new angular
reference, angular geometry and existing geometry-unit subset 3/3 (67.56 s).
Optimized ASan/UBSan passes the same subset 3/3 (132.50 s), with leak detection
enabled by default, no environment overrides/suppressions, and no diagnostics.
This does not claim a complete 17-test Debug/sanitizer run. The transcribed matrix
is `results/p2_angular_verification.json`, with four CTest logs. The inventory is
`results/p2_angular_source_manifest.json`; all new scientific/performance reports
share computational digest
`c52e5a6026bb3cf1bba12763b082aa93c5828475944c47000a625f1fbbd36de9`.

1. Implement position, momentum and frame chart transitions in the tracer.
   Validate pole-crossing flat/Schwarzschild/Kerr rays, patch-threshold refinement
   and Killing momentum drift before rerendering the paired scenes.
2. Derive a separate regular central continuation of the de Sitter core. The
   angular chart alone does not cover r=0 or decide the interior extension.
3. Add real pinned GYOTO integration and executable comparisons, then revisit
   whole-image convergence and difficult grazing/near-critical events.

Production rays still stop at the original chart boundaries. Preserve the saved
scientific failures. GPU/desktop and stability claims remain gated. The new
requested-order geometry API is implemented and benchmarked; adopting it in
ray integration remains part of the next chart-transition work.

## Reproduction

```text
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release --output-on-failure
build/headless-release/gravastar_sampling --scene benchmarks/optics_v1.json --output results/p2_toy_sampling.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_kerr_v1.json --output results/p2_kerr_sampling.json
build/headless-release/gravastar_axis_chart_tests --report results/p2_axis_chart_preflight.json
build/headless-release/gravastar_angular_geometry_tests --report results/p2_angular_geometry.json
build/headless-release/gravastar_angular_reference_tests --report results/p2_angular_reference.json
build/headless-release/gravastar_angular_benchmark --output results/p2_angular_benchmark_gcc.json
build/headless-release/gravastar_optics_tests --report results/local/reproduced_optics_audit.json
build/headless-release/gravastar_tools manifest --verify results/p2_audit_source_manifest.json
```

SSH pushes work with the direct command option
`-c core.sshCommand='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=10'`.
