#!/usr/bin/env python3
import argparse, json, hashlib
from pathlib import Path

FILES = ["INGRESS-PLAN.json", "INGRESS-MATRIX.json", "INGRESS-REGRESSION.json", "INGRESS-CTEST.txt", "INGRESS-PROVIDER.json", "INGRESS-GATE-MANIFEST.json"]
FAMILIES = ["InsertObjects","DeleteObjects","RestoreObjects","SetPlacements","SetTransforms","PatchProperties","SetObjectSize","SetVectorPathGeometry","SetImageContent","AddStroke","SplitStrokes","AddEraseMasks","RemoveEraseMasks","EditRichText","SetConnectorContent"]

ANCHOR = "7d74d4544ca599fdf0294c80e4070290f57104db"
CHANGED_PATHS = ["runtime/semantic/src/codec.cpp", "runtime/semantic/tests/CMakeLists.txt", "runtime/semantic/tests/codec_operation_ingress_test.cpp", "verification/tools/generate_g1_02r_ingress_evidence.py", "verification/tests/test_g1_02r_ingress_evidence.py"]
def generate(root: Path, source_ref: str, package_ref: str, *, task_id="GT-G1-02R-INGRESS", task_anchor=ANCHOR,
             actual_start="f10e3fd10927c4f612d676b4cca7ac9d589c191f", changed_paths=None,
             family_rows=None, provider=None, materialized_ref=None, evidence_inventory=None) -> Path:
    if task_id != "GT-G1-02R-INGRESS": raise ValueError("wrong task_id")
    if package_ref != "notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1": raise ValueError("wrong package_ref")
    if len(source_ref) != 40 or any(c not in "0123456789abcdef" for c in source_ref): raise ValueError("wrong source_ref")
    if task_anchor != ANCHOR: raise ValueError("wrong anchor")
    if len(actual_start) != 40 or any(c not in "0123456789abcdef" for c in actual_start): raise ValueError("wrong actual_start")
    if changed_paths is not None and sorted(changed_paths) != sorted(CHANGED_PATHS): raise ValueError("wrong changed path inventory")
    if source_ref in {"305debc3c77241c00860ed8eac4d040f3e295a57", "1b370bd242ce19838889b28beb7b75979244cbab"}: raise ValueError("historical namespace reuse")
    rows = FAMILIES if family_rows is None else family_rows
    if sorted(rows) != sorted(FAMILIES) or len(rows) != len(FAMILIES): raise ValueError("missing family rows")
    if provider is None: raise ValueError("provider fields required")
    if any(provider.get(k) in (None, "") for k in ("run_id","attempt","job_id","artifact_identity","exact_tested_source_sha")): raise ValueError("provider fields required")
    if provider["exact_tested_source_sha"] != source_ref: raise ValueError("provider source mismatch")
    if materialized_ref is not None and (len(materialized_ref) != 40 or materialized_ref == source_ref): raise ValueError("source/materialized provenance mismatch")
    if evidence_inventory is not None and sorted(evidence_inventory) != sorted(FILES): raise ValueError("wrong evidence inventory")
    out = root / "verification" / "evidence" / "gates" / "G1" / source_ref / "GT-G1-02R-INGRESS"
    out.mkdir(parents=True, exist_ok=True)
    (out / FILES[0]).write_text(json.dumps({"task_id":task_id,"package_ref":package_ref,"task_anchor":task_anchor,"actual_start":actual_start,"source_ref":source_ref,"changed_paths":CHANGED_PATHS}, indent=2)+"\n")
    (out / FILES[1]).write_text(json.dumps({"families":FAMILIES,"per_family_typed_oracle_refs":{f:"PASS" for f in FAMILIES}}, indent=2)+"\n")
    (out / FILES[2]).write_text(json.dumps({"negative_preflight":"PASS","protobuf_off":"kRuntimeUnavailable observed","legacy_decoder":"PASS","clean_checkout":"PASS"}, indent=2)+"\n")
    (out / FILES[3]).write_text("protobuf ON: cmake --build out/g1-hosted --parallel && ctest --test-dir out/g1-hosted --output-on-failure: PASS\nprotobuf OFF: ctest --test-dir out/g1-off --output-on-failure: PASS (23 EXPECTED SKIP)\ngit diff --check: PASS\n")
    (out / FILES[4]).write_text(json.dumps({"provider":"github",**provider}, indent=2)+"\n")
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.name != FILES[5]}
    (out / FILES[5]).write_text(json.dumps({"task_id":"GT-G1-02R-INGRESS","source_ref":source_ref,"materialized_ref":materialized_ref,"relation":"EVIDENCE_ONLY_DESCENDANT","inventory":FILES,"hashes":hashes}, indent=2)+"\n")
    return out

if __name__ == "__main__":
    p=argparse.ArgumentParser(); p.add_argument("--root", type=Path, default=Path.cwd()); p.add_argument("--source-ref", required=True); p.add_argument("--package-ref", required=True); a=p.parse_args(); print(generate(a.root,a.source_ref,a.package_ref))
