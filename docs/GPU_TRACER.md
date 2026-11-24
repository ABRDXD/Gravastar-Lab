# GPU tracer on the CPU tracer's contracts (Phase D steps 2–4, 2026-11-24)

`shaders/gravastar_trace.comp` (built FP64, the Phase D step 1 decision in
`GPU_PRECISION.md`) traces camera rays on the GPU. `gravastar_gpu_trace`
drives it, re-traces flagged rays on the CPU, and evaluates emission and the
background channel on the CPU from each final state. There is no painted
colour anywhere: the old renderer that painted the core cyan was never
brought across, and images use the Phase C emission model (`EMISSION.md`).

```text
build/<desktop>/gravastar_gpu_trace --config benchmarks/optics_keplerian_stars_v1.json \
    --resolution-scale 8 --compare-cpu --check --summary
ctest --test-dir build/<desktop> -R gpu_trace      # toy and Kerr, 32x24
```

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
  annulus) and the Kerr capture proxy. The extremum predicted at a clipping
  root, `f_ext = -f'^2/(2 f'')`, is checked against the CPU's tangency margin.
- Kerr scenes use the same closed form with zero shell radii (every radius
  exterior, m = M, no Lambda), which is exactly Kerr. Other families are
  refused.

Rays needing logic the GPU does not have end as `needs_cpu` with a reason,
and the same CPU tracer re-traces them from the same launch:

| Reason | When |
| --- | --- |
| `core_entry` | r reaches the core patch radius (or the inner chart limit without core continuation) |
| `near_tangency` | a turning point of a terminal surface function whose cubic Hermite extremum lies within 1e-3 of the level (or 1000 margins), a predicted hidden crossing, or a clipping root inside the tangency margin |
| `affine_budget`, `step_budget` | the CPU's limits |
| `numerical_failure`, `null_drift` | invalid RHS, minimum step, or null residual above the scene ceiling |

The near-tangency gate is deliberately wide: the GPU decides nothing close
to a terminal surface.

## Per-pixel comparison (128x96 Keplerian star scenes, RTX 3050, FP64)

Results: `results/p3_gpu_image_{toy,kerr}.json`. Only GPU-resolved rays are
compared; flagged rays are traced by the CPU tracer itself.

| Scene | Rays on GPU | Re-traced on CPU | Outcome mismatches | Disk intensity (rel.) max | Disk redshift max | Background (rel.) max | Final position max |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| toy (`optics_keplerian_stars_v1`) | 11709 / 12288 | 577 core entry, 2 near tangency | 0 | 6.3e-9 | 3.5e-9 | 1.8e-5 | 8.4e-8 |
| Kerr (`optics_keplerian_kerr_stars_v1`) | 12288 / 12288 | 0 | 0 | 5.2e-10 | 1.2e-10 | 6.5e-9 | 2.2e-8 |

Medians are 1e-11 to 1e-10. Every maximum except one background value is
below the CPU tracer's own integration error at the scene tolerance (about
3e-7 per pixel against tolerance 1e-13, `EMISSION.md`). The toy background
maximum 1.8e-5 is one ray on a steep flank of a 0.02-rad star, where a
final-position difference of 8e-8 changes the sampled intensity; its p99 is
1.5e-8.

The `--check` ceilings (no outcome mismatch; disk intensity and redshift
within 1e-7; position within 1e-6; at least 90% of rays on the GPU) are
enforced by the `gpu_trace_vs_cpu` and `gpu_trace_vs_cpu_kerr` CTests.

**Bug found by the comparison:** the first Kerr run used the toy closed form
with the toy shell radii, so rays near the capture proxy (r = 1.55) moved in
the wrong metric: 3977 mismatched outcomes. The zero-radii fix and the Kerr
CTest followed.

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

No Cartesian core patch on the GPU (core rays go to the CPU: 4.7% of the toy
scene). No nonterminal events (shell joins) on the GPU; they never affect a
path. No FP32 build of this tracer. The CPU fallback is single-threaded.
One GPU, one driver.
