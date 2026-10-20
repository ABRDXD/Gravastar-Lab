# Environment and build decision

Audit date: **2026-10-20**, beginning at 21:11 IST. These are observations from
the implementation session, not guarantees about another shell or machine.
The authorized P0/P1 scope uses a headless C++20 build and an independent Python
standard-library calculation. No global packages were installed.

## Host and toolchain

| Item | Observed value |
| --- | --- |
| OS | Arch Linux, x86-64; kernel `7.2.11-arch1-1`; glibc `2.44` |
| CPU | AMD Ryzen 7 7445HS w/ Radeon 740M Graphics; 6 cores, 12 logical CPUs |
| Memory | `free -h`: 14 GiB total, approximately 6.0 GiB available at audit |
| Project filesystem | 421 GiB total, 354 GiB available at audit |
| GCC / G++ | `16.2.1 20260810`; Arch package `16.2.1+r23+gd564253eb6c8-1` |
| CMake | `4.4.4` |
| Ninja | `1.13.2` |
| Python | `/usr/bin/python3`, `3.14.8`; GCC-built CPython |
| Decimal backend | Standard-library `decimal`, libmpdec `4.0.1` |
| pkg-config | `3.0.10` |

The P1 build uses only the C++ standard library and ordinary compiler/system
runtimes. It must configure and run without a GPU, window system, network, or
KerrLab checkout. Fast-math is excluded from the reference build.

## Scientific Python decision

System Python reports NumPy `2.5.4`; SciPy, SymPy, mpmath, and pytest were absent.
The project-local `.venv` already existed when this audit began and contained
Python `3.14.8` and pip `26.2.1`, with none of those scientific packages installed.
The environment was not modified by this audit.

P1 therefore uses **Python's standard-library `decimal` at 60 decimal digits**
with an independently expanded metric and high-precision numerical differences.
Trigonometric series and precision/refinement checks belong to that reference
implementation and must be checked explicitly. Merely setting Decimal precision
does not establish 60 correct digits in a differentiated result. The C++ evaluator
does not supply the Python formulas or derivatives.

There are no third-party Python dependency pins for P1 because it requires no
third-party Python packages. The measured interpreter/backend versions above
record the actual validation environment. NumPy's presence is not a hidden
requirement. A future mpmath/SymPy cross-check can be added with a pinned local
environment and preserved notices, but is not represented as performed here.

Package acquisition through the execution environment was unavailable during the
session because DNS/network access was restricted. The standard-library choice
also makes the headless benchmark reproducible offline from its source checkout.

## Graphics and later-stage dependencies

| Component | Actual probe | P0 decision |
| --- | --- | --- |
| NVIDIA hardware | `lspci -nn`: GeForce RTX 3050 **6GB Laptop GPU**, PCI `10de:25ac` | Device presence observed; usable VRAM and rendering performance unmeasured |
| AMD graphics | `lspci -nn`: HawkPoint2, PCI `1002:1901` | Present; Vulkan capability unverified |
| NVIDIA kernel/module packages | `/proc/driver/nvidia/version`: `615.78.08`; `nvidia-open 615.78.08-3`, `nvidia-utils 615.78.08-1` | Installed version does not establish working driver access |
| `nvidia-smi` | Exit 9: could not communicate with NVIDIA driver | GPU access unresolved |
| Vulkan headers / loader / tools | Arch `1.4.363.0`; pkg-config `1.4.363`; header present | Available for a later compile, not used in P1 |
| `vulkaninfo --summary` | Exit 1; NVIDIA ICD failed to provide `vkCreateInstance`; `ERROR_INCOMPATIBLE_DRIVER` | No accessible Vulkan device established |
| shaderc / `glslc` | `2026.4` | External build tool, unused in P1 |
| glslangValidator | `16.6.0`; Arch package `1:1.4.363.0-1` | External build tool, unused in P1 |
| GLFW | No pkg-config `glfw3`, no `/usr/include/GLFW/glfw3.h`, no `glfw` package | Not integrated or pinned |
| Dear ImGui | No `/usr/include/imgui/imgui.h`, no `imgui` package | Not integrated or pinned |
| ImPlot | No `/usr/include/implot.h`, no `implot` package | Not integrated or pinned |
| GYOTO | No `gyoto` executable or installed package | Adapter/API/ABI selection deferred to P2 |
| FFmpeg | `n9.0.2`; Arch `2:9.0.2-2`; GPL and version3 enabled | External executable available; recording acceptance not run |
| libpng | pkg-config `1.6.59` | Candidate for later lossless output |
| OpenEXR | pkg-config `3.5.2` | Candidate for later floating-point output |

The missing GLFW/ImGui/ImPlot probes establish absence from the checked system
locations and package registry, not absence from every directory on disk. Local
KerrLab dependency copies are not adopted by GravastarLab.

GPU features, queue families, usable memory budget, FP32/FP64 behavior, timestamp
support, validation-layer behavior, presentation, and frame rates remain
unmeasured. The observed driver errors do not identify their root cause. Repairing
system drivers is outside this P0/P1 implementation.

## Repeating the audit

Run from the project root. Failed probes are expected on machines without these
optional dependencies.

```sh
date --iso-8601=seconds
uname -a
lscpu
free -h
df -h .
cmake --version
g++ --version
ninja --version
python3 --version
python3 -c 'import decimal; print(decimal.__libmpdec_version__)'
pkg-config --modversion vulkan
pkg-config --modversion glfw3
pkg-config --modversion libpng
pkg-config --modversion OpenEXR
lspci -nn
nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
vulkaninfo --summary
glslc --version
glslangValidator --version
ffmpeg -version
ffmpeg -L
command -v gyoto
```

Build and scientific acceptance commands are recorded with the benchmark
implementation and its result report. This audit records availability; a
successful tool version probe is not a successful build or physics check.
