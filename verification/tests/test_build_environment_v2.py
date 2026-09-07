"""Developer entry points share the v2 Store and preserve v1 transition explicitly."""
from dataclasses import replace
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import detect_host_platform, HostPlatform, SdkError, OfflineError
from tools.semantic.tests.provider_fixtures import provider_fixture
import tools.setup_build_environment as build_env

ROOT = Path(__file__).resolve().parents[2]
LINUX = HostPlatform('linux', 'x86_64', 'linux-x86_64')


class BuildEnvironmentV2Test(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.repo, self.mirror, self.lock, self.index = provider_fixture(self.base)
        self.store = self.base / 'shared SDKs'

    def resolve(self, **overrides):
        kwargs = dict(core=False, semantic=True, target='native', repo_root=self.repo,
                      host=LINUX, store_root=self.store, mirror=str(self.mirror))
        kwargs.update(overrides)
        return build_env.setup_environment(**kwargs)

    def test_entrypoint_uses_provider_without_any_subprocess(self):
        with (mock.patch('tools.sdk.transport._open', side_effect=AssertionError('network')),
              mock.patch.object(build_env.subprocess, 'run', side_effect=AssertionError('source bootstrap'))):
            first = self.resolve()
            shutil.rmtree(self.mirror)
            second = self.resolve(offline=True, mirror=None)
        self.assertEqual(first['environment'], second['environment'])
        self.assertEqual(second['target'], 'linux-x86_64')
        self.assertEqual(second['semantic']['mode'], 'v2')
        self.assertEqual(second['semantic']['releaseSetId'], self.lock['releaseSetId'])
        self.assertFalse(second['semantic']['resolution']['networkUsed'])
        self.assertFalse((self.repo / '.deps').exists())

    def test_explicit_store_mirror_override_environment_values(self):
        with mock.patch.dict(os.environ, {'AXIOM_SDK_STORE': str(self.base / 'unused'),
                                         'AXIOM_SDK_MIRROR': str(self.base / 'missing')}):
            result = self.resolve()
        self.assertTrue(Path(result['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT']).is_relative_to(self.store.resolve()))
        self.assertFalse((self.base / 'unused').exists())

    def test_environment_store_mirror_used_without_cli_overrides(self):
        with mock.patch.dict(os.environ, {'AXIOM_SDK_STORE': str(self.store),
                                         'AXIOM_SDK_MIRROR': str(self.mirror)}):
            result = self.resolve(store_root=None, mirror=None)
        self.assertTrue(Path(result['environment']['AXIOM_PROTOC']).is_relative_to(self.store.resolve()))

    def test_host_autodetection_resolves_native_target(self):
        host = detect_host_platform()
        with mock.patch.object(build_env, 'detect_host_platform', return_value=host):
            result = self.resolve(host=None)
        self.assertEqual(result['host'], host.key)
        self.assertTrue(Path(result['environment']['AXIOM_PROTOC']).is_file())

    def test_malformed_present_v2_lock_never_uses_v1(self):
        (self.repo / 'semantic-sdk.lock.json').write_bytes(b'{}')
        with mock.patch.object(build_env, '_fetch_semantic', side_effect=AssertionError('v1 fallback')):
            with self.assertRaises(SdkError):
                self.resolve()
        self.assertFalse((self.repo / '.deps').exists())

    def test_present_v2_filename_cannot_downgrade_to_v1_format(self):
        shutil.copyfile(self.repo / 'semantic-toolchain.lock.json', self.repo / 'semantic-sdk.lock.json')
        with mock.patch.object(build_env, '_fetch_semantic', side_effect=AssertionError('implicit downgrade')):
            with self.assertRaises(SdkError):
                self.resolve(store_root=None, mirror=None)
        self.assertFalse((self.repo / '.deps').exists())

    def test_missing_explicit_lock_never_uses_default(self):
        with self.assertRaises(SdkError):
            self.resolve(lock_path=self.base / 'nonexistent.lock.json')
        self.assertFalse(self.store.exists())

    def test_offline_plus_core_fails_before_bootstrap(self):
        with mock.patch.object(build_env.subprocess, 'run', side_effect=AssertionError('network bootstrap')):
            with self.assertRaisesRegex(SdkError, 'offline.*core|core.*offline'):
                self.resolve(core=True, offline=True)

    def test_v1_offline_is_explicit_failure_not_download(self):
        (self.repo / 'semantic-sdk.lock.json').unlink()
        with mock.patch.object(build_env.subprocess, 'run', side_effect=AssertionError('v1 download')):
            with self.assertRaisesRegex(SdkError, 'offline'):
                self.resolve(offline=True)

    def test_github_env_contains_both_roots_and_no_legacy_package_dirs(self):
        result = self.resolve()
        output = self.base / 'env'
        build_env.write_github_env(output, result['environment'])
        values = dict(line.split('=', 1) for line in output.read_text().splitlines())
        for name in ('AXIOM_SEMANTIC_HOST_ROOT', 'AXIOM_SEMANTIC_RUNTIME_ROOT', 'AXIOM_PROTOC'):
            self.assertEqual(values[name], result['environment'][name])
        self.assertNotIn('PROTOBUF_DIR', values)
        self.assertNotIn('ABSL_DIR', values)
        self.assertNotIn('UTF8_RANGE_DIR', values)

    def test_export_rejects_newline_injection_before_writing(self):
        output = self.base / 'env'
        output.write_text('PRESERVE=1\n')
        with self.assertRaises(SdkError):
            build_env.write_github_env(output, {'AXIOM_PROTOC': 'path\nINJECTED=1'})
        self.assertEqual(output.read_text(), 'PRESERVE=1\n')


class BuildEnvironmentCliTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.repo, self.mirror, self.lock, self.index = provider_fixture(self.base)
        # A separate repository tree proves default authority is script-relative, not cwd-relative.
        shutil.copytree(ROOT / 'tools', self.repo / 'tools', ignore=shutil.ignore_patterns('__pycache__'))
        self.store = self.base / 'Shared SDKs'
        self.env = {**os.environ, 'AXIOM_SDK_STORE': str(self.store), 'AXIOM_SDK_MIRROR': str(self.mirror),
                    'CANVAS_SEMANTIC_SDK_BASE_URL': 'http://127.0.0.1:9'}
        self.setup_script = self.repo / 'tools/setup_build_environment.py'
        self.fetch_script = self.repo / 'tools/semantic_fetch.py'

    def invoke(self, script, *args):
        return subprocess.run([sys.executable, str(script), *map(str, args)], cwd=self.base,
                              env=self.env, capture_output=True, text=True)

    def test_cli_resolve_status_env_and_offline_second_invocation(self):
        facts, env_file = self.base / 'facts.json', self.base / 'github-env'
        first = self.invoke(self.setup_script, '--semantic', '--status', '--facts-output', facts,
                            '--github-env', env_file)
        self.assertEqual(first.returncode, 0, first.stderr)
        result = json.loads(first.stdout.splitlines()[-1])
        self.assertEqual(json.loads(facts.read_bytes()), result)
        self.assertIn(self.lock['releaseTag'], first.stderr)
        self.assertIn('network', first.stderr.lower())
        shutil.rmtree(self.mirror)
        second = self.invoke(self.setup_script, '--semantic', '--offline', '--status')
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(result['environment'], json.loads(second.stdout.splitlines()[-1])['environment'])
        self.assertFalse((self.repo / '.deps').exists())

    def test_fetch_cli_is_same_provider_and_destination_does_not_relocate_v2(self):
        first = self.invoke(self.setup_script, '--semantic')
        self.assertEqual(first.returncode, 0, first.stderr)
        second = self.invoke(self.fetch_script, '--offline')
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(json.loads(first.stdout)['environment']['AXIOM_PROTOC'],
                         json.loads(second.stdout)['environment']['AXIOM_PROTOC'])
        destination = self.base / 'legacy destination'
        rejected = self.invoke(self.fetch_script, '--destination', destination)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn('--destination', rejected.stderr)
        self.assertFalse(destination.exists())

    def test_explicit_v1_lock_remains_selectable_when_v2_exists(self):
        # A malformed explicit v1 lock must receive the v1 error, never resolve the valid default v2.
        v1 = json.loads((self.repo / 'semantic-toolchain.lock.json').read_bytes())
        v1['asset'] = 'wrong.zip'
        path = self.base / 'explicit-v1.json'
        path.write_bytes(canonical_bytes(v1))
        result = self.invoke(self.fetch_script, '--lock', path)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid asset or digest', result.stderr)
        self.assertFalse(self.store.exists())

    def test_default_fetch_rejects_v1_bytes_under_v2_filename(self):
        shutil.copyfile(self.repo / 'semantic-toolchain.lock.json', self.repo / 'semantic-sdk.lock.json')
        result = self.invoke(self.fetch_script)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('v2', result.stderr)
        self.assertFalse((self.repo / '.deps').exists())

    @unittest.skipUnless(sys.platform.startswith('linux'), 'v1 is a Linux-only historical toolchain')
    def test_real_v1_archive_still_consumes_explicitly_and_during_transition(self):
        from verification.tests.test_semantic_sdk import SemanticSdkTest, module as sdk
        from tools.sdk.archive import file_sha256
        fixture = SemanticSdkTest()
        install = fixture._complete_root(self.base)
        identity, sdk_id = sdk.make_identity(fixture._lock(), fixture._toolchain())
        asset = f'semantic-toolchain-linux-x86_64-{sdk_id}.zip'
        release = self.base / 'v1-mirror' / 'historical-fixture'
        release.mkdir(parents=True)
        sdk.create_archive(install, sdk.make_manifest(install, identity, sdk_id), release / asset)
        v1 = {'format': 'axiom-semantic-toolchain-lock-v1', 'repository': 'Mostorm-Labs/axiom',
              'releaseTag': release.name, 'target': 'linux-x86_64', 'asset': asset,
              'sdkId': sdk_id, 'sha256': file_sha256(release / asset)}
        path = self.repo / 'semantic-toolchain.lock.json'
        path.write_bytes(canonical_bytes(v1))
        self.env['CANVAS_SEMANTIC_SDK_BASE_URL'] = release.parent.as_uri()
        destination = self.base / 'v1-installed'
        fetched = self.invoke(self.fetch_script, '--lock', path, '--destination', destination)
        self.assertEqual(fetched.returncode, 0, fetched.stderr)
        self.assertEqual(json.loads(fetched.stdout)['sdkId'], sdk_id)
        self.assertEqual((destination / 'bin/protoc').read_bytes(), b'protoc')
        self.assertFalse(self.store.exists())
        (self.repo / 'semantic-sdk.lock.json').unlink()
        transition = self.invoke(self.setup_script, '--semantic')
        self.assertEqual(transition.returncode, 0, transition.stderr)
        self.assertEqual(json.loads(transition.stdout.splitlines()[-1])['semantic']['mode'], 'v1')
        self.assertTrue((self.repo / '.deps/protobuf/bin/protoc').exists())

    def test_status_without_semantic_selector_still_resolves_semantic(self):
        result = self.invoke(self.setup_script, '--status')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['semantic']['mode'], 'v2')

    def test_offline_cold_failure_does_not_emit_success_facts(self):
        facts = self.base / 'facts.json'
        result = self.invoke(self.setup_script, '--semantic', '--offline', '--facts-output', facts)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('offline', result.stderr.lower())
        self.assertFalse(facts.exists())
        self.assertFalse((self.repo / '.deps').exists())


if __name__ == '__main__':
    unittest.main()
