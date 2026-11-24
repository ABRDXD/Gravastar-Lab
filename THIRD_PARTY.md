# Third-party inventory

Environment audit date: **2026-10-20**. License choice updated **2026-11-23**:
original GravastarLab code is **GPL-3.0-or-later**; see [LICENSE](LICENSE) and
[license decision](docs/LICENSING.md). The P1 geometry implementation
and independent C++20 reference contain no vendored third-party implementation.
The validation/tool JSON adapter uses the installed header-only C++ dependency
`nlohmann_json 3.12.0`, required exactly by CMake. Its MIT notice is preserved in
`third_party/licenses/nlohmann-json-3.12.0.txt`; no Python runtime is required.
The scientific equation source and excluded KerrLab code are documented in
[docs/PROVENANCE.md](docs/PROVENANCE.md).

## Vendored for the optional desktop build (`GRAVASTAR_DESKTOP=ON`)

Brought across from `backup/astra-work` on 2026-11-23. Headless builds do not
compile or link either component.

| Component | Version | Relationship to project | License evidence |
| --- | --- | --- | --- |
| Khronos Vulkan Headers | `VK_HEADER_VERSION 321` (1.4.321), `third_party/include/vulkan/`, `third_party/include/vk_video/` | Vendored C header declarations; the loader is opened at run time with `dlopen` | Every file carries `SPDX-License-Identifier: Apache-2.0` and Khronos copyright lines. Apache-2.0 text added as `third_party/licenses/khronos-vulkan-headers-1.4.321-Apache-2.0.txt` (SHA-256 `074e6e32c86a4c0ef8b3ed25b721ca23aca83df277cd88106ef7177c354615ff`, copied from `/usr/share/licenses/spdx/Apache-2.0.txt`). Apache-2.0 is compatible with GPL-3.0. |
| GLFW | 3.4.0 (`GLFW_VERSION_*` in `glfw3.h`), `third_party/glfw/` | Vendored C source, built as a static library for the studio only | zlib license, `third_party/glfw/LICENSE.md` (SHA-256 `149704059b5d0bf551637e50042dd4de9c2cae921021f6636298911e3a5f9462`). GPL-compatible. |
| shaderc / glslc | system executable | External shader build tool, required only when `GRAVASTAR_DESKTOP=ON`; SPIR-V outputs stay in the build tree | Not bundled |

| Dear ImGui | 1.93.0, upstream `ocornut/imgui` tag `v1.93.0`, commit `c1aaac65e896a45f81f47a4d68f9959e0f86c859`; `third_party/imgui/` (core, `imgui_demo.cpp`, `backends/imgui_impl_{glfw,vulkan}`) | Vendored C++ source, built as `gravastar_imgui` for the studio only. Demo windows compiled out (`IMGUI_DISABLE_DEMO_WINDOWS`); Vulkan entry points loaded at run time (`IMGUI_IMPL_VULKAN_NO_PROTOTYPES`). Unmodified. | MIT, `third_party/imgui/LICENSE.txt` (SHA-256 `173506a2d6f7fb67990d257fb2507f188690eca39060c39469ae7bef43aae2a3`). GPL-compatible. |
| ImPlot | 1.1, upstream `epezent/implot` tag `v1.1`, commit `819f200639455a58b6b3d4156b70e4271aeb2d00`; `third_party/implot/` (core, no demo) | Vendored C++ source in `gravastar_imgui`. Unmodified. | MIT, `third_party/implot/LICENSE` (SHA-256 `38bd30c2111fe7e3d8fb86d8607491239659eff1df1b1004b9a8349e20a91967`). GPL-compatible. |

| LodePNG | upstream `lvandeve/lodepng` (no tags) commit `7b2893e35891c0c85bee5595caafaf3c2044b409`, `LODEPNG_VERSION_STRING` `20261102`; `third_party/lodepng/` (`lodepng.cpp`, `lodepng.h`) | Vendored C++ source, built as `gravastar_png` in every configuration for recording frames. Unmodified. Encoder adds no version text chunk (`add_id = 0`). | zlib licence, `third_party/lodepng/LICENSE` (SHA-256 `bdafa318b8e637d600ca1b60545e894881977a16a16b74809c95e805701621e3`). GPL-compatible. Per-file SHA-256: `third_party/LODEPNG_SHA256.txt`. |

ImGui and ImPlot were added on 2026-11-24 by a shallow clone of the tags
above; the 21 copied files are byte-identical to those tags. Their per-file
SHA-256 list is `third_party/IMGUI_IMPLOT_SHA256.txt`.

**Open provenance item:** the original vendoring commits (`4db1675`, `694f2c4`)
did not record an upstream tag, commit or archive hash. The versions above come
from the files themselves. Before release, compare both trees byte for byte
with upstream `KhronosGroup/Vulkan-Headers` `v1.4.321` and `glfw/glfw` `3.4`
and record the result here.

## Dependencies used by P0/P1

