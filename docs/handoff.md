# Handoff

## Current: branch `integration/reconcile` (2026-11-25)

The two lines of work are combined on `integration/reconcile`, built from
`codex/p2-reference-optics`, licensed GPL-3.0-or-later. Pushed to origin; `main` is
unchanged. Phases A (reconcile), B (docs match measured results) and C of
[ROADMAP.md](../ROADMAP.md) are done. Measured build and test results are in
[RECONCILE_STATUS.md](RECONCILE_STATUS.md).

Phase C (finish P2 reference optics) is done, 2026-11-24:

1. Ray families against an independent closed-form-Hamiltonian reference:
   pole and core in one ray, join grazes, the buried Kerr photon orbit
   (`ray_families`, docs/EVENT_COVERAGE.md).
2. Near-tangency flag; both reference scenes end every sampled ray in a
   named outcome with none censored or flagged.
3. Optional pinned GYOTO 2.0.2 build: Kerr rays agree to 7.8e-11, toy rays
   through an adapter to 2.7e-9 (docs/GYOTO_FEASIBILITY.md).
4. Keplerian disk motion, separate star background channel, tolerance and
   sample convergence (docs/EMISSION.md). The star background is not
   sample-converged at 64 samples per pixel.

Gate after step 4: GCC Release 31/31 (99 s), Clang desktop 33/33 (73 s),
ASan/UBSan desktop 35/35 with no diagnostics (470 s), GCC Debug 31/31
(634 s, -j2), optional GYOTO 1/1 (79 s). No compiler warnings in project
code; GYOTO's upstream `Metric.C:502` warns once.

Phase C was reviewed by the owner's ultrareview (PR #7); all nine findings
were fixed (ROADMAP.md, "Ultrareview of Phase C").

Phase D (GPU optics) is done for this stage, 2026-11-24: FP32/FP64 measured
(docs/GPU_PRECISION.md); an FP64 GPU tracer on the CPU contracts with CPU
fallback matches the CPU per pixel with 0 outcome mismatches on both
reference scenes (docs/GPU_TRACER.md). Gate: GCC Release 33/33, GCC
desktop 40/40, Clang desktop 38/38, ASan/UBSan 40/40 with no diagnostics,
GCC Debug 33/33; no compiler warnings.

Phase E (desktop studio) is done, 2026-11-24 (docs/STUDIO.md): ImGui/ImPlot
studio showing the real FP64 GPU render with progressive refinement, orbit
camera, scene/emission/science/inspection panels and a real-render smoke
test. Gate: GCC Release 33/33, GCC desktop 41/41, Clang desktop 39/39,
ASan/UBSan 41/41 with no diagnostics, GCC Debug 33/33.

Phase F (recording), 2026-11-25: steps 1–3 and the recorder for step 4 are
done (docs/RECORDING.md). That covers PNG frames with optional exact
radiance, the resumable job directory (hashes, lock, settings check,
crash-window reconciliation), the 600-frame fault-injection test, and
`gravastar_record` driving the GPU tracer along the camera track. Measuring
1080p frames found a GPU kernel bug: a hit whose root lay in the last
bisection interval of a step was dropped, so some rays flew through the
disk. It is fixed, and two loose-event-tolerance CTests fail on the old
kernel (docs/GPU_TRACER.md). FP32 at 1e-6 now takes 18–22 s per 1080p frame
(3.0–3.7 h for 600 frames), PSNR 57–58 dB against FP64 at scene tolerance.
Neither measured frame shows background inside the disk.

Gate at 0a83b47: GCC desktop 49/49 (129 s), Clang desktop 47/47 (91 s),
ASan/UBSan desktop 46/49 (1227 s). The three failures were the recording GPU
tests, on leak reports from libdbus and the GPU driver only. d5adb6c turns
leak detection off for them, as for the other GPU tests, and those three
then pass. The headless GCC Release and Debug suites were not rerun; no
headless code changed.

The FP32 acceptance run passed on 2026-11-25 (docs/RECORDING.md): 600
frames at 1080p in two parts, ffprobe-verified video, frames 0/137/299/599
byte-identical when re-rendered, 3.08 h. Still to do for P5: the
preview-independence demonstration.

GPU core patch (2026-11-25, `bce84dd`, `511453f`): core rays are traced on the
GPU by a second kernel build; transitions use the CPU's own to_core/from_core
on the host. Toy 128x96 vs CPU: 0 mismatches, core rays agree to 6.2e-10.
1080p frame 0 (`results/local/core/measure2_f0.json`, untracked): FP32
6.2 s per frame, 52 outcome changes against the reference, 85 pixels off by
more than 8 levels; FP64 at 1e-6 81.4 s; FP64 at scene tolerance 179.9 s.
Stopped there because the owner needed the laptop: frame 300, the tracked
reports, RECORDING.md and the Clang/sanitizer gates are still to do (ROADMAP.md).

