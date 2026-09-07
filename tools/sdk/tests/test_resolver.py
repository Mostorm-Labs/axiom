"""Real filesystem/HTTP tests for index acquisition and provider orchestration."""
from dataclasses import replace
from functools import partial
import http.server
import io
import json
import os
from pathlib import Path
import tempfile
import threading
import unittest
from unittest import mock
import urllib.error
import urllib.request

from tools.sdk.archive import file_sha256
from tools.sdk.model import IntegrityError, OfflineError, ProviderPlan, ResolveRequest, SdkError, detect_host_platform
from tools.sdk.resolver import ProviderRegistry, resolve_provider
from tools.sdk.store import SdkStore
from tools.sdk.transport import ensure_release_file, release_url, SafeRedirectHandler
from tools.sdk.tests.fixtures import make_provider


class SilentHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        return


class ResolverTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.mirror = self.base / "mirror"
        self.provider, self.ref = make_provider(self.base, self.mirror)
        self.request = ResolveRequest(self.base / "project-one", detect_host_platform(),
                                      "native", self.base / "store", str(self.mirror))
        self.store = SdkStore(self.request.store_root)

    def test_filesystem_mirror_then_second_project_is_offline(self):
        first = resolve_provider(self.provider, self.request)
        self.assertFalse(first["facts"]["networkUsed"])
        self.assertEqual(first["facts"]["index"]["source"], "mirror")
        self.assertEqual(first["facts"]["artifacts"][0]["source"], "mirror")
        with mock.patch("tools.sdk.transport._open", side_effect=AssertionError("unexpected network")):
            second = resolve_provider(self.provider, replace(self.request, repo_root=self.base / "project-two",
                                                             mirror=None, offline=True))
        self.assertEqual(first["environment"], second["environment"])
        self.assertEqual(second["facts"]["index"]["source"], "store")
        self.assertEqual(second["facts"]["artifacts"][0]["source"], "store")
        self.assertFalse((self.base / "project-two/.deps").exists())

    def test_file_uri_mirror(self):
        result = resolve_provider(self.provider, replace(self.request, mirror=self.mirror.as_uri()))
        self.assertFalse(result["facts"]["networkUsed"])

    def test_loopback_http_mirror(self):
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), partial(SilentHandler, directory=str(self.mirror)))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            result = resolve_provider(self.provider, replace(self.request, mirror=f"http://127.0.0.1:{server.server_port}"))
            self.assertTrue(result["facts"]["networkUsed"])
            self.assertEqual(result["facts"]["artifacts"][0]["source"], "mirror")
        finally:
            server.shutdown()
            thread.join(timeout=5)
            server.server_close()

    def test_https_mirror_digest_and_network_fact(self):
        payload = (self.mirror / self.ref.release_tag / self.ref.asset).read_bytes()
        with mock.patch("tools.sdk.transport._open", return_value=io.BytesIO(payload)) as opened:
            result = ensure_release_file(self.ref, self.store.archive_path(self.ref),
                                         mirror="https://mirror.example.test/sdk", offline=False)
        self.assertEqual(result.source, "mirror")
        self.assertTrue(result.network_used)
        self.assertEqual(opened.call_args.args[0].full_url,
                         f"https://mirror.example.test/sdk/{self.ref.release_tag}/{self.ref.asset}")
        self.assertEqual(file_sha256(result.path), self.ref.sha256)

    def test_offline_missing_index_has_no_transport(self):
        with mock.patch("tools.sdk.transport._open", side_effect=AssertionError("unexpected network")):
            with self.assertRaisesRegex(OfflineError, "example-a"):
                resolve_provider(self.provider, replace(self.request, offline=True))

    def test_offline_index_hit_but_missing_package_does_not_install_cached_archive(self):
        ensure_release_file(self.provider.index, self.store.release_index_path(self.provider.index),
                            mirror=str(self.mirror), offline=False)
        ensure_release_file(self.ref, self.store.archive_path(self.ref), mirror=str(self.mirror), offline=False)
        with self.assertRaises(OfflineError):
            resolve_provider(self.provider, replace(self.request, offline=True))
        self.assertFalse(self.store.package_path(self.ref).exists())

    def test_bad_index_digest_is_rejected_before_provider_parse(self):
        (self.mirror / self.provider.index.release_tag / "index.json").write_text("broken")
        with mock.patch.object(self.provider, "plan", side_effect=AssertionError("parsed unverified index")):
            with self.assertRaisesRegex(IntegrityError, "example-a"):
                resolve_provider(self.provider, self.request)
        self.assertFalse(self.store.release_index_path(self.provider.index).exists())

    def test_bad_package_digest_is_rejected_before_install(self):
        (self.mirror / self.ref.release_tag / self.ref.asset).write_bytes(b"bad")
        with mock.patch.object(self.provider, "install", side_effect=AssertionError("installed unverified ZIP")):
            with self.assertRaises(IntegrityError):
                resolve_provider(self.provider, self.request)
        self.assertFalse(self.store.package_path(self.ref).exists())

    def test_invalid_existing_materialization_fails_closed(self):
        resolve_provider(self.provider, self.request)
        (self.store.package_path(self.ref) / "payload.txt").write_bytes(b"corrupt")
        with mock.patch("tools.sdk.transport._open", side_effect=AssertionError("unexpected network")):
            with self.assertRaisesRegex(IntegrityError, "example-a"):
                resolve_provider(self.provider, self.request)

    def test_cached_index_is_rehashed_before_parse(self):
        resolve_provider(self.provider, self.request)
        self.store.release_index_path(self.provider.index).write_text("corrupt")
        with mock.patch.object(self.provider, "plan", side_effect=AssertionError("parsed corrupt index")):
            with self.assertRaises(IntegrityError):
                resolve_provider(self.provider, self.request)

    def test_missing_mirror_falls_back_to_exact_locked_release(self):
        payload = (self.mirror / self.ref.release_tag / self.ref.asset).read_bytes()
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch("tools.sdk.transport._open", return_value=io.BytesIO(payload)) as opened:
            result = ensure_release_file(self.ref, self.store.archive_path(self.ref),
                                         mirror=str(self.base / "missing"), offline=False)
        self.assertEqual(opened.call_args.args[0].full_url, release_url(self.ref))
        self.assertEqual(result.source, "github")
        self.assertTrue(result.network_used)

    def test_corrupt_mirror_never_falls_back_to_github(self):
        (self.mirror / self.ref.release_tag / self.ref.asset).write_bytes(b"wrong")
        with mock.patch("tools.sdk.transport._open", side_effect=AssertionError("unexpected fallback")):
            with self.assertRaises(IntegrityError):
                ensure_release_file(self.ref, self.store.archive_path(self.ref), mirror=str(self.mirror), offline=False)
        self.assertFalse(self.store.archive_path(self.ref).exists())

    def test_provider_cannot_return_foreign_family(self):
        foreign = replace(self.ref, family="example-b")
        with mock.patch.object(self.provider, "plan", return_value=ProviderPlan("example-a", (foreign,), {})):
            with self.assertRaisesRegex(SdkError, "family"):
                resolve_provider(self.provider, self.request)
        self.assertFalse(self.store.package_path(foreign).exists())

    def test_provider_cannot_return_foreign_index(self):
        self.provider.index = replace(self.provider.index, family="example-b")
        with self.assertRaisesRegex(SdkError, "family"):
            resolve_provider(self.provider, self.request)

    def test_provider_exception_identifies_family(self):
        with mock.patch.object(self.provider, "plan", side_effect=ValueError("wrong schema")):
            with self.assertRaisesRegex(SdkError, "example-a.*wrong schema"):
                resolve_provider(self.provider, self.request)

    def test_duplicate_registry_family_is_rejected(self):
        registry = ProviderRegistry()
        registry.register(self.provider)
        with self.assertRaisesRegex(SdkError, "already registered"):
            registry.register(self.provider)

    def test_private_github_uses_authenticated_api_without_token_in_mirror(self):
        payload = (self.mirror / self.ref.release_tag / self.ref.asset).read_bytes()
        metadata = json.dumps({"assets": [{"id": 42, "name": self.ref.asset}]}).encode()
        with mock.patch.dict(os.environ, {"GH_TOKEN": "fixture-token"}, clear=True), mock.patch(
                "tools.sdk.transport._open", side_effect=[io.BytesIO(metadata), io.BytesIO(payload)]) as opened:
            result = ensure_release_file(self.ref, self.store.archive_path(self.ref), mirror=None, offline=False)
        self.assertTrue(opened.call_args_list[0].args[0].full_url.startswith("https://api.github.com/"))
        self.assertEqual(opened.call_args_list[1].args[0].get_header("Accept"), "application/octet-stream")
        self.assertEqual(result.source, "github")
        with mock.patch.dict(os.environ, {"GH_TOKEN": "fixture-token"}, clear=True), mock.patch(
                "tools.sdk.transport._open", return_value=io.BytesIO(payload)) as opened:
            ensure_release_file(self.ref, self.base / "mirror-result.zip", mirror="https://mirror.example.test", offline=False)
        self.assertIsNone(opened.call_args.args[0].get_header("Authorization"))

    def test_redirect_drops_credentials_at_origin_boundary(self):
        handler = SafeRedirectHandler()
        request = urllib.request.Request("https://api.github.com/asset",
                                         headers={"Authorization": "Bearer fixture", "Cookie": "secret"})
        redirected = handler.redirect_request(request, None, 302, "Found", {}, "https://objects.example.test/asset")
        self.assertIsNone(redirected.get_header("Authorization"))
        self.assertIsNone(redirected.get_header("Cookie"))
        with self.assertRaises(SdkError):
            handler.redirect_request(request, None, 302, "Found", {}, "http://objects.example.test/asset")


if __name__ == "__main__":
    unittest.main()
