import copy
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "validate_workspace", ROOT / "verification/tools/validate_workspace.py"
)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class WorkspaceManifestTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((ROOT / "verification/workspace.json").read_text())

    def test_repository_workspace_is_valid(self):
        digest = validator.validate(self.manifest)
        self.assertEqual(len(digest), 64)

    def test_manifest_digest_is_deterministic(self):
        self.assertEqual(validator.validate(self.manifest), validator.validate(copy.deepcopy(self.manifest)))

    def test_unknown_top_level_field_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["unexpected"] = True
        with self.assertRaises(validator.WorkspaceError):
            validator.validate(invalid)

    def test_absolute_and_parent_paths_are_rejected(self):
        for value in ("/tmp/verification", "verification/../outside", "C:/verification"):
            invalid = copy.deepcopy(self.manifest)
            invalid["directories"]["corpus"] = value
            with self.assertRaises(validator.WorkspaceError):
                validator.validate(invalid)

    def test_missing_owner_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["corpus"][0]["owner"] = "GT-G0-03"
        with self.assertRaises(validator.WorkspaceError):
            validator.validate(invalid)

    def test_duplicate_corpus_id_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["corpus"][1] = copy.deepcopy(invalid["corpus"][0])
        with self.assertRaises(validator.WorkspaceError):
            validator.validate(invalid)

    def test_unknown_nested_field_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["corpus"][0]["comment"] = "not part of the contract"
        with self.assertRaises(validator.WorkspaceError):
            validator.validate(invalid)

    def test_missing_directory_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["directories"]["corpus"] = "verification/corpus-does-not-exist"
        with self.assertRaises(validator.WorkspaceError):
            validator.validate(invalid)

    def test_schema_is_parseable_and_strict(self):
        schema = json.loads((ROOT / "verification/schema/workspace-manifest.schema.json").read_text())
        self.assertEqual(schema["$schema"], "https://json-schema.org/draft/2020-12/schema")
        self.assertFalse(schema["additionalProperties"])


if __name__ == "__main__":
    unittest.main()