Next (owner, 2026-11-25): finish the core patch checks above, then presentation colours,
then a Kerr-app-style studio layout that keeps the raw ImGui look, then the
FP64 showpiece run (`benchmarks/recording_acceptance_fp64_v1.json`, not yet
committed: it will gain the colour setting).
Push with the timeout-safe SSH command in the owner's memory notes; a plain
`git push` hung once on 2026-11-24.

The section below is the pre-merge P2 handoff, kept as history. Its worktree
path and push cadence no longer apply.

## History: P2 GYOTO CMake build investigation (2026-10-27)

Updated 2026-10-27. Implementation is authorized. Active isolated worktree:
`/home/abrsdxd/Projects/GravastarLab-Cpp20`, branch
`codex/p2-reference-optics`. Preserve the original checkout and Gemini Argon's
concurrent work. Commit and push tested checkpoints roughly every three minutes.
This branch has not been merged to main.

All authored calculations, references, tools, tests and benchmarks remain C++20.
CMake only configures builds, dependencies and CTest. No dependency was added.
Optimizations require explainable algorithms, numerical verification and measured
baselines. This milestone makes no performance claim.

## Completed milestone

The next active gate is the optional real GYOTO reference build. Its pinned
2.0.2 source was inspected from an ignored bare Git store, with no upstream
scripts checked out or executed. Legacy integration avoids Boost odeint, but
the core photon code still requires Boost math; Eigen is also required. The
optional `GRAVASTAR_GYOTO_REFERENCE` CMake experiment is disabled by default.
It currently configures and compiles part of the core; Boost header integration
is the next build repair. No GYOTO build or comparison passes yet. See
`GYOTO_FEASIBILITY.md` for compatibility fixes and the source-selection approach.

Turning-point event isolation is verified; see `EVENT_COVERAGE.md`. Opposite
endpoint surface rates trigger a stationary-point search and split the crossing
search, recovering shallow crossing pairs that the preserved tracer missed.
There are 63 analytic event checks and 73 independent near-critical Schwarzschild
checks. GCC/Clang Release pass 24/24 each; targeted optimized ASan/UBSan passes
6/6 without diagnostics or overrides. New source digest:
`ef17eca351bbc5cb3a181a7cbaa4f281482c6f987ec43390d04902805a415908`.
The current matrix/inventory are `results/p2_events_verification.json` and
`results/p2_events_source_manifest.json`. Both refreshed toy budget-80 and Kerr
four-grid pilots pass with zero unresolved rays and unchanged finest-grid counts.
This does not certify arbitrary hidden extrema, exact tangencies or all ray
families. The GYOTO build/license assessment is in `GYOTO_FEASIBILITY.md`.

The owner's MIT choice supersedes the earlier GPL plan. The migration updates
the root license, original-code SPDX comments and documentation, preserving
third-party notices and historical numerical reports. See `LICENSING.md`.
The migration inventory is `results/mit_source_manifest.json`; byte-based source
hashes change because comments changed. Core reports below retain the source
digest of their original numerical validation, before the license migration.
The rebuilt MIT source passes all 22 GCC Release CTests; the transcript is
`results/mit_gcc_ctest.txt`. No executable code changed during this migration.

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

## Prior core verification and preserved evidence

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

Pre-migration core validation computational digest:
`58be5d8157e4bc8afe6070cbd850cd7f660d9a25be5aac83013cf52b29d5fdb7`.
Core reports use `results/p2_core_*`; their matrix and historical inventory are
`p2_core_verification.json` and `p2_core_source_manifest.json`.
Raw sampling rays stay in ignored unique `results/local/sampling/` directories;
tracked reports preserve their hashes and aggregate pixels. Documentation-only
changes do not alter the computational digest.

## Preserved foundations

- P1 is frozen at `609debe`. Its one-sided C0-C3 shell and outward K_AB tests,
  independent 90/110-decimal references and arbitrary-integer exact Delta
  certificates remain intact. `results/rotating_v1.json`,
  `results/source_manifest.json` are unchanged from that baseline. `README.txt`
  differs only in its licensing sentence, intentionally updated to MIT.
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
2. Extend beyond the verified single-turning-excursion and six Schwarzschild
   fixtures. Endpoint-rate reversal cannot certify arbitrary hidden extrema,
   exact tangencies or all near-critical Kerr/toy rays. Keep unresolved cases
   explicit and require refinement evidence for accepted examples.
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
build/headless-release/gravastar_event_optics_tests --report results/local/reproduced_events.json
build/headless-release/gravastar_critical_optics_tests --report results/local/reproduced_critical.json
build/headless-release/gravastar_tools manifest --verify results/p2_events_source_manifest.json
```

The verification matrix records the exact Clang and sanitizer CTest commands.
SSH pushes work with
`-c core.sshCommand='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=10'`.
