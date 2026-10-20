"""Current schema reproducibility must not overwrite historical G1 evidence."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from verification.tools import descriptor_refreeze_diff as descriptor
from verification.tools import validate_semantic_contract as contract

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = ROOT / "schema/axiom/v1/descriptor"


class CurrentSemanticDescriptorTest(unittest.TestCase):
    def test_historical_freeze_retains_its_exact_identity(self):
        self.assertEqual(
            contract.validate_descriptor_hash(ROOT, "descriptor.lock"),
            "8c1da1c19b754f9345472f96452e58505ebd18c0874b04480e378eb95a38654d",
        )

    def test_current_lock_hash_matches_its_artifact(self):
        contract.validate_descriptor_hash(ROOT, "descriptor.current")

    def test_existing_message_fields_and_enums_are_preserved(self):
        before = descriptor.parse_descriptor((DIRECTORY / "descriptor.lock.pb").read_bytes())
        after = descriptor.parse_descriptor((DIRECTORY / "descriptor.current.pb").read_bytes())
        self.assertEqual(before.keys(), after.keys())
        for name, original in before.items():
            with self.subTest(file=name):
                current = after[name]
                self.assertEqual(original.enums, current.enums)
                messages = {message.name: message for message in current.messages}
                for message in original.messages:
                    self.assertIn(message.name, messages)
                    for field in message.fields:
                        self.assertIn(field, messages[message.name].fields)

    def test_additions_are_limited_to_resource_release_and_fc02_text(self):
        before = descriptor.parse_descriptor((DIRECTORY / "descriptor.lock.pb").read_bytes())
        after = descriptor.parse_descriptor((DIRECTORY / "descriptor.current.pb").read_bytes())
        new_messages = {}
        extra_fields = {}
        for name, current in after.items():
            original = {message.name: message for message in before[name].messages}
            added = {message.name for message in current.messages} - original.keys()
            if added:
                new_messages[Path(name).name] = added
            for message in current.messages:
                if message.name in original:
                    fields = set(message.fields) - set(original[message.name].fields)
                    if fields:
                        extra_fields[(Path(name).name, message.name)] = {field.number for field in fields}
        self.assertEqual(new_messages, {
            "rich_text.proto": {"RichTextLayoutV2", "RichTextStateV2", "SetRichTextLayoutStep", "RestoreRichTextStateStep"},
            "operation.proto": {"ResourceExpectation", "ReactivateResource", "UpdateResourceManifestOp"},
            "snapshot.proto": {"ImageResourceMetadataV1", "FontResourceMetadataV1",
                "BrushResourceMetadataV1", "ResourceManifestEntry", "ResourceManifestTombstone",
                "ResourceManifest", "RecoveryFrontierV1", "CapabilityRequirementsV1"},
        })
        self.assertEqual(extra_fields, {
            ("operation.proto", "OperationPayload"): {16},
            ("rich_text.proto", "TextStyle"): {7},
            ("rich_text.proto", "ParagraphStyle"): {5},
            ("rich_text.proto", "RichTextContent"): {2},
            ("rich_text.proto", "RichTextStep"): {7, 8},
            ("rich_text.proto", "RichTextDelta"): {3},
            ("snapshot.proto", "DocumentSnapshot"): set(range(4, 11)),
        })

    def test_corrupt_current_or_historical_artifacts_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "schema/axiom/v1/descriptor"
            output.mkdir(parents=True)
            for name in ("descriptor.lock", "descriptor.current"):
                with self.subTest(descriptor=name):
                    (output / f"{name}.sha256").write_bytes((DIRECTORY / f"{name}.sha256").read_bytes())
                    (output / f"{name}.pb").write_bytes((DIRECTORY / f"{name}.pb").read_bytes() + b"corrupt")
                    with self.assertRaisesRegex(SystemExit, "hash mismatch"):
                        contract.validate_descriptor_hash(root, name)

    @unittest.skipUnless(os.environ.get("AXIOM_PROTOC"), "resolver-emitted AXIOM_PROTOC required")
    def test_unlocked_source_schema_is_rejected_by_real_generation(self):
        with tempfile.TemporaryDirectory(prefix="axiom schema with spaces ") as directory:
            root = Path(directory)
            proto = root / "schema/axiom/v1/proto"
            shutil.copytree(ROOT / "schema/axiom/v1/proto", proto)
            shutil.copytree(DIRECTORY, root / "schema/axiom/v1/descriptor")
            seed = Path("verification/corpus/semantic/v1/suites/seed-v0.1.json")
            (root / seed).parent.mkdir(parents=True)
            shutil.copyfile(ROOT / seed, root / seed)
            with (proto / "auditoryworks/axiom/v1/common.proto").open("a", encoding="utf-8") as source:
                source.write("\nmessage UnapprovedDescriptorProbe { uint32 value = 1; }\n")
            result = subprocess.run([
                sys.executable, str(ROOT / "verification/tools/validate_semantic_contract.py"),
                "--root", str(root), "--protoc", os.environ["AXIOM_PROTOC"],
            ], text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("current descriptor is not reproducible", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
