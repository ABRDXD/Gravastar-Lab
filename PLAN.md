# GravastarLab implementation plan

Status: **P0/P1 implemented; P2 bounded native reference optics in progress**
(2026-10-22). The earlier planning-only boundary was explicitly lifted by the
user. The subsequent C++20-only instruction supersedes all earlier language
choices. See `docs/STATUS.md` for measured acceptance and unresolved gates.
The P2 checkpoint and remaining full-domain/GYOTO gates are in `docs/P2_STATUS.md`.

All scientific references, tests, benchmarks, validation and development tools
must be C++20. CMake is limited to build/dependency configuration and CTest
registration. No other implementation or automation language may be introduced.
Future GPU stages must satisfy this same constraint. Historical shader-tool
proposals below do not authorize GLSL/HLSL source; a compatible C++20 GPU build
strategy remains unresolved and must be established before P3 implementation.

## Objective

Build an independent, extensible Linux desktop project at
`/home/abrsdxd/Projects/GravastarLab` for rotating gravastar research,
cinematic exploration, scientific inspection, and offline video production.
The first release covers a validated stationary model, interactive optics,
and deterministic recording. Stability and coupled spacetime–matter evolution
are separate research milestones, not claims of the first release.

## Physics foundation

- Reproduce the actual transition construction of the published
  [de Sitter-core/Kerr-exterior family](https://arxiv.org/abs/2309.00551/).
  Document coordinates, units, assumptions, parameters, and source equations.
- Establish a reproducible rapid-rotation benchmark before exposing interactive
  parameter controls. Check metric limits, derivatives, curvature, signature,
  matching, horizons, and a bounded tested parameter domain.
- Distinguish metric consistency, energy-condition diagnostics, and dynamical
  stability throughout the UI and reports. A stationary metric is not evidence
  of stable matter.
- Trace null geodesics through the exterior, transition, and interior. Include
  Schwarzschild, Kerr, and zero-rotation reference cases.
- Expose lensing, frequency shifts, photon paths, and ergoregions when the
  selected configuration and emission model support them.

## Architecture and dependencies

- C++20 and CMake; Vulkan compute, GLFW, Dear ImGui, and ImPlot for the desktop.
- Independently implemented high-precision C++20 calculations for validation.
- Separate geometry, integration, radiation, rendering, experiments, recording,
  and UI. Scientific modules must run headlessly.
- Replaceable interfaces for metric evaluation and derivatives, integrators,
  emission models, and stationary/time-dependent datasets.
- Version saved scenes, experiment metadata, and recording manifests.
- Use [GYOTO](https://gyoto.obspm.fr/) with a custom metric plugin for CPU
  reference rays; compare GPU results against those reference calculations.
- GPL-3.0-or-later for original code. Pin reused dependencies, preserve notices,
  and record source, revision, license, and modifications.
- Inspect existing KerrLab code for useful ideas and verified reuse while
  keeping GravastarLab independently buildable.
- Extend an existing numerical-relativity framework for later evolution work.

## Desktop and recording behavior

- Large cinematic viewport, orbit camera, reproducible presets, star background,
  and configurable disk/hotspot emission.
- Optional equation, sweep, convergence, ray inspection, and black-hole
  comparison panels.
- Keep scientific radiance separate from exposure, bloom, and tone mapping.
- Target RTX 3050 with adjustable resolution and progressive refinement;
  measure performance before making frame-rate claims.
- Offline export evaluates exact timeline timestamps, completes each frame's
  sample budget, then advances, independently of preview speed.
- Default video: **1920 × 1080, 60 fps**. Higher resolutions, still-image export,
  and explicit sample budgets must be supported.
- Persist numbered lossless frames, camera/parameter tracks, render settings,
  and progress. Support cancellation and restart/resume.
- Encode completed frames with FFmpeg; retain frames if encoding fails.

## Delivery sequence and acceptance gates

### 1. Rotating geometry benchmark

- Read the paper and implement its transition rather than substituting an
  unrelated rotating mass-profile model.
- Publish model documentation, reproducible parameters, and diagnostic output.
- Independently verify metric limits and matching, derivative convergence,
  curvature, Lorentzian signature, and horizon diagnostics.
- Record the tested parameter domain and all known coordinate limitations.

### 2. Optics and desktop

- Produce CPU reference rays/images, including full interior propagation.
- Check null constraints and stationary conserved quantities.
- Implement Vulkan optics and quantify CPU/GPU ray agreement and image
  convergence before describing the renderer as validated.
- Deliver cinematic controls, inspection tools, and scientific exports.

### 3. Recording

- Implement a deterministic timeline and resumable lossless frame sequence.
- Demonstrate a ten-second, 600-frame export at 60 fps, checking ordering,
  duration, interruption/resumption, and preview-rate independence.
- Encode video and verify that encoding failure preserves source frames.
- Record the resolution and sample budget actually used for each acceptance
  run; a reduced-cost smoke test does not establish full-HD render performance.

### 4. Stability research

- Select and document matter closure and perturbation equations before
  implementing or advertising stability results.

### 5. Full evolution

- Select an existing relativity framework and implement the chosen matter
  model there; export time-dependent datasets for desktop playback.
- Document when large runs require external compute.

## Implementation discipline

- Inspect local instructions, available tools, hardware, and dependency
  licenses before choosing concrete integrations.
- Keep completed work, measured checks, deferred work, and blockers explicit.
- Never label a placeholder renderer, mock reference calculation, sampled
  parameter scan, or stationary geometry as a completed scientific validation.
- Maintain reproducible commands and artifacts so another researcher can
  rebuild and repeat each accepted check.

## Session boundary and preliminary observations

Historical boundary: the original planning session changed Markdown only.
Implementation was subsequently authorized. The observations below describe
that earlier session; current implementation choices are in `docs/ENVIRONMENT.md`.

Preliminary read-only inspection found:

- Existing KerrLab trees at `../KerrLab-Restored` and `../KerrLab-Source-Raw`.
- CMake, GCC, Python, FFmpeg, GLSL compilation tools, Vulkan headers, and the
  Vulkan loader are present. Exact versions and build suitability need audit.
- `nvidia-smi` could not communicate with the NVIDIA driver in this session.
  RTX 3050 rendering and performance are therefore unverified.
- GLFW, Dear ImGui, ImPlot, and GYOTO availability has not been established.
- The earlier Python dependency investigation is superseded by C++20-only
  validation. Do not modify system packages for this project.
- KerrLab's provenance notes identify an imported browser renderer without an
  upstream license grant. Do not copy that renderer into this project.
- No KerrLab code, dependencies, or binary artifacts have been copied.

These are observations about this session, not permanent machine capabilities.
Repeat the relevant checks when implementation begins.

## Model specification to freeze before coding

Use [Ilijić and DeBenedictis, arXiv:2309.00551v2](https://arxiv.org/html/2309.00551v2)
as the pinned scientific reference. Start with its section III.I toy model:
equations (1), (6), and (21)–(22), including both transition functions.
The quoted example uses `M = 1`, `a = 7/8`, and `Lambda = 1/2`.
Recover and record shell radii and the inspected radial domain explicitly;
do not invent missing benchmark values and call them a reproduction.
The toy model does not establish a realistic atmosphere or stability.
The paper restricts its interior domain, so complete central ray continuation
requires a separately documented extension and chart decision.

Create a future `docs/MODEL.md` containing:

- A source-equation map, transition prescription, derivative order, endpoint
  conditions, and an explicit distinction between reproduced and new choices.
- `G = c = 1`, signature `(-,+,+,+)`, coordinate order, tensor index order,
  mass normalization, spin convention, and conversion to physical units.
- Dimensionless controls `a/M`, `Lambda M^2`, `r_in/M`, and `r_out/M`;
  declare whether physical mass changes units only or geometry parameters too.
- Metric and inverse, all first derivatives, derivatives needed for curvature,
  coordinate transformations, and the chosen Riemann sign convention.
- Interior extension, axis treatment, chart transitions, and excluded domains.
- Conditions for observer and emitter worldlines to remain timelike.

### Coordinate and interior decision gate

Begin with the paper's coordinates for formula reproduction. Before full optical
rendering, decide how rays cross axes and traverse the core without artificial
absorption or reflection. Derive transformations for this actual metric; a Kerr
coordinate transformation cannot be assumed to regularize the transition.

For a horizonless accepted configuration, the working chart must cover every
region advertised by the renderer. If multiple charts are needed, transform
positions, covariant momenta, and transported frames consistently. Reference
black-hole rays may stop at a documented exterior capture surface; crossing a
black-hole horizon requires an appropriate chart.

An unresolved central or axial boundary is an explicit scientific limitation
and blocks the claim of complete interior ray tracing. Avoid concealing it with
a black surface, an arbitrary bounce, or a visually chosen cutoff.

### Parameter-domain decision gate

The initial UI exposes fixed accepted presets. A later allowed control domain
must be produced by diagnostics, not guessed from a spin slider range.

For every candidate preset or parameter cell, record:

1. Finite inputs, positive mass, ordered shell radii, and transition smoothness.
2. Invertibility and Lorentzian inertia, with eigenvalue/conditioning margins.
3. Radial null-surface diagnostics and refined root searches, including near
   tangencies; distinguish these from a global event-horizon proof.
4. Curvature values and convergence near both joins and difficult chart regions.
5. Causal pathologies such as timelike azimuthal loops where applicable.
6. Observer existence, integration behavior, and error bounds.
7. Energy-condition findings, separately from mathematical acceptance.

Sampling establishes evidence on the sampled domain only. Use analytic bounds
or interval methods where feasible before claiming an entire continuous range.
Mark interpolated, extrapolated, and unchecked configurations accordingly.

## Proposed repository layout

The following is the overall target layout; P0/P1 and bounded native P2 optics
are implemented:

```text
GravastarLab/
  PLAN.md
  README.md
  LICENSE
  THIRD_PARTY.md
  CMakeLists.txt
  CMakePresets.json
  include/gravastar/
    math/ geometry/ integration/ radiation/ datasets/
    rendering/ experiments/ recording/ scene/
  src/
    geometry/ integration/ radiation/ datasets/
    rendering/vulkan/ experiments/ recording/ scene/ desktop/
  shaders/                    # Compute tracing and display passes
  plugins/gyoto/              # Adapter for the actual metric
  include/gravastar/reference/ # Independent C++20 scientific implementation
  tests/                      # Headless correctness and regression checks
  benchmarks/                 # Inputs and performance methodology
  scenes/                     # Versioned presets and animation examples
  schemas/                    # Scene, report, dataset, and job schemas
  tools/                      # Setup, validation, export, packaging helpers
  docs/                       # Physics, numerics, architecture, user guide
  third_party/licenses/       # Preserved dependency notices
  results/                    # Small reproducible reports and fixture manifests
```

Keep builds, local environments, downloaded sources, caches, full recordings,
and large datasets out of source control. Track small scientific fixtures with
origin and hashes; document how to regenerate large artifacts.

## Module contracts and dependency rules

| Module | Responsibilities | Required boundary |
| --- | --- | --- |
| Math | Tensors, precision types, transformations, numerical helpers | No GUI or graphics dependencies |
| Geometry | Metric, inverse, derivatives, domain and chart diagnostics | No renderer, disk, or camera policy |
| Integration | Hamiltonian evolution, adaptive stepping, events, transport | Consumes abstract geometry and tolerances |
| Radiation | Emissivity, absorption, frequency shifts, transfer | No exposure or bloom |
| Datasets | Analytic and sampled spacetime providers | Explicit coordinate, unit, derivative, and time coverage |
| Scene | Camera, immutable geometry selection, emitters, timeline | Versioned serializable state |
| Experiments | Sweeps, convergence runs, comparisons, reports | Runs without a window or Vulkan |
| Rendering | Sampling, GPU execution, accumulation, image buffers | Consumes scene snapshots; preserves radiance |
| Recording | Exact frame schedule, persistence, resume, encoding | Consumes a render-job interface, independent of UI |
| Desktop | Viewport, controls, inspection, job progress | Sends commands; does not own physics formulas |
| GYOTO plugin | Maps the geometry into GYOTO conventions | Explicit adapter and independent reference checks |

Design interfaces for metric evaluation and derivatives, integrator selection,
emission/absorption models, render backends, and analytic or sampled datasets.
Return structured domain and numerical errors instead of unchecked NaN values.
Keep parameter sets immutable during a ray batch; changes invalidate incompatible
accumulation and diagnostics.

Proposed command-line targets are `gravastar_benchmark`, `gravastar_trace`,
`gravastar_render`, `gravastar_experiment`, and `gravastar_studio`. Names may be
consolidated if a subcommand interface keeps the same headless capabilities.
C++20 tools orchestrate recording and experiments using the same documented
scene/job contracts as the desktop.

## Numerical and scientific work packages

### Geometry and curvature

- Implement reference geometry in C++ double precision without fast-math.
- Independently derive reference C++20 expressions; do not merely reuse the production evaluator
  and describe that as an independent check.
- Choose analytic expressions or automatic differentiation for production first
  derivatives; use convergent numerical differences as a cross-check.
- Check metric symmetry, inverse identity, determinant, eigenvalue inertia, and
  coordinate transformation consistency.
- Test matching from both sides of each join through the prescribed derivative
  order; check induced metric and extrinsic curvature where appropriate.
- Verify curvature in exact reference regions, including de Sitter Ricci scalar
  and invariants, and vacuum exterior Ricci residuals.
- Evaluate inferred stress-energy with the cosmological term counted once.
  Use appropriate frames and tensor classification for energy-condition tests;
  do not equate coordinate diagonal entries with physical density and pressures.
- Report failed classifications and unresolved cases rather than forcing a
  perfect-fluid interpretation.

### Geodesics and ray events

- Use covariant momenta and `H = 0.5 g^{mu nu} p_mu p_nu` for null rays.
- Evolve the full coupled system through the shell. Do not assume a Kerr Carter
  constant or separability survives the transition construction.
- Select an embedded adaptive Runge–Kutta CPU reference method; freeze error
  norms, absolute/relative tolerances, minimum/maximum steps, and rejection rules.
- Make disk crossings, escape, capture, chart changes, optical-depth limits,
  and integration-budget exhaustion explicit events with refined locations.
- Preserve stationary energy and axial angular momentum within measured error;
  time-dependent datasets must not advertise these as exact invariants.
- Classify every ray: escaped, captured, emitted/absorbed, unresolved/trapped
  within budget, domain failure, or numerical failure. A budget limit is not
  physical absorption.
- Export trajectories, momenta, constraint history, region crossings, and event
  reasons for selected rays.

### Observers, emission, and image formation

- Construct and verify orthonormal camera frames, including supported moving
  observers and frame-dragging regions.
- Define the past-directed tracing convention and verify positive physical
  frequency using consistent future-directed photon/emitter comparisons.
- Begin with a transparent background and a documented phenomenological thin
  disk. Add hotspots, volumetric transfer, opacity, and returning radiation as
  separately specified extensions.
- Restrict orbital emitter prescriptions to their supported timelike domain;
  label prescribed motion separately from a self-consistent matter solution.
- For time-varying hotspots or illumination, distinguish frozen-scene snapshots
  from emission evaluated at retarded ray times.
- Store linear radiance or intensity and frequency-shift data independently of
  artistic star intensity, display exposure, bloom, and tone mapping.
- Match mass, angular momentum, observer, emitters, sampling, and display settings
  in gravastar/black-hole comparisons; disclose any necessary differences.

### GYOTO reference integration

Use the [GYOTO project and documentation](https://gyoto.obspm.fr/) as the
integration starting point. Audit its exact source, supported plugin API,
compiler ABI, and license before selecting a revision.

- Implement a real custom-metric adapter with correct units, derivative order,
  and coordinate conventions.
- Compare GYOTO vectors with independent high-precision and production C++20 results.
- Compare trajectories first, then disk events and frequency shifts, then images.
- Keep GYOTO integration and the native CPU tracer separately identified.
  Agreement between two adapters sharing the same metric code alone does not
  independently validate that metric.
- If the dependency cannot be built, keep native checks useful but leave GYOTO
  reference acceptance explicitly incomplete.

## Vulkan and desktop implementation plan

### GPU rollout

1. Audit device access, Vulkan features, queue families, memory budget, supported
   formats, and timestamp-query support. Record actual GPU and driver versions.
2. Build a small headless compute harness with validation layers when available.
3. Compare metric and derivative vectors before implementing image rendering.
4. Compare short rays, then difficult rays and shell crossings, against the CPU.
5. Implement tiled sampling, persistent accumulation, and explicit work budgets.
6. Add radiance images, scientific auxiliary buffers, and separate display passes.
7. Integrate presentation and record measured quality/performance tradeoffs.

Decide precision from error measurements and device capability. Do not assume
consumer FP64 throughput is sufficient or that FP32 meets reference tolerances.
Record shader compiler options; avoid unmeasured fast-math changes.

GPU errors, queue loss, allocation failure, and unsupported configurations must
produce actionable diagnostics. Smaller tiles and bounded dispatches should
keep the desktop responsive during long jobs. Provide a headless CPU path for
verification and offline rendering when GPU access is unavailable.

### Desktop layout and interaction

- Central viewport occupies most of the window; panels can be hidden for filming.
- Toolbar: preset, camera mode, preview quality, pause/refine, image export, record.
- Scene panel: accepted geometry preset and checked parameter controls.
- Emission panel: disk/hotspot parameters with model and approximation labels.
- Scientific panel: geometry status, domain limits, null errors, ray events,
  redshift, and separate energy-condition/stability status.
- Inspection panel: click a pixel to inspect its rays, chart, crossings, and
  convergence; overlays remain optional.
- Experiments panel: parameter sweep setup, plots, and matched comparisons.
- Recording panel: resolution, frame rate, duration, samples, output location,
  progress, cancellation, resume, and encoder status.

Orbit controls need stable poles, reset/bookmark actions, sensitivity settings,
and a reproducible camera representation. Avoid silently interpreting Euclidean
screen positions as physical distances in curved spacetime.

Use ImGui for controls and ImPlot for numerical plots. Prefer explicit preview
quality presets over a promised frame rate. Lower-resolution preview and
progressive refinement are expected on the RTX 3050; publish measurements only
after the actual hardware run.

## Scene, dataset, and experiment formats

Use versioned JSON metadata for portability. Freeze schemas before persisting
real user scenes. Include migrations or a clear incompatible-version error.

Scene metadata must contain geometry family/version, units, parameters, chart,
accepted-domain report identifier, camera/observer, emitters, background source,
integrator settings, sampling seed and budget, and display processing.

Experiment reports additionally need input hashes, software/source revision,
dependency versions, device/compiler, tolerances, metrics, failed cases, and
artifact locations. Save the exact tested configuration, not just a preset name.

For future sampled datasets, select an appropriate array container after choosing
the evolution framework. Require coordinate and gauge metadata, units, grid/time
coverage, interpolation order, derivative policy, boundary rules, and validity
masks. Validate with analytic synthetic data before importing research runs.

## Deterministic recording specification

### Timeline and frame evaluation

- Default `width = 1920`, `height = 1080`, `fps = 60`, `duration = 10 seconds`.
- Frame indices are `0` through `599`, with sample times `i/60`; the encoded
  duration is ten seconds. Do not add a duplicate endpoint frame at `t = 10`.
- Represent frame rate as a rational and derive timestamps from frame indices;
  never accumulate wall-clock deltas or depend on preview rendering speed.
- Version camera, geometry, emission, and display tracks. Define interpolation,
  quaternion handling, keyframe endpoints, and whether each parameter is
  physically variable or simply a sequence of stationary snapshots.
- For the first release, animated geometry is a sequence of stationary models,
  not a solution of time-dependent field equations. Clearly label that mode.
- Reset accumulation for each output frame, use reproducible sample seeds, and
  finish the specified sample budget before advancing.
- If motion blur is added, specify shutter interval and deterministic subframe
  samples; do not use an unspecified real-time approximation.

### Persisted job and frame sequence

Each job directory will contain:

```text
job.json                 # Frozen scene/tracks/settings, version, content hash
progress.json            # Atomic checkpoint and job state
frames/frame_000000.png   # Numbered lossless display frames
radiance/                # Optional lossless floating-point scientific output
logs/                    # Render diagnostics and encoding logs
video.mp4                # Created only by an encoding stage
```

Choose PNG and floating-point scientific output libraries during the dependency
audit. Define bit depth, color space, transfer function, alpha handling, and
metadata explicitly. Preserve linear radiance when scientific output is enabled.

### Resume, cancellation, and failure behavior

- Write frames to temporary files, flush/close, then atomically rename; update
  progress only after the final frame exists and passes integrity checks.
- Record hashes and completion metadata. Reconcile manifests with existing files
  on restart, including the crash window between frame rename and checkpoint.
- Verify settings and scene hashes before reusing a frame; reject incompatible
  resumes and let the user create a new job.
- Lock a job against concurrent writers; recover stale locks deliberately.
- Cancel at a documented bounded work boundary. Keep completed frames and label
  a partial current frame unfinished unless sample-level resume is implemented.
- Detect missing, corrupt, truncated, or extra-numbered frames before encoding.
- Handle full disk, permissions errors, process interruption, GPU loss, and
  encoder failure without deleting completed source frames.
- Define determinism separately for identical builds/backends and cross-device
  numerical agreement. Do not promise bit-identical GPU results across drivers.

### Encoding and verification

- Invoke FFmpeg with an argument array, fixed input frame rate, explicit start
  number, codec/pixel format, color metadata, and a captured log.
- Encode only a complete validated sequence. Write a temporary video and rename
  it only after success and stream inspection.
- Verify with ffprobe: resolution, rational frame rate, decoded frame count,
  monotonic timestamps, and expected duration within a stated container tolerance.
- Keep frames after encoding failure and allow encoding to be retried separately.
- Estimate frame storage and render duration from a measured pilot run; warn
  from actual estimates before starting expensive jobs. Higher resolutions use
  explicit dimensions, tile sizes, and sample budgets.

## Verification matrix and evidence requirements

Numerical tolerances below must be fixed in benchmark configuration after pilot
convergence studies. Do not invent universal tolerances before measuring scale,
conditioning, precision, and accumulation error. Acceptance reports must state
absolute/relative norms, thresholds, measured maxima, and failing samples.

| Area | Required cases | Evidence and release gate |
| --- | --- | --- |
| Limits | Flat, Schwarzschild, Kerr exterior, de Sitter core, zero spin | Analytic/independent agreement with conventions stated |
| Matching | Both joins, multiple angles, both approach directions | Endpoint and derivative-order residuals; extrinsic-curvature check |
| Derivatives | Core, shell, joins, exterior | Independent derivatives and decreasing-error study |
| Curvature | Constant-curvature core, vacuum exterior, shell | Invariant/reference residuals and refinement plots |
| Domain | Rotating presets and candidate parameter cells | Signature/conditioning margins, refined root diagnostics, exclusions |
| Geodesics | Escape, shell crossing, interior paths, near critical rays | Constraint and invariant drift; tolerance and event convergence |
| Reference optics | Native CPU, independent C++20 fixtures, GYOTO | Trajectory, event, and redshift agreement with independent checks |
| GPU | Formula vectors and representative/difficult rays | Error distributions, failed-ray counts, CPU/GPU comparisons |
| Images | Fixed scene, increasing ray accuracy and samples | Linear-radiance convergence plus separate display comparisons |
| Recording | Known timeline and real rendered sequence | Frame/timestamp/order/hash checks and successful resume |
| Desktop | Input, resize, minimize, quality changes, long jobs | Usable window, no stale accumulation, responsive cancellation |
| Distribution | Clean build and launch | Dependency notices, setup instructions, reproducible commands |

Recording acceptance requires two distinct runs:

1. A low-cost scheduling/fault-injection test of 600 frames that exercises
   interruption, corrupt/missing frames, resume, and encoding failure.
2. A real ten-second **1920 × 1080, 60 fps, 600-frame** render using a published
   sample budget. Interrupt and resume it, verify the final sequence/video, and
   compare selected frame outputs with uninterrupted rendering.

Vary preview speed or disable the window during the same deterministic frame
schedule to demonstrate preview independence. A low-resolution or synthetic
600-frame test cannot substitute for the full-resolution acceptance run.

## Build, dependencies, licensing, and distribution

- Build with CMake presets for headless debug/release, sanitizers, Vulkan
  validation, and desktop release. Keep scientific validation available without
  any graphics dependencies.
- Pin exact revisions or immutable archive hashes for compiled third-party code.
  Verify the actual license files and preserve notices before reuse.
- Audit C++ scientific dependencies, GLFW, ImGui, ImPlot, GYOTO, Vulkan
  headers/loader, shader compiler, image libraries, and FFmpeg separately.
- Record whether each dependency is linked, bundled, a build tool, or an external
  executable. Check compatibility with GPL-3.0-or-later and bundled codec options.
- Use a project-local C++20 setup process; avoid global package-manager changes.
- Inspect KerrLab components and their provenance individually. Reimplement or
  obtain permission where license grants are absent. Keep a source/revision/file
  inventory for any lawful reuse; do not inherit historical validation claims.
- First distribute a documented Linux build/run workflow, then choose a portable
  package after driver and runtime behavior are tested. Never bundle GPU drivers.
- Include CLI help, a quick-start scene, a scientific limitations page, report
  examples, dependency notices, and a troubleshooting guide.

## Later research milestones

### Stability research: separate scientific project

Before implementation, define a matter model and equation of state/closure,
background compatibility, perturbation variables and gauge, shell/core matching,
boundary conditions, and the intended notion of stability.

Choose which questions can be answered by test-field modes, linear coupled
perturbations, or nonlinear evolutions. A test scalar calculation alone is not
a proof of matter–gravity stability. Study rapid-rotation and ergoregion issues
only with the relevant equations and validity limits stated.

Deliver a reviewed formulation, reference solutions, convergence evidence,
parameter-dependent growth/decay measurements, and explicit unresolved modes
before adding any stability status beyond "not assessed" to the application.

### Full evolution: existing framework integration

Select an existing numerical-relativity framework based on matter support,
rotation, gauge/formulation, constraint handling, mesh refinement, licensing,
available expertise, and reproducible deployment. Do not preselect a framework
without checking those requirements.

Develop constraint-consistent initial data and implement the selected closure.
Validate simple matter/gravity limits, constraint convergence, boundaries, and
resolution studies before evolving rotating candidates. Define restart/checkpoint
and dataset export behavior. Separate large external-compute workflows from
desktop visualization and post-processing.

## Delivery gates and work order

| Stage | Work to deliver | Gate before proceeding |
| --- | --- | --- |
| P0: specification | Environment/provenance audit, source equation map, chart/interior decision, dependency pins | Scientific domain and build strategy documented |
| P1: geometry | C++20 metric, independent C++20 checks, benchmark report and accepted presets | Limits, derivatives, matching, curvature, and domain checks pass |
| P2: reference optics | Observers, native adaptive tracer, GYOTO plugin, emission, reference images | Complete advertised ray domain and reference convergence accepted |
| P3: GPU optics | Vulkan harness, derivative/ray agreement, radiance output | Measured accuracy and resource behavior accepted |
| P4: desktop | GLFW/ImGui/ImPlot studio, controls, inspection and exports | Scientific labels, rendering behavior, and desktop smoke checks pass |
| P5: recording | Frozen timeline, lossless sequence, resume, FFmpeg integration | Full 600-frame recording acceptance passes |
| P6: release | Reproducible setup, dependency notices, documentation, packaging | Clean build and intended-machine acceptance |
| R1: stability | Matter closure and perturbation formulation, then validated calculations | Research results support each claimed stability statement |
| R2: evolution | Existing-framework matter integration and dataset playback | Constraint/convergence evidence and export validation |

Headless schema and timeline design can begin after P0. Production recording
acceptance waits for an accepted renderer. UI sketches can proceed early, but
scientific controls and claims remain gated by the corresponding physics stage.
Research stages do not block the stationary first release.

Do not promise completion dates or interactive frame rates before measuring the
benchmark and a representative render on the intended machine. Track effort per
work package and revise estimates after P1, P2, and the first GPU timing run.

## Main risks and planned responses

| Risk | Planned response |
| --- | --- |
| Paper's local core domain does not specify complete ray continuation | Resolve extension and chart explicitly before full optics acceptance |
| Transition choices introduce horizons or invalid regions | Restrict presets to diagnosed configurations; expose failures in reports |
| Toy atmosphere violates desired energy conditions | Report it; pursue alternate transitions as a separate model version |
| Metric complexity causes derivative or curvature mistakes | Independent derivation, limit identities, and convergence checks |
| Near-critical rays fail to converge within GPU budgets | Preserve unresolved classifications and offer reference re-tracing |
| GYOTO plugin/ABI integration is difficult | Keep native work useful and the missing reference gate visible |
| RTX 3050 driver is inaccessible or memory is limited | Headless CPU validation, bounded tiling, and later hardware verification |
| Recording consumes excessive time/storage | Measured pilot estimate, explicit budgets, resumable outputs |
| A scene animation is confused with physical evolution | Explicit stationary-snapshot mode and separate evolution datasets |
| Unlicensed KerrLab imports contaminate distribution | Inspect grants per file/dependency and keep uncertain code out |

## Outstanding decisions for the implementation session

Resolve through evidence and document the choice; do not block planning on
questions whose answers can be found in the references or local environment.

- Exact published shell benchmark radii and the central extension to use.
- Working charts for axes, core traversal, and black-hole reference capture.
- Production derivative method and independent high-precision validation stack.
- Exact dependency pins, GYOTO API/revision, and offline setup method.
- Render precision, image libraries, floating-point export format, and GPU memory
  budget after feature and performance measurements.
- Error thresholds, accepted parameter presets, and the first bounded domain.
- Default disk/hotspot physical conventions and artistic presentation settings.
- Linux package format after a clean desktop launch test.

## Next-session starting instructions

1. Read this plan and any applicable local `AGENTS.md` before changing files.
2. Implementation is authorized. Preserve the C++20-only architectural constraint.
3. When authorized, begin P0: audit the environment, inspect licenses, and write
   the precise model/chart specification. Do not start with cinematic effects.
4. Implement the smallest reproducible geometry benchmark and independent
   checks, then publish its measured results and unresolved domain questions.
5. Update this checklist and create a concise status/handoff file after each
   accepted stage. Record commands, actual outcomes, limitations, and next work.

## Progress

- [x] Create the Markdown plan before implementation.
- [x] Record the planning-only session boundary.
- [x] Expand architecture, work packages, acceptance, risks, and handoff details.
- [x] Complete P0 environment, provenance, model, and bounded P1 chart decisions;
      full ray-domain extension remains a documented P2 gate.
- [x] Implement and validate P1 rotating geometry benchmark for the fixed
      project-selected preset; independent audit remains requested.
- [ ] Implement and validate P2 reference optics and GYOTO integration.
      Native observers, adaptive tracing, emission and bounded images are
      implemented. Regular angular geometry, independent derivative/curvature
      verification and kernel benchmarks are complete; ray chart transitions,
      central continuation and GYOTO comparisons remain incomplete.
- [ ] Implement and compare P3 Vulkan optics.
- [ ] Implement and verify P4 desktop studio.
- [ ] Implement and verify P5 deterministic recording and encoding.
- [ ] Complete P6 documentation and Linux release acceptance.
- [ ] Define and carry out R1 stability research.
- [ ] Define and carry out R2 full evolution work.
