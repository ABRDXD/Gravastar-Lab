# Handoff: P2 sampling and angular-chart preflight

Updated 2026-10-23. The user resumed implementation after `fb348c4`; its stop
instruction is superseded. Active isolated worktree:
`/home/abrsdxd/Projects/GravastarLab-Cpp20`, branch
`codex/p2-reference-optics`. Keep the original checkout and Argon's concurrent
work untouched. Commit and push checkpoints roughly every three minutes.
This branch has not been merged to main.

All authored code and tools remain C++20; CMake is only for build/dependency/
CTest configuration. No dependency was added. The optimization philosophy is
recorded in `AGENTS.md`: creative algorithms, controlled complexity, measured
baselines, verified numerical accuracy and documented trade-offs.

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

## Angular-chart preflight and next implementation

`AXIS_CHART_DESIGN.md` derives north/south stereographic angular patches for the
actual metric, including a finite pole limit, analytic inverse, covariant
momentum transformations and axial Killing momentum. `tests/axis_chart_tests.cpp`
is an executable preflight, not a production geometry backend. Its 14 aggregate
checks cover 480 double points and 120 points at each of 90 and 110 decimal
places, plus pole limits and patch transitions. Candidate/reference calculations
share the previously tested radial profile helper; coordinate expressions and
inverse algorithms are independently compared. No speedup is claimed.

1. Implement a canonical angular metric/first-derivative API with shared radial
   profiles. Independently verify derivatives and curvature near poles and joins.
2. Implement position, momentum and frame chart transitions in the tracer.
   Validate pole-crossing flat/Schwarzschild/Kerr rays, patch-threshold refinement
   and Killing momentum drift before rerendering the paired scenes.
3. Derive a separate regular central continuation of the de Sitter core. The
   angular chart alone does not cover r=0 or decide the interior extension.
4. Add real pinned GYOTO integration and executable comparisons, then revisit
   whole-image convergence and difficult grazing/near-critical events.

Production rays still stop at the original chart boundaries. Preserve the saved
scientific failures. GPU/desktop and stability claims remain gated. A possible
later optimization is avoiding curvature on every ray RHS evaluation while
retaining one canonical geometry and benchmarking against the original evaluator;
it has not been implemented.

## Reproduction

```text
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release --output-on-failure
build/headless-release/gravastar_sampling --scene benchmarks/optics_v1.json --output results/p2_toy_sampling.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_kerr_v1.json --output results/p2_kerr_sampling.json
build/headless-release/gravastar_axis_chart_tests --report results/p2_axis_chart_preflight.json
build/headless-release/gravastar_tools manifest --verify results/p2_continuation_source_manifest.json
```

SSH pushes work with the direct command option
`-c core.sshCommand='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=10'`.
