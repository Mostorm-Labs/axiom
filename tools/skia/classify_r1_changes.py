#!/usr/bin/env python3
"""Classify R1 Skia changes into producer scope or consumer-only validation.

This deliberately keeps the expensive producer trigger conservative: anything
that can change a packaged archive or its build identity selects the complete
matrix.  Consumer/download/lock changes never select a Skia source build.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import sys
from typing import Any


ALL_TARGETS = [
    "windows-x64-d3d12",
    "web-wasm-webgl2",
    "macos-arm64-metal",
    "macos-x64-metal",
    "ios-arm64-metal",
    "ios-simulator-arm64-metal",
    "android-arm64-v8a-gles3",
    "android-x86_64-gles3",
]
PLATFORM_TARGETS = {
    "windows": ["windows-x64-d3d12"],
    "web": ["web-wasm-webgl2"],
    "apple": [
        "macos-arm64-metal", "macos-x64-metal", "ios-arm64-metal",
        "ios-simulator-arm64-metal",
    ],
    "android": ["android-arm64-v8a-gles3", "android-x86_64-gles3"],
}
TARGET_MATRIX = {
    "windows-x64-d3d12": {
        "os": "windows-2025", "family": "windows",
        "toolchain": "windows-2025-llvm-22.1.8",
    },
    "web-wasm-webgl2": {
        "os": "ubuntu-24.04", "family": "web",
        "toolchain": "ubuntu-24.04-emscripten-6.0.6",
    },
    "macos-arm64-metal": {
        "os": "macos-15", "family": "apple", "toolchain": "macos-15-xcode",
    },
    "macos-x64-metal": {
        "os": "macos-15", "family": "apple", "toolchain": "macos-15-xcode",
    },
    "ios-arm64-metal": {
        "os": "macos-15", "family": "apple", "toolchain": "macos-15-xcode",
    },
    "ios-simulator-arm64-metal": {
        "os": "macos-15", "family": "apple", "toolchain": "macos-15-xcode",
    },
    "android-arm64-v8a-gles3": {
        "os": "ubuntu-24.04", "family": "android",
        "toolchain": "ubuntu-24.04-ndk-27.2.12479018",
    },
    "android-x86_64-gles3": {
        "os": "ubuntu-24.04", "family": "android",
        "toolchain": "ubuntu-24.04-ndk-27.2.12479018",
    },
}

# These lock entries contribute to the packaged Skia SDK identity or to a
# platform toolchain used to produce it. Keep this list explicit: unrelated
# dependency changes (for example the semantic codec toolchain) must not start
# the expensive producer matrix.
SKIA_LOCK_KEYS = {
    "skia",
    "skia_builds",
    "emscripten",
    "windows_llvm",
    "android_ndk",
    "apple_toolchain",
    "roboto_regular",
    "noto_sans_cjk_subset",
}
SEMANTIC_LOCK_KEYS = {"protobuf", "abseil"}
CLASSIFICATION_ONLY_PATHS = {
    "tools/skia/classify_r1_changes.py",
    "tools/skia/tests/test_change_classifier.py",
    ".github/workflows/r1-full-producer-contract.yml",
}


def classify_lock_documents(before: dict[str, Any], after: dict[str, Any]) -> dict[str, object]:
    """Classify a lock-file change by comparing dependency subtrees.

    Comparing complete decoded documents is intentional. A diff that changes a
    nested version/hash must still identify its owning dependency, while a
    line-based parser cannot do that reliably.
    """
    before_dependencies = before.get("dependencies", {})
    after_dependencies = after.get("dependencies", {})
    if not isinstance(before_dependencies, dict) or not isinstance(after_dependencies, dict):
        return {"mode": "full", "targets": ALL_TARGETS, "reason": "lock dependency shape changed"}
    changed_dependencies = {
        key for key in set(before_dependencies) | set(after_dependencies)
        if before_dependencies.get(key) != after_dependencies.get(key)
    }
    if before.get("skia_builds") != after.get("skia_builds"):
        changed_dependencies.add("skia_builds")
    if not changed_dependencies:
        # A top-level lock schema/metadata change may affect identity even when
        # dependency values happen to compare equal.
        before_without_dependencies = {key: value for key, value in before.items() if key not in {"dependencies", "skia_builds"}}
        after_without_dependencies = {key: value for key, value in after.items() if key not in {"dependencies", "skia_builds"}}
        if before_without_dependencies != after_without_dependencies:
            return {"mode": "full", "targets": ALL_TARGETS, "reason": "lock identity metadata changed"}
        return {"mode": "none", "targets": [], "reason": "lock contents unchanged"}
    if changed_dependencies & SKIA_LOCK_KEYS:
        return {
            "mode": "full",
            "targets": ALL_TARGETS,
            "reason": "Skia identity or producer toolchain lock changed",
            "changed_lock_keys": sorted(changed_dependencies),
        }
    if changed_dependencies <= SEMANTIC_LOCK_KEYS:
        return {
            "mode": "none",
            "targets": [],
            "reason": "semantic codec dependency lock changed; Skia producer not required",
            "changed_lock_keys": sorted(changed_dependencies),
        }
    return {
        "mode": "none",
        "targets": [],
        "reason": "non-Skia dependency lock changed; Skia producer not required",
        "changed_lock_keys": sorted(changed_dependencies),
    }


def classify(
    paths: list[str],
    *,
    lock_before: dict[str, Any] | None = None,
    lock_after: dict[str, Any] | None = None,
) -> dict[str, object]:
    normalized = [PurePosixPath(path).as_posix() for path in paths]
    if "deps.lock.json" in normalized:
        if lock_before is None or lock_after is None:
            return {
                "mode": "full",
                "targets": ALL_TARGETS,
                "reason": "lock diff unavailable; conservative producer fallback",
            }
        lock_result = classify_lock_documents(lock_before, lock_after)
        other_paths = [path for path in normalized if path != "deps.lock.json"]
        if not other_paths:
            return lock_result
        # Continue classifying other paths below; a full producer path must not
        # be weakened by a semantic-only lock change.
        if lock_result["mode"] == "full":
            return lock_result
    full_exact = {
        "tools/skia/profiles/r1-full-v1.json",
        "tools/skia/sdk.py",
        "tools/skia/build.py",
        "tools/skia/package.py",
        "tools/skia/identify_toolchain.py",
        "tools/skia/aggregate.py",
        "tools/skia/publish_release.py",
        ".github/workflows/skia-sdk-r1-full-producer.yml",
    }
    if any(path in full_exact for path in normalized):
        return {"mode": "full", "targets": ALL_TARGETS, "reason": "SDK identity or recipe"}

    platform_targets: set[str] = set()
    for path in normalized:
        for family, targets in PLATFORM_TARGETS.items():
            if path.startswith(f"tools/skia/platform/{family}/"):
                platform_targets.update(targets)
    if platform_targets:
        return {
            "mode": "platform",
            "targets": [target for target in ALL_TARGETS if target in platform_targets],
            "reason": "platform-specific producer source",
        }

    consumer_prefixes = (
        "tools/skia/fetch.py",
        "tools/skia/consumer.py",
        "tools/skia/update_lock.py",
        "tools/skia/reuse_artifact.py",
        "tools/skia/verify.py",
        "tools/skia/check_consumer_ci.py",
        "tools/skia/cmake/",
        "tools/skia/cmake_probe/",
        "tools/skia/tests/",
        "r1-full-skia-sdk.lock.json",
        ".github/workflows/r1-full-consumer-validation.yml",
    )
    if any(
        (path == prefix or path.startswith(prefix)) and path not in CLASSIFICATION_ONLY_PATHS
        for path in normalized for prefix in consumer_prefixes
    ):
        return {"mode": "consumer", "targets": [], "reason": "consumer or schema validation"}

    if any(path in CLASSIFICATION_ONLY_PATHS for path in normalized):
        return {"mode": "none", "targets": [], "reason": "classification orchestration change"}

    return {"mode": "none", "targets": [], "reason": "documentation or unrelated change"}


def build_matrix(targets: list[str]) -> dict[str, list[dict[str, str]]]:
    unknown = sorted(set(targets) - set(TARGET_MATRIX))
    if unknown:
        raise ValueError(f"unknown R1 Skia target(s): {', '.join(unknown)}")
    include = []
    for target in ALL_TARGETS:
        if target not in targets:
            continue
        for variant in ("release", "debug", "asan"):
            include.append({
                "target": target, "variant": variant, **TARGET_MATRIX[target],
            })
    if not include:
        raise ValueError("R1 Skia producer matrix cannot be empty")
    return {"include": include}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--matrix", help="emit a target × variant matrix for this JSON array")
    parser.add_argument("--lock-base", type=Path)
    parser.add_argument("--lock-head", type=Path)
    args = parser.parse_args()
    if args.matrix is not None:
        targets = json.loads(args.matrix)
        if not isinstance(targets, list) or any(not isinstance(item, str) for item in targets):
            parser.error("--matrix must be a JSON string array")
        print(json.dumps(build_matrix(targets), separators=(",", ":")))
        return 0
    paths = args.paths
    # The PR workflow pipes `git diff --name-only` into this tool. Keep the
    # positional form useful for local checks, but make the pipeline form
    # equivalent instead of silently classifying an empty change set.
    if not paths and not sys.stdin.isatty():
        paths = [line.strip() for line in sys.stdin if line.strip()]
    lock_before = None
    lock_after = None
    if args.lock_base is not None or args.lock_head is not None:
        if args.lock_base is None or args.lock_head is None:
            parser.error("--lock-base and --lock-head must be provided together")
        lock_before = json.loads(args.lock_base.read_text(encoding="utf-8"))
        lock_after = json.loads(args.lock_head.read_text(encoding="utf-8"))
    print(json.dumps(classify(paths, lock_before=lock_before, lock_after=lock_after), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
