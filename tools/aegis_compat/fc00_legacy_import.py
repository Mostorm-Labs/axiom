"""One-time authored-manifest import; official Aegis alone computes state."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import subprocess
from pathlib import Path

SOURCE_REVISION = "6fc1508f1a971abc5a90e068d0cc5f6ae82c064b"
PACKAGE_REVISION = "06bcf8348f7898ddfb3a327a81ba1874857e47e1"
PACKAGE_PATH = ".aegis/packages/FC-00-LEGACY-IMPORT"
MANIFEST_NAMES = ("project.json", "authorities.json", "gates.json",
                  "evidence.json", "integrations.json", "state.json")


def git_bytes(repository: Path, subject: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(repository), "show", subject])


def render(value: dict) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def build_import(repository: Path | str, destination: Path | str, *,
                 archive_root: Path | str) -> Path:
    """Verify frozen inputs, then write authored output; never generate state."""
    repository, destination, archive_root = map(Path, (repository, destination, archive_root))
    package = json.loads(git_bytes(repository, f"{PACKAGE_REVISION}:{PACKAGE_PATH}/package.json"))
    locked = {}
    for key, ref in package.items():
        if key.endswith("_ref") and isinstance(ref, dict) and "sha256" in ref:
            raw = git_bytes(repository, f"{PACKAGE_REVISION}:{PACKAGE_PATH}/{ref['path']}")
            if hashlib.sha256(raw).hexdigest() != ref["sha256"]:
                raise ValueError(f"package SHA-256 mismatch: {ref['path']}")
            locked[ref["path"]] = json.loads(raw) if ref["path"].endswith(".json") else raw
    fixture = locked["expected-crosswalk.json"]
    raw_source = {name: git_bytes(repository, f"{SOURCE_REVISION}:.aegis/{name}")
                  for name in MANIFEST_NAMES}
    identities = {}
    for name, raw in raw_source.items():
        path = f".aegis/{name}"
        blob = hashlib.sha1(f"blob {len(raw)}\0".encode() + raw).hexdigest()
        if blob != fixture["provenance"]["original_blob_sha1"][path]:
            raise ValueError(f"source Git blob mismatch: {path}")
        identities[path] = {"sha1": blob, "sha256": hashlib.sha256(raw).hexdigest(),
                            "length": len(raw)}
    source = {name: json.loads(raw) for name, raw in raw_source.items()}
    source_gates = {item["id"]: item for item in source["gates.json"]["gates"]}
    formal = fixture["gate_decision_stage_map"]
    readiness_ids = {item["gate_id"] for item in fixture["legacy_readiness_records"]}
    if set(source_gates) != {item["gate_id"] for item in formal} | readiness_ids:
        raise ValueError("source Gate cohort differs from frozen crosswalk")
    gates, decisions = [], []
    for mapping in formal:
        old = source_gates[mapping["gate_id"]]
        decision = old["decision"]
        if (decision["id"], decision["verdict"], old["evidence_ids"],
            [decision["authority_ref"]]) != (
                mapping["legacy_decision_id"], mapping["verdict"],
                mapping["evidence_ids"], mapping["authority_ids"]):
            raise ValueError(f"source Gate facts differ: {old['id']}")
        gate = {key: copy.deepcopy(value) for key, value in old.items()
                if key not in {"decision", "evidence_ids"}}
        gate.update(stage=mapping["stage"], authority_ids=mapping["authority_ids"])
        gates.append(gate)
        imported = copy.deepcopy(decision)
        imported.update(id=mapping["normalized_decision_id"], gate_id=old["id"],
                        legacy_decision_id=decision["id"], evidence_ids=old["evidence_ids"])
        decisions.append(imported)
    integration_map = {item["integration_id"]: item for item in fixture["integration_binding_map"]}
    old_integrations = source["integrations.json"]["integrations"]
    if {item["id"] for item in old_integrations} != set(integration_map):
        raise ValueError("source Integration cohort differs from frozen crosswalk")
    integrations = []
    for old in old_integrations:
        mapping = integration_map[old["id"]]
        if old["gate_decision_binding"] != {"kind": "bound",
                                          "gate_decision_id": mapping["legacy_decision_id"]}:
            raise ValueError(f"source binding differs: {old['id']}")
        for key in ("status", "kind", "ref", "target_ref", "integrated_revision", "evidence_ids"):
            if old.get(key) != mapping[f"original_{key}"]:
                raise ValueError(f"source occurrence differs: {old['id']} {key}")
        imported = copy.deepcopy(old)
        imported["gate_decision_binding"]["gate_decision_id"] = mapping["normalized_decision_id"]
        # A closed branch review is not a merged occurrence. Its original
        # reviewed revision remains in the immutable archive and crosswalk.
        if imported["status"] == "closed_unmerged":
            imported.pop("integrated_revision")
        integrations.append(imported)
    evidence = []
    for old in source["evidence.json"]["evidence"]:
        imported = copy.deepcopy(old)
        imported["type"] = imported.pop("kind")
        evidence.append(imported)
    documents = {
        "project.json": {"schema_version": "0.6", "project":
                         {"id": source["project.json"]["project_id"],
                          "name": source["project.json"]["project_name"], "profile": "standard"}},
        "authorities.json": copy.deepcopy(source["authorities.json"]),
        "gates.json": {"schema_version": "0.6", "gates": gates, "decisions": decisions},
        "evidence.json": {"schema_version": "0.6", "evidence": evidence},
        "integrations.json": {"schema_version": "0.6", "integrations": integrations},
    }
    # Prepare all output and check existing artifacts before writing any file.
    output = {destination / ".aegis" / name: render(doc) for name, doc in documents.items()}
    output.update({archive_root / "legacy-source" / SOURCE_REVISION / name: raw
                   for name, raw in raw_source.items()})
    output[archive_root / "legacy-readiness.json"] = render({
        "source_revision": SOURCE_REVISION,
        "records": [source_gates[item["gate_id"]] for item in fixture["legacy_readiness_records"]]})
    output[archive_root / "source-blob-identities.json"] = render({
        "repository": "Mostorm-Labs/axiom", "revision": SOURCE_REVISION, "files": identities})
    for path, raw in output.items():
        if path.exists() and path.read_bytes() != raw:
            raise ValueError(f"refusing to replace an existing import artifact: {path}")
    for path, raw in output.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    return destination


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--archive-root", required=True, type=Path)
    args = parser.parse_args(argv)
    build_import(args.repository, args.destination, archive_root=args.archive_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
