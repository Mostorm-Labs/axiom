#!/usr/bin/env python3
"""Classify whether a PR must rebuild the hosted semantic toolchain asset.

The semantic toolchain Producer is intentionally more conservative than its
consumer.  A source build is required only for a change that can alter the
packaged asset or its identity.  Consumer lock/fetch changes must never pay
the Producer build cost.
"""

from __future__ import annotations

import argparse
import json
from pathlib import PurePosixPath
import subprocess
import sys
from typing import Any


SEMANTIC_PRODUCER_LOCK_KEYS = frozenset({"protobuf", "abseil"})
PRODUCER_RECIPE_PATHS = frozenset({
    "tools/bootstrap_deps.py",
    "tools/semantic_sdk.py",
    "verification/tests/test_semantic_sdk.py",
    ".github/workflows/semantic-toolchain-producer.yml",
})


def _result(should_build: bool, reason: str, *, changed_lock_keys: list[str] | None = None) -> dict[str, object]:
    result: dict[str, object] = {"should_build": should_build, "reason": reason}
    if changed_lock_keys is not None:
        result["changed_lock_keys"] = changed_lock_keys
    return result


def classify_lock_documents(before: dict[str, Any], after: dict[str, Any]) -> dict[str, object]:
    """Classify the lock by the exact dependency subtrees used by the Producer."""
    before_dependencies = before.get("dependencies")
    after_dependencies = after.get("dependencies")
    if not isinstance(before_dependencies, dict) or not isinstance(after_dependencies, dict):
        return _result(True, "lock dependency shape is unavailable; conservative producer fallback")

    changed = sorted(
        key for key in set(before_dependencies) | set(after_dependencies)
        if before_dependencies.get(key) != after_dependencies.get(key)
    )
    if set(changed) & SEMANTIC_PRODUCER_LOCK_KEYS:
        return _result(
            True,
            "semantic toolchain dependency lock changed",
            changed_lock_keys=changed,
        )

    before_other = {key: value for key, value in before.items() if key != "dependencies"}
    after_other = {key: value for key, value in after.items() if key != "dependencies"}
    if before_other != after_other:
        return _result(True, "lock metadata changed; conservative producer fallback", changed_lock_keys=changed)
    if changed:
        return _result(
            False,
            "unrelated dependency lock changed; semantic producer not required",
            changed_lock_keys=changed,
        )
    return _result(False, "lock contents unchanged", changed_lock_keys=[])


def classify(
    paths: list[str],
    *,
    lock_before: dict[str, Any] | None = None,
    lock_after: dict[str, Any] | None = None,
) -> dict[str, object]:
    """Return the producer decision for a list of POSIX repository paths."""
    normalized = {PurePosixPath(path).as_posix() for path in paths}
    if normalized & PRODUCER_RECIPE_PATHS:
        return _result(True, "semantic producer recipe or package contract changed")
    if "deps.lock.json" in normalized:
        if lock_before is None or lock_after is None:
            return _result(True, "lock diff unavailable; conservative producer fallback")
        return classify_lock_documents(lock_before, lock_after)
    return _result(False, "consumer-only, classification-only, or unrelated change")


def _git_output(*args: str) -> str:
    return subprocess.run(
        ["git", *args], check=True, capture_output=True, text=True
    ).stdout


def _git_lock(ref: str) -> dict[str, Any]:
    value = json.loads(_git_output("show", f"{ref}:deps.lock.json"))
    if not isinstance(value, dict):
        raise ValueError("dependency lock is not an object")
    return value


def _git_classification(base_ref: str, head_ref: str) -> dict[str, object]:
    paths = [
        line for line in _git_output("diff", "--name-only", base_ref, head_ref).splitlines()
        if line
    ]
    if "deps.lock.json" not in paths:
        return classify(paths)
    try:
        return classify(paths, lock_before=_git_lock(base_ref), lock_after=_git_lock(head_ref))
    except (OSError, subprocess.CalledProcessError, ValueError, json.JSONDecodeError) as error:
        return _result(True, f"lock diff unavailable; conservative producer fallback: {error}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--base-ref")
    parser.add_argument("--head-ref")
    args = parser.parse_args()
    if bool(args.base_ref) != bool(args.head_ref):
        parser.error("--base-ref and --head-ref must be provided together")
    if args.base_ref:
        result = _git_classification(args.base_ref, args.head_ref)
    else:
        paths = args.paths
        if not paths and not sys.stdin.isatty():
            paths = [line.strip() for line in sys.stdin if line.strip()]
        result = classify(paths)
    print(json.dumps(result, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
