# Project license decision

The project owner explicitly selected the MIT License on 2026-10-25, to make
the original project freely reusable, including by people without access to
paid AI tools. The migration was completed on 2026-10-26. This decision
supersedes the GPL-3.0-or-later choice in the initial plan.

The current original source and documentation are covered by [LICENSE](../LICENSE).
The copyright attribution remains `2026 GravastarLab contributors`. The license
text follows the [Open Source Initiative MIT text](https://opensource.org/license/mit).
Original C++ source/header SPDX identifiers, NOTICE, both READMEs, the plan and
provenance documentation have been updated consistently.

## Third-party boundary

The current tracked third-party inventory contains the unmodified MIT notice
for nlohmann/json 3.12.0. It retains its own copyright holder and terms. No
KerrLab implementation, GYOTO implementation, Boost implementation or other
GPL third-party source is imported into the project. See
[PROVENANCE.md](PROVENANCE.md) and [THIRD_PARTY.md](../THIRD_PARTY.md) for the
recorded evidence and external compiler/runtime details. This project license
does not replace the terms of dependencies, tools, scientific publications or
other external material.

GYOTO remains a future reference dependency with its own GPL terms. Its exact
build, adapter and distribution arrangement must be reviewed before integration.
The core project must remain usable under MIT without requiring GYOTO. The
planned dependency does not authorize changing the owner's license choice.

## Scientific provenance across the change

All 45 changed C++ files differ only in their first-line SPDX identifier.
No algorithm, equation, threshold, fixture, build rule or dependency changed.
`README.txt` changes only its licensing sentence; its previously frozen
scientific content is preserved. This is the intentional exception to earlier
instructions to preserve that entire file byte-for-byte.

Historical numerical reports and their source manifests retain their original
bytes and digests. In particular, the P1 reports still identify `609debe`, and
the core continuation reports identify the pre-migration source at `e28e674`.
SPDX comment edits change the byte-based computational source digest even
though numerical behavior is unchanged. Do not rewrite those historical
digests or claim they were generated from the newly licensed source.

`results/mit_source_manifest.json` inventories the current source and notices.
The rebuilt GCC Release suite passes 22/22 CTests in 58.53 seconds; its
transcript is `results/mit_gcc_ctest.txt`. The current computational digest is
`c63a1f39ebb724ee0675d49a9621331372aa5e48ac48684aceaaadd344f7da3b`.
No new numerical measurements or performance
claims are introduced by the license migration. Git retains the earlier
license text and provenance history.
