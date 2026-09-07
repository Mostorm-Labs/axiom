"""Release production remains separated from ordinary Semantic consumers."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SemanticSdkWorkflowTest(unittest.TestCase):
    def test_host_packages_are_qualified_on_matching_hosts_without_publication(self):
        workflow = (ROOT / ".github/workflows/semantic-sdk-producer-contract.yml").read_text(encoding="utf-8")
        for value in ("linux-x86_64", "windows-x64", "macos-universal", "ubuntu-24.04", "windows-2025", "macos-15"):
            self.assertIn(value, workflow)
        self.assertIn("tools/semantic/package_host.py", workflow)
        self.assertIn("--probe", workflow)
        self.assertIn("package-a", workflow)
        self.assertIn("package-b", workflow)
        self.assertIn("file_sha256", workflow)
        self.assertNotIn("contents: write", workflow)
        self.assertNotIn("gh release", workflow)
        self.assertNotIn("--semantic-codec", workflow)


if __name__ == "__main__":
    unittest.main()
