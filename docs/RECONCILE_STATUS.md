# Branch reconciliation status (Phase A)

Recorded 2026-11-23 on branch `integration/reconcile`, built from
`codex/p2-reference-optics` (`5306243`) with work brought across from
`backup/astra-work` (`b3dd0c1`). The step-by-step decisions and what was not
ported are in [ROADMAP.md](../ROADMAP.md), Phase A.

This is an engineering and consistency record. It is not P2 acceptance, a
GYOTO comparison, an energy-condition result or a stability result.

## Measured test results

Host: Arch Linux, GCC 16.2.1, Clang 23.1.4, NVIDIA RTX 3050 6GB Laptop GPU
(Vulkan 1.4.351), Wayland session. Times are CTest wall times with other work
on the machine, not performance claims.

| Configuration | Command | Result | Time |
| --- | --- | --- | --- |
| GCC Release, headless | `cmake --preset headless-release` | 28/28 | 29 s |
| GCC Release, desktop | `cmake --preset desktop-release` | 32/32 | 30 s |
| Clang Release, desktop | `CXX=clang++ CC=clang cmake --preset desktop-release` | 30/30 (2 quad-precision standalone suites skipped: no libquadmath for Clang) | 23 s |
| GCC Debug, headless | `cmake --preset headless-debug` | 28/28 | 679 s for 27 tests; `optics_cli` run directly took 680 s and passed 58/58 |
| GCC ASan/UBSan, RelWithDebInfo, desktop | `cmake --preset sanitizers -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGRAVASTAR_DESKTOP=ON` | 32/32, no sanitizer diagnostics in the CTest log | 357 s |

All five builds compiled with no compiler warnings. `optics_cli` now has a
1500 s limit in Debug builds only; Release keeps 600 s. In sanitizer builds,
LeakSanitizer is off for the four Vulkan tests only: every leak report there
is allocated in the run-time-loaded GPU driver or libdbus.

## New cross-checks and what they measured

| Test | Compares | Measured |
| --- | --- | --- |
| `shell_turning` (104 checks) | Production adaptive tracer vs independent equatorial potential W(r) | Turning radii agree to 2.8e-12–2.0e-9 (tolerance 1e-12); four entry thresholds agree with astra closed forms to ≤ 6.8e-11; join crossing counts correct either side of each threshold |
| `vulkan_metric_fp32` | FP32 GLSL metric kernel vs CPU evaluator | max metric 1.74e-5, inverse 1.70e-6, d_r g 4.99e-6 (absolute) |
| `vulkan_ray_fp32` | FP32 fixed-step GPU rays vs exact turning radius | 2.4e-6–5.3e-5; core classification correct; null drift ≤ 2.8e-4; ceiling 1e-4 |
| `recording_lifecycle` | Synthetic 64×64, 30-frame job: cancel, resume, malformed frame, FFmpeg, ffprobe exit | Pass; not the 1920×1080 600-frame acceptance run |
| `standalone_geodesic_reference`, `standalone_zamo_shadow_reference` | Their own metric copies only | Pass; they do not exercise production code |

## Open items carried forward

- Upstream pins for the vendored Vulkan headers and GLFW (THIRD_PARTY.md).
- GPU image renderer and emission model: rebuilt in Phase D.
- Recorder gaps listed in ROADMAP.md, Phase F.
- Re-point the standalone shadow and Carter-drift checks at the tracer
  (Phase C).
