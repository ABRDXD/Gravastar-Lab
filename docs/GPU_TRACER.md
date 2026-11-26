# GPU tracer on the CPU tracer's contracts (Phase D steps 2–4, 2026-11-24)

`shaders/gravastar_trace.comp` (built FP64, the Phase D step 1 decision in
`GPU_PRECISION.md`, plus an FP32 recording build) traces camera rays on the
GPU, including their passage through the gravastar core (below).
`gravastar_gpu_trace` drives it, re-traces flagged rays on the CPU, and
evaluates emission and the background channel on the CPU from each final
state. There is no painted
colour anywhere: the old renderer that painted the core cyan was never
brought across, and images use the Phase C emission model (`EMISSION.md`).

```text
build/<desktop>/gravastar_gpu_trace --config benchmarks/optics_keplerian_stars_v1.json \
    --resolution-scale 8 --compare-cpu --check --summary
ctest --test-dir build/<desktop> -R gpu_trace      # toy and Kerr, 32x24, two event tolerances
```

`--event-tolerance E` sets the event tolerance of the GPU path and the CPU
reference alike. `--check-outcomes` keeps only the outcome and GPU-share
ceilings of `--check` (for loose event tolerances, below).

## What the GPU does, and what it hands back

Same contracts as the CPU angular tracer (`OPTICS.md`):

- Dormand–Prince 5(4) with the CPU's tableau, error norm
  `max |err_i| / (atol + rtol max(|y_i|, |next_i|))`, safety 0.9, exponent
  -1/5 (computed to double precision by Newton, since GLSL has no double
  `pow`), growth limits [0.2, 5], rejection rule and step limits.
- The trig-free stereographic Hamiltonian (`GPU_PRECISION.md`), and the
  patch switch at q >= `angular_switch_q` at accepted endpoints.
- Terminal events found by bisection on the substep length from the trial
  start, with the CPU's local-error check: escape, disk (q = 1 inside the
  annulus) and the Kerr capture proxy. As on the CPU, a root in the last
  bisection interval ends the ray at the trial end (`hi = h`). The extremum
  predicted at a clipping root, `f_ext = -f'^2/(2 f'')`, is checked against
  the CPU's tangency margin.
- Kerr scenes use the same closed form with zero shell radii (every radius
  exterior, m = M, no Lambda), which is exactly Kerr. Other families are
  refused, and so is core continuation outside the toy family, as on the
  CPU.
- With core continuation (`CORE_CONTINUATION.md`), the Cartesian core chart
  in a second build of the kernel (below).

Rays needing logic the GPU does not have end as `needs_cpu` with a reason,
and the same CPU tracer re-traces them from the same launch:

| Reason | When |
| --- | --- |
| `core_entry` | r reaches the inner chart limit without core continuation, or a launch lies inside the core entry radius |
| `near_tangency` | a turning point of a terminal surface function whose cubic Hermite extremum lies within 1e-3 of the level (or 1000 margins), a predicted hidden crossing, or a clipping root inside the tangency margin |
| `affine_budget`, `step_budget` | the CPU's limits |
| `numerical_failure`, `null_drift` | invalid RHS, minimum step, or null residual above the scene ceiling |

The near-tangency gate is deliberately wide: the GPU decides nothing close
to a terminal surface.

## The core chart: a second kernel build, transitions on the host (2026-11-25)

Rays entering the core used to be handed to the CPU (4.7% of the toy scene's
rays; at 1080p in FP32 their re-traces took 12–16 s of a 19 s frame). Now:

- `gravastar_trace.comp` builds twice per precision. The angular build has
  the stereographic patches, terminal events and core entry. The core build
  (`-DGRAVASTAR_CORE_CHART`, `gravastar_trace_coreNN.spv`) has the core
  right-hand side from `shaders/gravastar_core.glsl`: the closed form
  `H = (-P_T^2/F + P.P - k W^2)/2` with evaluate_core's domain (exact toy
  core ellipsoid, F > 64 epsilon). It also has the exit ellipsoid as its
  only event surface.
- A ray reaching an entry or exit root takes the clipped step and carries the
  controller's next step, as the CPU does around `change_core`. It then
  pauses with its integrator state: next step, affine parameter, counters,
  attempts used.
