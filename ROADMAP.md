# GravastarLab roadmap: what to do next

Written 2026-11-23 after an audit of `backup/astra-work` (commit `b3dd0c1`)
against [PLAN.md](PLAN.md). PLAN.md stays the specification. This file is the
work order and the honest status.

## 1. Where the project really stands

### Measured today

`cmake --preset headless-release && cmake --build --preset headless-release &&
ctest --preset headless-release` gives **16/16 tests passed** (61 s, RTX 3050
present and working through Vulkan).

### What the PLAN.md checklist claims and what is actually true

PLAN.md ticks P0 through P6 as done. The code does not support that:

| Stage | Claimed | What exists on this branch | Missing against PLAN.md |
| --- | --- | --- | --- |
| P1 geometry | done | C3 toy metric, Taylor jets, curvature, Sturm/Bernstein horizon check, Python reference | Mostly sound. Independent audit still open. |
| P2 reference optics | done | Fixed-step RK4 with a hand-made step shrink near the shell, ZAMO camera, CPU renderer | No error-controlled adaptive method, no pole/axis chart, no core chart, no refined events, **no GYOTO at all**. 77 % of comparator rays end by hitting the step budget, not by a physical outcome. |
| P3 Vulkan optics | done | Metric and ray kernels vs CPU, raymarch shader | FP32 only (1.7e-5 agreement) even though the GPU has FP64. Core rays are painted a fixed cyan colour: an artistic marker, not physics. No emission model, no image convergence study. |
| P4 desktop studio | done | GLFW window, swapchain, calibration pattern, Esc to quit | No ImGui/ImPlot, no orbit camera, no panels, no inspection. It shows a test pattern, not the renderer. |
| P5 recording | done | Rational timeline, atomic writes, resume, FFmpeg + ffprobe | Tested at 64x64, 30 frames, synthetic pattern. Frames are PPM, not PNG. No hashes in progress, no lock, no settings-hash check on resume. The required 1920x1080 600-frame real render has never been run. |
| P6 release | done | Docs, CPack | Clean-clone build never checked. README says both "16 tests" and "15/15". |

### The unmerged branch that matters most

`codex/p2-reference-optics` holds **52 commits** (2026-10-21 to 10-27) that are
not on this branch. It is the more careful line of P2 work:

- Error-controlled adaptive solver shared across charts.
- Pole-regular angular charts with tested momentum transitions.
- A regular Cartesian chart that carries rays through the de Sitter core.
- Refined, null-checked ray events and near-critical Schwarzschild references.
- A pinned GYOTO 2.0.2 candidate and a feasibility study (not yet built).
- Its own PLAN.md, which honestly marks P2 as **not done**.

It also **relicensed original code to MIT**, while this branch is GPL-3.0.
A trial merge conflicts in 11 files (CMake, geometry, benchmark, validate,
docs, results). Vulkan, platform, recording and tools code on this branch
does not conflict: those files are new on this side only.

The two branches were each built as if the other did not exist. Everything
after P2 on this branch sits on the weaker tracer.

## 2. Decisions the owner must make

1. **License:** keep GPL-3.0-or-later (what PLAN.md says) or MIT (what the
   p2 branch chose). Either works with GLFW, Vulkan headers and nlohmann/json.
   GYOTO is GPL, so a GYOTO adapter must be GPL either way; with MIT it would
   live as a separate GPL tool.
2. **Base branch:** recommended to use `codex/p2-reference-optics` as the
   science base and bring this branch's Vulkan, window, recording and tools
   work across on top of it.

**Decided 2026-11-23 by the owner:** keep **GPL-3.0-or-later**, and use
`codex/p2-reference-optics` as the base for `integration/reconcile`.

## 3. Work order

Each phase ends with: tests pass, docs updated with measured numbers, a
commit, and a short entry in HANDOFF.md. No box in PLAN.md is ticked without
the evidence named in its gate.

Steps marked **[MAX]** are physics or hard numerical work. Before each one,
Claude stops, notifies the owner, and waits for effort to be switched to max.
Unmarked steps are plumbing (build, file moves, docs).

### Probe results already gathered for Phase A (2026-11-23)

- `codex/p2-reference-optics` builds cleanly here: **23/24 tests pass**.
  The one failure, `cpp20_language_policy`, trips on the local `.kilo/`
  scratch folder, not on project code.
- That policy only allows `.cpp .hpp .cmake .json .md .txt`. This branch's
  GLSL shaders (5 files) and vendored GLFW (37 `.c`, 66 `.h` and others) would
  fail it. Python under `python/` and `tools/*.py` was retired on p2 in favour
  of C++ (`tools/project.cpp`, `src/experiments/validate.cpp`).
