#!/usr/bin/env python3
"""Connect a worktree's ignored .deps entry to a shared dependency cache.

The repository's CMake files intentionally address dependencies through
``<worktree>/.deps``.  This helper keeps that interface stable while allowing
the source checkouts and compiled toolchains to live outside any one worktree.
It refuses to replace a real directory or a link pointing at another cache.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import sys


ROOT = Path(__file__).resolve().parents[1]


def default_shared_root() -> Path:
    configured = os.environ.get("AXIOM_SHARED_DEPS")
    if configured:
        return Path(configured).expanduser()
    platform_name = platform.system().lower()
    machine = platform.machine().lower()
    return Path.home() / "Desktop" / "sources" / "git" / "deps" / "axiom" / f"{platform_name}-{machine}"


def connect(shared_root: Path) -> Path:
    shared_root = shared_root.expanduser().resolve()
    link = ROOT / ".deps"
    if shared_root == link:
        raise RuntimeError("shared dependency root must not be the worktree .deps path")
    shared_root.mkdir(parents=True, exist_ok=True)

    if link.is_symlink():
        actual = link.resolve()
        if actual != shared_root:
            raise RuntimeError(f"{link} already points to {actual}; refusing to replace it")
        return link
    if link.exists():
        raise RuntimeError(f"{link} is a real directory/file; move it aside before connecting a shared cache")
    link.symlink_to(shared_root, target_is_directory=True)
    return link


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--shared-root",
        type=Path,
        default=default_shared_root(),
        help="shared dependency directory (or set AXIOM_SHARED_DEPS)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify the existing .deps link without creating it",
    )
    args = parser.parse_args()
    shared_root = args.shared_root.expanduser().resolve()
    link = ROOT / ".deps"
    if args.check:
        if not link.is_symlink():
            raise RuntimeError(f"{link} is not a symlink to a shared dependency cache")
        if link.resolve() != shared_root:
            raise RuntimeError(f"{link} points to {link.resolve()}, expected {shared_root}")
    else:
        connect(shared_root)
    print(f"shared dependency cache: {shared_root}")
    print(f"worktree entry: {link} -> {link.resolve()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(2)
