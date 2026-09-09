import json, tempfile, unittest
from pathlib import Path
from verification.tools.generate_g1_02r_ingress_evidence import FILES, FAMILIES, generate

class IngressEvidenceTest(unittest.TestCase):
    def test_generates_exact_six_bound_files(self):
        with tempfile.TemporaryDirectory() as d:
            out=generate(Path(d), "a"*40, "notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1", family_rows=FAMILIES, oracle_refs={f:f"oracle:{f}" for f in FAMILIES}, proof_facts={k:"PASS" for k in ("negative_preflight","protobuf_off","legacy_decoder","clean_checkout","protobuf_on_command","protobuf_on_result","protobuf_off_command","protobuf_off_result")}, provider={"run_id":"r","attempt":1,"job_id":"j","artifact_identity":"a","exact_tested_source_sha":"a"*40})
            self.assertEqual(sorted(p.name for p in out.iterdir()), sorted(FILES))
            self.assertEqual(json.loads((out/"INGRESS-PLAN.json").read_text())["source_ref"], "a"*40)

    def test_rejects_wrong_package(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError): generate(Path(d), "a"*40, "wrong")

    def test_rejects_frozen_contract_mismatches(self):
        good = dict(source_ref="a"*40, package_ref="notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1")
        base = {"family_rows":FAMILIES, "oracle_refs":{f:f"oracle:{f}" for f in FAMILIES}, "proof_facts":{k:"PASS" for k in ("negative_preflight","protobuf_off","legacy_decoder","clean_checkout","protobuf_on_command","protobuf_on_result","protobuf_off_command","protobuf_off_result")}, "provider":{"run_id":"r","attempt":1,"job_id":"j","artifact_identity":"a","exact_tested_source_sha":"a"*40}}
        cases = [{"task_id":"OTHER"}, {"task_anchor":"b"*40}, {"changed_paths":[]}, {"family_rows":["DeleteObjects"]}, {"provider":{"run_id":None,"attempt":1,"job_id":"j","artifact_identity":"a","exact_tested_source_sha":"a"*40}}, {"evidence_inventory":[]}, {"materialized_ref":"a"*40}, {"target_namespace_exists":True}, {"materialized_parent":"b"*40}, {"proof_facts":None}, {"oracle_refs":{"DeleteObjects":"x"}}]
        for extra in cases:
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as d:
                args = dict(base); args.update(extra)
                with self.assertRaises(ValueError): generate(Path(d), **good, **args)

if __name__ == "__main__": unittest.main()