| Component | Actual version | Relationship to project | License evidence and decision |
| --- | --- | --- | --- |
| GCC / G++ | `16.2.1 20260810` | External compiler, not bundled | Toolchain executable; no compiler source copied |
| libstdc++ / libgcc | Arch `16.2.1+r23+gd564253eb6c8-1` | System C++ runtime, not bundled | Installed `bits/c++config.h` states GPL-3.0-or-later with GCC Runtime Library Exception 3.1 |
| CMake | `4.4.4` | External build tool, not bundled | Actual `/usr/share/licenses/cmake/LICENSE.rst` inspected; BSD-3-Clause terms |
| Ninja | `1.13.2` | External build tool, not bundled | Actual `/usr/share/licenses/ninja/COPYING` inspected; Apache-2.0 text |
| nlohmann/json | `3.12.0`, upstream release `v3.12.0` | System header-only C++ library for validator/tools; not bundled | MIT; installed notice copied verbatim; source `https://github.com/nlohmann/json/tree/v3.12.0` |

The C++ runtime's installed grant explicitly includes the GCC exception.
Its upstream text permits eligible compiled combinations subject to its stated
terms. [GCC Runtime Library Exception 3.1](https://raw.githubusercontent.com/gcc-mirror/gcc/master/COPYING.RUNTIME).
External tools and runtimes keep their own terms; their licenses are not
replaced by GravastarLab's GPL-3.0-or-later license.

No interpreter or non-C++ language dependency is used by P1. The reference's
integer, rational, fixed-decimal and trigonometric arithmetic are original
C++20 implementation. GMP/MPFR were detected but not adopted. No language
runtime or global package was installed for the migration.

Installed `json.hpp` SHA-256:
`88d7eb42b19599cab48856576dd4ea52538df72ad7c57200c50de98bda246458`.
Preserved MIT notice SHA-256:
`46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603`.
The former identifies the installed entry header, not every transitive header;
the exact package version is the dependency constraint. The library is unmodified.

The audited toolchain versions are the tested environment, not claims of
reproducible binary identity across compiler or library versions. No network
fetch or implicit sibling-directory dependency should occur during the P1 build.

## Optional reference build (`GRAVASTAR_GYOTO_REFERENCE=ON`, off by default)

Configure fetches these pinned sources into `build/reference-sources` (or the
cache named by `GRAVASTAR_REFERENCE_GIT_CACHE`) and exports only the listed
files into the build tree. No upstream build script is run. Nothing here is
vendored into the repository or linked into the default build.

| Component | Pin | License (notice file, SHA-256) | Use |
| --- | --- | --- | --- |
| GYOTO | 2.0.2, commit `d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a` | GPL-3.0-or-later (`COPYING`, `8ceb4b9e…5eb65b903`) | 28 core sources compiled as C++20 into `gravastar_gyoto_compare` |
| Eigen | 3.4.0, commit `3147391d946bb4b6c68edd901f2add6ac1f31f8c` | MPL-2.0 and others (`COPYING.MPL2`, `fab3dd6b…0a13863e85`; all `COPYING.*` kept) | Headers required by GYOTO |
| Boost | 1.84.0 source archive, SHA-256 `a5800f405508f5df8114558ca9855d2640a2de8f0445f051fa1c7c3383045724` | BSL-1.0 (`LICENSE_1_0.txt`, `c9bff757…284ad566`) | Headers only (Boost.Math used by GYOTO `Photon.C`) |

Three textual patches are applied at configure time and fail loudly if the
pinned text no longer matches: two XML-free guards (`Object.C` vector
parsing, `UniformSpectrometer.C` XML output) and one thread-free declaration
(`Scenery.C`). No numerical code is changed. The comparison executable links
GPL GYOTO and this project's GPL-3.0-or-later code. See
`docs/GYOTO_FEASIBILITY.md`.

## Available but not integrated

P2 native optics adds no dependency. Its Dormand-Prince coefficient reference
and scientific conventions are documented in `docs/OPTICS.md`; no Boost code
was copied or linked. GYOTO is built only by the optional reference build
above. PPM serialization
uses original C++20 and requires no image library.

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
| Dear ImGui / ImPlot | Not found by checked system package/header probes | Choose immutable source pins and inspect actual notices before integration (GLFW is now vendored; see above) |
| GYOTO | Executable/package absent | Built from pinned source by the optional reference build above |

The [GYOTO project](https://gyoto.obspm.fr/) describes custom metric plugins and
its GPL-3.0-or-later terms. These are GYOTO's terms, not the license selected for
original GravastarLab code. Before implementation, review the actual revision,
linking/distribution arrangement and any separately licensed adapter. Prefer an
optional independent reference workflow that leaves the project usable
without GYOTO; process separation alone is not a license-compatibility verdict.
Scientific attribution must also be retained. GYOTO code is compiled only in
the optional reference build; the adapter in `reference/gyoto/compare.cpp` is
GPL-3.0-or-later like the rest of this project.

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
