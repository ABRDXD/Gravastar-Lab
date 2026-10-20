# Gravastar-Lab

> *VIbeCoded btw so dont actually try to copy this use anything else*  
> *This project contains general relativity, Vulkan compute shaders, numerical integration, and several thousand lines of C++. I barely understand half of it. Proceed at your own risk.*


Gravastar-Lab (≧▽≦)ノ
VibeCoded btw so don't actually go and try to copy this, use something else.

A rotating gravastar simulator in C++20 (someone thought making windows was a good way to get into programming!) Scientific accuracy is being attempted rather than assured. (￣▽￣;)

What the hell is this?
Gravastar-Lab is an experimental computational astrophysics project looking at rotating gravastars. These are hypothetical objects which might appear very much like black holes but which may well not have event horizons at all.

The proposed geometry has a de Sitter core, a transition shell and a Kerr exterior. We'll calculate it, look at the curvature and eventually want to ray trace through it. The end game is of course a Vulkan renderer, a decent desktop app and some videos which will make the GPU want to go back to the drawing board.

This all started with a very simple desire to render a spinning space object. General relativity had other ideas though and immediately produced us four dimensional tensors and a whole host of opportunities to get an index wrong. (╥﹏╥)
Why am I doing this? (・_・ヾ
I like astrophysics, C++, graphics programming and just about any project that's a bit over the top. C++ was my first language. Rather than do the standard sort of exercises I just went straight to making windows. That was educational in pretty much the same way that falling down the stairs is educational in terms of gravity.

My maths is at trig and introductory calculus at the moment. I'm learning the rest of it as I go along with the development. I'm not in any way claiming to have derived all the equations myself or to have any real understanding of the implementation details.

AI coding assistants have made a big contribution. The suggestions they make can be useful, can be impressive and can be absolutely and completely wrong. Every meaningful scientific claim still requires evidence beyond just an enthusiastic chat response.

One rule above all others: the program must be perfectly honest about what it has actually shown. A passing unit test is a passing unit test, not a peer reviewed paper. A nice render is a nice render, not evidence of physical stability. An AI claiming something is mathematically sound doesn't make it so. I want this repo to be a record of our mistakes, a repository of failed experiments and a slow accumulation of things that we can actually trust. Preferably before my laptop hits retirement age. (；ω；)
What actually exists? (｀・ω・´)
The current repository contains a headless C++20 geometry and validation foundation, including:
- Rotating metric, analytical inverse expressions.
- Bivariate Taylor jets for automatic differentiation.
- Metric derivatives, Christoffel symbols and curvature.
Riemann and Ricci tensors, Ricci scalar, and Kretschmann scalar.
- Coordinate domain checks and numerical failure handling.
- CMake presets, unit tests, headless benchmarks and Python validation utilities.
These are components not certificates of physical correctness. The fact that the computer has computed a curvature tensor is proof that the computer has computed something. Whether that something is a correct description of the spacetime in question is a separate issue which will require independent verification.

The graphical renderer, the interactive desktop and the cinematic export system are all things we're looking at for the future. Don't confuse a roadmap with a release. (¬_¬)
Roadmap: six increasingly questionable decisions
1. Stop the maths lying
Firstly reproduce the published rotating metric and document its coordinate system, its assumptions and the definitions of its parameters. Check the flat, Schwarzschild, Kerr and de Sitter limits. Check the inverse metric identities, the accuracy of the derivatives, the curvature invariants, the shell matching and the appropriate numerical convergence.

Be very careful with the map parameter ranges. Look at the potential horizons, the coordinate singularities, the metric signature and any pathological regions. Work out the effective stress energy tensor implied by the geometry and check the energy conditions.

A smooth interpolation doesn't necessarily describe physically reasonable matter. A stationary model in no way implies dynamical stability. Einstein is rather fussy on this point. (ಠ_ಠ)
2. Start firing photons at it
Develop null geodesic integration, observer models, ray trajectories, escape and capture diagnostics, frequency shifts and gravitational lensing. Rays should only cross the shell and interior if the particular mathematical model and coordinate coverage we are using actually allows for that.

Use analytical benchmarks and independent CPU references. Watch the conserved quantities and numerical constraints. A beautiful image of a wrong photon path is still a wrong path even if you've turned the bloom all the way up to eleven.
3. Introduce the RTX 3050 to suffering
Build Vulkan compute acceleration for ray tracing and image accumulation. The main dev machine is an RTX 3050 laptop GPU with 6GB of VRAM rather than a research supercomputer hidden in a schoolbag.

Use adjustable resolution, progressive refinement, sensible workgroup configurations and memory budgets. Benchmark your GPU kernels properly. Compare GPU and CPU calculations before you can even claim to have succeeded.

A comment saying "// optimised" is not a benchmark. The compiler doesn't respond to motivational speaking. (；￣Д￣)
4. Make a real desktop application.
The intended interface will have an interactive viewport with camera controls, scientifically labelled parameter presets, ray inspection, plots, experiment configuration and diagnostic panels.

The stack we're planning on is GLFW, Dear ImGui, ImPlot and Vulkan. The geom lib has to be testable on its own without a window. Our compositor shouldn't have any say over spacetime.

Keep the scientific measurements separate from the artistic presentation. Any change to exposure or bloom must never silently change the underlying physics.
5. Make cinematic nonsense in a way that you can repeat.
Support camera keyframes, deterministic timelines, configurable sample budgets, high res stills, lossless frame sequences, resume and FFmpeg encoding.

The initial export target is 1920 by 1080 at 60fps. That's the video format, not a commitment to real time ray tracing. The laptop has constraints, unlike my planning docs. (´；ω；`)

Every exported scene must record its parameters, its camera state, its numerical tolerances and its render settings. Scientific reproducibility is important even if the end result is essentially just there to look utterly ridiculous.
6. Try real gravastar research.
Long term goals would be to model the shell matter, look at the perturbations, look at the stability and potentially use some of the existing numerical relativity codes to do the full dynamical evolution.

These are research ambitions rather than features that have been implemented. Before I'd be in any position to claim a stable rotating gravastar I'd need a specific matter model, appropriate equations, pretty convincing numerical work and probably a lot more learning.

A program running for ten minutes without crashing is not evidence that the universe permits the object. (눈_눈)
Repository map (・ω・)ノ
GravastarLab/
├── include/       C++ mathematical interfaces
├── src/           Geometry and experiments
├── python/        Independent reference calculations
├── tests/         Regression and unit tests
├── benchmarks/    Reproducible numerical configurations
├── docs/          Physics and architecture notes
├── tools/         Validation scripts
├── results/       Small experimental outputs
├── PLAN.md
├── CMakeLists.txt
├── CMakePresets.json
├── NOTICE
├── THIRD_PARTY.md
└── LICENSE
The detailed plan is in [PLAN.md](PLAN.md). That document has all the technical decisions and the open issues, minus most of the emotional damage.
Build instructions (ง •̀_•́)ง
You need a C++20 compiler, CMake 3.25 or later, Ninja and Python 3.10 or later. GCC 13 or Clang 16 are reasonable starting points.
git clone https://github.com/ABRDXD/Gravastar-Lab.git
cd Gravastar-Lab
cmake --preset headless-release
cmake --build --preset headless-release
ctest --preset headless-release
python3 tools/validate.py --preset headless-release
There are also debug and sanitizer presets. Sanitizers can catch certain memory errors. They can't tell you whether your stress-energy tensor violates an energy condition. Unfortunate omission by the compiler industry.

Build failures must be reported with the compiler version, the command used and the relevant errors. Physics failures need numerical parameters and a reproducible case.
Things this project does not prove
This software does NOT establish that gravastars exist, that any of the proposed configurations are stable, or that the light bending we see is in any way consistent with an astrophysical model.

Coords can become singular without spacetime itself being singular. Floating point calculations can lose precision. A numerical scan can't prove behaviour over the whole of a continuous domain. Agreement between two calculations is much stronger when the implementations are truly independent.

The geometry gives rise to an effective stress energy tensor, but mathematical consistency in itself does not establish a physically reasonable matter source. Don't be in the business of declaring the discovery of exotic matter on the basis of a single successful ctest run. (￣ー￣)
AI assistance and contributions
This project is very much AI assisted. I'm not in any way going to pretend that every function came to me via the sheer force of academic brilliance. That would be a rather embarrassing lie.

The intended workflow is straightforward: suggest changes, explain your assumptions, implement on a separate branch, run the tests, review the evidence and then merge with caution. Multiple models can review each other, but three "confident" answers don't add up to a proof.

Bug reports, mathematical corrections, reproducible experiments and performance benchmarks are all very much welcome. Please include source locations, expected results, observed results, numerical tolerances and references where possible.

Don't go and rewrite the entire engine because some AI didn't like a variable name. The tensors have had it bad enough. (╥_╥)
References and license
The scientific model is based on the work of S. Ilijić and A. DeBenedictis (arXiv:2309.00551). Model conventions, provenance, limitations and dependency acknowledgements can be found in docs/MODEL.md, docs/SOURCE_AUDIT.md, NOTICE and THIRD_PARTY.md respectively.

Original project code is licensed under GPL-3.0-or-later. All external components retain their own respective licences. Citing a researcher in no way implies that they endorse this project or my various decisions.
Final warning (ﾉಥ益ಥ)ﾉ
I wanted a spinning gravastar. I got tensor calculus, automatic differentiation, numerical analysis and a very good relationship with the debugger.

If it's wrong then report it. If it's faster then benchmark it. If it's of any scientific significance then independently verify it. If the simulator does in fact create a real gravitational singularity then please attach the logs prior to the planet vanishing.

VibeCoded btw so don't actually go trying to copy this. Use something else.

The original warning remains. Future archaeologists are going to need some context. (≧▽≦)ゞ



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