- 50 of 188 tracked files on p2 carry `SPDX-License-Identifier: MIT`.

### Phase A: one honest codebase (first)

**Progress on `integration/reconcile` (2026-11-23):** steps 1–5 done
(commits `40bafcb` policy, `e8f82f2` license, `9800c2c` desktop plumbing).
Changes from the plan, found while doing the work:

- 4a–4c: the GPU metric/ray tests, `gravastar_vulkan_render` and the
  raymarch/metric/geodesic shaders call astra's geometry and integrator, so
  they move to step 8 with the physics. Brought across now: Vulkan headers,
  compute context, window renderer, GLFW window, presentation shader, studio,
  device audit and studio smoke tests.
- 4d: `artifacts.hpp` and `process.hpp` are byte-identical on both branches;
  only `testing.hpp` was new.
- 4e: the recorder renders through astra's CPU renderer, so it also moves to
  step 8.
- Owner rule added to AGENTS.md: GLSL is allowed under `shaders/` only, as a
  C-family relative of C++.
- Vendored Vulkan headers had no license file; Apache-2.0 text added. Upstream
  pins for Vulkan headers and GLFW were never recorded (open item in
  THIRD_PARTY.md).
- Sanitizer runs: GPU-test leak reports all come from the GPU driver and
  libdbus, so leak checking is off for those two tests only.

Measured after step 5 (GCC 16.2.1, Clang 23.1.4, RTX 3050, Wayland):

| Build | Result | Time |
| --- | --- | --- |
| GCC headless release | 24/24 | 25 s |
| GCC desktop release | 26/26, no compiler warnings | — |
| Clang desktop release | 26/26, no compiler warnings | — |
| GCC ASan/UBSan, RelWithDebInfo, desktop on | 26/26, no sanitizer diagnostics in the CTest log | 387 s |

The plain Debug sanitizer build still times out on 4 slow science tests
(300 s limit). The base branch already documented this and uses the optimized
instrumented build above. The Debug suite has not been run yet; it is part of
the step 9 gate.

**Phase A complete, 2026-11-23.** Step 9 gate met: GCC Release 28/28, GCC
desktop 32/32, Clang desktop 30/30, GCC Debug 28/28, optimized ASan/UBSan
32/32, no compiler warnings. Full record in
[docs/RECONCILE_STATUS.md](docs/RECONCILE_STATUS.md). The branch is local
only; `main` has not been moved.

**Steps 6–8 (physics), 2026-11-23:**

- Step 6: p2 wins in all 11 clashing files; the astra side has nothing p2
  lacks. p2's validator covers every astra check with a stronger reference
  (90–110 decimal places vs `__float128`), already tests extrinsic curvature
  (4.2e-32) and explicit-region rejection. Astra's `extrinsic_curvature` and
  `riemann_lower` fields are not ported. Both branches' exact horizon
  certificates agree: shell lower bound 2237/8000, core 5/8, exterior
  649/1600, no shell roots (two C++ implementations, likely of the same
  earlier Python script).
- Step 7 (`c0cf2e0`, `6c91661`, `16cf7e5`): new `shell_turning` test, 104
  checks. The four astra equatorial rays are traced with the production
  tracer against an independent radial potential W(r)
  (`tests/equatorial_reference.hpp`): turning radii agree to 2.8e-12–2.0e-9
  at 1e-12 tolerance. The four astra critical impact thresholds are
  re-derived by global bisection and agree with its closed forms to ≤ 6.8e-11;
  traced rays at ±1e-4 cross the joins as expected. Astra's hard-coded radii
  were correct to their 9 digits. `geodesic_cross_validation` and
  `zamo_shadow_validation` come across unchanged as `standalone_*` tests
  (own metric copy, no production code; GCC only). Re-pointing their
  shadow and Carter-drift checks at the tracer moves to Phase C.
- Step 8 (`c7f53d5`, `cefd1cc`, recorder commit): GPU metric kernel ported
  unchanged (max metric error 1.74e-5, FP32). GPU ray kernel now compared with
  the exact turning radius: errors 2.4e-6–5.3e-5, core classification
  correct, null drift up to 2.8e-4; three GPU minima fall below the exact
  root (FP32 step error). The old ledger's "≤ 2.4e-6" was the best ray only.
- Recorder decoupled from physics: it takes a `FrameSource` callback and
  rejects mis-sized frames; `job.json` stores caller scene metadata. Test
  uses the synthetic pattern as before (64×64, 30 frames). Plugging in a real
  renderer is Phase F.
