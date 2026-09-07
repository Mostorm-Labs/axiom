"""Real CMake discovery through the relocated SDK prefix, without an SDK build."""
from __future__ import annotations

import ctypes
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class RuntimeCMakeDiscoveryTest(unittest.TestCase):
    def arguments(self, runtime: Path, protoc: Path) -> list[str]:
        spec = importlib.util.find_spec("tools.semantic.cmake_consumer")
        self.assertIsNotNone(spec, "the runtime smoke must expose its shared CMake search arguments")
        from tools.semantic.cmake_consumer import consumer_cmake_arguments
        return consumer_cmake_arguments(runtime, protoc)

    def packages(self, runtime: Path) -> None:
        configs = {
            "protobuf/protobuf-config.cmake": (
                'find_package(absl CONFIG REQUIRED)\n'
                'find_package(utf8_range CONFIG REQUIRED)\n'
                'set(AXIOM_TEST_PROTOBUF_FOUND TRUE)\n'
            ),
            "absl/abslConfig.cmake": "set(AXIOM_TEST_ABSL_FOUND TRUE)\n",
            "utf8_range/utf8_range-config.cmake": "set(AXIOM_TEST_UTF8_FOUND TRUE)\n",
        }
        for name, text in configs.items():
            path = runtime / "lib/cmake" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")

    def alias(self, root: Path) -> Path:
        if os.name == "nt":
            from ctypes import wintypes
            get_short = ctypes.WinDLL("kernel32", use_last_error=True).GetShortPathNameW
            get_short.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD]
            get_short.restype = wintypes.DWORD
            length = get_short(str(root), None, 0)
            if not length:
                raise ctypes.WinError(ctypes.get_last_error())
            buffer = ctypes.create_unicode_buffer(length)
            if not get_short(str(root), buffer, length):
                raise ctypes.WinError(ctypes.get_last_error())
            result = Path(buffer.value)
            self.assertNotEqual(str(result), str(root), "Windows regression requires an actual short-path alias")
            return result
        alias = root.parent / "alias"
        alias.symlink_to(root, target_is_directory=True)
        return alias

    def configure(self, directory: Path, runtime: Path, *, decoy: Path | None = None) -> subprocess.CompletedProcess:
        script = directory / "discover.cmake"
        script.write_text(
            'cmake_minimum_required(VERSION 3.30)\n'
            'find_package(Protobuf CONFIG REQUIRED)\n'
            'if(NOT AXIOM_TEST_PROTOBUF_FOUND OR NOT AXIOM_TEST_ABSL_FOUND OR NOT AXIOM_TEST_UTF8_FOUND)\n'
            '  message(FATAL_ERROR "The SDK dependency closure was not found")\n'
            'endif()\n', encoding="utf-8")
        protoc = directory / "host/bin/protoc"
        protoc.parent.mkdir(parents=True, exist_ok=True)
        protoc.touch()
        environment = os.environ.copy()
        if decoy is not None:
            environment["CMAKE_PREFIX_PATH"] = str(decoy)
        return subprocess.run(
            ["cmake", *self.arguments(runtime, protoc), "--debug-find", "-P", str(script)],
            capture_output=True, text=True, env=environment, check=False,
        )

    def test_prefix_and_search_root_use_the_same_real_path(self):
        with tempfile.TemporaryDirectory(prefix="axiom-discovery-") as temporary:
            directory = Path(temporary).resolve()
            runtime = directory / "runtime with spaces"
            runtime.mkdir()
            protoc = runtime / "protoc.exe"
            protoc.touch()
            alias = self.alias(runtime)
            arguments = self.arguments(alias, alias / protoc.name)
            expected = runtime.resolve().as_posix()
            self.assertIn(f"-DCMAKE_PREFIX_PATH={expected}", arguments)
            self.assertIn(f"-DCMAKE_FIND_ROOT_PATH={expected}", arguments)
            self.assertIn(f"-DAXIOM_PROTOC={protoc.resolve().as_posix()}", arguments)

    def test_locked_candidate_workflow_uses_shared_cmake_argument_contract(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / ".github/workflows/semantic-sdk-consumer-validation.yml").read_text(encoding="utf-8")
        self.assertGreaterEqual(source.count("consumer_cmake_arguments"), 2)
        self.assertGreaterEqual(source.count("target_cmake_arguments"), 2)
        self.assertGreaterEqual(source.count("subprocess.run"), 4)
        self.assertNotIn('cmake -S . -B "out/consumer-$TARGET_KEY"', source)
        self.assertIn('build_type = "Debug" if family == "linux" else "Release"', source)

    def test_locked_candidate_web_lane_links_and_runs_node_smoke(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / ".github/workflows/semantic-sdk-consumer-validation.yml").read_text(encoding="utf-8")
        self.assertIn("canvas_semantic_golden_probe", source)
        self.assertIn('subprocess.run(["node"', source)
        self.assertIn("returncode == 64", source)

    def test_cross_compilers_do_not_see_definition_only_replace_batch_template(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / "runtime/semantic/src/operation_specific_validation.cpp").read_text(encoding="utf-8")
        self.assertNotEqual(
            source.count("replaceBatch("),
            1,
            "a definition-only anonymous template is rejected by Emscripten Clang with -Wunused-template",
        )

    def test_geometry_decoder_braces_loop_before_success_return(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / "runtime/semantic/src/protobuf_object_mapping.cpp").read_text(encoding="utf-8")
        self.assertNotIn(
            "for(const auto& command:source.commands()) switch(command.command_case()) {",
            source,
            "Windows and Android Clang reject the unbraced loop/switch followed by return with -Wmisleading-indentation",
        )

    def test_clangcl_generated_protobuf_offsetof_warning_is_suppressed_only_on_generated_sources(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / "runtime/semantic/CMakeLists.txt").read_text(encoding="utf-8")
        generated_begin = source.index('if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")')
        generated_end = source.index("  add_custom_command(", generated_begin)
        generated_policy = source[generated_begin:generated_end]
        self.assertIn(
            "-Wno-invalid-offsetof",
            generated_policy,
            "clang-cl 22 emits -Winvalid-offsetof from protoc-generated *.pb.cc; suppress only on generated sources",
        )
        self.assertEqual(source.count("-Wno-invalid offsetof"), 0)
        self.assertEqual(source.count("-Wno-invalid-offsetof"), 1)

    def test_snapshot_bootstrap_sort_declares_algorithm_dependency(self):
        root = Path(__file__).resolve().parents[3]
        source = (root / "runtime/semantic/tests/g1_06_snapshot_bootstrap_test.cpp").read_text(encoding="utf-8")
        include_block = source.split("namespace canvas::semantic", 1)[0]
        self.assertIn("std::sort(", source)
        self.assertIn(
            "#include <algorithm>",
            include_block,
            "std::sort must not depend on incidental transitive standard-library includes",
        )

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for the real package discovery probe")
    def test_relocated_sdk_is_discovered_through_filesystem_alias(self):
        with tempfile.TemporaryDirectory(prefix="axiom-discovery-") as temporary:
            directory = Path(temporary).resolve()
            actual = directory / "runtime with spaces"
            self.packages(actual)
            aliased = self.alias(actual)
            result = self.configure(directory, aliased)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for the real package discovery probe")
    def test_missing_runtime_does_not_fall_back_to_host_packages(self):
        with tempfile.TemporaryDirectory(prefix="axiom-discovery-") as temporary:
            directory = Path(temporary).resolve()
            runtime, decoy = directory / "runtime", directory / "host-install"
            runtime.mkdir()
            self.packages(decoy)
            result = self.configure(directory, runtime, decoy=decoy)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('provided by "Protobuf"', result.stdout + result.stderr)

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for the real package discovery probe")
    def test_missing_transitive_package_does_not_fall_back_to_host(self):
        with tempfile.TemporaryDirectory(prefix="axiom-discovery-") as temporary:
            directory = Path(temporary).resolve()
            runtime, decoy = directory / "runtime", directory / "host-install"
            self.packages(runtime)
            self.packages(decoy)
            (runtime / "lib/cmake/absl/abslConfig.cmake").unlink()
            result = self.configure(directory, runtime, decoy=decoy)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('provided by "absl"', result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
