#!/usr/bin/env python3
"""Resolve locked SDKs into a shared Store and export real build paths."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.sdk.model import HostPlatform, OfflineError, SdkError, detect_host_platform
from tools.sdk.model import ResolveRequest
from tools.sdk.resolver import ProviderRegistry
from tools.sdk.store import default_store_root
from tools.semantic.contract import LOCK_FORMAT, read_json, validate_v2_lock
from tools.semantic.provider import SemanticProvider, select_keys, select_lock_path

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
        "AXIOM_SEMANTIC_RUNTIME_ROOT": str(root),
        "AXIOM_SEMANTIC_HOST_ROOT": str(root),
        "CMAKE_PREFIX_PATH": str(root),
        **{name: str(path.resolve()) for name, path in required.items()},
    }


def _fetch_semantic(target: str, *, lock_path: Path | None = None,
                    destination: Path | None = None, repo_root: Path | None = None) -> dict[str, Any]:
    """Explicit v1 subprocess retained only for historical/transition consumers."""
    try:
        result = subprocess.run(
            [
                sys.executable,
                "tools/semantic_fetch.py",
                "--target",
                target,
                "--destination",
                str(destination if destination is not None else SEMANTIC_ROOT),
                "--lock",
                str(lock_path if lock_path is not None else SEMANTIC_LOCK),
            ],
            cwd=repo_root if repo_root is not None else ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, end="")
        if error.stderr:
            print(error.stderr, end="", file=sys.stderr)
        raise
    if result.stdout:
        print(result.stdout, end="")
    lines = result.stdout.splitlines()
    if not lines:
        raise RuntimeError("semantic fetch produced no machine-readable result")
    return json.loads(lines[-1])


def setup_environment(*, core: bool, semantic: bool, target: str = "native",
                      repo_root: Path | None = None, host: HostPlatform | None = None,
                      store_root: Path | None = None, mirror: str | None = None,
                      offline: bool = False, lock_path: Path | None = None) -> dict[str, Any]:
    if offline and core:
        raise SdkError("--offline cannot be combined with --core: lightweight source bootstrap may use network")
    root = ROOT if repo_root is None else repo_root.expanduser().resolve()
    deps = DEPS_ROOT if repo_root is None else root / ".deps"
    legacy_root = SEMANTIC_ROOT if repo_root is None else deps / "protobuf"
    host = host if host is not None else detect_host_platform()
    _, target = select_keys(host, target)
    lock = None
    selected_lock = None
    mode = None
    if semantic:
        selected_lock = select_lock_path(root, lock_path)
        lock = read_json(selected_lock)
        if lock.get("format") == LOCK_FORMAT:
            validate_v2_lock(lock)
            mode = "v2"
        elif lock.get("format") == "axiom-semantic-toolchain-lock-v1":
            mode = "v1"
            if offline:
                raise SdkError("semantic: --offline requires v2 Store authority; v1 acquisition is not offline")
            if host.key != "linux-x86_64" or target != "linux-x86_64":
                raise SdkError("semantic: historical v1 is Linux-only; this host/target requires a v2 lock")
            if store_root is not None or mirror is not None:
                raise SdkError("semantic: --store/--mirror require v2 authority, not the historical v1 path")
        else:
            raise SdkError("semantic: unsupported lock format; refusing fallback")
    if core:
        subprocess.run([sys.executable, "tools/bootstrap_deps.py", "--core"], cwd=root, check=True)
    environment = {"AXIOM_DEPS_DIR": str(deps.resolve())}
    semantic_facts = None
    if mode == "v2":
        store = store_root if store_root is not None else default_store_root()
        request = ResolveRequest(root, host, target, store,
                                 mirror if mirror is not None else os.environ.get("AXIOM_SDK_MIRROR"), offline)
        providers = ProviderRegistry()
        providers.register(SemanticProvider(selected_lock))
        resolved = providers.resolve("semantic", request)
        environment.update(resolved["environment"])
        semantic_facts = {"enabled": True, "mode": "v2", "lockSha256": file_sha256(selected_lock),
                          "releaseSetId": lock["releaseSetId"], "releaseTag": lock["releaseTag"],
                          "indexSha256": lock["indexSha256"], "resolution": resolved["facts"]}
    elif mode == "v1":
        fetched = _fetch_semantic(target, lock_path=selected_lock, destination=legacy_root, repo_root=root)
        environment.update(validate_semantic_install(legacy_root))
        environment["AXIOM_DEPS_DIR"] = str(deps.resolve())
        semantic_facts = {"enabled": True, "mode": "v1", "lockSha256": file_sha256(selected_lock),
                          "releaseTag": lock["releaseTag"], "asset": lock["asset"],
                          "sdkId": lock["sdkId"], "sha256": lock["sha256"], "fetch": fetched}
    return {"format": "axiom-build-environment-v2" if mode == "v2" else "axiom-build-environment-v1",
            "host": host.key, "target": target, "environment": environment,
            "core": {"enabled": core, "depsLockSha256": file_sha256(root / "deps.lock.json")},
            "semantic": semantic_facts}


def write_github_env(path: Path, environment: dict[str, str]) -> None:
    names = ("AXIOM_DEPS_DIR", "AXIOM_SEMANTIC_HOST_ROOT", "AXIOM_SEMANTIC_RUNTIME_ROOT",
             "AXIOM_SEMANTIC_SDK_ROOT", "CMAKE_PREFIX_PATH", "PROTOBUF_DIR", "ABSL_DIR",
             "UTF8_RANGE_DIR", "AXIOM_PROTOC")
    lines = []
    for name in names:
        if name in environment:
            value = environment[name]
            if not isinstance(value, str) or any(char in value for char in "\r\n\0"):
                raise SdkError(f"cannot safely export {name} to GitHub environment")
            lines.append(f"{name}={value}\n")
    with path.open("a", encoding="utf-8", newline="\n") as output:
        output.writelines(lines)


def write_status(facts: dict[str, Any]) -> None:
    """Human diagnostics go to stderr; stdout remains machine-readable JSON."""
    lines = ["Axiom Build Environment", f"Host: {facts['host']}", f"Target: {facts['target']}"]
    semantic = facts.get("semantic")
    if semantic and semantic["mode"] == "v2":
        resolution = semantic["resolution"]
        lines += [f"Semantic release: {semantic['releaseTag']}",
                  f"releaseSetId: {semantic['releaseSetId']}"]
        for item in resolution["artifacts"]:
            lines += [f"{item['kind']}: {item['key']} / {item['identity']}",
                      f"  source: {item['source']}", f"  path: {item['root']}"]
        lines += ["ABI: " + json.dumps(resolution["metadata"]["abi"], sort_keys=True),
                  "Network: " + ("used" if resolution["networkUsed"] else "not used")]
    elif semantic:
        lines += [f"Semantic: historical v1 transition ({semantic['releaseTag']})", "Network: v1 acquisition"]
    print("\n".join(lines), file=sys.stderr)


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
    parser.add_argument("--target", default="native")
    parser.add_argument("--lock", type=Path, help="Explicit Semantic lock; default prefers repository v2")
    parser.add_argument("--store", type=Path, help="Shared SDK Store (overrides AXIOM_SDK_STORE)")
    parser.add_argument("--mirror", help="Mirror (overrides AXIOM_SDK_MIRROR)")
    parser.add_argument("--offline", action="store_true", help="Require already-materialized v2 packages")
    parser.add_argument("--status", action="store_true", help="Resolve Semantic and report diagnostics to stderr")
    parser.add_argument("--github-env", type=Path)
    parser.add_argument("--facts-output", type=Path)
    args = parser.parse_args()

    facts = setup_environment(core=args.core, semantic=args.semantic or args.status or args.lock is not None,
                              target=args.target, store_root=args.store, mirror=args.mirror,
                              offline=args.offline, lock_path=args.lock)
    if args.status:
        write_status(facts)
    environment = facts["environment"]
    if args.github_env:
        write_github_env(args.github_env, environment)
    if args.facts_output:
        write_facts(args.facts_output, facts)
    print(json.dumps(facts, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except OfflineError as error:
        print(f"semantic: offline resolution failed: {error}", file=sys.stderr)
        raise SystemExit(1)
    except (SdkError, OSError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
