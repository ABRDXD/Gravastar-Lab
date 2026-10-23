# Spatial sampling experiment

`gravastar_sampling` is a native C++20 driver for `gravastar_trace`. It fixes
the physical scene, camera, field of view, aspect ratio and integrator while
refining the image-plane midpoint grid. The default levels are 1, 2, 4 and 8
samples per original pixel axis: 16x12, 32x24, 64x48 and 128x96 rays for the
paired optics presets. Each original pixel receives N squared equal-area
samples. This measures image-plane averages; it is not solid-angle flux.

## Missing data and comparison definitions

The per-pixel mean intensity and alpha are available only if every sub-sample
is resolved. Emitted, escaped and reference-capture rays are resolved; domain
and integration-budget stops are unresolved. Numerical failure aborts the
experiment and preserves its previous accepted report. Unknown values are never
replaced with zero, and resolved samples are never renormalized to hide holes.

Each pixel retains the fractions emitted, escaped, captured and unresolved.
`known_contribution` is the sum of known intensities divided by the full sample
count. It is explicitly a partial contribution when samples are unresolved,
not an estimate of the missing radiance.

All successive-level intensity comparisons use the same mask: original pixels
whose samples are resolved at every supplied level. This mask and its coverage
are saved. On that mask, report mean absolute error, maximum absolute error and
relative L1 difference, `sum(abs(coarse-fine))/sum(abs(fine))`. An empty mask
has unavailable absolute and relative errors. Zero reference radiance has an
unavailable relative error. These are differences between numerical estimates,
not errors relative to a known exact image.

Classification comparisons use every original pixel, including unresolved ones.
For each pixel, total variation is half the sum of absolute differences between
its four class fractions. Report the image mean and maximum, plus the image mean
absolute change in unresolved fraction. Therefore lost coverage remains visible
even when intensity comparisons exclude it.

## Frozen pilot criteria

`benchmarks/sampling_v1.json` was committed before the toy/Kerr experiment:

- Last successive-level relative intensity L1 <= 0.05 on the common mask.
- Last mean classification total variation <= 0.02 on the whole image.
- Common resolved coverage >= 0.90.
- At least three sampling levels, with nonincreasing last mean absolute
  intensity and classification differences.

The report distinguishes passing these pilot criteria on the common mask from
whole-image acceptance, which additionally requires every sample at every
level to be resolved. Neither establishes a rigorous error bound, asymptotic
convergence order for discontinuous images, full interior propagation, GYOTO
agreement or complete P2 acceptance. A completed measurement exits successfully
even if these scientific criteria fail; inspect the explicit report booleans.

## Validation and provenance

The arithmetic tests cover exact constant/linear/quadratic integrals on
rectangular images, the analytic midpoint error, discontinuous edge coverage,
shared unresolved masks, zero radiance and malformed inputs. CLI tests trace a
three-level flat-space scene, check an independently derived camera launch,
reconstruct aggregates directly from raw pixels, verify raw/input/source hashes,
and check that child failure preserves the prior report and manifest.

Each run keeps full raw ray reports in a unique `results/local/sampling/run-*`
directory. The compact summary records their paths and SHA256 digests, every
aggregate pixel, all settings, executable hashes and computational source hash.
Local raw reports are deliberately excluded from Git. The checked-in summary
contains the numerical aggregate evidence, and raw rays can be regenerated.
Timing and temporary paths make byte-identical reports across runs unnecessary.
Source or tracer changes during a run reject its final report. JSON and manifest
writes are individually atomic, not one multi-file transaction.

```text
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release --output-on-failure
build/headless-release/gravastar_sampling --scene benchmarks/optics_v1.json --output results/p2_toy_sampling.json
build/headless-release/gravastar_sampling --scene benchmarks/optics_kerr_v1.json --output results/p2_kerr_sampling.json
```

An explicit `--trace` selects a tracer executable; the default is the sibling
`gravastar_trace`. `--config`, `--root` and `--output` support native test fixtures.
All scene/policy inputs must be inside `benchmarks/`, and outputs inside
`results/`. No shell, Python or other language automation is used.
