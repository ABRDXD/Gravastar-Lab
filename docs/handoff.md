# Handoff: P2 sampling continuation

Resumed by the user on 2026-10-23. The sampling arithmetic is now integrated
into CTest and passes exact smooth-image, discontinuity, missing-data and
malformed-input checks. Empty comparison masks report unavailable errors,
not zero. The experiment driver and actual spatial refinement runs are next.
The earlier stop notes below describe the starting checkpoint only.

Date: 2026-10-22. Active isolated worktree:
`/home/abrsdxd/Projects/GravastarLab-Cpp20`, branch
`codex/p2-reference-optics`. The user requested stopping and committing all
work. Do not continue implementation until asked. No background task remains.

All authored code and tools must be C++20; CMake is for build/dependency/CTest
configuration only. Preserve the original GravastarLab checkout and concurrent
Argon work. Commit and push checkpoints approximately every three minutes when
work resumes. Main remains at the preserved P1 commit `609debe`.

## Completed this continuation

- Added `gravastar_trace image --resolution-scale N`, integer 1..8 with a
  maximum effective image size of 128x128. It retains the original camera and
  physical scene, records effective dimensions, and rejects scaling in ray mode.
- Added six CLI rejection checks for malformed scales and unsupported sizes.
  Rebuilt Release successfully; `optics_cli` passed all 37 checks in 22.02 s.
  This includes the existing toy/Kerr integration-refinement image checks.
- Pushed the tested change as `ed2a9e8`. The final stop checkpoint also preserves
  the unfinished draft described below. All commits stay on the P2 branch.

## Draft and next implementation

`include/gravastar/optics/sampling.hpp` is not integrated into CMake or any
consumer. It has only been syntax-checked, not numerically tested. It defines
sample aggregation over uniform image-plane area, keeps intensity/alpha absent
when any sub-sample is unresolved, and compares levels using a common resolved
mask. No spatial convergence result exists yet.

Next, validate this arithmetic with independent manufactured smooth and
discontinuous images, unresolved masks, zero radiance and malformed inputs.
Build a native C++20 experiment that invokes the existing tracer on increasing
grids (for example 16x12, 32x24, 64x48) with fixed camera and integrator, aggregates
each grid onto the original pixels, and reports intensity differences,
classification fractions and unresolved coverage. Test a positive scaled render;
current new CLI tests cover rejection paths only. Avoid claiming convergence
from shrinking masks or replacing unresolved rays with zero radiance. Preserve
raw report hashes and computational source provenance. Select acceptance
criteria before interpreting the experiment, and report failures honestly.

The earlier milestone at `d5480e5` passed GCC/Clang 12/12 tests and Debug plus
ASan/UBSan optics 3/3 each. Those complete matrices were not rerun for this small
continuation. See `P2_STATUS.md` and retained transcripts for their exact scope.
Full P2 still needs central/axial continuation, genuine GYOTO comparisons,
spatial/sample convergence, and broader difficult event checks.

## Reproduction and publication

```text
cmake --build --preset headless-release --parallel 3
ctest --preset headless-release -R optics_cli --output-on-failure
build/headless-release/gravastar_tools manifest --verify results/p2_sampling_checkpoint_manifest.json
```

The checkpoint manifest describes the stopped tree, including the draft.
Earlier scientific reports and source manifests were intentionally preserved.
SSH pushes work with the direct command option
`-c core.sshCommand='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=10'`.
