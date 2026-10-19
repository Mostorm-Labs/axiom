"""Independent frozen FC-00 historical oracle and evidence collector.

Expected facts are parsed from immutable source blobs and the pre-P32 fixture.
The oracle never imports or invokes converter code; only importer behavior tests do.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TASK = "FC-00-LEGACY-IMPORT"
SOURCE_REVISION = "6fc1508f1a971abc5a90e068d0cc5f6ae82c064b"
PACKAGE_REVISION = "06bcf8348f7898ddfb3a327a81ba1874857e47e1"
TOOL_REVISION = "00d3690ca2cdde15b3f919c54ccc8ef6290f55ba"
PACKAGE = ROOT / ".aegis/packages" / TASK
ARCHIVE = ROOT / ".aegis/legacy" / TASK
NAMES = ("project.json", "authorities.json", "gates.json", "evidence.json",
         "integrations.json", "state.json")
AUTHORED = NAMES[:-1]


def git(*args, root=ROOT):
    return subprocess.check_output(["git", "-C", str(root), *args])


def json_read(path):
    return json.loads(Path(path).read_bytes())


def rendered(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode()


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def blob(raw):
    return hashlib.sha1(f"blob {len(raw)}\0".encode() + raw).hexdigest()


def source_bytes():
    return {name: git("show", f"{SOURCE_REVISION}:.aegis/{name}") for name in NAMES}


def documents(root):
    return {name: json_read(Path(root) / ".aegis" / name) for name in AUTHORED}


def fixture():
    return json.loads(git("show", f"{PACKAGE_REVISION}:.aegis/packages/{TASK}/expected-crosswalk.json"))


def tool_root():
    value = os.environ.get("FC00_AEGIS_ROOT")
    if not value:
        raise RuntimeError("set FC00_AEGIS_ROOT to a checkout of the frozen Aegis tool")
    root = Path(value).resolve()
    if git("rev-parse", "HEAD", root=root).decode().strip() != TOOL_REVISION:
        raise RuntimeError("Aegis tool revision mismatch")
    if git("status", "--porcelain", root=root).strip():
        raise RuntimeError("Aegis tool checkout is modified")
    remote = git("remote", "get-url", "origin", root=root).decode().strip().lower()
    if not remote.endswith("mostorm-labs/aegis.git"):
        raise RuntimeError("Aegis tool repository mismatch")
    return root


def official(root, *command):
    tool = tool_root()
    env = os.environ.copy()
    env["PYTHONPATH"] = str(tool)
    proc = subprocess.run([sys.executable, "-m", "tools.aegis_state.cli",
                           *command[:1], str(root), *command[1:]], cwd=tool, env=env,
                          text=True, capture_output=True)
    return {"command": ["python", "-m", "tools.aegis_state.cli", *command[:1],
                        str(root), *command[1:]], "exit_code": proc.returncode,
            "stdout": proc.stdout, "stderr": proc.stderr}


def by_id(items, label, errors):
    result = {item["id"]: item for item in items}
    if len(result) != len(items):
        errors.append(f"{label}: duplicate identities")
    return result


def compare_import(target, archive_raw, readiness):
    """Compare actual target against independently parsed original facts."""
    expected = fixture()
    original_raw = source_bytes()
    old = {name: json.loads(raw) for name, raw in original_raw.items()}
    errors = []
    for name in NAMES:
        raw = archive_raw.get(name, b"")
        if raw != original_raw[name] or blob(raw) != expected["provenance"]["original_blob_sha1"][f".aegis/{name}"]:
            errors.append(f"source_blob: {name}")
    if any(doc.get("schema_version") != "0.6" for doc in target.values()):
        errors.append("schema: expected v0.6")
    if target["project.json"] != {"schema_version": "0.6", "project":
                                 {"id": "axiom", "name": "Axiom", "profile": "standard"}}:
        errors.append("project: identity/profile/lifecycle_hint")
    old_gates = by_id(old["gates.json"]["gates"], "legacy gates", errors)
    gate_map = {m["gate_id"]: m for m in expected["gate_decision_stage_map"]}
    target_gates = by_id(target["gates.json"].get("gates", []), "gates", errors)
    target_decisions = by_id(target["gates.json"].get("decisions", []), "decisions", errors)
    if set(target_gates) != set(gate_map) or len(target_gates) != 25:
        errors.append("gate_cohort: readiness promotion or missing formal Gate")
    if set(target_decisions) != {m["normalized_decision_id"] for m in gate_map.values()} or len(target_decisions) != 25:
        errors.append("decision_cohort: new/dropped decision")
    for gid, mapping in gate_map.items():
        original = old_gates[gid]
        decision = original["decision"]
        if (decision["id"], decision["verdict"], original["evidence_ids"], [decision["authority_ref"]]) != (
                mapping["legacy_decision_id"], mapping["verdict"], mapping["evidence_ids"], mapping["authority_ids"]):
            errors.append(f"fixture_source: {gid}")
        contract = target_gates.get(gid, {})
        if contract.get("stage") != mapping["stage"] or contract.get("authority_ids") != mapping["authority_ids"]:
            errors.append(f"gate_facts: {gid}")
        # Official v0.6 Gate Contracts retain identity, stage and authority
        # bindings; legacy lifecycle/status/scope fields remain in the archive.
        new = target_decisions.get(mapping["normalized_decision_id"], {})
        if new.get("gate_id") != gid or new.get("evidence_ids") != original["evidence_ids"]:
            errors.append(f"decision_identity: {gid}")
        for key, value in decision.items():
            if key not in {"id", "authority_ref", "result_revision", "official_pass_claimed", "source_authority", "review_ref", "reviewed_branch_head", "integration_revision", "source_report", "accepted_capability", "physical_pass", "pressure_pass"} and new.get(key) != value:
                errors.append(f"decision_fact: {gid}/{key}")
    ready = [old_gates[item["gate_id"]] for item in expected["legacy_readiness_records"]]
    if readiness != {"source_revision": SOURCE_REVISION, "records": ready}:
        errors.append("readiness_archive: incomplete raw records")
    originals = by_id(old["integrations.json"]["integrations"], "legacy integrations", errors)
    imported = by_id(target["integrations.json"]["integrations"], "integrations", errors)
    imap = {m["integration_id"]: m for m in expected["integration_binding_map"]}
    if set(originals) != set(imported) or set(originals) != set(imap) or len(imported) != 29:
        errors.append("integration_cohort: identity set")
    for iid, original in originals.items():
        mapping = imap[iid]
        new = imported.get(iid, {})
        if original["gate_decision_binding"] != {"kind": "bound", "gate_decision_id": mapping["legacy_decision_id"]}:
            errors.append(f"fixture_binding: {iid}")
        if new.get("gate_decision_binding") != {"kind": "bound", "gate_decision_id": mapping["normalized_decision_id"]}:
            errors.append(f"binding: {iid}")
        for key, value in original.items():
            if key == "gate_decision_binding":
                continue
            if key == "integrated_revision" and original["status"] == "closed_unmerged":
                if key in new:
                    errors.append(f"closed_revision: {iid}")
                if mapping["original_integrated_revision"] != value:
                    errors.append(f"closed_provenance: {iid}")
            elif new.get(key) != value:
                errors.append(f"occurrence: {iid}/{key}")
        for key in ("kind", "ref", "target_ref", "status", "integrated_revision", "evidence_ids"):
            if original.get(key) != mapping[f"original_{key}"]:
                errors.append(f"fixture_occurrence: {iid}/{key}")
    if sum(i["status"] == "integrated" for i in imported.values()) != 27 or sum(i["status"] == "closed_unmerged" for i in imported.values()) != 2:
        errors.append("occurrence_status: 27/2")
    old_ev = by_id(old["evidence.json"]["evidence"], "legacy evidence", errors)
    new_ev = by_id(target["evidence.json"]["evidence"], "evidence", errors)
    if set(old_ev) != set(new_ev) or len(new_ev) != 34:
        errors.append("evidence_cohort")
    for eid, record in old_ev.items():
        new = new_ev.get(eid, {})
        if new.get("type") != record["kind"] or "kind" in new:
            errors.append(f"evidence_type: {eid}")
        for key, value in record.items():
            if key != "kind" and new.get(key) != value:
                errors.append(f"evidence_fact: {eid}/{key}")
    if target["authorities.json"] != old["authorities.json"] or len(target["authorities.json"]["authorities"]) != 4:
        errors.append("authorities: all four records/supersession must remain exact")
    return errors


def archive_inputs(root=ARCHIVE):
    raw = {name: (root / "legacy-source" / SOURCE_REVISION / name).read_bytes() for name in NAMES}
    return raw, json_read(root / "legacy-readiness.json")


def negative_cases(target, raw, readiness):
    cases = []
    wrong = copy.deepcopy(target)
    item = next(i for i in wrong["integrations.json"]["integrations"] if i["id"] == "int-pr85-g3-01")
    item["gate_decision_binding"]["gate_decision_id"] = "gt-g3-03::decision::0001"
    cases.append(("different-valid-PASS-binding", wrong, raw, readiness, "binding:"))
    promoted = copy.deepcopy(target)
    promoted["gates.json"]["gates"].append({"id": "g3-entry-readiness", "stage": "P34",
                                          "authority_ids": ["g3-capability-authority"]})
    promoted["gates.json"]["decisions"].append({"id": "g3-entry-readiness::decision::0001",
                                               "gate_id": "g3-entry-readiness", "verdict": "PASS",
                                               "evidence_ids": ["ev-g3-entry-readiness"]})
    cases.append(("readiness-promoted-PASS", promoted, raw, readiness, "gate_cohort:"))
    changed = copy.deepcopy(target)
    changed["integrations.json"]["integrations"][0]["integrated_revision"] = "f" * 40
    cases.append(("changed-integrated-occurrence", changed, raw, readiness, "occurrence:"))
    swapped = dict(raw)
    swapped["gates.json"] += b"\n"
    cases.append(("different-source-blob", target, swapped, readiness, "source_blob:"))
    return cases


class FC00LegacyImportOracle(unittest.TestCase):
    def test_published_historical_bijection_and_lossless_archive(self):
        raw, ready = archive_inputs()
        self.assertEqual(compare_import(documents(ROOT), raw, ready), [])

    def test_deterministic_import_preserves_facts_and_has_no_state_generator(self):
        from tools.aegis_compat.fc00_legacy_import import build_import
        with tempfile.TemporaryDirectory() as td:
            destination, archive = Path(td) / "output", Path(td) / "archive"
            build_import(ROOT, destination, archive_root=archive)
            raw, ready = archive_inputs(archive)
            self.assertEqual(compare_import(documents(destination), raw, ready), [])
            self.assertFalse((destination / ".aegis/state.json").exists())
            before = {p.relative_to(destination): p.read_bytes() for p in destination.rglob("*") if p.is_file()}
            build_import(ROOT, destination, archive_root=archive)
            self.assertEqual(before, {p.relative_to(destination): p.read_bytes() for p in destination.rglob("*") if p.is_file()})

    def test_conflicting_output_aborts_before_publication(self):
        from tools.aegis_compat.fc00_legacy_import import build_import
        with tempfile.TemporaryDirectory() as td:
            destination, archive = Path(td) / "output", Path(td) / "archive"
            conflict = destination / ".aegis/project.json"
            conflict.parent.mkdir(parents=True)
            conflict.write_bytes(b"existing valid work")
            with self.assertRaisesRegex(ValueError, "refusing to replace"):
                build_import(ROOT, destination, archive_root=archive)
            self.assertEqual(conflict.read_bytes(), b"existing valid work")
            self.assertFalse(archive.exists())
            self.assertEqual(list(destination.rglob("*.json")), [conflict])

    def test_four_frozen_negatives_rejected_by_same_independent_oracle(self):
        raw, ready = archive_inputs()
        for name, target, mutated_raw, readiness, reason in negative_cases(documents(ROOT), raw, ready):
            with self.subTest(name=name):
                failures = compare_import(target, mutated_raw, readiness)
                self.assertTrue(any(f.startswith(reason) for f in failures), failures)
                if name == "different-valid-PASS-binding":
                    with tempfile.TemporaryDirectory() as td:
                        root = Path(td)
                        for filename, value in target.items():
                            path = root / ".aegis" / filename
                            path.parent.mkdir(exist_ok=True)
                            path.write_bytes(rendered(value))
                        self.assertEqual(official(root, "validate")["exit_code"], 0)

    def test_official_red_is_immutable_legacy_and_exit_2(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for name, raw in source_bytes().items():
                path = root / ".aegis" / name
                path.parent.mkdir(exist_ok=True)
                path.write_bytes(raw)
            result = official(root, "validate")
            self.assertEqual(result["exit_code"], 2)
            self.assertIn("INVALID", result["stdout"])

    def test_official_published_canonical_valid_and_state_current(self):
        self.assertEqual(official(ROOT, "validate")["exit_code"], 0)
        self.assertEqual(official(ROOT, "check")["exit_code"], 0)


def collect_evidence(output):
    output.mkdir(parents=True, exist_ok=True)
    package = json.loads(git("show", f"{PACKAGE_REVISION}:.aegis/packages/{TASK}/package.json"))
    locks = []
    for key, ref in package.items():
        if key.endswith("_ref") and isinstance(ref, dict) and "sha256" in ref:
            raw = git("show", f"{PACKAGE_REVISION}:.aegis/packages/{TASK}/{ref['path']}")
            assert sha(raw) == ref["sha256"]
            # Checkout line endings are transport only; frozen Git blobs own hashes.
            assert (PACKAGE / ref["path"]).read_bytes().replace(b"\r\n", b"\n") == raw.replace(b"\r\n", b"\n")
            locks.append({"path": ref["path"], "sha256": sha(raw)})
    raw, ready = archive_inputs()
    errors = compare_import(documents(ROOT), raw, ready)
    assert not errors, errors
    inputs = {"source_revision": SOURCE_REVISION, "package_revision": PACKAGE_REVISION,
              "tool_revision": TOOL_REVISION, "repository": "Mostorm-Labs/axiom"}
    def save(name, value):
        (output / name).write_bytes(rendered(dict(inputs, **value)))
    save("source-identity.json", {
        "repository_identity": git("remote", "get-url", "origin").decode().strip(),
        "starting_revision": PACKAGE_REVISION, "position": "ANCHOR_DESCENDANT_WITHOUT_CURSOR",
        "anchor_ancestor": subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                                         SOURCE_REVISION, PACKAGE_REVISION]).returncode == 0,
        "package_locks": locks, "tool_repository": git("remote", "get-url", "origin", root=tool_root()).decode().strip(),
        "source_files": {f".aegis/{name}": {"git_blob_sha1": blob(value), "sha256": sha(value),
                                         "length": len(value), "archive_matches": value == source_bytes()[name]}
                         for name, value in raw.items()},
        "red_before_first_root_mutation": True,
        "red_original_observed_during_preflight": "official pinned CLI exit 2 / INVALID; also reproduced below"})
    save("legacy-import-equivalence.json", {
        "oracle": "tests/project_state/test_fc00_legacy_import.py::compare_import",
        "oracle_sha256": sha(Path(__file__).read_bytes()), "errors": errors,
        "cohort": fixture()["historical_cohort"],
        "gate_decision_bijection": fixture()["gate_decision_stage_map"],
        "integration_occurrences": fixture()["integration_binding_map"],
        "readiness": ready,
        "closed_unmerged_representation": "canonical integrated_revision omitted; exact original value preserved in immutable source archive and occurrence crosswalk",
        "authority_records_exact": True, "evidence_all_fields_preserved_except_kind_to_type": True,
        "historical_physical_and_pressure_flags_preserved": True, "new_pass_events": 0, "absent_bindings": 0})
    logs = []
    with tempfile.TemporaryDirectory() as td:
        legacy = Path(td)
        for name, value in source_bytes().items():
            p = legacy / ".aegis" / name
            p.parent.mkdir(exist_ok=True)
            p.write_bytes(value)
        red = official(legacy, "validate")
        assert red["exit_code"] == 2 and "INVALID" in red["stdout"]
        logs.append(dict(id="legacy-red", **red))
    for ident, command in (("validate", ("validate",)), ("recompute-1", ("recompute", "--write")),
                           ("check-1", ("check",)), ("recompute-2", ("recompute", "--write")),
                           ("check-2", ("check",))):
        result = official(ROOT, *command)
        assert result["exit_code"] == 0, result
        entry = dict(id=ident, **result)
        if ident.startswith("recompute"):
            state = (ROOT / ".aegis/state.json").read_bytes()
            entry.update(state_sha256=sha(state), state_length=len(state))
            if ident.endswith("1"): first = state
            else: assert first == state
        logs.append(entry)
    save("official-v06-cli.json", {"observations": logs, "state_bytes_identical": True,
                                   "state_sha256": sha((ROOT / ".aegis/state.json").read_bytes())})
    negatives = []
    for name, target, mutated_raw, readiness, reason in negative_cases(documents(ROOT), raw, ready):
        failures = compare_import(target, mutated_raw, readiness)
        assert any(f.startswith(reason) for f in failures), (name, failures)
        record = {"id": name, "expected_invariant": reason, "oracle_errors": failures,
                  "rejected": True}
        if name == "different-valid-PASS-binding":
            with tempfile.TemporaryDirectory() as td:
                invalid = Path(td)
                for filename, value in target.items():
                    p = invalid / ".aegis" / filename
                    p.parent.mkdir(exist_ok=True)
                    p.write_bytes(rendered(value))
                observation = official(invalid, "validate")
                assert observation["exit_code"] == 0
                record["official_validator"] = observation
        negatives.append(record)
    save("negative-oracle.json", {"legacy_red": logs[0], "negative_controls": negatives,
                                  "all_targeted_invariants_rejected": True})
    allowed = [".aegis/project.json", ".aegis/authorities.json", ".aegis/gates.json",
               ".aegis/evidence.json", ".aegis/integrations.json", ".aegis/state.json",
               ".aegis/legacy/FC-00-LEGACY-IMPORT/**", ".aegis/results/FC-00-LEGACY-IMPORT/**",
               "tools/aegis_compat/fc00_legacy_import.py", "tests/project_state/test_fc00_legacy_import.py"]
    forbidden_prefixes = ("runtime/", "arc/", "apps/", "debug_ui/", "verification/", "docs/",
                          ".aegis/results/GT-", ".aegis/results/PX0-07/", ".aegis/packages/")
    tracked = git("diff", "--name-only", PACKAGE_REVISION).decode().splitlines()
    untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard"], cwd=ROOT, text=True).splitlines()
    changed = sorted(set(tracked) | set(untracked))
    forbidden = [p for p in changed if p.startswith(forbidden_prefixes)]
    (output / "scope-identity.json").write_bytes(rendered({
        "repository": "Mostorm-Labs/axiom", "result_base": PACKAGE_REVISION,
        "immutable_source_anchor": SOURCE_REVISION,
        "changed_paths": changed, "allowed_scope": allowed,
        "forbidden_changes": forbidden, "git_diff_check": subprocess.run(
            ["git", "diff", "--check", PACKAGE_REVISION], cwd=ROOT, capture_output=True, text=True).returncode == 0,
        "product_sources_unchanged": not forbidden, "fc00_only": not forbidden,
    }))
    print("FC00_EVIDENCE_WRITTEN", output)


if __name__ == "__main__":
    if "--collect-evidence" in sys.argv:
        parser = argparse.ArgumentParser()
        parser.add_argument("--collect-evidence", type=Path, required=True)
        collect_evidence(parser.parse_args().collect_evidence)
    else:
        unittest.main()
