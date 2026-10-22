# P2 checkpoint: bounded native reference optics

P2 is in progress on `codex/p2-reference-optics`, based on the frozen P1
commit `609debe`. This checkpoint implements a C++20 observer frame, adaptive
Hamiltonian null tracer, refined surface events, prescribed disk emission,
and a headless ray/image command. The contract is in `OPTICS.md`.

The completed GCC release run passed all 12 CTest targets, including 49
analytic/reference optics checks and 27 CLI/image checks. The 16x12 toy image
has 87 emitted, 101 escaped and 4 explicitly unresolved chart-boundary pixels.
The comparable Kerr image has 76 emitted, 8 escaped and 108 capture-surface
pixels. All pixel classifications agree when integration tolerances and the
maximum step are tightened. Generated acceptance reports and independent
compiler/sanitizer runs are pending at this checkpoint.

A failed event-refinement substep now rejects and shortens the entire trial
step; a regression ray covers the pixel that exposed this issue. Additional
failure-isolation hardening and final reproducibility artifacts are underway.

Complete P2 acceptance remains open: central and axial chart continuation,
real GYOTO integration/comparison, spatial image convergence, and broader
near-critical/grazing-ray checks are not established. The tracer does not
replace an unresolved boundary with artificial absorption or reflection.

Use the headless-release CMake build and CTest presets. Preserve this branch
separately from concurrent work on `main`; checkpoints are reviewable progress,
not a claim that the complete P2 gate has passed.
