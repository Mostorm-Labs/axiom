#!/usr/bin/env python3
"""Generate commit-bound GT-G1-02 codec Evidence without promoting candidates."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DESCRIPTOR = "schema/axiom/v1/descriptor/descriptor.lock.pb"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def generate(
    root: Path,
    source_commit: str,
    output_root: Path,
    hosted_url: str | None,
    runtime_status: str,
    generated_code_sha256: str,
) -> dict[str, Any]:
    descriptor_path = root / DESCRIPTOR
    descriptor_hash = sha256(descriptor_path)
    differential_path = output_root / "differential.json"
    if differential_path.exists():
        differential = json.loads(differential_path.read_text(encoding="utf-8"))
    else:
        tool_path = root / "verification/tools/run_g1_semantic_differential.py"
        spec = importlib.util.spec_from_file_location("g1_semantic_differential", tool_path)
        if spec is None or spec.loader is None:
            raise RuntimeError(f"cannot load differential tool: {tool_path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        differential = module.run_differential(root)
    reasons: list[str] = []
    if runtime_status != "PASS":
        reasons.append("hosted semantic-codec runtime/codegen Evidence is missing or failed")
    if not hosted_url:
        reasons.append("hosted semantic-codec workflow URL is missing")
    if differential.get("status") != "PASS":
        reasons.append("BG/BGX authority-promoted corpus and differential oracle are missing")
    status = "PASS" if not reasons else "BLOCKED"
    result: dict[str, Any] = {
        "format": "axiom-gt-g1-02-commit-bound-evidence-v1",
        "taskId": "GT-G1-02",
        "sourceCommit": source_commit,
        "status": status,
        "blockingReasons": reasons,
        "hostedWorkflowUrl": hosted_url,
        "runtimeStatus": runtime_status,
        "descriptorPath": DESCRIPTOR,
        "descriptorSha256": descriptor_hash,
        "generatedCodeSha256": generated_code_sha256,
        "differential": differential,
    }
    output_root.mkdir(parents=True, exist_ok=True)
    (output_root / "summary.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    files = []
    for path in sorted(output_root.iterdir()):
        if path.name == "manifest.json" or not path.is_file():
            continue
        files.append({"path": path.name, "sha256": sha256(path)})
    manifest = {
        "format": "axiom-gt-g1-02-evidence-manifest-v1",
        "taskId": "GT-G1-02",
        "sourceCommit": source_commit,
        "status": status,
        "files": files,
    }
    (output_root / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--hosted-url")
    parser.add_argument("--runtime-status", choices=("PASS", "FAIL", "BLOCKED"), default="BLOCKED")
    parser.add_argument("--generated-code-sha256", required=True)
    args = parser.parse_args()
    result = generate(ROOT, args.source_commit, args.output_root, args.hosted_url, args.runtime_status, args.generated_code_sha256)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
