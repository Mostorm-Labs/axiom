#!/usr/bin/env python3
"""Validate the G0-01 verification workspace skeleton deterministically."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "verification/workspace.json"
SCHEMA = ROOT / "verification/schema/workspace-manifest.schema.json"
PATH = re.compile(r"^(?!/)(?![A-Za-z]:)(?!.*(?:^|/)\.\.(?:/|$))(?!.*//)[^\s]+$")
REQUIRED_DIRS = ("corpus", "fixtures", "runners", "reports", "evidence", "schema")
CORPUS_OWNERS = {
    "protocol": "GT-G0-02",
    "semantic": "GT-G0-06",
    "platform": "GT-G0-09",
    "golden": "GT-G0-08",
    "performance": "GT-G0-09",
    "fault": "GT-G0-04",
    "physical": "GT-G0-13",
}
REQUIRED_SERVICES = ("fake-clock", "deterministic-random", "stable-id-fixture", "fake-resource-provider")


class WorkspaceError(ValueError):
    """A manifest or workspace ownership error."""


def load_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise WorkspaceError(f"cannot read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise WorkspaceError(f"JSON root must be an object: {path}")
    return value


def require_keys(value: dict, keys: set[str], label: str) -> None:
    actual = set(value)
    missing = keys - actual
    unknown = actual - keys
    if missing or unknown:
        details = []
        if missing:
            details.append("missing=" + ",".join(sorted(missing)))
        if unknown:
            details.append("unknown=" + ",".join(sorted(unknown)))
        raise WorkspaceError(f"{label}: " + "; ".join(details))


def relative_path(value: object, label: str) -> Path:
    if not isinstance(value, str) or not PATH.fullmatch(value):
        raise WorkspaceError(f"{label}: unsafe relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise WorkspaceError(f"{label}: unsafe relative path")
    return path


def validate(manifest: dict) -> str:
    require_keys(manifest, {
        "schema_version", "format", "workspace_id", "repository", "promotion_route",
        "gate", "state", "directories", "corpus", "services", "policies",
    }, "manifest")
    if manifest["schema_version"] != 1 or manifest["format"] != "axiom-verification-workspace":
        raise WorkspaceError("manifest: unsupported schema or format")
    if manifest["workspace_id"] != "axiom-g0-verification":
        raise WorkspaceError("manifest: unexpected workspace_id")
    if manifest["repository"] != "Mostorm-Labs/axiom" or manifest["gate"] != "G0":
        raise WorkspaceError("manifest: repository or gate mismatch")
    if manifest["state"] not in {"skeleton", "active"}:
        raise WorkspaceError("manifest: invalid state")

    directories = manifest["directories"]
    if not isinstance(directories, dict):
        raise WorkspaceError("manifest.directories: must be an object")
    require_keys(directories, set(REQUIRED_DIRS), "manifest.directories")
    for name, value in directories.items():
        path = ROOT / relative_path(value, f"manifest.directories.{name}")
        if not path.is_dir():
            raise WorkspaceError(f"manifest.directories.{name}: directory does not exist: {value}")

    corpus = manifest["corpus"]
    if not isinstance(corpus, list) or len(corpus) != len(CORPUS_OWNERS):
        raise WorkspaceError("manifest.corpus: expected exactly seven entries")
    corpus_ids: list[str] = []
    for entry in corpus:
        if not isinstance(entry, dict):
            raise WorkspaceError("manifest.corpus: entry must be an object")
        require_keys(entry, {"id", "path", "owner", "status", "expected_policy"}, "corpus entry")
        corpus_ids.append(entry["id"])
        if entry["id"] not in CORPUS_OWNERS or entry["owner"] != CORPUS_OWNERS[entry["id"]]:
            raise WorkspaceError(f"corpus entry {entry['id']!r}: invalid id or owner")
        path = ROOT / relative_path(entry["path"], f"corpus.{entry['id']}.path")
        if not path.is_dir():
            raise WorkspaceError(f"corpus {entry['id']}: directory does not exist")
        if entry["status"] != "reserved" or entry["expected_policy"] not in {"reviewed", "explicit-review"}:
            raise WorkspaceError(f"corpus {entry['id']}: invalid status or expected policy")
    if sorted(corpus_ids) != sorted(CORPUS_OWNERS):
        raise WorkspaceError("manifest.corpus: ids must be unique and complete")

    services = manifest["services"]
    if not isinstance(services, list) or len(services) != len(REQUIRED_SERVICES):
        raise WorkspaceError("manifest.services: expected exactly four entries")
    service_ids: list[str] = []
    for entry in services:
        if not isinstance(entry, dict):
            raise WorkspaceError("manifest.services: entry must be an object")
        require_keys(entry, {"id", "owner", "status"}, "service entry")
        service_ids.append(entry["id"])
        if entry["id"] not in REQUIRED_SERVICES or entry["owner"] != "GT-G0-01" or entry["status"] != "reserved":
            raise WorkspaceError(f"service entry {entry['id']!r}: invalid ownership")
    if sorted(service_ids) != sorted(REQUIRED_SERVICES):
        raise WorkspaceError("manifest.services: ids must be unique and complete")

    policies = manifest["policies"]
    if not isinstance(policies, dict):
        raise WorkspaceError("manifest.policies: must be an object")
    require_keys(policies, {"encoding", "canonical_json", "expected_artifacts", "absolute_paths", "generated_outputs"}, "manifest.policies")
    if policies["encoding"] != "UTF-8" or policies["absolute_paths"] != "forbidden":
        raise WorkspaceError("manifest.policies: unsafe encoding or path policy")

    canonical = json.dumps(manifest, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--schema", type=Path, default=SCHEMA)
    parser.add_argument("--print-digest", action="store_true")
    args = parser.parse_args(argv)
    try:
        schema = load_json(args.schema)
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise WorkspaceError("schema: expected JSON Schema draft 2020-12")
        if schema.get("$id", "").endswith("workspace-manifest-v1.json") is False:
            raise WorkspaceError("schema: unexpected $id")
        digest = validate(load_json(args.manifest))
    except (OSError, WorkspaceError) as exc:
        print(f"workspace validation failed: {exc}", file=sys.stderr)
        return 1
    print("workspace: valid")
    if args.print_digest:
        print(f"manifest_sha256: {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
