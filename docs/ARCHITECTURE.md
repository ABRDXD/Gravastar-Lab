# Headless geometry and reference optics architecture

All authored implementation and tooling is C++20. CMake configures builds,
integrates dependencies, and registers CTest checks. JSON stores data;
Markdown documents the model. No interpreter, runtime code generator, shell
script, network fetch, GPU or sibling checkout is needed for P1.

| Component | Responsibility |
| --- | --- |
| `gravastar_geometry` | Double metric/inverse, third-order Taylor jets, Christoffel curvature, domain errors |
| `include/gravastar/reference/` | Independent expanded metric, 90/110-place differences, covariant curvature, exact polynomials |
| `gravastar_benchmark` | Individual evaluation and JSONL batch interface |
| `gravastar_optics` | Circular observer frames, adaptive past-directed Hamiltonian tracing, refined events and prescribed emission |
| `gravastar_trace` | Versioned camera/scene input, individual trajectories, linear image JSON and diagnostic PPM |
| `gravastar_validate` | Configuration, reference comparisons, scientific acceptance, JSON reports |
| `gravastar_tools` | Native build/test workflow, source manifest generation and verification |
| `tests/` | Numerical identities, input failures, tooling behavior, migration verification |

The independent reference has no include of production geometry or jets. Its
fixed-decimal arithmetic truncates toward zero and has arbitrary integer range.
The 90-place working scale and 110-place refinement are implementation choices,
not a claim of 90 correct digits after numerical differentiation. Analytic
curvature identities, step refinement, arithmetic identities and precision
refinement bound observed error on the reported samples. Exact rational Sturm
sequences and Bernstein coefficients use arbitrary-length integers.

`nlohmann_json 3.12.0` is the sole additional C++ library, used by validation and
development tools. Production geometry uses the standard library only. Linux
tools use POSIX process/file APIs from C++20: direct argument vectors,
`fsync`, and atomic renames. No shell interpreter executes workflow commands.

## Reproducible workflow

From the source root, invoke these compiled programs directly:

```text
cmake --preset headless-release
cmake --build --preset headless-release --parallel 4
ctest --preset headless-release
build/headless-release/gravastar_validate --output results/rotating_v1.json
build/headless-release/gravastar_tools manifest
build/headless-release/gravastar_tools manifest --verify results/source_manifest.json
```

After the initial build, `gravastar_tools workflow --preset headless-release`
configures, builds, tests and produces the final report. Select `headless-debug`
or `sanitizers` with the same commands for those builds. Absolute `--root`
supports invocation from another directory. The workflow does not recurse
through CTest: tests invoke scientific validation, never the build orchestrator.

The validator rejects unsupported schemas, unsupported reference precision,
invalid parameters and outputs outside `results/`. A failed check writes a
separate `.failed.json` report and returns nonzero; it preserves the last
accepted report. CTest exercises this with deliberately impossible tolerances.
Source changes during a run also prevent acceptance.

Reports state exact configuration, input/executable hashes, compiler, platform,
thresholds, measured errors, failed checks and computational source digest.
Manifests hash ordered records with SHA-256 and separate computational/documentation
digests. Build/results/cache/VCS trees are excluded; selected symlinks are rejected.
Two separately renamed report/manifest files are not one transaction; compare
their computational digests after an interruption. Hashes identify bytes, not authorship.

## Consolidated implementation

The earlier `metric.hpp` API, duplicate implementation, and unused configuration
were retired after their coverage was ported to `tests/cpp_geometry.cpp`.
The `[1.8,2.2]` shell remains regression input; `rotating_v1.json` is the sole
accepted preset. See `MIGRATION.md` for the pre-removal verification record.
CTest also enforces an allow list of C++/CMake source and data/document formats;
this format check supplements review of the C++20 architectural constraint.

The optics library calls the canonical geometry evaluator; no duplicate metric
implementation was added. It currently computes curvature along with metric
derivatives on each evaluation, favoring a single validated path over speed.
Its Dopri5 pair is original C++20. Independent expanded high-precision geometry
checks its RHS; exact analytic rays and a separately stepped RK4 integrator
check trajectories. RK4 shares production geometry, a limitation recorded in
the report. No GYOTO dependency or adapter is present.

The tracer uses future covariant momentum while advancing a past-directed
affine parameter. It preserves unresolved domain/budget outcomes in scientific
data. Event substep failure rejects the complete trial before any events are
recorded. JSON and PPM are individually atomic files, not one transaction;
an interrupted multi-file write requires checking the associated source digest.
Failed numerical runs use `.failed.json` and preserve accepted images/reports.

Chart transitions, central continuation, GYOTO integration, GPU/desktop work,
recording and stability research remain outside this checkpoint. See
`OPTICS.md` for the bounded chart, capture proxy and emission conventions.
