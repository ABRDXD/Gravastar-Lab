# Environment and build decision

Audit date: **2026-10-20**, beginning at 21:11 IST. These are observations from
the implementation session, not guarantees about another shell or machine.
Updated 2026-10-21: P0/P1 uses C++20 throughout, including the independent
reference and development tools. No global packages were installed.

P2 update, 2026-10-22: Clang 23.1.3 is also available. GCC and Clang release
builds passed all 12 CTest targets. The three optics targets passed optimized
ASan/UBSan with default leak detection in the unrestricted session; the older
ptrace-related leak limitation did not recur. No dependency was added for
native optics. See `P2_STATUS.md` for the exact coverage and retained logs.

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
| JSON library | Installed header-only C++ `nlohmann_json 3.12.0` |
| pkg-config | `3.0.10` |

Production geometry uses the C++ standard library and ordinary compiler/system
runtimes. Validation/tools additionally use nlohmann/json. It configures and runs
without a GPU, window system, network, or KerrLab checkout. Fast-math is disabled.

## Scientific reference decision

The user's C++20-only constraint supersedes the initial Python prototype.
Its migration is documented in `MIGRATION.md`. The `.venv` and caches in the
original working directory are not source dependencies and were not modified.

Boost.Multiprecision headers were absent. GMP and MPFR were present but were
not adopted. The reference uses original C++20 arbitrary integers, normalized
rationals and fixed-decimal arithmetic at 90 places, with 110-place refinement.
Arithmetic and transcendental identities have their own CTest checks; the place
count is not an accuracy claim for curvature. No packages or language
dependencies were installed. Restricted local DNS prevented downloading the
paper source archive during P0.

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
