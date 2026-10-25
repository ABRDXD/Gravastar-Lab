# Handoff: P2 central continuation verified

Updated 2026-10-25. Implementation is authorized. Active isolated worktree:
`/home/abrsdxd/Projects/GravastarLab-Cpp20`, branch
`codex/p2-reference-optics`. Preserve the original checkout and Gemini Argon's
concurrent work. Commit and push tested checkpoints roughly every three minutes.
This branch has not been merged to main.

All authored calculations, references, tools, tests and benchmarks remain C++20.
CMake only configures builds, dependencies and CTest. No dependency was added.
Optimizations require explainable algorithms, numerical verification and measured
baselines. This milestone makes no performance claim.

## Completed milestone

The opt-in regular angular tracer now continues through the exact core in
static Cartesian coordinates. Geometry is regular at the origin and oblate
focal disk. Entry/exit transformations handle positions, covariant momenta,
vectors and observer frames. Physical global energy and axial momentum use
their proper chart-dependent expressions; raw Cartesian P_T is not BL p_t.

`CORE_CONTINUATION.md` derives the map, metric, inverse, invariants, canonical
transformations, domain and analytic rays. The smooth single-sheet filling is
an explicit project choice because the paper leaves the deep interior topology
unspecified. It is not claimed as a unique extension of zero-mass Kerr.

Enable `integrator.coordinates="angular"` and `continue_core=true`. The preset
`benchmarks/optics_core_v1.json` uses entry radius .75, exit 1.0 and affine
budget 40. Both transitions lie strictly inside the exact core; the disk lies
outside the Cartesian patch. The original inner cutoff is explicitly replaced.
Earliest transition events clip the trial before chart conversion; later hits
are discarded and both representations must satisfy the null ceiling. The
constant-curvature backend rejects trial stages outside the core. No bounce,
absorption or null projection is introduced.

The original bounded BL and angular modes remain available. Camera launch
still requires a nonsingular BL position; arbitrary frame transformations do
not imply polarization parallel transport. Static/capture/escape limits retain
their existing meanings; r=10 escape is a finite-domain event.

## Current verification

GCC and Clang Release pass **22/22** CTests (48.18 s and 38.20 s). Optimized
ASan/UBSan passes **5/5** in 25.50 s: core geometry, core chart, core optics,
core CLI and the original optics reference. No diagnostics, overrides or
suppressions were used; default Linux leak detection remained enabled.
This is targeted sanitizer coverage, not the full 22-test suite. Durations
include concurrent validation and are not performance measurements.

New native reports contain:

- 15 geometry checks over 72 points, including the exact center/focal disk,
  metric/inverse, closed derivative/curvature identities and fourth-order
  finite-difference convergence.
- 10 chart checks, including independent metric pullback, arbitrary-covector
  Hamiltonians, canonical pairing, transformed frames/frequency, spin signs,
  poles and rejection of inverse focal-disk coordinates.
- 55 optical checks. The oblique de Sitter reference crosses both transitions
  and matches its independent Cartesian path/time within 7.44e-14. A toy ray
  that previously reached the inner cutoff now escapes and remains stable
  under tighter integration and changed overlap radii.
- 28 CLI checks. Paired 16x12 images preserve previously resolved classifications
  and intensities, resolve four formerly stopped pixels, verify chart tags and
  reject unsupported settings. A selected 128x96 ray verifies the affine-budget
  interpretation and longer-run convergence.

| Finest toy grid: 128x96 | Budget 40 | Budget 80 |
| --- | ---: | ---: |
| Emitted | 5453 | 5453 |
| Escaped | 6834 | 6835 |
| Radial-domain stops | 0 | 0 |
| Affine-budget stops | 1 | 0 |
| Common resolved base pixels | 191/192 | 192/192 |
| Final relative intensity L1 | 0.02639910359 | 0.02670175532 |
| Final mean classification TV | 0.01041666667 | 0.01033528646 |
| Common-mask pilot criteria | pass | pass |
| Full-image refinement criteria | fail | pass |

