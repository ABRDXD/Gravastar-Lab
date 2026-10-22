# P0/P1 status and audit handoff

This is the historical P1 acceptance record, frozen at `609debe`. Publication
to remote `main` was verified on 2026-10-21. Current P2 work and its separate
artifacts are documented in [P2 status](P2_STATUS.md); the original P1 reports
and manifests remain preserved rather than being regenerated for P2 sources.

2026-10-21. P0 specification and P1 implementation cover the bounded,
positive-radius, off-axis `toy-c3-v1` geometry benchmark.
This is a stationary geometry consistency result, not a stability result or
a reproduction of the paper's plotted figures.

## Implemented scope

- Original C++20 production geometry with derivatives through radial order three,
  inverse, Christoffel curvature, analytic limits, domain guards, and JSONL CLI.
- Independent C++20 reference with 90 fixed decimal places and 110-place
  refinement, numerical derivatives, matrix inversion, and covariant curvature.
- Exact arbitrary-integer rational Sturm and Bernstein shell certificates,
  supplemented by analytic core/exterior bounds.
- Explicit one-sided shell endpoint comparisons through C3 and outward-normal
  extrinsic curvature; the endpoint tests bypass region clamping.
- Native validation, source inventories, build/test workflow and failure-safe
  report output. Python and the duplicate production API were retired after
  replacement verification. See [migration evidence](MIGRATION.md).

The accepted parameters are `M=1, a=0.875, Lambda=0.5, r_in=1.5, r_out=1.8`.
The radii are project choices. Numerical coverage is `r/M=[0.25,10]`,
`theta=[0.05,pi-0.05]`, with 4,150 domain samples and 51 reference vectors.
Exact radial positivity is stronger than the sampled domain checks but does
not establish global event-horizon absence.

## Numerical evidence

The final machine-readable report is `results/rotating_v1.json`; the associated
inventory and `results/source_manifest.json` identify the tested sources.
Errors below use the report's mixed relative/absolute norm,
`max |production-reference|/(1+|reference|)`, unless stated otherwise.

| Check | Observed maximum |
| --- | ---: |
| Metric | 2.39e-15 |
| Inverse | 2.83e-15 |
| First derivatives | 1.24e-14 |
| Second derivatives | 7.35e-14 |
| Third radial derivatives | 1.88e-12 |
| Curvature / analytic limits | 3.47e-13 |
| Independent shell C0–C3 matching | 3.26e-31 |
| Independent outward extrinsic curvature matching | 4.16e-32 |
| Reference step refinement | 5.52e-29 |
| Reference 90-to-110-place refinement | 1.17e-71 |

The exact shell lower bound is `Delta >= 2237/8000 = 0.279625`; Sturm counts
zero shell roots. Core and exterior inequalities are positive. Coordinate
eigenvalue magnitude stays above 0.0018345 and sampled coordinate condition
below 1126.24. These conditioning values depend on the coordinate basis.
Production forced-shell endpoint residuals vanish, as expected from the exact
C3 endpoint identities; independent numerical residuals are nonzero.

The effective stress-energy diagnostic finds a negative null contraction
approximately `-2.25985`, an NEC violation witness. It is reported separately
from geometric consistency and does not assume a perfect-fluid interpretation.
The report includes the point, null vector and null residual.

## Verification and reproduction

Release CTest passed **9/9** in 12.69 s (scientific test 7.69 s).
Debug CTest passed **9/9** in 141.62 s (scientific test 85.50 s).
Optimized ASan/UBSan CTest passed **9/9** in 351.96 s (scientific test 214.80 s),
with leak detection disabled as described below. Its CTest log contains no
address-sanitizer or undefined-behavior diagnostic.
The earlier pre-removal checkpoint passed 8/8 release CTests before deleting
any Python. Runs overlapped other local verification; timings are observational.
Structured command outcomes, including the unsuccessful initial sanitizer
attempt, are retained in `results/verification.json`. The scientific report
passes all **30** acceptance checks.

```text
cmake --preset headless-release
cmake --build --preset headless-release --parallel 4
ctest --preset headless-release
build/headless-release/gravastar_validate --output results/rotating_v1.json
build/headless-release/gravastar_tools manifest
build/headless-release/gravastar_tools manifest --verify results/source_manifest.json
```

Use `headless-debug` or `sanitizers` for the other presets. In this sandbox,
LeakSanitizer reports that it cannot operate under ptrace; the address/UB run
requires `ASAN_OPTIONS=detect_leaks=0`. Leak checking remains unverified here.
No other sanitizer suppression is intended. The default preset retains leak
detection for execution on a suitable host.
The first unoptimized sanitizer attempt also hit the scientific test's 300 s
timeout. The follow-up uses an optimized instrumented build with debug symbols:

```text
cmake --preset sanitizers -B build/sanitizers-optimized -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/sanitizers-optimized --parallel 3
env ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers-optimized --output-on-failure
```

## Remaining gates and Argon review

Argon should independently audit frozen P1 while the authorized bounded P2
work proceeds separately. Review the forced shell selection,
inner de Sitter/outer Kerr pairing, all C0–C3 tensor entries and K_AB convention;
then reproduce the reference step/precision convergence and exact rational
certificate from a clean build. Review the original arbitrary-precision code
as well as the geometry: analytic and arithmetic tests give evidence, not a
formal proof of the implementation.

The following remain unresolved:

1. Exact source figure radii. This preset must retain its project-selected label.
2. Central extension, axis charts, horizon-crossing reference charts, and a
   complete ray domain. P1 guards must not become artificial photon absorbers.
3. P2 observer/emitter conventions, adaptive integration, and GYOTO API/ABI,
   dependency pins and independent ray comparisons.
4. Full energy-condition classification, a matter closure, and stability.
5. Working GPU/driver access, graphics, desktop interaction, and recording.
   No image, FPS, or evolution claim follows from this benchmark.
6. Independent compiler/platform reproduction and leak checking outside the
   sandbox. Timings are validation wall times, not rendering benchmarks.
7. A GPU implementation strategy compatible with the C++20-only constraint.
   Earlier shader-tool proposals do not authorize another source language.

The original checkout has concurrent uncommitted work and is preserved.
P1 was developed in `GravastarLab-Cpp20` on `codex/p1-cpp20`; P2 uses the
isolated `codex/p2-reference-optics` branch in that same worktree.
`README.txt` is preserved from the backup merge; its historical build commands
are superseded by `README.md`. Audit the published migration tree and its
manifest, rather than mixing files from the unfinished checkout.

The initial P1 publication was blocked by DNS and GitHub integration permissions.
It was subsequently published; remote `main` was verified at `609debe`. The
verified Git bundle remains a recovery artifact. P2 checkpoints are pushed to
their feature branch for independent review.
