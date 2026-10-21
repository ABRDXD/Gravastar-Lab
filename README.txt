================================================================================
Gravastar-Lab (≧▽≦)ノ
================================================================================

"VibeCoded btw so dont actually try to copy this use anything else."

This project contains general relativity, numerical methods, C++20, and several
opportunities to accidentally invent incorrect physics. (´；ω；`)

I understand some of it. The rest is under investigation.

"If your research depends on this repository being correct, please reconsider
your research methodology." (￣▽￣;)

--------------------------------------------------------------------------------
What the hell is this? (・_・ヾ
--------------------------------------------------------------------------------

Gravastar-Lab is an experimental C++20 computational astrophysics project for
studying rotating gravastars.

Gravastars are hypothetical compact objects that might resemble black holes
from the outside while having a fundamentally different interior.

Instead of an ordinary black-hole interior, the model uses:

- A de Sitter-like core.
- A smooth transition shell.
- A rotating Kerr-like exterior.
- A concerning quantity of tensor calculus. (╥﹏╥)

The goal is to develop a scientifically testable simulation with gravitational
lensing, ray tracing, numerical diagnostics, and eventually a beautiful
Vulkan-powered renderer.

I originally wanted to see a cool spinning space object.

Somehow, this now involves the Riemann curvature tensor.

I would like to speak to whoever authorized this escalation. (ಠ_ಠ)

--------------------------------------------------------------------------------
Why am I making this? (￣ω￣)
--------------------------------------------------------------------------------

Because:

1. Black holes and gravastars are fascinating.
2. I like C++, Vulkan, Linux, and making computers do unnecessarily
   complicated things.
3. I thought learning computational astrophysics through a massive
   programming project would be entertaining.
4. Apparently, normal hobbies were unavailable.

I started learning C++ by making windows rather than printing "Hello, World!".

C++ is still my first programming language.

I also haven't formally learned most of the mathematics involved here. My
current mathematical foundation is mainly trigonometry and introductory
calculus.

This is a learning and research project, not a claim that I personally
derived every equation.

AI coding assistants have contributed significantly. The goal is to understand
and verify their work, not blindly trust whatever mathematical monstrosity
they produce.

Sometimes they write beautiful code.

Sometimes they confidently hallucinate an entire branch of physics. (；￣Д￣)

--------------------------------------------------------------------------------
Current state (｀・ω・´)
--------------------------------------------------------------------------------

The project currently has a headless C++20 numerical-geometry foundation.

What exists:
- Rotating metric evaluation.
- Bivariate Taylor-jet automatic differentiation.
- First, second, and selected higher metric derivatives.
- Christoffel symbols.
- Riemann and Ricci tensors.
- Ricci scalar and Kretschmann scalar.
- Analytical inverse-metric expressions.
- Coordinate-domain and numerical guards.
- Headless geometry benchmarks.
- CMake and CTest infrastructure.
- Python reference and validation utilities.

Important distinction:

A function returning a curvature tensor does not mean the physics is correct.

It means the function returned some numbers.

Those numbers could describe a valid spacetime.

They could also describe complete mathematical nonsense with excellent
formatting. (＾▽＾)

Scientific validation is an ongoing process.

--------------------------------------------------------------------------------
Development roadmap (ง •̀_•́)ง
--------------------------------------------------------------------------------

Phase 1: Verify that spacetime actually works
Status: Foundation exists; validation and further diagnostics remain ongoing.

The first objective is to check the mathematical model against known analytical
results.

Planned work:
[ ] Reproduce and document the published rotating gravastar metric.
[ ] Verify Schwarzschild, Kerr, flat-space, and de Sitter limits.
[ ] Validate inverse metric identities.
[ ] Check first, second, and higher derivatives.
[ ] Test shell-boundary smoothness.
[ ] Verify curvature invariants.
[ ] Examine coordinate singularities.
[ ] Map acceptable parameter domains.
[ ] Calculate effective stress-energy.
[ ] Investigate energy conditions and possible horizons.

One cannot simply write "return true;" and expect Einstein to approve. (¬_¬)


Phase 2: Ray tracing
Status: Planned.

Once the geometry behaves, the next task is to throw light rays at it.

Scientifically, of course. (ﾉ◕ヮ◕)ﾉ

Planned features:
[ ] Null geodesic integration.
[ ] Photon trajectories.
[ ] Gravitational lensing.
[ ] Frequency-shift calculations.
[ ] Camera and observer models.
[ ] Propagation across the shell and interior.
[ ] Independent Schwarzschild and Kerr comparisons.
[ ] Numerical constraint monitoring.
[ ] CPU reference ray traces.

The photons will follow the equations.
Not my artistic preferences.
Unfortunately. (╥_╥)


Phase 3: Vulkan-powered GPU suffering
Status: Planned.

Primary development hardware:
NVIDIA GeForce RTX 3050 Laptop GPU, 6 GB VRAM.

It is not a supercomputer.
It is a laptop GPU that has somehow become involved in general relativity.

Planned features:
[ ] Vulkan compute ray tracing.
[ ] GPU-based numerical integration.
[ ] Progressive accumulation.
[ ] Adjustable resolution.
[ ] Ray-tracing quality presets.
[ ] GPU profiling.
[ ] CPU/GPU numerical comparisons.
[ ] Physically motivated emission models.
[ ] Scientific and cinematic render modes.

Optimization will be based on measured performance.
Adding "// EXTREMELY OPTIMIZED" above a function does not make it faster.
Believe me, I wish it did. (눈_눈)


Phase 4: Desktop application
Status: Planned.

I want an actual graphical application rather than a program that generates
thousands of CSV rows and expects me to experience happiness.

Planned features:
[ ] Interactive 3D viewport.
[ ] Orbit and free cameras.
[ ] Parameter controls.
[ ] Geometry inspection.
[ ] Photon trajectory visualization.
[ ] Convergence plots.
[ ] Scientific diagnostics.
[ ] Scene presets.
[ ] Experiment configuration.
[ ] Save and load functionality.

Planned technologies include C++20, Vulkan, GLFW, Dear ImGui, and ImPlot.

The interface should look good.
The mathematics should also be correct.
Apparently, both are expected. (￣▽￣;)


Phase 5: Cinematic rendering
Status: Planned.

Because eventually I want to create visually ridiculous videos of hypothetical
spacetime objects. (☆▽☆)

Planned features:
[ ] Camera animation and keyframes.
[ ] Deterministic timeline evaluation.
[ ] Progressive offline rendering.
[ ] High-resolution screenshots.
[ ] Configurable sample budgets.
[ ] Lossless image sequences.
[ ] Resumable rendering jobs.
[ ] FFmpeg encoding.
[ ] Reproducible scene metadata.

Initial export target: 1920 x 1080 at 60 FPS.

This describes the video output, not real-time rendering performance.
My RTX 3050 has six gigabytes of VRAM and a limited tolerance for my ambitions.
Please respect its sacrifice. (っ˘̩╭╮˘̩)っ


Phase 6: Actual astrophysics
Status: Long-term research.

This is where the mathematical difficulty becomes genuinely unpleasant.

Planned research directions:
[ ] Effective stress-energy tensors.
[ ] Shell matter modeling.
[ ] Energy-condition analysis.
[ ] Perturbation theory.
[ ] Stability investigations.
[ ] Time-dependent spacetime simulations.
[ ] Comparisons with established numerical-relativity frameworks.
[ ] Physically meaningful parameter studies.

A stationary geometry is not proof of stability.
A beautiful render is not evidence of a viable matter configuration.
And three AI models agreeing with each other does not constitute a
mathematical proof. (ಠ益ಠ)

--------------------------------------------------------------------------------
Repository structure (・ω・)ノ
--------------------------------------------------------------------------------

GravastarLab/
├── include/          C++ interfaces and mathematical types
├── src/              Geometry and experimental code
├── python/           Independent reference calculations
├── tests/            Tests designed to catch my mistakes
├── benchmarks/       Reproducible numerical experiments
├── docs/             Scientific specifications and explanations
├── tools/            Validation and development scripts
├── results/          Experimental outputs
├── CMakeLists.txt
├── CMakePresets.json
├── PLAN.md
├── THIRD_PARTY.md
├── NOTICE
└── LICENSE

The repository contains the current foundation. The larger rendering,
interactive desktop, and recording systems remain planned.

Read PLAN.md for the full technical roadmap.
It's considerably more serious than this README.
My condolences. (￣ー￣)

--------------------------------------------------------------------------------
Building it (ง'̀-'́)ง
--------------------------------------------------------------------------------

You'll need:
- A C++20 compiler, preferably GCC 13+ or Clang 16+.
- CMake 3.25 or newer.
- Ninja.
- Python 3.10 or newer for the headless validation workflow.
- A functioning computer.
- Patience.

Clone and build:
  git clone https://github.com/ABRDXD/Gravastar-Lab.git
  cd Gravastar-Lab

  cmake --preset headless-release
  cmake --build --preset headless-release

Run the tests:
  ctest --preset headless-release

Run extended validation:
  python3 tools/validate.py --preset headless-release

Debug build:
  cmake --preset headless-debug
  cmake --build --preset headless-debug

A sanitizer preset is also available for catching certain memory errors and
undefined behavior.

Unfortunately, AddressSanitizer cannot tell you whether your proposed gravastar
violates an energy condition.
Maybe in C++26. (´･ω･`)

