#!/usr/bin/env python3
"""Verify that verification-only native targets stay outside product builds."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


VERIFICATION_TARGETS = (
    "axiom_verification_platform_hooks",
    "axiom_verification_platform_host_common",
)
PRODUCTION_TARGET = "canvas_runtime_scene"


class BoundaryError(RuntimeError):
    """A verification/product build boundary is invalid."""


def run(command: list[str], *, cwd: Path) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise BoundaryError(
            f"command failed ({result.returncode}): {' '.join(command)}\n{result.stdout}"
        )
    return result.stdout


def target_names(build_dir: Path, *, ninja: str) -> str:
    return run([ninja, "-C", str(build_dir), "-t", "targets", "all"], cwd=build_dir)


def configure(
    root: Path,
    build_dir: Path,
    *,
    cmake: str,
    verification: bool,
    build_runtime: bool,
) -> None:
    options = [
        f"-DAXIOM_BUILD_VERIFICATION={'ON' if verification else 'OFF'}",
        "-DBUILD_TESTING=OFF",
        "-DCANVAS_BUILD_POC01=OFF",
        "-DCANVAS_BUILD_POC02=OFF",
        "-DCANVAS_BUILD_POC03=OFF",
        "-DCANVAS_BUILD_POC05=OFF",
        "-DCANVAS_BUILD_POC06=OFF",
        f"-DCANVAS_BUILD_RF01={'ON' if build_runtime else 'OFF'}",
        "-DCANVAS_RF01_BUILD_TESTS=OFF",
    ]
    run([cmake, "-S", str(root), "-B", str(build_dir), "-G", "Ninja", *options], cwd=root)


def assert_header_boundary(root: Path) -> None:
    source_roots = (root / "runtime", root / "docs/api")
    forbidden = ("axiom/verification/", "AXIOM_BUILD_VERIFICATION")
    for source_root in source_roots:
        for path in source_root.rglob("*"):
            if not path.is_file() or path.suffix not in {".h", ".hpp", ".c", ".cc", ".cpp", ".cmake"}:
                continue
            text = path.read_text(encoding="utf-8")
            for marker in forbidden:
                if marker in text:
                    raise BoundaryError(f"product source references verification marker: {path}")


def check(root: Path, *, cmake: str, ninja: str, keep: Path | None) -> dict[str, object]:
    assert_header_boundary(root)
    temp_root = Path(tempfile.mkdtemp(prefix="axiom-g0-08-boundary-", dir=keep))
    try:
        on_dir = temp_root / "on"
        off_dir = temp_root / "off"
        configure(root, on_dir, cmake=cmake, verification=True, build_runtime=True)
        configure(root, off_dir, cmake=cmake, verification=False, build_runtime=True)

        on_targets = target_names(on_dir, ninja=ninja)
        off_targets = target_names(off_dir, ninja=ninja)
        missing_on = [target for target in VERIFICATION_TARGETS if target not in on_targets]
        leaked_off = [target for target in VERIFICATION_TARGETS if target in off_targets]
        if missing_on or leaked_off:
            raise BoundaryError(f"target boundary failed: missing_on={missing_on}, leaked_off={leaked_off}")

        run([cmake, "--build", str(on_dir), "--target", PRODUCTION_TARGET], cwd=root)
        run([cmake, "--build", str(off_dir), "--target", PRODUCTION_TARGET], cwd=root)
        for build_dir in (on_dir, off_dir):
            query = run([ninja, "-C", str(build_dir), "-t", "query", PRODUCTION_TARGET], cwd=build_dir)
            if any(target in query for target in VERIFICATION_TARGETS):
                raise BoundaryError("production target query contains a verification target")

        return {
            "verification_on_targets": list(VERIFICATION_TARGETS),
            "verification_off_targets": [],
            "production_target_built": PRODUCTION_TARGET,
            "product_dependency_query_contains_verification": False,
        }
    finally:
        if keep is None:
            shutil.rmtree(temp_root, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--cmake", default=os.environ.get("CMAKE", "cmake"))
    parser.add_argument("--ninja", default=os.environ.get("NINJA", "ninja"))
    parser.add_argument("--keep-build", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.root.resolve(), cmake=args.cmake, ninja=args.ninja, keep=args.keep_build)
    except (BoundaryError, OSError) as exc:
        print(f"build boundary validation failed: {exc}")
        return 1
    rendered = json.dumps(result, sort_keys=True, separators=(",", ":"))
    if args.json:
        args.json.write_text(rendered + "\n", encoding="utf-8")
    print("build boundary: valid")
    print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
