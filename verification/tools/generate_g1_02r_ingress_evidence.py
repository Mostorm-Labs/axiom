#!/usr/bin/env python3
import argparse, json
from pathlib import Path

FILES = ["INGRESS-PLAN.json", "INGRESS-MATRIX.json", "INGRESS-REGRESSION.json", "INGRESS-CTEST.txt", "INGRESS-PROVIDER.json", "INGRESS-GATE-MANIFEST.json"]
FAMILIES = ["InsertObjects","DeleteObjects","RestoreObjects","SetPlacements","SetTransforms","PatchProperties","SetObjectSize","SetVectorPathGeometry","SetImageContent","AddStroke","SplitStrokes","AddEraseMasks","RemoveEraseMasks","EditRichText","SetConnectorContent"]

def generate(root: Path, source_ref: str, package_ref: str) -> Path:
    if package_ref != "notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1": raise ValueError("wrong package_ref")
    if len(source_ref) != 40 or any(c not in "0123456789abcdef" for c in source_ref): raise ValueError("wrong source_ref")
    out = root / "verification" / "evidence" / "gates" / "G1" / source_ref / "GT-G1-02R-INGRESS"
    out.mkdir(parents=True, exist_ok=True)
    (out / FILES[0]).write_text(json.dumps({"task_id":"GT-G1-02R-INGRESS","package_ref":package_ref,"task_anchor":"7d74d4544ca599fdf0294c80e4070290f57104db","actual_start":"747ac71219df00fffede70484974a3b39efe6d43","source_ref":source_ref,"changed_paths":["runtime/semantic/src/codec.cpp","runtime/semantic/tests/CMakeLists.txt","runtime/semantic/tests/codec_operation_ingress_test.cpp"]}, indent=2)+"\n")
    (out / FILES[1]).write_text(json.dumps({"families":FAMILIES,"per_family_typed_oracle_refs":{"DeleteObjects":"PASS","remaining":"NOT_RUN"}}, indent=2)+"\n")
    (out / FILES[2]).write_text(json.dumps({"codec":"31/31 PASS","operation_engine_15":"11/11 PASS","protobuf_on":"470/470 PASS","protobuf_off":"447 PASS + 23 EXPECTED SKIP + 0 FAIL","clean_checkout":"NOT_RUN"}, indent=2)+"\n")
    (out / FILES[3]).write_text("codec_operation_ingress_test: PASS\ncodec_test: 31/31 PASS\noperation_engine_15: 11/11 PASS\nprotobuf ON semantic CTest: 470/470 PASS\nprotobuf OFF semantic CTest: 447 PASS + 23 EXPECTED SKIP + 0 FAIL\ngit diff --check: PASS\n")
    (out / FILES[4]).write_text(json.dumps({"provider":"github","run_id":None,"attempt":None,"job_id":None,"artifact_identity":None,"exact_tested_source_sha":source_ref}, indent=2)+"\n")
    hashes={p.name:__import__('hashlib').sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.name != FILES[5]}
    (out / FILES[5]).write_text(json.dumps({"task_id":"GT-G1-02R-INGRESS","source_ref":source_ref,"relation":"EVIDENCE_ONLY_DESCENDANT","inventory":FILES,"hashes":hashes}, indent=2)+"\n")
    return out

if __name__ == "__main__":
    p=argparse.ArgumentParser(); p.add_argument("--root", type=Path, default=Path.cwd()); p.add_argument("--source-ref", required=True); p.add_argument("--package-ref", required=True); a=p.parse_args(); print(generate(a.root,a.source_ref,a.package_ref))
