# P0/P1 architecture and reproducibility

The accepted implementation boundary is a headless stationary-geometry
benchmark in an independent C++20 project. The scientific contract is
[MODEL.md](MODEL.md); environmental and licensing choices are in
[ENVIRONMENT.md](ENVIRONMENT.md) and [THIRD_PARTY.md](../THIRD_PARTY.md).
Geometry can be built and inspected without graphics, network access, Python
packages, or a neighboring KerrLab checkout.

## Implemented boundaries

| Layer | Responsibility | Dependency boundary |
| --- | --- | --- |
| `include/gravastar/math` | Bivariate Taylor jets and elementary operations | C++ standard library |
| `gravastar_geometry` | Metric, inverse, derivatives, curvature, structured domain errors | Math and C++ standard library |
| `gravastar_benchmark` | Headless JSON/JSONL evaluation interface | Geometry; owns parsing and output only |
| `python/` | Independent high-precision formulas, tensor calculation, exact polynomial diagnostics, acceptance checks | Python standard library; invokes the evaluator as a separate process |
| `benchmarks/` | Versioned parameters, sampling domain, precision and thresholds | Explicit JSON configuration |
| `tools/` | Configure/build/test orchestration and source provenance | External CMake/Ninja/compiler plus Python standard library |
| `results/` | Small generated benchmark reports and provenance manifests | Output artifacts; never imported as source formulas |

The C++ public evaluation contract uses `Parameters`, `Evaluation`, and
`Result = variant<Evaluation, Error>`. The coordinate/index order, derivative
array layout, units, chart exclusions, and reference families are fixed by
the model document. Parameters are values passed into an evaluation; no GUI
or renderer state controls a metric evaluation.

The Python reference independently expands the metric and uses high-precision
finite differences, while C++ uses Taylor jets. Python contracts covariant
curvature independently; C++ differentiates Christoffel symbols. Analytic
reference limits and step/precision refinement check both implementations.
Agreement alone cannot eliminate a shared interpretation error, so the
source-equation audit and explicit chart assumptions remain part of acceptance.

## Validation entry point

From any working directory:

```sh
python3 /path/to/GravastarLab/tools/validate.py
```

The default selects `headless-release`, configures and builds with CMake,
runs CTest, then invokes:

```sh
python3 python/validate.py \
  --executable build/headless-release/gravastar_benchmark \
  --config benchmarks/rotating_v1.json \
  --output results/rotating_v1.json
```

The orchestrator actually writes the reference report to a temporary file
first, attaches its computational source identity, and atomically replaces the
final JSON only after successful command exits. It retains the previous report
if compilation or checks fail. It rejects computational source changes during
the run. Arguments are passed directly to subprocesses, so project paths with
spaces are supported without shell evaluation.

Use `--preset headless-debug` or `--preset sanitizers` for the other configured
builds, `--jobs N` to bound build concurrency, and `--output results/NAME.json`
for a separate run artifact. Configuration inputs must be inside `benchmarks/`;
generated report/manifest destinations must remain under `results/`.
Sanitizers and debug builds are independent checks, not replacements for
reported release-build measurements.

## Source identity

`tools/source_manifest.py` records SHA-256 and byte count for each selected
source/configuration/documentation file. It produces:

- `computational_sha256`: code, build files, benchmark configuration, and tools.
- `documentation_sha256`: project Markdown, model/audit documents and licenses.
- `source_sha256`: the combined inventory.

Each digest hashes deterministic UTF-8 JSON of its ordered file records,
with sorted keys and compact separators. Absolute host paths and timestamps
are excluded. Reports embed **only the computational digest**, avoiding a
cycle when documentation describes a report. Documentation can change after a
run without changing which implementation produced the numerical output.

The selected directories and top-level files are explicit in the script.
`build/`, `results/`, `.git/`, `.venv/`, `.cache/`, Python caches, and common
compiled outputs are excluded. Unselected top-level files are excluded too;
add new source roots to the inventory when the architecture grows. Symlinked
selected files/directories are rejected so a manifest cannot silently depend
on a sibling project. A manifest hashes content; it is not a signature or a
proof of authorship.

Regenerate or verify a manifest with:

```sh
python3 tools/source_manifest.py
python3 tools/source_manifest.py --verify results/source_manifest.json
```

The report and manifest each use atomic replacement, but are not a two-file
transaction. Compare their computational digests to detect interruption
between replacements. Re-run validation if they disagree. Excluded binary
artifacts and host/compiler versions need their own identity in measured
results; the source hash alone does not promise identical binaries or
cross-platform floating-point output.

## Deferred boundaries

P1 provides no geodesic integrator, GYOTO adapter, radiation model, Vulkan
renderer, desktop UI, timeline, video writer, central extension, or chart atlas.
The source chart is deliberately bounded. These gaps are explicit gates for
P2 and later, particularly for any claim of complete interior ray tracing.
The fixed preset can be accepted only for the diagnostics and domain recorded
in its actual report; this architecture does not establish matter stability.