- `GpuTracer` transforms it with the CPU tracer's own `to_core`/`from_core`,
  in the order of `change_core` and `switch_patch` with their null-residual
  checks, and resumes it in the other build. A ray through the core takes
  three dispatch rounds; re-entries take more. Any throw hands the ray to the
  CPU, which re-traces it from its launch.
- Event, tangency and hidden-crossing rules on the exit surface are the same
  as for every other surface. Its level scale is 1, like the disk's.

**Why two builds.** The first version (`bce84dd`) put both charts and
GLSL transforms in one kernel. Every ray then ran about five times slower in
FP32, even on Kerr with no core rays at all: 14 to 72 ms at 128x96. The
driver's statistics (`VK_KHR_pipeline_executable_properties`) showed why.
The old kernel already used all 255 registers per thread, and the combined
kernel spilled 1776 bytes per thread to local memory instead of 32.
Matrix-free transforms, position-only validation, moving the transform out
of the event code and `-Os` did not change that; stubbing the transforms
out did. With two builds:

| Build | Registers | Local memory per thread |
| --- | ---: | ---: |
| angular FP32 (old single kernel) | 255 | 48 B (32 B) |
| core FP32 | 224 | 32 B |
| angular FP64 (old single kernel) | 255 | 1840 B (1808 B) |
| core FP64 | 255 | 256 B |

GPU time at 128x96, FP32 at 1e-6: Kerr 14 ms (14 before), toy 21 ms with
every core ray on the GPU (18 ms before, plus 576 CPU re-traces). FP64 at
scene tolerance: Kerr 923–988 ms (986), toy 1545 ms (1411 plus the CPU).

## Per-pixel comparison (128x96 Keplerian star scenes, RTX 3050, FP64)

Results: `results/p3_gpu_image_{toy,kerr}.json` (regenerated 2026-11-26 with the two-kernel tracer, `511453f`). Only GPU-resolved rays are
compared; flagged rays are traced by the CPU tracer itself.

| Scene | Rays on GPU | Re-traced on CPU | Outcome mismatches | Disk intensity (rel.) max | Disk redshift max | Background (rel.) max | Final position max |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| toy (`optics_keplerian_stars_v1`), two-kernel tracer | 12286 / 12288 | 2 near tangency | 0 | 2.8e-9 | 1.4e-9 | 1.8e-5 | 8.4e-8 |
| Kerr (`optics_keplerian_kerr_stars_v1`) | 12288 / 12288 | 0 | 0 | 5.2e-10 | 1.2e-10 | 6.0e-9 | 2.1e-8 |
| toy, before the core chart (single kernel) | 11709 / 12288 | 577 core entry, 2 near tangency | 0 | 6.3e-9 | 3.5e-9 | 1.8e-5 | 8.4e-8 |

The 577 core rays alone have 0 mismatches, final
position max 6.2e-10 and background max 1e-9 (`gravastar_gpu_trace` reports
them as `core_rays`). Medians are 1e-11 to 1e-10. Every maximum except one background value is
below the CPU tracer's own integration error at the scene tolerance (about
3e-7 per pixel against tolerance 1e-13, `EMISSION.md`). The toy background
maximum 1.8e-5 is one ray on a steep flank of a 0.02-rad star, where a
final-position difference of 8e-8 changes the sampled intensity; its p99 is
1.3e-8.

The `--check` ceilings (no outcome mismatch; disk intensity and redshift
within 1e-7; position within 1e-6; at least 90% of rays on the GPU) are
enforced by the `gpu_trace_vs_cpu` and `gpu_trace_vs_cpu_kerr` CTests.

**Bug found by the comparison:** the first Kerr run used the toy closed form
with the toy shell radii, so rays near the capture proxy (r = 1.55) moved in
the wrong metric: 3977 mismatched outcomes. The zero-radii fix and the Kerr
CTest followed.

## Bug found by the recording measurements: dropped last-interval roots (2026-11-25)

The kernel kept the earliest terminal root with `hi < hit_tau`, where
`hit_tau` starts at the trial length `h`. When a root lay in the last
bisection interval before the trial end, bisection never lowered `hi`, so
`hi == h`, the comparison failed and the hit was dropped. The step was then
accepted past the surface. A dropped disk hit let the ray fly on through the
disk. A dropped escape, capture or core-entry hit was usually healed by a CPU
re-trace (affine budget, numerical failure), but not always. The CPU tracer
has no such comparison. The fix counts the first hit by itself
(`hit < 0 || hi < hit_tau`).

