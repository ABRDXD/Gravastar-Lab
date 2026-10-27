# GYOTO reference integration boundary

Initial assessment on 2026-10-26; source-build investigation resumed 2026-10-27.
The original GravastarLab project remains MIT licensed and C++20 only.

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
