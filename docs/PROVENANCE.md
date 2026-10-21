# Independence and provenance audit

Audit date: **2026-10-20**. GravastarLab is an independent project. Its P0/P1
implementation is written from the specified scientific equations and standard
mathematics; no KerrLab source, shader, asset, numerical table, dependency tree,
or binary is copied or linked.

## Scientific source

The model reference is Ilijić and DeBenedictis,
[arXiv:2309.00551v2](https://arxiv.org/html/2309.00551v2). The source-equation
map, benchmark choices, and coordinate restrictions are recorded in
[MODEL.md](MODEL.md). Citation of mathematical results is separate from
redistributing the paper's text or figures. The benchmark must distinguish
published parameter values from locally selected sampling points and thresholds.

## Local KerrLab inspection

Read-only inspection covered the neighboring trees `../KerrLab-Restored`,
`../KerrLab-Source-Raw`, and the inventory of `../KerrLab(Arch)`.
The archived tarball was not extracted or incorporated.

The Restored tree's `AGENTS.md`, `THIRD_PARTY.md`, `web-demo/UPSTREAM.md`,
`web-demo/UPSTREAM_FILES.json`, and `MIGRATION-INFO.json` were inspected as
provenance evidence. They describe a native C++/Vulkan project and a separately
imported browser renderer. Those documents' reported validation totals are
historical claims about KerrLab and were not rerun or inherited by GravastarLab.

The browser import is recorded as
[`kyleyhw/black_hole`](https://github.com/kyleyhw/black_hole), revision
`96a6835f2624bc6425682f723a050f397893cb6d`. The local manifest records 124 imported
files, original archive SHA-256
`52cb785e38b1ac50e63b3bb92f0608b000113d2fca2a4f8c991b0e272f5d851c`,
and absence of an upstream LICENSE, COPYING, or NOTICE file at that revision.
This audit corroborated the local provenance record; it did not redownload or
independently reconstruct that historical upstream archive.

No top-level LICENSE or COPYING file was found in `KerrLab-Restored`. Its
dependency-specific notices do not provide a blanket license for all project
files. Earlier user authorization for a local KerrLab import does not supply a
third-party license grant for GravastarLab distribution.

**Decision:** keep all KerrLab implementation and assets outside this project.
Potential reuse later requires an exact file inventory, an actual applicable
grant, immutable revision/hash, modifications, and preserved notices. The
independent project is not conditional on making any such reuse.

## Evidence hashes

These are SHA-256 hashes of the inspected local documents, not upstream
signatures and not hashes of a newly verified source distribution.

| Relative to `../KerrLab-Restored` | SHA-256 |
| --- | --- |
| `THIRD_PARTY.md` | `d299e8835cc57e9330921d80119f62179411fe693f8861b6820058dc8f17c7ce` |
| `web-demo/UPSTREAM.md` | `4befedad03facea5d5b97b3db2f7981ebad2aa6e4a04093d6edcfe5d49fb2599` |
| `web-demo/UPSTREAM_FILES.json` | `b7a88e9db07ee860af1646e0d0898da06ab9d39d7a66a789e4a9ecd248e66f20` |
| `MIGRATION-INFO.json` | `5d8ba79b2a44543df5c4e2d35abe15dfe69ce434c7060fed8a354958363526e2` |
| `AGENTS.md` | `59e077ca630ca00717422a174d63435eff99ae793d9416ebd0a7afbd6831c3eb` |

The Raw tree's `THIRD_PARTY.md` had the same hash as the Restored copy.

## Dependency boundary

Original GravastarLab code uses GPL-3.0-or-later as specified in the plan.
P1 has no vendored third-party implementation and no linked scientific,
graphics, or GUI library. C++ standard runtimes and local build tools
are recorded in [ENVIRONMENT.md](ENVIRONMENT.md) and
[THIRD_PARTY.md](../THIRD_PARTY.md). No Electron runtime, npm lockfile,
Windows toolchain, KerrLab coefficient table, or historical report is adopted.
The native migration additionally uses installed header-only C++
`nlohmann_json 3.12.0` for JSON reports and tools, with its MIT notice retained.
The independent integer, rational, decimal and polynomial code is original
C++20 and has no linked numerical-library dependency.

Later compiled dependencies must be reviewed and pinned before integration.
Installed package version numbers are observations, not immutable source pins
for a future distribution. The same rule applies to GYOTO: its official site
states GPL-3.0-or-later, but no concrete source revision or plugin ABI has yet
been selected. [GYOTO conditions for use](https://gyoto.obspm.fr/#conditions).
