# Gravastar-Lab

> *VIbeCoded btw so dont actually try to copy this use anything else*  
> *This project contains general relativity, Vulkan compute shaders, numerical integration, and several thousand lines of C++. I barely understand half of it. Proceed at your own risk.*


---

Rotating gravastar research, geometric benchmarking, and scientific evaluation framework.

GravastarLab provides an independent C++20 and Python research environment for analyzing rotating gravastars based on the [de Sitter-core / Kerr-exterior metric family](https://arxiv.org/abs/2309.00551). The framework emphasizes rigorous scientific validation, exact polynomial diagnostics, Taylor jet arithmetic, and cross-language consistency checks.

---

## Architecture Overview

| Directory / Layer | Purpose | Dependencies |
| :--- | :--- | :--- |
| [`include/gravastar/`](file:///home/abrsdxd/Projects/GravastarLab/include/gravastar) | Public C++ headers for bivariate Taylor jets and metric evaluation | C++20 standard library |
| [`src/geometry/`](file:///home/abrsdxd/Projects/GravastarLab/src/geometry) | Metric tensor, derivatives, Christoffel symbols, and Riemann/Ricci/Einstein curvature tensors | C++20 standard library |
| [`src/experiments/`](file:///home/abrsdxd/Projects/GravastarLab/src/experiments) | Headless benchmark executable (`gravastar_benchmark`) with JSON/JSONL output | Geometry library |
| [`python/`](file:///home/abrsdxd/Projects/GravastarLab/python) | Independent multi-precision reference formulas, polynomial bounds, and finite-difference validation | Python standard library |
| [`benchmarks/`](file:///home/abrsdxd/Projects/GravastarLab/benchmarks) | Versioned parameter domains, sampling configs, and precision tolerances | JSON specifications |
| [`docs/`](file:///home/abrsdxd/Projects/GravastarLab/docs) | Detailed scientific specifications, architecture, and provenance records | Markdown |
| [`tests/`](file:///home/abrsdxd/Projects/GravastarLab/tests) | C++ unit tests for Taylor jets, metric limits, and curvature | CTest |
| [`tools/`](file:///home/abrsdxd/Projects/GravastarLab/tools) | Orchestration scripts for build, test, and validation workflows | Python 3 |

For deep-dive documentation on design contracts and scientific specifications:
- [Architecture & Boundaries](file:///home/abrsdxd/Projects/GravastarLab/docs/ARCHITECTURE.md)
- [Scientific Model & Contract](file:///home/abrsdxd/Projects/GravastarLab/docs/MODEL.md)
- [Implementation Plan](file:///home/abrsdxd/Projects/GravastarLab/PLAN.md)
- [Environment & Toolchains](file:///home/abrsdxd/Projects/GravastarLab/docs/ENVIRONMENT.md)

---

## Prerequisites

- **C++ Compiler**: GCC 13+ or Clang 16+ supporting C++20
- **Build System**: CMake (>= 3.25) and Ninja
- **Python**: Python 3.10+ (Standard library only; no extra pip packages required for headless validation)

---

## Quick Start

### 1. Configure and Build (C++)

Using CMake presets:

```bash
# Configure with Ninja (headless release)
cmake --preset headless-release

# Build benchmark and geometry libraries
cmake --build --preset headless-release
```

Other available presets:
- `headless-debug`: Debug symbols enabled
- `sanitizers`: Address and Undefined Behavior sanitizers (ASan/UBSan)

### 2. Run Tests

```bash
ctest --preset headless-release
```

### 3. Run Benchmark Validation

Execute the end-to-end benchmark and cross-language validation pipeline:

```bash
python3 tools/validate.py --preset headless-release
```

---

## License

See [`LICENSE`](file:///home/abrsdxd/Projects/GravastarLab/LICENSE), [`NOTICE`](file:///home/abrsdxd/Projects/GravastarLab/NOTICE), and [`THIRD_PARTY.md`](file:///home/abrsdxd/Projects/GravastarLab/THIRD_PARTY.md) for licensing terms and upstream citations.
