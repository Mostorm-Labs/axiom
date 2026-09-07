"""Unit tests inspect producer commands without compiling dependencies."""
from pathlib import Path
import tempfile
import unittest

from tools.sdk.model import SdkError
from tools.semantic.contract import load_profile, RUNTIME_KEYS
from tools.semantic.toolchain import target_cmake_arguments
from tools.semantic.build_runtime import runtime_build_commands


class RuntimeBuildPlanTest(unittest.TestCase):
    def setUp(self):
        self.profile = load_profile()
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)

    def arguments(self, key):
        return target_cmake_arguments(self.profile, key, ndk=self.base / "ndk",
                                      emscripten=self.base / "emscripten", cc="test-cc", cxx="test-cxx")

    def test_same_target_contract_reaches_both_dependencies(self):
        required = ["-Dprotobuf_BUILD_TESTS=OFF", "-Dprotobuf_BUILD_CONFORMANCE=OFF",
                    "-Dprotobuf_BUILD_EXAMPLES=OFF", "-Dprotobuf_BUILD_LIBPROTOC=OFF",
                    "-Dprotobuf_BUILD_PROTOC_BINARIES=OFF", "-Dprotobuf_BUILD_LIBUPB=OFF",
                    "-Dprotobuf_ABSL_PROVIDER=package", "-Dprotobuf_BUILD_SHARED_LIBS=OFF",
                    "-Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON", "-Dprotobuf_WITH_ZLIB=OFF"]
        for key in RUNTIME_KEYS:
            args = self.arguments(key)
            commands = runtime_build_commands(self.base / "protobuf", self.base / "abseil",
                                               self.base / "build", self.base / "install", args, jobs=2)
            with self.subTest(key=key):
                abseil, build_abseil, protobuf, build_protobuf = commands
                self.assertIn("-DCMAKE_CXX_STANDARD=20", abseil)
                self.assertIn("-DCMAKE_CXX_STANDARD=20", protobuf)
                self.assertIn("-DABSL_ENABLE_INSTALL=ON", abseil)
                self.assertIn("-DABSL_BUILD_TESTING=OFF", abseil)
                for arg in args:
                    self.assertIn(arg, abseil)
                    self.assertIn(arg, protobuf)
                for arg in required:
                    self.assertIn(arg, protobuf)
                for command in (build_abseil, build_protobuf):
                    self.assertEqual(command[-4:], ["--target", "install", "--parallel", "2"])

    def test_windows_static_crt_and_iterator_flags(self):
        args = self.arguments("windows-x64-msvc-static")
        self.assertIn("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded", args)
        self.assertTrue(any("_ITERATOR_DEBUG_LEVEL=0" in arg for arg in args))
        self.assertTrue(any("_HAS_ITERATOR_DEBUGGING=0" in arg for arg in args))

    def test_apple_architecture_sdk_and_deployment(self):
        for key, arch, sdk in (("macos-arm64", "arm64", "macosx"),
                               ("macos-x64", "x86_64", "macosx"),
                               ("ios-arm64", "arm64", "iphoneos"),
                               ("ios-simulator-arm64", "arm64", "iphonesimulator")):
            args = self.arguments(key)
            with self.subTest(key=key):
                self.assertIn(f"-DCMAKE_OSX_ARCHITECTURES={arch}", args)
                self.assertIn(f"-DCMAKE_OSX_SYSROOT={sdk}", args)
                if key.startswith("ios"):
                    self.assertIn("-DCMAKE_SYSTEM_NAME=iOS", args)
                    self.assertIn("-DCMAKE_OSX_DEPLOYMENT_TARGET=17.0", args)

    def test_android_ndk_abi_api_and_web_toolchain(self):
        for key, abi in (("android-arm64-v8a", "arm64-v8a"), ("android-x86_64", "x86_64")):
            args = self.arguments(key)
            self.assertIn(f"-DANDROID_ABI={abi}", args)
            self.assertIn("-DANDROID_PLATFORM=android-26", args)
            self.assertTrue(any("android.toolchain.cmake" in arg for arg in args))
        args = self.arguments("web-wasm32")
        self.assertTrue(any("Emscripten.cmake" in arg for arg in args))
        self.assertFalse(any("-pthread" in arg for arg in args))

    def test_missing_cross_toolchain_and_unknown_target_fail_before_build(self):
        for key in ("android-arm64-v8a", "web-wasm32", "unknown"):
            with self.subTest(key=key), self.assertRaises(SdkError):
                target_cmake_arguments(self.profile, key)

    def test_build_job_count_cannot_be_invalid(self):
        with self.assertRaises(SdkError):
            runtime_build_commands(self.base, self.base, self.base, self.base, [], jobs=0)


if __name__ == "__main__":
    unittest.main()