Core continuation removes the 235 radial stops in the preceding angular pilot.
The remaining budget-40 pixel (42,0) is outward-moving at r=9.84038 when it is
censored. `benchmarks/optics_core_long_v1.json` is a separate follow-up chosen
after that observation; only the affine budget changes to 80. It escapes at
s=40.1973976. Tightening tolerances/step/event limits and changing patch
thresholds changes its escape affine value by 6.14e-7 and scaled state by
4.37e-7. All four grids resolve completely in the budget-80 pilot. The
budget-40 evidence and frozen sampling policy remain intact. The small change
in aggregate refinement measures reflects different common masks.

These results establish this fixed-scene pilot and tested continuation domain,
not full P2 acceptance or global accuracy over all ray families/parameters.

Current computational digest:
`58be5d8157e4bc8afe6070cbd850cd7f660d9a25be5aac83013cf52b29d5fdb7`.
Reports use `results/p2_core_*`; the final matrix and inventory are
`p2_core_verification.json` and `p2_core_source_manifest.json`.
Raw sampling rays stay in ignored unique `results/local/sampling/` directories;
tracked reports preserve their hashes and aggregate pixels. Documentation-only
changes do not alter the computational digest.

## Preserved foundations

- P1 is frozen at `609debe`. Its one-sided C0-C3 shell and outward K_AB tests,
  independent 90/110-decimal references and arbitrary-integer exact Delta
  certificates remain intact. `results/rotating_v1.json`,
  `results/source_manifest.json` and `README.txt` are unchanged from that baseline.
- The 2026-10-24 audit fixed report/PPM path collision (`9740933`), refined-event
  null ceiling/diagnostic bypass (`d16fdf7`) and empty Bernstein input segfault
  (`2e213e5`). See `AUDIT_2026-10-24.md` and historical `p2_audit_*` reports.
- The preceding angular milestone is recorded in `ANGULAR_TRACING.md` and
  `p2_angular_tracing_verification.json`: GCC/Clang 18/18, targeted sanitizer
  3/3, independent flat/Schwarzschild/Kerr pole checks and fixed-scene pilots.
  Its measured 48-ray BL/angular timings are historical evidence, not a new
  performance claim for this core extension. Earlier reports keep their hashes.
- Sampling preserves unresolved intensity/alpha as null and uses a common mask
  at every level. `benchmarks/sampling_v1.json` remains unchanged.

## Next work and review targets

1. Audit the explicit topology choice, time/azimuth normalization, global energy
   transformation, canonical transitions and analytic oblique reference.
2. Strengthen grazing/near-critical event coverage. Endpoint sign-change
   bracketing does not certify multiple hidden crossings or tangencies within
   one trial step; accepted examples require refinement evidence.
3. Establish an actual pinned GYOTO dependency/build and executable comparison
   compatible with the C++20-only constraint. A placeholder adapter is not an
   accepted comparison. Revisit complete advertised-domain acceptance after
   those checks. GPU, desktop, recording and stability remain later stages.

## Reproduction

```text
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release --output-on-failure
build/headless-release/gravastar_core_geometry_tests --report results/local/reproduced_core_geometry.json
build/headless-release/gravastar_core_chart_tests --report results/local/reproduced_core_chart.json
build/headless-release/gravastar_core_optics_tests --report results/local/reproduced_core_optics.json
build/headless-release/gravastar_core_cli_tests build/headless-release/gravastar_trace --report results/local/reproduced_core_images.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_core_v1.json --output results/local/reproduced_core_sampling.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_core_long_v1.json --output results/local/reproduced_core_long_sampling.json
build/headless-release/gravastar_tools manifest --verify results/p2_core_source_manifest.json
```

The verification matrix records the exact Clang and sanitizer CTest commands.
SSH pushes work with
`-c core.sshCommand='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=10'`.
