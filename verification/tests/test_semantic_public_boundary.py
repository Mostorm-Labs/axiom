import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
PUBLIC_HEADERS = ROOT / "runtime/semantic/include/canvas/semantic"
BOUNDARY_SCRIPT = ROOT / "tools/check_runtime_boundaries.py"
SPEC = importlib.util.spec_from_file_location("runtime_boundaries", BOUNDARY_SCRIPT)
assert SPEC and SPEC.loader
boundary = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(boundary)


class SemanticPublicBoundaryTest(unittest.TestCase):
    def test_old_semantic_revision_is_not_a_public_contract(self):
        self.assertFalse((PUBLIC_HEADERS / "semantic_revision.hpp").exists())
        for header in PUBLIC_HEADERS.glob("*.hpp"):
            self.assertNotIn("SemanticRevision", header.read_text(encoding="utf-8"))

    def test_public_semantic_headers_do_not_leak_derived_or_platform_layers(self):
        forbidden = ("RuntimeScene", "Skia", "Arc", "jni.h", "UIKit", "windows.h", "emscripten/")
        for header in PUBLIC_HEADERS.glob("*.hpp"):
            text = header.read_text(encoding="utf-8")
            for token in forbidden:
                self.assertNotIn(token, text, f"{header.relative_to(ROOT)} leaks {token}")

    def test_runtime_boundary_checker_explicitly_rejects_derived_scene_and_arc_symbols(self):
        patterns = boundary.FORBIDDEN_CONTRACT_PATTERNS
        self.assertIsNotNone(patterns["derived scene"].search("RuntimeScene"))
        self.assertIsNotNone(patterns["Arc"].search("ArcCanonicalToken"))

    def test_derived_scene_rule_applies_only_to_semantic_public_headers(self):
        scene_header = ROOT / "runtime/scene/include/canvas/scene/scene.hpp"
        semantic_header = PUBLIC_HEADERS / "change_set.hpp"
        scene_failures = boundary.check_files(
            [scene_header], boundary.FORBIDDEN_CONTRACT_PATTERNS, ROOT
        )
        semantic_failures = boundary.check_files(
            [semantic_header], boundary.FORBIDDEN_CONTRACT_PATTERNS, ROOT
        )
        self.assertFalse(any("derived scene" in failure for failure in scene_failures))
        self.assertFalse(any("derived scene" in failure for failure in semantic_failures))

    def test_g1_hosted_workflow_runs_reconciliation_regressions(self):
        workflow = (ROOT / ".github/workflows/g1-semantic-codec.yml").read_text(encoding="utf-8")
        self.assertIn("verification.tests.test_semantic_public_boundary", workflow)
        self.assertIn("verification.tests.test_g1_01r_evidence", workflow)


if __name__ == "__main__":
    unittest.main()