--------------------------------------------------------------------------------
Scientific limitations (－‸ლ)
--------------------------------------------------------------------------------

This is experimental software.

Please do not confuse the presence of complicated equations with the presence
of scientific certainty.

Known limitations and important distinctions:
- The implemented model is stationary.
- Stationarity does not establish dynamical stability.
- The transition region has an effective stress-energy distribution that needs
  careful interpretation.
- Some coordinate singularities are properties of the chosen coordinates, not
  necessarily physical singularities.
- Numerical results depend on precision, step-size control, parameter choices,
  and convergence.
- Sampled parameter scans cannot establish properties over an entire continuous
  domain.
- Rendered images are visualizations, not independent physical evidence.

If you discover that a calculation contradicts established mathematics,
file an issue.

If you discover a new law of physics, verify it independently before naming it
after yourself.

If you discover a negative-mass pigeon, I want absolutely nothing to do with
it. (ಠ_ಠ)

--------------------------------------------------------------------------------
AI-assisted development (¬‿¬)
--------------------------------------------------------------------------------

Yes, this project is heavily AI-assisted.

No, I'm not pretending I personally wrote every line of this code using sheer
intellectual superiority.

That would be an impressive lie.

The project has involved coding assistants for architecture, implementation,
research, debugging, and code reviews.

