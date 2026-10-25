# Handoff: P2 regular angular tracing

Updated 2026-10-25. Implementation is authorized. Active isolated worktree:
`/home/abrsdxd/Projects/GravastarLab-Cpp20`, branch
`codex/p2-reference-optics`. Preserve the original checkout and Gemini Argon's
concurrent work. Commit and push tested checkpoints roughly every three minutes.
This branch has not been merged to main.

All authored calculations, references, tools, tests and benchmarks remain C++20.
CMake only configures builds, dependencies and CTest. No dependency was added.
The optimization policy in `AGENTS.md` requires explainable algorithms,
controlled complexity, numerical verification and measured baselines.

## Current implementation

Central continuation now has a working opt-in solver path. The static core
geometry passes 15 checks over 72 points. Ten independent chart checks verify
metric pullback, arbitrary covector Hamiltonians, frame/frequency and Killing
invariance, poles, spin signs and domain restrictions. Fifty-four core optics
checks cover analytic flat/de Sitter lines through the center and focal disk,
transactional entry/exit and a toy ray that previously stopped at the radial
cutoff and now escapes. Five focused CTests (including prior optics suites)
pass. The CLI/presets, complete compiler/sanitizer matrix and image pilot still
remain. `CORE_CONTINUATION.md` makes the single-sheet extension choice explicit;
it is not claimed as the source paper's unspecified deep-interior topology.

The opt-in `trace_angular` integrates through both angular poles using the regular
geometry backend. It shares the original Dormand-Prince stepping, event refinement
and rejection machinery with the preserved BL tracer. Canonical state, covector,
vector and frame transformations live in `optics/chart.hpp`. Each output point
retains its chart. Accepted endpoints switch at configurable q>1 (default 1.5),
with q->1/q hysteresis and null checks on both sides. Physical axial momentum
-y*p_x+x*p_y is monitored rather than treating p_y as conserved.

The CLI selects this path with `integrator.coordinates: "angular"`; original
presets still use BL. New presets are `benchmarks/optics_angular_v1.json` and
`benchmarks/optics_angular_kerr_v1.json`. Reports label every point's chart and
the initial BL camera state. Disk hits convert at the regular equator to the
same physical ZAMO emission prescription. The camera itself still requires a
nonsingular BL position. Parallel transport of polarization is not implemented.

`ANGULAR_TRACING.md` documents transitions, independent references, results
interpretation and reproduction. `AXIS_CHART_DESIGN.md` retains the metric
derivation and historical geometry evidence. Angular continuation does not
cross r=0, radial horizons or the configured radial limits.

## Verification and measured results

GCC and Clang Release pass 18/18 (40.14 s and 43.27 s). These are test durations
with other validation work running, not performance measurements. Targeted
optimized ASan/UBSan passes 3/3 in 247.43 s: `angular_optics`, `optics_reference`,
and the full `optics_cli` suite. No diagnostics, environment overrides or
suppressions were used; Linux default leak detection remained enabled. This is
a targeted subset, not a complete 18-test sanitizer run.

There are 50 new angular optical checks and 58 CLI/image checks. Flat Cartesian
pole-crossing error is 5.88e-13. A multi-patch Schwarzschild polar photon orbit
agrees with its analytic solution within 5.81e-10 under switch thresholds 1.2,
1.5 and 1.9. Both Kerr spin signs cross the north pole and agree with independent
separated-equation RK4 references within 1.78e-13; reference step refinement is
8.00e-15 and regular Carter drift is 2.97e-15. Shell joins, disk timing, capture,
invalid switches and existing audit regressions pass. Coarse paired images retain
all previously resolved BL classifications and intensities under integration and
patch-threshold refinement.

The native 48-ray performance experiment measures median BL/angular integration
ratios 28.65 (GCC) and 35.85 (Clang), with maximum scaled endpoint difference
6.27e-10. It includes conversions, stepping, error control and events; camera,
emission, image I/O and trajectory storage are excluded. The two compilers ran
sequentially without concurrent project builds/tests. No affinity/frequency
control or universal performance claim is made. Reports retain individual
timings, checksums, RHS/step counts, dataset and executable/source hashes.

Both new four-level spatial pilots ran through 128x96 without numerical failures:

