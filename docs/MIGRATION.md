# C++20 migration evidence

The user's 2026-10-21 language policy supersedes the original Python plan.
All six Python modules were retained while their C++20 replacements were built
and verified. No Python program was executed after that instruction.

## Before removal

`cmake --build --preset headless-release --parallel 4` succeeded, followed by
`ctest --preset headless-release`: **8/8 tests passed** in 9.76 seconds, including
the explicit production shell-branch endpoint checks. The scientific run took
5.95 seconds. This is the migration checkpoint, before any Python deletion.
The checkpoint report and its source inventory are retained as
`results/migration_pre_removal.json` and
`results/migration_pre_removal_manifest.json`. Those inventories intentionally
identify the historical tree containing the Python modules.

| Retired module | Native replacement | Equivalent verification |
| --- | --- | --- |
| `python/reference.py` | `reference/number.hpp`, `reference/geometry.hpp` | Arbitrary integers, trig/sqrt identities, Gauss-Jordan inverse, derivatives, independent curvature and 51 comparison points |
| `python/polynomial.py` | `reference/polynomial.hpp` | Exact Sturm counts/isolation, repeated/tangent and close roots, endpoint errors, Bernstein bounds |
| `python/radial_bounds.py` | `reference/polynomial.hpp`, validator radial certificate | Shell lower/upper bounds, subdivision depth, analytic core/exterior positivity; both prior parameter choices |
| `python/validate.py` | `src/experiments/validate.cpp` | All limits, 4,150 domain points, C0–C3 matching, extrinsic curvature, convergence, energy diagnostic, reports |
| `tools/source_manifest.py` | `tools/artifacts.hpp`, `tools/project.cpp` | SHA-256 known vectors, deterministic manifests, separate doc digest, change/symlink detection, atomic outputs |
| `tools/validate.py` | `tools/project.cpp`, `tools/process.hpp` | Direct process argument vectors, status propagation, native workflow, report preservation on failure |

The 90-place fixed-decimal backend uses arbitrary-length base-10^9 integer
storage. It does not use `long double`, `__float128`, a C numerical library,
or an interpreter. It truncates individual arithmetic operations; 110-place
refinement, analytic identities and step convergence measure the resulting
error. Rational operations normalize with exact arbitrary-length GCDs; Sturm
remainders also receive positive leading-coefficient normalization.

The previous alternate C++ geometry API is consolidated into
`geometry.hpp`/`geometry.cpp`. Its grid, endpoint and domain-rejection coverage
was ported to `tests/cpp_geometry.cpp` before retirement. Its `[1.8,2.2]` shell
is retained as regression input, not as a second accepted preset.

## Audit requests

1. `Region::shell` explicitly bypasses endpoint clamping and rejects shell
   extrapolation. Both the validator and canonical CTest compare this branch
   against de Sitter at `r_in` and Kerr at `r_out`, through third derivatives
   and outward-normal `K_AB`. Production endpoint zeros are expected from
   exact C3 endpoint identities; independently differentiated shell branches
   have nonzero residuals, about `3.26e-31` through order three.
2. `h=1e-9` is evaluated with 90 decimal places. The measured 90-to-110-place
   Hessian refinement error is about `1.17e-71`, not long-double cancellation.
   Thresholds were tightened and frozen after the first C++ pilot.
3. Exact shell lower bound is `2237/8000 = 0.279625`. Core factor and exterior
   monotonicity bounds are positive. This excludes radial null surfaces for
   this fixed preset; it is not a global event-horizon proof.
4. Python is removed only after the checkpoint above. One canonical production
   geometry target remains; reference independence is provided by different
   high-precision formulas, numerical differences and curvature contractions.
5. Final reports and source manifest are generated from the final source tree.
   The remote baseline reviewed was `a316415cc7bd44246a3b0e222d6cca91066d1f6b`;
   its only difference from local `42de923` was a README blank line.
