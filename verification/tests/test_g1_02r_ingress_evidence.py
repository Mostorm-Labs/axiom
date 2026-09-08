import json, tempfile, unittest
from pathlib import Path
from verification.tools.generate_g1_02r_ingress_evidence import FILES, generate

class IngressEvidenceTest(unittest.TestCase):
    def test_generates_exact_six_bound_files(self):
        with tempfile.TemporaryDirectory() as d:
            out=generate(Path(d), "abc123", "notion://GT-G1-02R-INGRESS")
            self.assertEqual(sorted(p.name for p in out.iterdir()), sorted(FILES))
            self.assertEqual(json.loads((out/"INGRESS-PLAN.json").read_text())["source_ref"], "abc123")

if __name__ == "__main__": unittest.main()
