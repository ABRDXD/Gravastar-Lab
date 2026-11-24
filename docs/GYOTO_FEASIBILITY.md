# GYOTO reference integration boundary

Initial assessment on 2026-10-26; source-build investigation resumed 2026-10-27.
GravastarLab is GPL-3.0-or-later (owner decision, 2026-11-23) and C++20 only.
The 2026-10 sections below were written while the p2 branch was MIT; they are
kept as history.

## Candidate and build constraints

The official release inspected is GYOTO **2.0.2**, released 2024-01-19. Its
peeled tag resolves to commit `d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a`.
This is a candidate pin, not an adopted dependency or a verified build.
[Official release](https://github.com/gyoto/Gyoto/releases/tag/2.0.2).

The release's standard build uses Autotools, shell configure and Make, and its
configuration performs Python detection before the `--without-python` option
disables bindings. That stock workflow does not satisfy this project's
C++20/CMake-only constraint. The release documents C++11-era requirements;
compilation as C++20 has not been established here.
[Installation instructions](https://github.com/gyoto/Gyoto/blob/d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a/INSTALL.Gyoto.md),
[configuration source](https://github.com/gyoto/Gyoto/blob/d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a/configure.ac).

Local probes found no GYOTO executable/package, Eigen, Boost, Xerces-C or
UDUNITS packages; CFITSIO 4.7.0 was present. These are observations on this
host, not dependency pins. Eigen and Boost support upstream numerical code;
the standard XML/FITS executable also uses Xerces-C and CFITSIO. A narrower
C++ comparison harness may avoid the standard executable's input/output path.

## Next bounded experiment

Attempt an optional CMake-only build of a pinned subset of upstream C++ core
sources, compiling as C++20 with pinned numerical dependencies and a native
reference executable. Start from the actual upstream source inventory and
explicitly resolve generated configuration headers, plugin registration, ABI,
and dependencies before claiming integration. This is a feasibility proposal,
not a completed port or an assertion that no source adaptation will be needed.
[Upstream core source list](https://github.com/gyoto/Gyoto/blob/d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a/lib/Makefile.am).

GYOTO and its metric headers retain their upstream GPL terms. Any imported
source and separately built comparison artifact must retain those notices;
the actual linking and distribution arrangement needs review. An optional
process boundary alone is not proof of license compatibility. The MIT project
must remain usable without this candidate dependency, and its license must not
be silently changed to fit a planned integration.
[Upstream terms](https://github.com/gyoto/Gyoto/blob/d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a/README.md).

Only a real executable comparison, with recorded source revisions, conventions,
fixtures, tolerances and measured errors, can satisfy the GYOTO gate. The native
flat/Schwarzschild/Kerr references remain useful but do not substitute for it.

## Source inspection checkpoint, 2026-10-27

The pinned tag was fetched into an ignored bare Git object store and its peeled
commit checked against the pin above. No upstream scripts were checked out or
executed. Local headers confirm that Boost, Eigen and Xerces-C are absent.
The actual `GyotoWorldline.h` and `WorldlineIntegState.C` guard Boost integration
with `GYOTO_HAVE_BOOST_INTEGRATORS`; the upstream Legacy integrator remains an
option without Boost. Eigen is still unconditionally included by `GyotoAstrobj.h`
and `GyotoPhoton.h`, so it cannot be omitted or replaced by a fake interface.
XML, FITS, MPI, threading, unit conversion and optional special-function
backends will remain disabled in the first bounded reference build.

Next: materialize only pinned C++ sources/headers and license notices through
CMake dependency integration, configure the genuine core and Kerr/Minkowski
metrics, then test a linked C++20 executable. Build and scientific acceptance
are still open; fetching source is not evidence that either has passed.

The first C++20 compile exposed an unconditional Boost math dependency in
`Photon.C`, despite the optional Boost integrator. Boost headers are therefore
still needed. The XML-free build also leaves two `FactoryMessenger` array-parser
calls unguarded. The optional CMake integration explicitly rejects those textual
vector-property inputs; typed setters are unaffected. Deprecated APIs are
disabled, and `<cmath>` is included before the upstream fallback `sincos` macro
to avoid corrupting system declarations. No scientific formula has been changed.
The default build still configures and its language-policy/geometry checks pass;
the optional upstream core has not finished compiling at this checkpoint.

## Build and comparison, 2026-11-24 (Phase C step 3)

The optional build now completes and its comparison passes.

```text
cmake --preset headless-release -B build/gyoto-release -DGRAVASTAR_GYOTO_REFERENCE=ON
cmake --build build/gyoto-release --target gravastar_gyoto_compare
ctest --test-dir build/gyoto-release -R gyoto_reference     # ~79 s
```

`GRAVASTAR_REFERENCE_GIT_CACHE` can point at an existing cache of the pinned
GYOTO/Eigen bare repositories and the Boost 1.84.0 archive; with all three
present and matching, nothing is downloaded. Pins and notices are in
`THIRD_PARTY.md`.

**What it took.** The October work (a hash-pinned Boost 1.84 header archive and
a first comparison program) was uncommitted in the p2 worktree; it is now on
`integration/reconcile`. Two more upstream 2.0.2 problems in an XML- and
thread-free build were patched textually: `Scenery.C` uses `thread_safe`
outside its `HAVE_PTHREAD` block, and `UniformSpectrometer.C` defines its XML
writer unguarded. `GridData2D.C` and `FitsRW.C` were dropped: only FITS-backed
astrobj plugins outside this build use them. GCC 16 then compiles the 28
upstream sources as C++20 with one upstream warning (`Metric.C:502`, a
missing return after `throwError`). The old smoke test had `dt/dlambda = -1`,
which GYOTO's generic integrator rejects; GYOTO states hold contravariant
velocities with dt/dlambda > 0, and backward integration uses a negative step.

**Method.** GYOTO's adaptive Legacy integrators use fixed internal
tolerances (~1e-4), so the comparison uses its fixed-step Legacy RK4 and
measures GYOTO's error by halving the step. Its initial velocity is
g^{mu nu} p_nu from GYOTO's own inverse metric, so lambda = -s exactly.
Crossings of the target radius are located on the cubic Hermite interpolant
of GYOTO's stored positions and velocities.

**Results** (`results/p2_gyoto_comparison.json`, 94 checks):

| Check | Measured |
| --- | ---: |
| Kerr metric, 36 points, 3 spins | 3e-13 ceiling met |
| Kerr Christoffel symbols, 36 points | 3e-12 ceiling met |
| GYOTO RK4 on an exact flat null line | 1e-13 ceiling met |
| 8 Kerr rays (a = ±0.875; equatorial, off-equatorial, near-critical b=2.96 winding 15.7 rad, two captured): native vs GYOTO generic at r=20 or r=2.5 in s, t, theta, phi | max 7.8e-11 |
| Same rays: GYOTO KerrBL-specific vs generic integrator | max 2.6e-12 |
| Same rays: GYOTO step-halving change (h 2e-3 to 1e-3) | max 4.5e-11 |
| 7 toy rays through shell and core (r_min down to 0.47), native vs GYOTO with the adapter | max 2.7e-9 |
| Same toy rays: GYOTO step-halving change (h 5e-4 to 2.5e-4) | max 5.2e-8 (error at h=2.5e-4 ~ 1/15 of this) |

**Gravastar adapter.** `ToyGravastar` exposes the production toy geometry to
GYOTO (metric, inverse, Christoffels from first derivatives). GYOTO's own
geodesic equation, integrator and storage then trace the ray. It shares the
metric with the native tracer, so it is an integration check; the metric's
independent checks are the 90-digit reference, the closed-form-Hamiltonian
reference (`tests/toy_ray_reference.hpp`) and the GYOTO Kerr tensors above.

**Not covered.** BL coordinates only: no pole crossings and no Cartesian core
patch (GYOTO KerrBL steps over the axis; the native angular and core charts
are checked against the independent reference instead). No GYOTO image,
astrobj, emission or redshift comparison. Fixed-step Legacy integrators
only; GYOTO's Boost integrators are not built. Only Linux/GCC was tried for
this build.

