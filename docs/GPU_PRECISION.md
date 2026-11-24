# GPU precision: FP32 versus FP64 (Phase D step 1, 2026-11-24)

Measured on an NVIDIA GeForce RTX 3050 6GB Laptop GPU (Vulkan 1.4), driver as
installed on 2026-11-24. Reproduce with the desktop build:

```text
build/<desktop>/gravastar_gpu_precision --check --width 128 --height 96 --reference-stride 4
ctest --test-dir build/<desktop> -R gpu_precision      # 32x24 rays, ~5 s
```

## What was measured

One algorithm, run five ways on the same rays: fixed-step RK4 (h = 0.01,
at most 4000 steps) on the closed-form toy Hamiltonian in the stereographic
patches, with the patch switch at q >= 1.5 and stops at r >= 10 and
r <= 0.75 (the core is not traced here). Rays are the optics_v1 camera
(ZAMO, r = 8, theta = pi/3, 50 degrees), 128x96 = 12288 rays.

- GPU: `shaders/gravastar_precision.comp` built three ways from one source:
  FP32, FP64, and mixed (double state and RK4 sums, float Hamiltonian).
- CPU: `tests/stereographic_hamiltonian.hpp` in float, double and long
  double, on every fourth ray (3072 rays).

GLSL has no double-precision sin or cos, so the kernel uses the
stereographic patches, where the metric is rational in (r, x, y). Its
Hamiltonian is rewritten to stay regular at the pole; the CPU twin agrees
with the production angular RHS to 3.8e-13 over 4000 states
(`stereographic_closed_form` CTest).

The long-double CPU run is the reference for this algorithm, so the
differences measure arithmetic precision only, not RK4 truncation. The
error is max(|dr|, |dt|/max(1,|t|), |d unit direction|) at the final step.
Rays whose stop or patch switch fell on a different step are counted
separately rather than measured.

## Results (12288 rays, 3072 compared; `results/p3_gpu_precision.json`)

| Run | Error median | p99 | max | Different stop/switch step | Different stop kind | Ray-steps per second |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| GPU FP32 | 8.6e-5 | 2.1e-3 | 4.2e-3 | 132 (4.3%) | 0 | 1.67e9 |
| GPU mixed | 7.2e-5 | 2.3e-3 | 3.7e-3 | 103 | 0 | 3.48e8 |
| GPU FP64 | 1.7e-13 | 5.2e-12 | 1.0e-11 | 0 | 0 | 2.25e7 |
| CPU FP32 | 6.9e-5 | 1.9e-3 | 4.0e-3 | 117 | 0 | — |
| CPU FP64 | 1.4e-13 | 3.9e-12 | 7.9e-12 | 0 | 0 | — |

FP64 is 74x slower than FP32 on this GPU (its hardware FP64 rate is 1/64).
Mixed is 4.8x slower than FP32 and no more accurate: the error comes from
evaluating the Hamiltonian in float, not from accumulating the steps.
GPU and CPU float behave alike, so this is float arithmetic, not a GPU defect.

## Step-size sweep (3072 rays)

| h | FP32 error median / p99 | FP64 error median / p99 | RK4 null drift abs(H), FP32 median | FP64 median |
| ---: | --- | --- | ---: | ---: |
| 0.02 | 1.2e-4 / 4.9e-3 | 2.5e-13 / 1.0e-11 | 9.8e-4 | 9.8e-4 |
| 0.01 | 8.9e-5 / 2.6e-3 | 1.9e-13 / 5.6e-12 | 3.6e-5 | 3.3e-5 |
| 0.005 | 8.0e-5 / 1.7e-3 | 1.7e-13 / 4.3e-12 | 1.2e-5 | 9.4e-7 |

FP32 has a floor near 1e-4 in final position and direction that does not
improve with smaller steps; its null drift stops improving below h = 0.01.
FP64 rounding stays near 1e-12 and leaves the algorithm's own truncation in
charge. No ray changed its stop kind in any run.

## What it means

- Per-pixel agreement with the CPU tracer (scene tolerance 1e-10, image
  error about 3e-7 per pixel, `EMISSION.md`) is only possible in FP64.
- FP32's typical error, 1e-4 rad, is about 0.1 pixel at 1920x1080 with a
  50-degree field (8e-4 rad per pixel); its p99, 2e-3, is about 2.5 pixels.
- At h = 0.01 the average ray took 1988 steps. A 1920x1080 frame of this
  workload would take about 2.5 s in FP32 and about 183 s in FP64; 600
  frames about 25 minutes versus about 31 hours. These are estimates from
  this fixed-step kernel, not from a finished renderer.

## Decision

Keep one kernel source with a compile-time precision:

- **FP64** for reference renders, the Phase D CPU/GPU per-pixel gate and
  recordings that claim agreement with the CPU tracer.
- **FP32** for interactive preview, labelled with its measured floor (about
  1e-4 typical, 5e-3 worst in final position and direction).
- **Mixed is dropped:** 4.8x the FP32 cost for no accuracy gain.

## Comparison with published GPU tracers

GRay (Chan, Psaltis & Ozel 2013, GPL-3.0) reports 3.27 ns per photon step in
single precision on a GeForce GT 650M laptop GPU, and 71.87 ns in a row the
paper does not label (very probably double): 22x. On a Tesla M2090, whose
double rate is not cut down, the gap was 6x. The 74x measured here fits a
consumer GPU with a 1/64 FP64 rate. Both GRay and Odyssey (Pu et al. 2016,
GPL-3.0) map one ray to one GPU thread in blocks of 64, as this kernel does.
No code from either was copied. Sources: arXiv:1303.5057, arXiv:1601.02063.

## Not covered

Adaptive stepping, events and the Cartesian core on the GPU (Phase D step 2);
emission; other GPUs and drivers; power or thermal limits during long runs.
