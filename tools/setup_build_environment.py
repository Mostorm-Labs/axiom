#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEPS_ROOT = ROOT / ".deps"
SEMANTIC_ROOT = DEPS_ROOT / "protobuf"
SEMANTIC_LOCK = ROOT / "semantic-toolchain.lock.json"


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_semantic_install(root: Path) -> dict[str, str]:
    root = root.resolve()
    required = {
        "AXIOM_PROTOC": root / "bin/protoc",
        "PROTOBUF_DIR": root / "lib/cmake/protobuf",
        "ABSL_DIR": root / "lib/cmake/absl",
        "UTF8_RANGE_DIR": root / "lib/cmake/utf8_range",
    }
    required_files = (
        root / "bin/protoc",
        root / "lib/cmake/protobuf/protobuf-config.cmake",
        root / "lib/cmake/absl/abslConfig.cmake",
        root / "lib/cmake/utf8_range/utf8_range-config.cmake",
    )
    missing = [str(path) for path in required_files if not path.is_file()]
    if missing:
        raise RuntimeError(
            "semantic SDK consumer contract is incomplete: " + ", ".join(missing)
        )
    return {
        "AXIOM_DEPS_DIR": str(DEPS_ROOT.resolve()),
        "AXIOM_SEMANTIC_SDK_ROOT": str(root),
        "CMAKE_PREFIX_PATH": str(root),
        **{name: str(path.resolve()) for name, path in required.items()},
    }


def setup_environment(*, core: bool, semantic: bool, target: str) -> dict[str, Any]:
    if target != "linux-x86_64":
        raise RuntimeError(f"unsupported build-environment target: {target}")
    if core:
        subprocess.run(
            [sys.executable, "tools/bootstrap_deps.py", "--core"],
            cwd=ROOT,
            check=True,
        )
    semantic_fetch: dict[str, Any] | None = None
    environment = {"AXIOM_DEPS_DIR": str(DEPS_ROOT.resolve())}
    if semantic:
        result = subprocess.run(
            [
                sys.executable,
                "tools/semantic_fetch.py",
                "--target",
                target,
                "--destination",
                str(SEMANTIC_ROOT),
            ],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        if result.stdout:
            print(result.stdout, end="")
        lines = result.stdout.splitlines()
        if not lines:
            raise RuntimeError("semantic fetch produced no machine-readable result")
        semantic_fetch = json.loads(lines[-1])
        environment.update(validate_semantic_install(SEMANTIC_ROOT))
    lock = json.loads(SEMANTIC_LOCK.read_text(encoding="utf-8")) if semantic else None
    return {
        "format": "axiom-build-environment-v1",
        "target": target,
        "environment": environment,
        "core": {
            "enabled": core,
            "depsLockSha256": file_sha256(ROOT / "deps.lock.json"),
        },
        "semantic": None if not semantic else {
            "enabled": True,
            "lockSha256": file_sha256(SEMANTIC_LOCK),
            "releaseTag": lock["releaseTag"],
            "asset": lock["asset"],
            "sdkId": lock["sdkId"],
            "sha256": lock["sha256"],
            "fetch": semantic_fetch,
        },
    }


def write_github_env(path: Path, environment: dict[str, str]) -> None:
    with path.open("a", encoding="utf-8") as output:
        for name in (
            "AXIOM_DEPS_DIR",
            "AXIOM_SEMANTIC_SDK_ROOT",
            "CMAKE_PREFIX_PATH",
            "PROTOBUF_DIR",
            "ABSL_DIR",
            "UTF8_RANGE_DIR",
            "AXIOM_PROTOC",
        ):
            if name in environment:
                output.write(f"{name}={environment[name]}\n")


def write_facts(path: Path, facts: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(facts, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", action="store_true")
    parser.add_argument("--semantic", action="store_true")
    parser.add_argument("--target", default="linux-x86_64")
    parser.add_argument("--github-env", type=Path)
    parser.add_argument("--facts-output", type=Path)
    args = parser.parse_args()

    facts = setup_environment(core=args.core, semantic=args.semantic, target=args.target)
    environment = facts["environment"]
    if args.github_env:
        write_github_env(args.github_env, environment)
    if args.facts_output:
        write_facts(args.facts_output, facts)
    print(json.dumps(facts, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