The intended development workflow is:
1. Propose changes.
2. Explain the mathematics or engineering reason.
3. Implement on a separate branch.
4. Run relevant tests.
5. Review the results.
6. Merge only after verification.

I also want to experiment with multiple AI coding agents reviewing each other's
work.

Basically, several advanced reasoning models arguing over tensor calculus while
I supervise from an Arch Linux desktop. (≧▽≦)

This sounds like an incredibly efficient engineering strategy.
It also sounds like an excellent way to produce three independently confident
explanations of the same bug.

We'll find out.

"AI-assisted does not mean scientifically validated."

Please remember this before trusting anything important to the simulator.
(；・∀・)

--------------------------------------------------------------------------------
Contributions (づ｡◕‿‿◕｡)づ
--------------------------------------------------------------------------------

Bug reports, reproducible experiments, mathematical corrections, documentation
improvements, and performance benchmarks are welcome.

Especially mathematical corrections.

Please provide:
- The affected file or function.
- The problem.
- Steps to reproduce it.
- Expected and observed results.
- Numerical parameters and tolerances.
- Scientific references, where relevant.

Please don't rewrite the entire physics engine because you dislike a variable
name.

And please don't tell me to switch the whole project to Rust.
I already have enough compiler-related problems. (╥﹏╥)

--------------------------------------------------------------------------------
References and licensing (｀・ω・´)
--------------------------------------------------------------------------------

The scientific foundation is based on work by S. Ilijic and A. DeBenedictis:
Rotating gravastar research, arXiv:2309.00551 (https://arxiv.org/abs/2309.00551)

See docs/MODEL.md, docs/SOURCE_AUDIT.md, NOTICE, and THIRD_PARTY.md for model
assumptions, provenance information, dependency licensing, and scientific
limitations.

Original project code is licensed under GPL-3.0-or-later.
Third-party components retain their applicable licenses and attribution
requirements.

No endorsement by the cited researchers is implied.
Please don't make those people responsible for whatever I've done here.
(；￣Д￣)

--------------------------------------------------------------------------------
Final warning (ﾉಥ益ಥ)ﾉ
--------------------------------------------------------------------------------

This project started because I wanted to simulate a rotating gravastar.

It has since escalated into:
- General relativity.
- Differential geometry.
- Tensor calculus.
- Automatic differentiation.
- Numerical integration.
- C++20.
- Vulkan.
- GPU optimization.
- Scientific validation.
- An increasingly concerning number of Greek letters.

I'm still learning.

Some things work. Some things are unfinished. Some things might be wrong in
extremely sophisticated ways.

If you find a bug, report it.
If you improve the mathematics, document it.
If you make it run faster, provide benchmarks.
If this program somehow produces an actual gravitational singularity, please
open an issue with reproduction steps.

"And if my RTX 3050 achieves sentience, tell it I'm sorry." (´；ω；`)

--------------------------------------------------------------------------------
A message to future me (￣▽￣)ノ
--------------------------------------------------------------------------------

If you're reading this years later and you've actually become an astrophysicist:

Congratulations.

Please fix the code.

And don't remove the original README warning.
It deserves to remain in the archaeological record.

"VibeCoded btw so dont actually try to copy this use anything else."

*(≧▽≦)ゞ*
