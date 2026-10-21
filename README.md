# Gravastar-Lab

> *VIbeCoded btw so dont actually try to copy this use anything else*
> *This project contains general relativity, Vulkan compute shaders, numerical integration, and several thousand lines of C++. I barely understand half of it. Proceed at your own risk.*


The original project warning is preserved above. Current implemented scope is
the headless stationary geometry benchmark; Vulkan rendering and integration
remain future work.

GravastarLab implements the transition construction of
[Ilijić and DeBenedictis, arXiv:2309.00551v2](https://arxiv.org/html/2309.00551v2).
Production geometry, independent high-precision references, tests, benchmarks
and development tools are **C++20 only**. CMake is used only for build/dependency
configuration and CTest registration.

P1 uses `M=1`, `a=0.875`, `Lambda=0.5` with **project-selected** shell radii
`1.5` and `1.8`. Numerical results cover a bounded chart. Exact reproduction of
the paper's figures and complete central/axis ray continuation remain unresolved.
Stationary metric consistency does not establish matter stability.

## Build and validate

Requires a C++20 compiler, CMake 3.25+, Ninja, and installed header-only C++
library `nlohmann_json 3.12.0`. No packages are downloaded during the build.
Tested versions and notices are in [THIRD_PARTY.md](THIRD_PARTY.md).

```text
cmake --preset headless-release
cmake --build --preset headless-release --parallel 4
ctest --preset headless-release
build/headless-release/gravastar_validate --output results/rotating_v1.json
```

Individual metric/derivative/curvature evaluation:

```text
build/headless-release/gravastar_benchmark evaluate toy 1 .875 .5 1.5 1.8 1.65 1.2
```

`gravastar_benchmark batch` accepts whitespace-separated evaluation records on
standard input and returns JSONL. `--help` documents its interface. Other build
presets are `headless-debug` and `sanitizers`.

The compiled `gravastar_tools workflow` repeats configuration, build, CTest and
scientific validation; `gravastar_tools manifest` generates a SHA-256 inventory.
Generated files stay under `results/`. Failed runs preserve accepted reports.

Read [status and handoff](docs/STATUS.md), [model contract](docs/MODEL.md),
[architecture](docs/ARCHITECTURE.md), [migration evidence](docs/MIGRATION.md),
[source audit](docs/SOURCE_AUDIT.md), and [the plan](PLAN.md).

Original code is GPL-3.0-or-later. See [LICENSE](LICENSE), [NOTICE](NOTICE), and
[THIRD_PARTY.md](THIRD_PARTY.md). No KerrLab renderer or uncertain-license code
was copied into this project.
