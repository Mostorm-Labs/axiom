import json
import tempfile
import unittest
from pathlib import Path
import preflight

class IntegrityTests(unittest.TestCase):
    def test_missing_required_local_ref_fails(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                preflight.check_ref(Path(d), {"path":"missing.json","sha256":"0"*64})
    def test_escape_fails_even_when_target_exists(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/"inside").mkdir();(p/"secret").write_text("x")
            with self.assertRaises(ValueError):
                preflight.check_ref(p/"inside", {"path":"../secret","sha256":"0"*64})
    def test_hash_mismatch_fails(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/"x").write_text("x")
            with self.assertRaises(ValueError):
                preflight.check_ref(p,{"path":"x","sha256":"0"*64})
    def test_exact_hash_succeeds(self):
        import hashlib
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/"x").write_text("x")
            self.assertEqual(preflight.check_ref(p,{"path":"x","sha256":hashlib.sha256(b"x").hexdigest()}),p/"x")
    def test_product_authorization_rejected(self):
        with self.assertRaises(ValueError):
            preflight.check_scope({"authorized_work":"verification_materialization_only","product_P32_authorized":True})
    def test_only_materialization_authorized(self):
        self.assertIsNone(preflight.check_scope({"authorized_work":"verification_materialization_only","product_P32_authorized":False}))

if __name__=="__main__": unittest.main()
