#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Hash the independent source tree without reading build or result artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRECTORIES = (
    "include", "src", "python", "tests", "benchmarks", "tools",
    "schemas", "scenes", "shaders", "plugins", "third_party",
)
SOURCE_FILES = ("CMakeLists.txt", "CMakePresets.json")
DOCUMENTATION_FILES = ("README.md", "PLAN.md", "LICENSE", "NOTICE", "THIRD_PARTY.md")
EXCLUDED_DIRECTORIES = frozenset({
    ".git", ".venv", ".cache", "__pycache__", ".pytest_cache", "build",
    "results", "node_modules",
})
EXCLUDED_SUFFIXES = frozenset({".pyc", ".pyo", ".o", ".a", ".so", ".exe"})


def _files_below(path: Path):
    """Traverse only a selected source directory, without following symlinks."""
    if path.is_symlink():
        raise ValueError(f"symlinked source directory is not independent: {path}")
    if not path.exists():
        return
    for directory, subdirectories, filenames in os.walk(path, followlinks=False):
        parent = Path(directory)
        subdirectories[:] = sorted(
            name for name in subdirectories if name not in EXCLUDED_DIRECTORIES
        )
        for name in subdirectories:
            if (parent / name).is_symlink():
                raise ValueError(f"symlinked source directory: {parent / name}")
        for name in sorted(filenames):
            candidate = parent / name
            if candidate.suffix in EXCLUDED_SUFFIXES:
                continue
            if candidate.is_symlink():
                raise ValueError(f"symlinked source file: {candidate}")
            if candidate.is_file():
                yield candidate


def _digest(entries: list[dict]) -> str:
    canonical = json.dumps(
        entries, ensure_ascii=True, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def build_manifest(project_root: Path = PROJECT_ROOT) -> dict:
    """Return deterministic file hashes and separate code/documentation digests.

    The computational digest is suitable for embedding in result reports.
    Documentation can cite reports without creating a report/hash cycle.
    """
    root = Path(project_root).resolve()
    selected: dict[Path, str] = {}
    for name in SOURCE_FILES:
        candidate = root / name
        if candidate.is_symlink():
            raise ValueError(f"symlinked source file: {candidate}")
        if candidate.is_file():
            selected[candidate] = "computational"
    for name in SOURCE_DIRECTORIES:
        for candidate in _files_below(root / name):
            selected[candidate] = "computational"
    for name in DOCUMENTATION_FILES:
        candidate = root / name
        if candidate.is_symlink():
            raise ValueError(f"symlinked documentation file: {candidate}")
        if candidate.is_file():
            selected[candidate] = "documentation"
    for candidate in _files_below(root / "docs"):
        selected[candidate] = "documentation"

    entries = []
    for path, scope in sorted(selected.items(), key=lambda pair: pair[0].as_posix()):
        content = path.read_bytes()
        entries.append({
            "path": path.relative_to(root).as_posix(),
            "scope": scope,
            "bytes": len(content),
            "sha256": hashlib.sha256(content).hexdigest(),
        })
    computational = [entry for entry in entries if entry["scope"] == "computational"]
    documentation = [entry for entry in entries if entry["scope"] == "documentation"]
    if not computational:
        raise ValueError(f"no computational source files found in {root}")
    return {
        "schema_version": 1,
        "algorithm": "sha256",
        "digest_encoding": "UTF-8 JSON of ordered entries; sort_keys=True, separators=(',', ':')",
        "computational_sha256": _digest(computational),
        "documentation_sha256": _digest(documentation),
        "source_sha256": _digest(entries),
        "file_count": len(entries),
        "files": entries,
        "scope_note": (
            "Computational source/configuration and documentation are hashed separately. "
            "Build outputs, result artifacts, caches, environments, VCS metadata, and "
            "unselected top-level files are excluded. No timestamp or absolute path is hashed."
        ),
    }


def write_json_atomic(path: Path, value: dict) -> None:
    """Commit a complete JSON document within its destination filesystem."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent,
            prefix=f".{path.name}.", suffix=".tmp", delete=False,
        ) as stream:
            temporary = Path(stream.name)
            json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def result_path(value: str | Path, project_root: Path = PROJECT_ROOT) -> Path:
    """Require generated outputs to stay under this project's results directory."""
    root = Path(project_root).resolve()
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = root / candidate
    candidate = candidate.resolve()
    if not candidate.is_relative_to(root / "results") or candidate == root / "results":
        raise ValueError("generated output must be a file inside the project results directory")
    return candidate


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", default="results/source_manifest.json",
        help="JSON output under this project's results directory",
    )
    parser.add_argument(
        "--verify", metavar="MANIFEST",
        help="compare current files against an existing manifest without writing",
    )
    arguments = parser.parse_args(argv)
    try:
        manifest = build_manifest()
        if arguments.verify:
            previous_path = result_path(arguments.verify)
            previous = json.loads(previous_path.read_text(encoding="utf-8"))
            if previous != manifest:
                print(f"Source manifest differs: {previous_path}", file=sys.stderr)
                return 1
            print(f"Source manifest verified: {manifest['file_count']} files")
        else:
            output = result_path(arguments.output)
            write_json_atomic(output, manifest)
            print(f"Source manifest: {output}")
            print(f"Computational SHA-256: {manifest['computational_sha256']}")
    except (OSError, ValueError) as error:
        print(f"Source manifest failed: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
