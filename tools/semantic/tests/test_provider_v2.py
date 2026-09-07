"""Semantic consumes pinned packages through generic Store, never source builds."""
from dataclasses import replace
from pathlib import Path
import copy
import shutil
import tempfile
import unittest
from unittest import mock

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import (HostPlatform, ResolveRequest, SdkError, IntegrityError,
                             OfflineError, detect_host_platform)
from tools.sdk.resolver import resolve_provider
from tools.sdk.store import SdkStore
from tools.semantic.contract import ROOT, read_json
from tools.semantic.provider import SemanticProvider, select_keys
from tools.semantic.tests.provider_fixtures import provider_fixture

LINUX = HostPlatform('linux', 'x86_64', 'linux-x86_64')
WINDOWS = HostPlatform('windows', 'x64', 'windows-x64')
MAC_ARM = HostPlatform('macos', 'arm64', 'macos-arm64')
MAC_X64 = HostPlatform('macos', 'x64', 'macos-x64')


class ProviderSelectionTest(unittest.TestCase):
    def test_native_and_cross_pairings(self):
        cases = ((LINUX, 'native', 'linux-x86_64', 'linux-x86_64'),
                 (WINDOWS, 'native', 'windows-x64', 'windows-x64-msvc-static'),
                 (MAC_ARM, 'native', 'macos-universal', 'macos-arm64'),
                 (MAC_X64, 'native', 'macos-universal', 'macos-x64'),
                 (MAC_ARM, 'ios-arm64', 'macos-universal', 'ios-arm64'),
                 (MAC_ARM, 'macos-x64', 'macos-universal', 'macos-x64'),
                 (LINUX, 'android-x86_64', 'linux-x86_64', 'android-x86_64'),
                 (LINUX, 'web-wasm32', 'linux-x86_64', 'web-wasm32'),
                 (WINDOWS, 'android-arm64-v8a', 'windows-x64', 'android-arm64-v8a'))
        for host, target, host_key, target_key in cases:
            with self.subTest(host=host.key, target=target):
                self.assertEqual(select_keys(host, target), (host_key, target_key))

    def test_invalid_pair_fails_before_index_or_archive_download(self):
        for host, target in ((LINUX, 'ios-arm64'), (WINDOWS, 'macos-arm64'),
                             (MAC_ARM, 'windows-x64-msvc-static'), (LINUX, 'typo'),
                             (HostPlatform('linux', 'aarch64', 'linux-arm64'), 'native')):
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                request = ResolveRequest(root, host, target, root / 'store')
                with mock.patch('tools.sdk.transport._open', side_effect=AssertionError('network')):
                    with self.assertRaisesRegex(SdkError, 'unsupported.*host|unsupported.*target|unsupported.*pair'):
                        resolve_provider(SemanticProvider(), request)
                self.assertFalse((root / 'store').exists())


class SemanticProviderTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.repo, self.mirror, self.lock, self.index = provider_fixture(self.base)
        self.request = ResolveRequest(self.repo, LINUX, 'native', self.base / 'shared store', str(self.mirror))
        self.provider = SemanticProvider()
        self.store = SdkStore(self.request.store_root)
        self.network = mock.patch('tools.sdk.transport._open', side_effect=AssertionError('network forbidden'))
        self.network.start()
        self.addCleanup(self.network.stop)

    def test_only_required_pair_is_materialized_with_real_paths(self):
        resolved = resolve_provider(self.provider, self.request)
        env, facts = resolved['environment'], resolved['facts']
        self.assertEqual({p['key'] for p in facts['artifacts']}, {'linux-x86_64'})
        self.assertEqual({p['kind'] for p in facts['artifacts']}, {'host-tools', 'runtimes'})
        self.assertEqual(len(list((self.store.root / 'archives/sha256').glob('*/*.zip'))), 2)
        runtime = self.store.root / 'packages/semantic/runtimes' / self.index['runtimes']['linux-x86_64']['runtimeId']
        host = self.store.root / 'packages/semantic/host-tools' / self.index['hostTools']['linux-x86_64']['hostToolId']
        self.assertEqual(env['AXIOM_PROTOC'], str((host / 'bin/protoc').resolve()))
        self.assertEqual(env['AXIOM_SEMANTIC_RUNTIME_ROOT'], str(runtime.resolve()))
        self.assertEqual(env['AXIOM_SEMANTIC_HOST_ROOT'], str(host.resolve()))
        self.assertEqual(env['CMAKE_PREFIX_PATH'], env['AXIOM_SEMANTIC_SDK_ROOT'])
        self.assertNotIn('PROTOBUF_DIR', env)
        self.assertFalse((self.repo / '.deps').exists())
        self.assertFalse(facts['networkUsed'])
        self.assertEqual(facts['metadata']['releaseSetId'], self.lock['releaseSetId'])
        self.assertEqual(facts['metadata']['target'], 'linux-x86_64')

    def test_second_worktree_reuses_materialized_packages_without_archives_or_mirror(self):
        first = resolve_provider(self.provider, self.request)
        roots = [Path(p['root']) for p in first['facts']['artifacts']]
        before = [(p / 'manifest.json').stat().st_mtime_ns for p in roots]
        second_repo = self.base / 'project two'
        shutil.copytree(self.repo, second_repo)
        shutil.rmtree(self.mirror)
        shutil.rmtree(self.store.root / 'archives')
        with mock.patch('tools.semantic.provider.install_host', side_effect=AssertionError('reinstall')), \
             mock.patch('tools.semantic.provider.install_runtime', side_effect=AssertionError('reinstall')):
            second = resolve_provider(SemanticProvider(), replace(self.request, repo_root=second_repo,
                                       mirror=None, offline=True))
        self.assertEqual(first['environment'], second['environment'])
        self.assertEqual(before, [(p / 'manifest.json').stat().st_mtime_ns for p in roots])
        self.assertTrue(all(a['source'] == 'store' for a in second['facts']['artifacts']))
        self.assertFalse(second['facts']['networkUsed'])
        self.assertFalse((second_repo / '.deps').exists())

    def test_windows_tool_name_and_cross_runtime_root_are_separate(self):
        result = resolve_provider(self.provider, replace(self.request, host=WINDOWS, target='android-x86_64'))
        self.assertTrue(result['environment']['AXIOM_PROTOC'].endswith('protoc.exe'))
        self.assertNotEqual(result['environment']['AXIOM_SEMANTIC_HOST_ROOT'],
                            result['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT'])
        self.assertEqual([a['key'] for a in result['facts']['artifacts']], ['windows-x64', 'android-x86_64'])

    def test_corrupt_index_rejected_before_provider_parsing(self):
        (self.mirror / self.lock['releaseTag'] / 'semantic-sdk-index.json').write_bytes(b'{broken')
        with mock.patch.object(self.provider, 'plan', side_effect=AssertionError('unverified index parsed')):
            with self.assertRaises(IntegrityError):
                resolve_provider(self.provider, self.request)

    def test_canonical_index_identity_must_match_lock(self):
        wrong = copy.deepcopy(self.lock)
        wrong['releaseSetId'] = 'f' * 64
        wrong['releaseTag'] = 'semantic-sdk-v2-' + 'f' * 16
        shutil.copytree(self.mirror / self.lock['releaseTag'], self.mirror / wrong['releaseTag'])
        (self.repo / 'semantic-sdk.lock.json').write_bytes(canonical_bytes(wrong))
        with self.assertRaisesRegex(IntegrityError, 'releaseSetId'):
            resolve_provider(self.provider, self.request)

    def test_corrupt_archive_or_materialization_never_falls_back_to_source(self):
        key = self.index['runtimes']['linux-x86_64']
        archive = self.mirror / self.lock['releaseTag'] / key['asset']
        saved = archive.read_bytes()
        archive.write_bytes(b'corrupt')
        with self.assertRaises(IntegrityError):
            resolve_provider(self.provider, self.request)
        archive.write_bytes(saved)
        result = resolve_provider(self.provider, self.request)
        runtime = Path(result['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT'])
        (runtime / 'lib/libprotobuf.a').write_bytes(b'corrupt installed library')
        with self.assertRaises(IntegrityError):
            resolve_provider(self.provider, self.request)
        self.assertEqual((runtime / 'lib/libprotobuf.a').read_bytes(), b'corrupt installed library')

    def test_offline_cold_store_has_no_network_or_materialization(self):
        with self.assertRaises(OfflineError):
            resolve_provider(self.provider, replace(self.request, offline=True))
        self.assertFalse((self.store.root / 'packages').exists())

    def test_historical_identity_not_compared_with_current_producer_profile(self):
        # Deliberately unusable current profile: consumers must read accepted index identities only.
        with mock.patch('tools.semantic.contract.load_profile', side_effect=AssertionError('current recipe used')):
            result = resolve_provider(self.provider, self.request)
        self.assertEqual(result['facts']['metadata']['releaseSetId'], self.lock['releaseSetId'])

    def test_forged_ref_cannot_validate_a_different_package(self):
        self.provider.index_ref(self.request)
        plan = self.provider.plan(self.request, self.mirror / self.lock['releaseTag'] / 'semantic-sdk-index.json')
        forged = replace(plan.artifacts[0], identity='a' * 64)
        with self.assertRaises(SdkError):
            self.provider.validate(forged, self.base / 'missing')


if __name__ == '__main__':
    unittest.main()