- **Not ported, by decision:** astra's fixed-step integrator and its CPU
  OpenMP renderer (no remaining test needs them; p2's tracer replaces them),
  and `gravastar_vulkan_render` with `gravastar_raymarch.comp` (it paints a
  fixed cyan colour on core rays, an artistic marker). Both are rebuilt in
  Phase D on the p2 contracts with a real emission model. They stay on
  `backup/astra-work`.
- **Recorder gaps found, for Phase F:** the video is renamed into place
  before ffprobe runs; ffprobe output is never compared with the expected
  size/rate/count; FFmpeg gets an integer frame rate (30000/1001 would be
  wrong); `initialize()` overwrites job.json on resume without checking the
  settings match; any non-empty frame file is trusted on resume; no hashes
  or job lock. The "fault injection" in the test is a clean cancel only.

1. Create `integration/reconcile` from `codex/p2-reference-optics`. Leave the
   existing branches untouched. Use build dir `build/reconcile-*` so the old
   build tree is not disturbed.
2. Fix the policy checker (`tools/project.cpp`): skip `.kilo` and
   `.reference`; allow `.comp`/`.glsl` only under `shaders/`; skip
   `third_party/` (vendored code is tracked in THIRD_PARTY.md instead). Add a
   self-test case for each rule. Gate: 24/24.
3. License back to GPL-3.0-or-later: LICENSE text from this branch, NOTICE,
   the 50 SPDX headers, docs/LICENSING.md, THIRD_PARTY.md. Keep
   nlohmann/json's MIT notice. One commit, nothing else in it.
4. Bring across new-only files in this order, building and testing after each:
   a. `third_party/include` (Vulkan headers) + `vulkan_device_audit`.
   b. `include|src/rendering/vulkan`, `shaders/` + metric/ray GPU tests.
   c. `third_party/glfw`, `platform/Window`, `WindowRenderer`, studio smoke.
   d. `tools/process.hpp`, `artifacts.hpp`, `testing.hpp`: check for overlap
      with p2's own `tools` helpers (p2 already has SHA-256 and atomic writes)
      and keep one copy.
   e. `recording/` + `deterministic_recording_test`.
   Do not bring `python/` or `tools/*.py` back; p2 replaced them with C++.
5. Hand-merge CMakeLists.txt: keep p2's targets and labels, add the Vulkan
   and desktop targets behind an option so `headless-*` stays graphics-free.
6. **[MAX]** Resolve the physics conflicts: `geometry.hpp/.cpp`,
   `experiments/benchmark.cpp`, `validate.cpp`, `tests/cpp_geometry.cpp`,
   `results/rotating_v1.json`. Default to p2's versions; diff the astra side
   line by line for any check it has that p2 lacks, and port that as an extra
   test. Regenerate `results/*.json` from a real run, never by hand-merge.
7. **[MAX]** Port astra-only physics tests as cross-checks against the p2
   tracer: ZAMO tetrad/shadow suite, critical impact solver, quad-precision
   geodesic cross-validation, equatorial turning points. If a ported test
   disagrees with p2, stop and record the disagreement; do not tune either
   side until the cause is known.
8. **[MAX]** Re-point the GPU ray test and CPU renderer at the p2 tracer API;
   the old fixed-step RK4 stays only as a named legacy reference if any test
   still needs it.
9. Gate: GCC and Clang release suites, debug suite and sanitizer preset all
   pass; record the counts and times in docs/STATUS.md.

### Phase B: truthful documentation

1. Reset PLAN.md progress to match the evidence (done in this commit for the
   current branch; redo on the merged branch).
2. Rewrite docs/VALIDATION.md from measured runs only. Label the cyan core
   colour as an artistic marker. Call the old stepper "heuristic step shrink",
   not adaptive. State FP32 GPU tolerances as FP32.
3. Fix README test counts and the "16/16" vs "15/15" mismatch.
4. Mark the 9th-grade guides whose source files change in Phase A as
   "describes the pre-merge code" until they are refreshed (Phase G).

**Phase B done (2026-11-24) on `integration/reconcile`:**
1. PLAN.md already marked P2 to P6 as not done (inherited from the p2
   branch). Added a short note of what exists under P3, P4 and P5.
2. The astra `docs/VALIDATION.md` was not brought across, nor were the cyan
   core renderer or the fixed-step stepper it described. The measured GPU
   numbers live in `docs/RECONCILE_STATUS.md`, labelled FP32. No new
   VALIDATION.md was written, to avoid a second copy of the same numbers.
3. README.md had no test counts on this branch. It now describes the
   optional desktop build and its limits. README.txt pointed at a Python
   script that no longer exists (`tools/validate.py`) and asked for Python;
   both fixed, and its Phase 3 status now says "started".
4. The 9th-grade guides were not brought across, so there is nothing to
   mark. They will be written against the merged code in Phase G.
Also: `docs/ARCHITECTURE.md` lists the desktop and recording modules, and
`docs/handoff.md` starts with the current state; the old P2 handoff stays
below it as history.

### Phase C: finish P2 reference optics

1. **[MAX]** Broaden ray families: hidden extrema, near tangencies at both joins,
   prograde rays near the buried photon sphere, pole and core crossings in
   one ray.
   **Done 2026-11-24** (`ray_families` CTest, 456 checks; details and
   numbers in docs/EVENT_COVERAGE.md). New independent off-equatorial
   reference from the closed-form Hamiltonian. Eight rays crossing one or
   two poles and the core, including the Cartesian patch, match it to 8.7e-10
   at crossings and 7.5e-9 at escape, at four step settings. Equatorial
   grazes of both joins are classified correctly down to depth 5e-11 (fine)
   and 5e-9 (coarse); the limit is accumulated position error, not the event
   search. The Kerr prograde photon orbit (r=1.629) is inside the shell; the
   toy model has none there, and its rays at b_ph(1+-1e-6) escape smoothly
   while the Kerr rays wind or fall in. Found: grazes below resolution are
   misclassified with no flag (feeds step 2). Not covered: off-equatorial
   grazes, core-patch grazes, focal-disk rays against this reference.
2. **[MAX]** Replace budget exhaustion with explicit outcomes. Gate: every pixel of
   the reference scene ends in a named outcome, with censored rays counted.
   Also flag near-tangent crossings whose classification is below the
   measured resolution (step 1 finding), instead of guessing silently.
3. **[MAX]** GYOTO: try the CMake-only build of the pinned 2.0.2 subset as a separate
   optional GPL tool. Compare Kerr rays first, then the gravastar adapter.
   If it cannot be built, record that and leave the GYOTO gate open.
4. **[MAX]** Thin-disk and star-background emission with redshift, linear radiance
   only. Image convergence study in step tolerance and samples.

### Phase D: redo P3 GPU optics on the P2 contracts

1. **[MAX]** FP64 vs FP32 measurement on the RTX 3050 (throughput and error); choose
   precision from data.
2. **[MAX]** GPU kernels follow the same charts and outcomes as the CPU tracer, or flag
   the rays they cannot handle so the CPU can re-trace them.
3. Remove the painted core colour; use the Phase C emission model.
4. Gate: per-pixel CPU/GPU error distribution and failed-ray counts on the
   reference scene; tiled dispatch with a work budget.

### Phase E: P4 desktop studio

1. Vendor and pin Dear ImGui and ImPlot (MIT), record them in THIRD_PARTY.md.
2. Show the real GPU render, with progressive accumulation that resets on
   change.
3. Orbit camera with stable poles, reset, bookmarks.
4. Panels from PLAN.md: scene presets, emission, scientific status, pixel
   inspection (ray path, charts crossed, events), recording.
5. Gate: smoke test covering resize, minimise, quality change, long job
   cancel.

### Phase F: P5 recording on the real renderer

1. Lossless PNG frames (pick a small zlib-licensed encoder; record it), plus
   optional float radiance output.
2. **[MAX]** Progress with per-frame hashes, job lock, scene/settings hash check on
   resume, crash-window reconciliation, corrupt/missing frame detection.
3. Low-cost 600-frame fault-injection run.
4. The real acceptance run: **1920x1080, 60 fps, 600 frames**, published
   sample budget, interrupted and resumed, selected frames compared with an
   uninterrupted render, ffprobe verified. Record wall time and storage.

### Phase G: P6 release

1. Fresh-clone build and test from the README alone.
2. CPack package, dependency notices, limitations page, troubleshooting.
3. Refresh the 9th-grade guides against the merged code.

### Rough size and order

A is the blocker for everything. B can run alongside the end of A. C must
finish before D's image comparisons mean anything. E can start once A's
desktop target builds, but its scientific panels wait for C. F's real
acceptance run waits for D. G is last. No dates are promised; PLAN.md asks
for estimates only after measured runs.

### Later research (unchanged, not started)

R1 stability and R2 evolution stay as PLAN.md describes. Before any code, a
written formulation (matter model, perturbation equations, framework choice).
The app keeps showing stability as "not assessed".

## 4. Housekeeping found during the audit

- `results/*.ppm` binary dumps are tracked although `.gitignore` now ignores
  new ones; move them out of git or replace with PNG plus hashes.
- `python/__pycache__/` and `.reference/` (third-party code, not ours) sit in
  the tree; both are already ignored. Keep `.reference/` code out of the build.
- `main` is 78 commits behind; it should move only after Phase A passes.
