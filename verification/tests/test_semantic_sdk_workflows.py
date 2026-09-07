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

    def test_runtime_matrix_uses_producer_only_build_and_relocated_smoke(self):
        workflow = (ROOT / ".github/workflows/semantic-sdk-producer-contract.yml").read_text(encoding="utf-8")
        for key in ("linux-x86_64", "windows-x64-msvc-static", "macos-arm64", "macos-x64",
                    "ios-arm64", "ios-simulator-arm64", "android-arm64-v8a", "android-x86_64", "web-wasm32"):
            self.assertIn("target: " + key, workflow)
        self.assertIn("cmake==3.30.5 ninja==1.11.1.4", workflow)
        self.assertNotIn("ninja==1.12.1", workflow)
        self.assertIn("needs: [host-tools, runtime-discovery]", workflow)
        self.assertIn("tools/semantic/qualify_runtime.py", workflow)
        self.assertIn("semantic-v2-host-${{ matrix.host }}", workflow)
        self.assertIn("semantic-v2-runtime-${{ matrix.target }}", workflow)
        self.assertNotIn("contents: write", workflow)
        self.assertNotIn("gh release", workflow)

    def test_package_discovery_preflight_does_not_build_dependencies(self):
        workflow = (ROOT / ".github/workflows/semantic-sdk-producer-contract.yml").read_text(encoding="utf-8")
        preflight = workflow.split("  runtime-discovery:\n", 1)[1].split("  runtimes:\n", 1)[0]
        self.assertIn("os: [ubuntu-24.04, windows-2025, macos-15]", preflight)
        self.assertIn("cmake==3.30.5", preflight)
        self.assertIn("tools.semantic.tests.test_runtime_cmake_discovery", preflight)
        self.assertNotIn("bootstrap_deps.py", preflight)
        self.assertNotIn("qualify_runtime.py", preflight)
        self.assertNotIn("build_runtime.py", preflight)


if __name__ == "__main__":
    unittest.main()
