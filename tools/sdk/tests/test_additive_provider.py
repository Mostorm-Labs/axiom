"""A second provider cannot invalidate a previously materialized family."""
from dataclasses import replace
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools.sdk.archive import file_sha256
from tools.sdk.model import ResolveRequest, detect_host_platform
from tools.sdk.resolver import ProviderRegistry
from tools.sdk.tests.fixtures import make_provider


class AdditiveProviderTest(unittest.TestCase):
    def test_second_provider_preserves_first_identity_paths_exports_and_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            mirror = base / "mirror"
            first, first_ref = make_provider(base, mirror, "example-a")
            second, _ = make_provider(base, mirror, "example-b")
            request = ResolveRequest(base / "worktree-a", detect_host_platform(), "native", base / "store", str(mirror))
            registry = ProviderRegistry()
            registry.register(first)
            before = registry.resolve("example-a", request)
            index_before = first.index_ref(request)
            root = Path(before["facts"]["artifacts"][0]["root"])
            bytes_before = {p.relative_to(root).as_posix(): file_sha256(p) for p in root.rglob("*") if p.is_file()}
            registry.register(second)
            other = registry.resolve("example-b", request)
            with mock.patch("tools.sdk.transport._open", side_effect=AssertionError("network on cache hit")):
                after = registry.resolve("example-a", replace(request, repo_root=base / "worktree-b", offline=True))
            self.assertEqual(index_before, first.index_ref(request))
            self.assertEqual(before["environment"], after["environment"])
            self.assertEqual(before["facts"]["artifacts"][0]["root"], after["facts"]["artifacts"][0]["root"])
            self.assertEqual(after["facts"]["artifacts"][0]["identity"], first_ref.identity)
            self.assertEqual(bytes_before, {p.relative_to(root).as_posix(): file_sha256(p) for p in root.rglob("*") if p.is_file()})
            self.assertNotEqual(other["facts"]["artifacts"][0]["root"], str(root))
            self.assertEqual(after["facts"]["artifacts"][0]["source"], "store")
            self.assertFalse(after["facts"]["networkUsed"])


if __name__ == "__main__":
    unittest.main()