How often: the last interval is about `event_tolerance/2` to
`event_tolerance` long. At the recording tolerance (1e-6, so event tolerance
1e-5) with steps up to 0.1, that is about 1 in 16,000 crossings. At scene
tolerances (event tolerance 1e-9, steps 0.01 to 0.1) it is about 1 in 10^7
to 10^8, so the 128x96 comparison above could not see it.

Found on acceptance frame 0 (1920x1080, FP32 at 1e-6): 56 escaped pixels
had all 48 neighbours (7x7) on the disk. All 56 were GPU-resolved, and the
CPU tracer at the same settings put every one of them on the disk. The FP64
kernel at 1e-6 missed 36 of the same 56. With the fix, none remain and
emitted rays go from 1,169,974 to 1,170,031 (the FP64 scene-tolerance
reference, rendered before the fix, had 1,170,051). Fresh measurements are in
`RECORDING.md`.

**Regression tests.** At event tolerance 1e-2 roots often fall in the last
interval. GPU-resolved outcome mismatches against the CPU at the same
settings, 32x24 rays:

| Scene, event tolerance | Old kernel: mismatches, rays handed to the CPU | Fixed kernel |
| --- | --- | --- |
| Kerr, 1e-9 | 0, 0 | 0, 0 |
| Kerr, 1e-3 | 2, 242 | 0, 0 |
| Kerr, 1e-2 | 8, 456 | 0, 0 |
| Kerr, 0.05 | 31, 579 | 0, 0 |
| toy, 1e-9 | 0, 36 | 0, 36 |
| toy, 1e-3 | 2, 38 | 0, 36 |
| toy, 1e-2 | 14, 58 | 0, 36 |
| toy, 0.05 | 68, 310 | 0, 36 |

The fixed kernel also gives 0 mismatches at event tolerance 1. The toy
scene's 36 hand-backs are core entries. `gpu_trace_loose_events_kerr` (full
`--check`) and `gpu_trace_loose_events_toy` (`--check-outcomes`) run at
1e-2; both fail on the old kernel.

**Why toy values differ at loose event tolerances.** On the toy scene, a
few GPU-resolved values differ from the CPU by up to one bisection cell, at
most the event tolerance. Medians stay at rounding level. At event
tolerance 1e-2 the largest final-position difference is 5.2e-3, with median
7.7e-8. The step grids drift apart at the shell joins, where the metric is
only C3. Rounding-level differences between the GPU closed form and the CPU
then decide whether a step straddling a join is accepted. Measured on ray
310 of the 32x24 toy scene: the two grids agree to 9e-9 until the outer
join crossing at s = 8.619, differ by 1.3e-6 ten steps later, and by 1.9e-5
at escape. Both paths stay accurate: at event tolerance 1e-12 the final
radius and direction of the worst ten rays agree to 3e-9 and the affine
parameter of the hit to 3e-8. A loose event tolerance only moves the final
bisection cell. Kerr, with no joins, agrees at rounding level at
every event tolerance. That is why the toy test checks outcomes only.

## Timing (honest)

| Scene | GPU kernel | CPU, 12 threads, all rays | Ratio |
| --- | ---: | ---: | ---: |
| toy | 1.47 s (+ 1.33 s single-threaded CPU fallback and report assembly) | 3.39 s | 1.2x including fallback |
| Kerr | 0.98 s | 1.50 s | 1.5x |

In FP64, this laptop GPU (FP64 at 1/64 rate) is only modestly faster than
the CPU on all its threads. A single-threaded CPU run took 16.8 s for the toy
scene; comparing with that would overstate the GPU by 5x. The FP32 path is
74x faster per step (`GPU_PRECISION.md`) but cannot meet the per-pixel gate.

## Work budget

Rays are dispatched in batches (default 4096, `--batch`), each within the
compute context's limits of 16384 workgroups and a 30-second fence deadline;
a 4096-ray FP64 batch of these scenes takes about 0.5 s. Each ray is bounded
by the scene's attempt and affine budgets.

## Not covered

No nonterminal events (shell joins) on the GPU; they never affect a path. The FP32 build is the recording path (`RECORDING.md`) and does not meet
the per-pixel gate above. Since Phase F, the CPU fallback runs on every
hardware thread; the timings above predate that. One GPU, one driver.
