#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build, test, and independently validate the bounded rotating benchmark."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

from source_manifest import PROJECT_ROOT, build_manifest, result_path, write_json_atomic


def run(command: list[str]) -> None:
    print("+ " + shlex.join(command), flush=True)
    subprocess.run(command, cwd=PROJECT_ROOT, check=True)


def positive_integer(text: str) -> int:
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return value


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--preset", choices=("headless-release", "headless-debug", "sanitizers"),
        default="headless-release",
    )
    parser.add_argument("--jobs", type=positive_integer, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--config", default="benchmarks/rotating_v1.json")
    parser.add_argument("--output", default="results/rotating_v1.json")
    parser.add_argument("--manifest", default="results/source_manifest.json")
    arguments = parser.parse_args(argv)
    temporary_report = None
    try:
        output = result_path(arguments.output)
        manifest_output = result_path(arguments.manifest)
        if output == manifest_output:
            raise ValueError("report and manifest outputs must be distinct")
        config = Path(arguments.config)
        if not config.is_absolute():
            config = PROJECT_ROOT / config
        config = config.resolve()
        if not config.is_relative_to(PROJECT_ROOT / "benchmarks") or not config.is_file():
            raise ValueError("configuration must be an existing file under project benchmarks/")
        reference = PROJECT_ROOT / "python" / "validate.py"
        if not reference.is_file():
            raise ValueError(f"independent validator is missing: {reference}")
        baseline = build_manifest()
        run(["cmake", "--preset", arguments.preset])
        run(["cmake", "--build", "--preset", arguments.preset, "--parallel", str(arguments.jobs)])
        run(["ctest", "--preset", arguments.preset, "--output-on-failure"])
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            dir=output.parent, prefix=f".{output.name}.", suffix=".pending", delete=False,
        ) as stream:
            temporary_report = Path(stream.name)
        executable = PROJECT_ROOT / "build" / arguments.preset / "gravastar_benchmark"
        run([
            sys.executable, str(reference), "--executable", str(executable),
            "--config", str(config), "--output", str(temporary_report),
        ])
        # A changing source tree cannot receive a new acceptance artifact.
        current = build_manifest()
        if current["computational_sha256"] != baseline["computational_sha256"]:
            raise ValueError("computational sources changed during validation; rerun on a stable tree")
        report = json.loads(temporary_report.read_text(encoding="utf-8"))
        if not isinstance(report, dict):
            raise ValueError("validator report must be a JSON object")
        report["source_manifest"] = {
            "schema_version": 1,
            "computational_sha256": current["computational_sha256"],
            "path": manifest_output.relative_to(PROJECT_ROOT).as_posix(),
        }
        # Preserve previous completed results when compilation or checks fail.
        # The common digest lets readers detect the interruption window between
        # these two independent atomic file replacements.
        write_json_atomic(manifest_output, current)
        write_json_atomic(output, report)
        print(f"Validation report: {output}")
        print(f"Source manifest: {manifest_output}")
    except subprocess.CalledProcessError as error:
        print(f"Validation stopped: command exited {error.returncode}", file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1
    except (OSError, ValueError) as error:
        print(f"Validation stopped: {error}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("Validation interrupted; previous completed report retained.", file=sys.stderr)
        return 130
    finally:
        if temporary_report is not None:
            temporary_report.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
