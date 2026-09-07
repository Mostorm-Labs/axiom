"""Cheap host coverage must not become a dependency producer."""
from pathlib import Path
import unittest


class SdkInfrastructureWorkflowTest(unittest.TestCase):
    def test_generic_contract_runs_on_three_hosts_without_bootstrap(self):
        workflow = (Path(__file__).resolve().parents[2] / ".github/workflows/build-environment-contract.yml").read_text()
        self.assertEqual(workflow.count('      - "tools/sdk/**"'), 2)
        self.assertIn("  sdk-infrastructure:", workflow)
        job = workflow.split("  sdk-infrastructure:", 1)[1]
        for runner in ("ubuntu-24.04", "windows-2025", "macos-15"):
            self.assertIn(runner, job)
        self.assertIn("python -m unittest discover -s tools/sdk/tests -v", job)
        self.assertIn("python-version: \"3.12\"", job)
        self.assertNotIn("bootstrap_deps.py", job)
        self.assertNotIn("contents: write", job)
        self.assertNotIn("gh release", job)


if __name__ == "__main__":
    unittest.main()