| Finest grid / common mask | Toy | Kerr |
| --- | ---: | ---: |
| Emitted | 5453 | 4665 |
| Escaped | 6599 | 612 |
| Capture proxy | 0 | 7011 |
| Radial-domain stops | 235 | 0 |
| Affine-budget stops | 1 | 0 |
| Common resolved base pixels | 183/192 | 192/192 |
| Final relative intensity L1 | 0.02639910359 | 0.01846384698 |
| Final mean classification TV | 0.01180013021 | 0.006022135417 |
| Common-mask pilot criteria | pass | pass |
| Full-image refinement criteria | fail | pass |

Kerr's full-image refinement flag applies only to this fixed-scene four-level
pilot; full P2 acceptance remains false. The toy's remaining central/affine
stops are scientific failures, not transparent or renormalized pixels.
Compared with historical BL pilots, toy domain stops fall from 424 to 235 and
Kerr from 108 to zero. The earlier audit changed null monitoring too; historical
reports retain their original digests, rather than being relabeled as current.

Current computational digest:
`6efaffd9fdafd50e88fabc9bc9bede4c2c87aef7f2b00b69d465cfc7acd89489`.
Current reports use `p2_angular_optics_validation`, `p2_angular_image_validation`,
`p2_angular_ray_benchmark_{gcc,clang}`, `p2_angular_{toy,kerr}_sampling` and
`p2_angular_tracing_*` prefixes. Raw sampling rays are in ignored unique
`results/local/sampling/` directories; tracked summaries preserve their hashes
and all aggregate pixels. The completed matrix is
`results/p2_angular_tracing_verification.json`; the current inventory is
`results/p2_angular_tracing_source_manifest.json`. All required work for this
angular-tracing milestone is complete; central/GYOTO work remains separate.

## Preserved foundations

- P1 is frozen at `609debe`. Its one-sided C0-C3 shell and outward K_AB tests,
  independent 90/110-decimal references and arbitrary-integer exact Delta
  certificates remain intact. `results/rotating_v1.json`,
  `results/source_manifest.json` and `README.txt` are unchanged from that baseline.
- The earlier audit reproduced and fixed report/PPM path collision (`9740933`),
  refined-event null ceiling/diagnostic bypass (`d16fdf7`) and empty Bernstein
  input segfault (`2e213e5`). See `AUDIT_2026-10-24.md`; its 17-test reports use
  `p2_audit_*` and digest `d0e73493552790f529738e9dee5029b82937ee53d950f14ed4eae85b69909f76`.
- The regular geometry milestone had GCC/Clang 17/17 and targeted Debug and
  ASan/UBSan 3/3. First derivatives and Hessians have independent 90/110-place
  checks, including poles and shell joins. Its `p2_angular_*` geometry reports
  use digest `c52e5a6026bb3cf1bba12763b082aa93c5828475944c47000a625f1fbbd36de9`.
  Do not confuse those geometry/kernel reports with the new tracing evidence.
- Sampling preserves unresolved intensity/alpha as null and uses a common mask
  at all levels. Preselected policy in `benchmarks/sampling_v1.json` is unchanged.

## Next work

1. Derive and independently verify a regular central de Sitter continuation.
   The remaining radial stops are not solved by angular patches.
2. Add real pinned GYOTO integration/executable comparisons; improve grazing and
   near-critical event coverage, then revisit complete advertised ray-domain
   acceptance. GPU, desktop and stability claims remain gated.

## Reproduction

```text
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release --output-on-failure
build/headless-release/gravastar_angular_optics_tests --report results/local/reproduced_angular_optics.json
build/headless-release/gravastar_optics_cli_tests build/headless-release/gravastar_trace --report results/local/reproduced_angular_images.json
build/headless-release/gravastar_ray_benchmark --output results/local/reproduced_ray_benchmark.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_angular_v1.json --output results/local/reproduced_angular_toy_sampling.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_angular_kerr_v1.json --output results/local/reproduced_angular_kerr_sampling.json
build/headless-release/gravastar_tools manifest --verify results/p2_angular_tracing_source_manifest.json
```

SSH pushes work with
`-c core.sshCommand='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=10'`.
