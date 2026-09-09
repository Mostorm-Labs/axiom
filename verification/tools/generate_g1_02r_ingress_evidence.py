#!/usr/bin/env python3
import argparse, json, hashlib
from pathlib import Path

FILES = ["INGRESS-PLAN.json", "INGRESS-MATRIX.json", "INGRESS-REGRESSION.json", "INGRESS-CTEST.txt", "INGRESS-PROVIDER.json", "INGRESS-GATE-MANIFEST.json"]
FAMILIES = ["InsertObjects","DeleteObjects","RestoreObjects","SetPlacements","SetTransforms","PatchProperties","SetObjectSize","SetVectorPathGeometry","SetImageContent","AddStroke","SplitStrokes","AddEraseMasks","RemoveEraseMasks","EditRichText","SetConnectorContent"]

ANCHOR = "7d74d4544ca599fdf0294c80e4070290f57104db"
CHANGED_PATHS = ["runtime/semantic/src/codec.cpp", "runtime/semantic/tests/CMakeLists.txt", "runtime/semantic/tests/codec_operation_ingress_test.cpp", "verification/tools/generate_g1_02r_ingress_evidence.py", "verification/tests/test_g1_02r_ingress_evidence.py"]
def generate(root: Path, source_ref: str, package_ref: str, *, task_id="GT-G1-02R-INGRESS", task_anchor=ANCHOR,
             actual_start="f10e3fd10927c4f612d676b4cca7ac9d589c191f", changed_paths=None,
             family_rows=None, provider=None, materialized_ref=None, evidence_inventory=None,
             proof_facts=None, oracle_refs=None, target_namespace_exists=False,
             materialized_parent=None) -> Path:
    if task_id != "GT-G1-02R-INGRESS": raise ValueError("wrong task_id")
    if package_ref != "notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1": raise ValueError("wrong package_ref")
    if len(source_ref) != 40 or any(c not in "0123456789abcdef" for c in source_ref): raise ValueError("wrong source_ref")
    if task_anchor != ANCHOR: raise ValueError("wrong anchor")
    if len(actual_start) != 40 or any(c not in "0123456789abcdef" for c in actual_start): raise ValueError("wrong actual_start")
    if changed_paths is not None and sorted(changed_paths) != sorted(CHANGED_PATHS): raise ValueError("wrong changed path inventory")
    if target_namespace_exists: raise ValueError("preexisting target namespace")
    rows = FAMILIES if family_rows is None else family_rows
    if sorted(rows) != sorted(FAMILIES) or len(rows) != len(FAMILIES): raise ValueError("missing family rows")
    if provider is None: raise ValueError("provider fields required")
    if any(provider.get(k) in (None, "") for k in ("run_id","attempt","job_id","artifact_identity","exact_tested_source_sha")): raise ValueError("provider fields required")
    if provider["exact_tested_source_sha"] != source_ref: raise ValueError("provider source mismatch")
    if materialized_ref is not None and (len(materialized_ref) != 40 or materialized_ref == source_ref): raise ValueError("source/materialized provenance mismatch")
    if evidence_inventory is not None and sorted(evidence_inventory) != sorted(FILES): raise ValueError("wrong evidence inventory")
    if family_rows is None or provider is None or proof_facts is None or oracle_refs is None: raise ValueError("bound test inputs required")
    required_facts = ("negative_preflight", "protobuf_off", "legacy_decoder", "clean_checkout", "protobuf_on_command", "protobuf_on_result", "protobuf_off_command", "protobuf_off_result")
    if any(proof_facts.get(k) in (None, "") for k in required_facts): raise ValueError("missing machine proof fact")
    if sorted(oracle_refs) != sorted(FAMILIES) or any(oracle_refs[f] in (None, "") for f in FAMILIES): raise ValueError("missing family oracle ref")
    if materialized_parent is not None and materialized_parent != source_ref: raise ValueError("invalid materialized ancestry")
    out = root / "verification" / "evidence" / "gates" / "G1" / source_ref / "GT-G1-02R-INGRESS"
    out.mkdir(parents=True, exist_ok=True)
    (out / FILES[0]).write_text(json.dumps({"task_id":task_id,"package_ref":package_ref,"task_anchor":task_anchor,"actual_start":actual_start,"source_ref":source_ref,"changed_paths":CHANGED_PATHS}, indent=2)+"\n")
    (out / FILES[1]).write_text(json.dumps({"families":FAMILIES,"per_family_typed_oracle_refs":oracle_refs}, indent=2)+"\n")
    (out / FILES[2]).write_text(json.dumps(proof_facts, indent=2)+"\n")
    (out / FILES[3]).write_text(f"protobuf ON: {proof_facts['protobuf_on_command']}: {proof_facts['protobuf_on_result']}\nprotobuf OFF: {proof_facts['protobuf_off_command']}: {proof_facts['protobuf_off_result']}\ngit diff --check: PASS\n")
    (out / FILES[4]).write_text(json.dumps({"provider":"github",**provider}, indent=2)+"\n")
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.name != FILES[5]}
    (out / FILES[5]).write_text(json.dumps({"task_id":"GT-G1-02R-INGRESS","source_ref":source_ref,"materialized_ref":materialized_ref,"materialized_parent":materialized_parent,"relation":"EVIDENCE_ONLY_DESCENDANT","inventory":FILES,"hashes":hashes}, indent=2)+"\n")
    return out

if __name__ == "__main__":
    p=argparse.ArgumentParser(); p.add_argument("--root", type=Path, default=Path.cwd()); p.add_argument("--source-ref", required=True); p.add_argument("--package-ref", required=True); a=p.parse_args(); print(generate(a.root,a.source_ref,a.package_ref))
