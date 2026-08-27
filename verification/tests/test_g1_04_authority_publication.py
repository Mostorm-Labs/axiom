import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "validate_g1_04_authority_publication",
    ROOT / "verification/tools/validate_g1_04_authority_publication.py",
)
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class G104AuthorityPublicationTest(unittest.TestCase):
    def test_current_matrix_is_manifest_published_and_complete(self):
        result = module.validate(ROOT)
        self.assertEqual(result, [])

    def test_manifest_parser_rejects_duplicate_current_authority_key(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / "manifest.yaml"
            manifest.write_text("current_authority:\n  one: 1\n  one: 2\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                module._load_yaml(manifest)


if __name__ == "__main__":
    unittest.main()
