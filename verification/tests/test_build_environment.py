import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools.sdk.model import HostPlatform
import tools.setup_build_environment as build_env


class BuildEnvironmentTest(unittest.TestCase):
    def make_semantic_root(self, directory: str) -> Path:
        root = Path(directory) / "protobuf"
        (root / "bin").mkdir(parents=True)
        (root / "lib/cmake/protobuf").mkdir(parents=True)
        (root / "lib/cmake/absl").mkdir(parents=True)
        (root / "lib/cmake/utf8_range").mkdir(parents=True)
        (root / "bin/protoc").write_text("protoc\n", encoding="utf-8")
        (root / "lib/cmake/protobuf/protobuf-config.cmake").write_text("# protobuf\n", encoding="utf-8")
        (root / "lib/cmake/absl/abslConfig.cmake").write_text("# absl\n", encoding="utf-8")
        (root / "lib/cmake/utf8_range/utf8_range-config.cmake").write_text("# utf8\n", encoding="utf-8")
        return root

    def test_validate_semantic_install_returns_one_canonical_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            environment = build_env.validate_semantic_install(root)
            self.assertEqual(environment["AXIOM_SEMANTIC_SDK_ROOT"], str(root.resolve()))
            self.assertEqual(environment["CMAKE_PREFIX_PATH"], str(root.resolve()))
            self.assertEqual(environment["AXIOM_SEMANTIC_RUNTIME_ROOT"], str(root.resolve()))
            self.assertEqual(environment["AXIOM_SEMANTIC_HOST_ROOT"], str(root.resolve()))
            self.assertEqual(environment["PROTOBUF_DIR"], str((root / "lib/cmake/protobuf").resolve()))
            self.assertEqual(environment["ABSL_DIR"], str((root / "lib/cmake/absl").resolve()))
            self.assertEqual(environment["UTF8_RANGE_DIR"], str((root / "lib/cmake/utf8_range").resolve()))
            self.assertEqual(environment["AXIOM_PROTOC"], str((root / "bin/protoc").resolve()))

    def test_validate_semantic_install_fails_when_package_contract_is_incomplete(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            (root / "lib/cmake/absl/abslConfig.cmake").unlink()
            with self.assertRaisesRegex(RuntimeError, "semantic SDK consumer contract"):
                build_env.validate_semantic_install(root)

    def test_write_github_env_exports_single_semantic_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            environment = build_env.validate_semantic_install(root)
            output = Path(directory) / "github-env"
            build_env.write_github_env(output, environment)
            text = output.read_text(encoding="utf-8")
            self.assertIn(f"AXIOM_SEMANTIC_SDK_ROOT={root.resolve()}\n", text)
            self.assertIn(f"CMAKE_PREFIX_PATH={root.resolve()}\n", text)
            self.assertEqual(text.count("CMAKE_PREFIX_PATH="), 1)
            self.assertIn(f"AXIOM_SEMANTIC_RUNTIME_ROOT={root.resolve()}\n", text)
            self.assertIn(f"AXIOM_SEMANTIC_HOST_ROOT={root.resolve()}\n", text)

    @mock.patch("tools.setup_build_environment.subprocess.run")
    def test_setup_environment_never_requests_semantic_source_bootstrap(self, run):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            original = build_env.SEMANTIC_ROOT
            build_env.SEMANTIC_ROOT = root
            run.side_effect = [
                mock.Mock(stdout=""),
                mock.Mock(stdout=json.dumps({
                    "sdkId": "a" * 64,
                    "asset": "asset.zip",
                    "sha256": "b" * 64,
                    "url": "https://example.invalid/asset.zip",
                }) + "\n"),
            ]
            try:
                build_env.setup_environment(core=True, semantic=True, target="linux-x86_64",
                                            host=HostPlatform("linux", "x86_64", "linux-x86_64"),
                                            lock_path=build_env.SEMANTIC_LOCK)
            finally:
                build_env.SEMANTIC_ROOT = original
            commands = [call.args[0] for call in run.call_args_list]
            self.assertIn([sys.executable, "tools/bootstrap_deps.py", "--core"], commands)
            self.assertFalse(any("--semantic-codec" in command for command in commands))
            self.assertTrue(any("tools/semantic_fetch.py" in command for command in commands))

    @mock.patch("tools.setup_build_environment.subprocess.run")
    def test_setup_environment_surfaces_semantic_fetch_diagnostics(self, run):
        run.side_effect = subprocess.CalledProcessError(
            1,
            [sys.executable, "tools/semantic_fetch.py"],
            output="fetch stdout\n",
            stderr="semantic identity mismatch\n",
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            with self.assertRaises(subprocess.CalledProcessError):
                build_env.setup_environment(core=False, semantic=True, target="linux-x86_64",
                                            host=HostPlatform("linux", "x86_64", "linux-x86_64"),
                                            lock_path=build_env.SEMANTIC_LOCK)
        self.assertIn("fetch stdout", stdout.getvalue())
        self.assertIn("semantic identity mismatch", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
