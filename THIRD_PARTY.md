# Third-party inventory

Audit date: **2026-10-20**. Original GravastarLab code is
**GPL-3.0-or-later**; see [LICENSE](LICENSE). The P1 geometry implementation
and independent Python checks contain no vendored third-party implementation.
The scientific equation source and excluded KerrLab code are documented in
[docs/PROVENANCE.md](docs/PROVENANCE.md).

## Dependencies used by P0/P1

| Component | Actual version | Relationship to project | License evidence and decision |
| --- | --- | --- | --- |
| GCC / G++ | `16.2.1 20260810` | External compiler, not bundled | Toolchain executable; no compiler source copied |
| libstdc++ / libgcc | Arch `16.2.1+r23+gd564253eb6c8-1` | System C++ runtime, not bundled | Installed `bits/c++config.h` states GPL-3.0-or-later with GCC Runtime Library Exception 3.1 |
| CMake | `4.4.4` | External build tool, not bundled | Actual `/usr/share/licenses/cmake/LICENSE.rst` inspected; BSD-3-Clause terms |
| Ninja | `1.13.2` | External build tool, not bundled | Actual `/usr/share/licenses/ninja/COPYING` inspected; Apache-2.0 text |
| CPython | `3.14.8` | External interpreter and standard library, not bundled | PSF license and incorporated notices; no Python implementation copied |
| `decimal` / libmpdec | libmpdec `4.0.1` | Part of the installed Python runtime | No separate vendored library; backend version recorded for reproducibility |

The C++ runtime's installed grant explicitly includes the GCC exception.
Its upstream text permits eligible compiled combinations subject to its stated
terms. [GCC Runtime Library Exception 3.1](https://raw.githubusercontent.com/gcc-mirror/gcc/master/COPYING.RUNTIME).
The Python project identifies current Python releases as GPL compatible and
documents its separate incorporated-software notices.
[Python license documentation](https://docs.python.org/3/license.html).
These choices permit the planned GPL project without relicensing external
tools as GravastarLab code.

No NumPy, SciPy, SymPy, mpmath, or pytest dependency is required by P1. The Python
reference uses standard-library Decimal arithmetic; its own formulas and
trigonometric evaluation are original project implementation. There is no
third-party Python lockfile to maintain for this stage.

The audited toolchain versions are the tested environment, not claims of
reproducible binary identity across compiler or library versions. No network
fetch or implicit sibling-directory dependency should occur during the P1 build.

## Available but not integrated

These are environment findings and future candidates, not adopted dependency
pins. No code, notice, or binary was copied from these packages into P1.

| Component | Installed observation | Relationship and remaining decision |
| --- | --- | --- |
| Vulkan headers | `1.4.363.0`; installed MIT license inspected | Future API declarations; pin selected source and preserve applicable notices when adopted |
| Vulkan loader | `1.4.363.0` | Future external system runtime; device access failed in this session |
| shaderc / glslc | `2026.4` | Future external shader build tool; not invoked for a project shader |
| glslang | Reports `16.6.0`; installed composite LICENSE inspected | Future external build tool; includes multiple notices, so do not reduce a bundled distribution to one blanket label |
| libpng | `1.6.59`; installed PNG Reference Library License v2 inspected | Future linked image library; not selected yet |
| OpenEXR | `3.5.2`; installed BSD-3-Clause notice inspected | Future linked scientific image library; not selected yet |
| FFmpeg | `n9.0.2`, configured `--enable-gpl --enable-version3`; `ffmpeg -L` reports GPL-3.0-or-later | Future external executable; no binary or codecs bundled |
| GLFW / Dear ImGui / ImPlot | Not found by checked system package/header probes | Choose immutable source pins and inspect actual notices before integration |
| GYOTO | Executable/package absent | Official project states GPL-3.0-or-later; exact revision, dependency notices, API, ABI, and build remain P2 work |

The [GYOTO project](https://gyoto.obspm.fr/) describes custom metric plugins and
its GPL-3.0-or-later terms. This matches the intended whole-project license.
The website also requests scientific citation and publication of research
extensions; an eventual adapter must document both source obligations and
scientific attribution. No GYOTO adapter or reference result exists in P1.

For a later distribution, review each actually linked/bundled component and its
transitive notices at the selected immutable revision. The presence of a system
package is not an adoption decision. GPU drivers will remain external. FFmpeg
codec/build options must be recorded for the actual executable distributed or
invoked, rather than assuming all FFmpeg builds have the same license.

## Inspected notice hashes

The paths below refer to this host's files. Their SHA-256 values preserve the
audit identity; these notices are not substitutes for pinning adopted sources.

| Local path | SHA-256 |
| --- | --- |
| `/usr/share/licenses/spdx/GPL-3.0-or-later.txt` | `fb981668c18a279e285fc4d83fba1e836cc84dd4daa73c9697d3cfd2d8aca6e0` |
| `/usr/share/licenses/cmake/LICENSE.rst` | `4382e7c1879ac90e3f101a395d23846fa4dbcaa1eed7265b43681e348754825d` |
| `/usr/share/licenses/ninja/COPYING` | `eb7e9ab9690124c5c9f42bdc81383d886a3dede26345b6ed15bbad7caf81f7ea` |
| `/usr/share/licenses/vulkan-headers/MIT.txt` | `1ca3502222d967f3be5751c55f6b7ee735b5383909c3b501495f54b216dbf227` |
| `/usr/share/licenses/glslang/LICENSE` | `17e70c676e1521ff3e4686f04a2053d93a7e28a33be8de7ec37ab0ff72feb677` |
| `/usr/share/licenses/libpng/LICENSE` | `bdb0a645ea18c60507d0368379b1ac5474b92255fcc2d115e07486a7672ba526` |
| `/usr/share/licenses/openexr/LICENSE.md` | `84853c91d4c2e0310741d4773469d98bb93e61029608e0ba0882a3fffdf330bd` |

## Future import record

Before adding reused implementation, record source URL, immutable revision or
archive hash, license path/hash, exact file inventory, modifications, retained
notices, and whether the component is linked, bundled, a build tool, or an
external executable. An absent grant leaves the component excluded.
