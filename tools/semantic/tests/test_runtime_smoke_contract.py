"""Source-free qualification must link an executable, not only make an archive."""
from pathlib import Path
import unittest

from tools.sdk.model import HostPlatform
from tools.semantic.smoke_consumer import can_execute_target

ROOT = Path(__file__).resolve().parents[3]


class RuntimeSmokeContractTest(unittest.TestCase):
    def test_native_execution_is_not_claimed_for_cross_targets(self):
        linux = HostPlatform("linux", "x86_64", "linux-x86_64")
        apple = HostPlatform("macos", "arm64", "macos-arm64")
        self.assertTrue(can_execute_target(linux, "linux-x86_64"))
        self.assertTrue(can_execute_target(apple, "macos-arm64"))
        self.assertFalse(can_execute_target(apple, "macos-x64"))
        self.assertFalse(can_execute_target(apple, "ios-arm64"))
        self.assertFalse(can_execute_target(linux, "android-x86_64"))

    def test_smoke_cmake_uses_separate_host_protoc(self):
        cmake = (ROOT / "tools/semantic/smoke/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('COMMAND "${AXIOM_PROTOC}"', cmake)
        self.assertIn("protobuf::libprotobuf", cmake)
        self.assertIn("add_executable(semantic_sdk_smoke", cmake)
        self.assertNotIn("protobuf::protoc", cmake)
        self.assertNotIn("FetchContent", cmake)
        self.assertNotIn("ExternalProject", cmake)


if __name__ == "__main__":
    unittest.main()
