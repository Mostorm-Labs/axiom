"""Selection oracles and real Git/CLI tests; no dependency builds are needed."""
from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.semantic import classify_producer_changes as module
from tools.semantic.contract import make_host_tool_identity
from tools.skia.classify_r1_changes import classify as classify_skia

ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / 'tools/semantic/classify_producer_changes.py'
PROFILE_PATH = 'tools/semantic/profile-v2.json'
HOSTS = ['linux-x86_64', 'windows-x64', 'macos-universal']
RUNTIMES = ['linux-x86_64', 'windows-x64-msvc-static', 'macos-arm64', 'macos-x64',
            'ios-arm64', 'ios-simulator-arm64', 'android-arm64-v8a', 'android-x86_64', 'web-wasm32']
ANDROID = ['android-arm64-v8a', 'android-x86_64']
APPLE = ['macos-arm64', 'macos-x64', 'ios-arm64', 'ios-simulator-arm64']


def lock_document() -> dict:
    return json.loads((ROOT / 'deps.lock.json').read_bytes())


def profile_document() -> dict:
    return json.loads((ROOT / PROFILE_PATH).read_bytes())


class SelectionAssertions(unittest.TestCase):
    def assertSelection(self, result: dict, hosts: list[str], runtimes: list[str]) -> None:
        mode = 'full' if hosts == HOSTS and runtimes == RUNTIMES else 'partial' if hosts or runtimes else 'none'
        self.assertEqual(result.get('mode'), mode, result)
        self.assertEqual(result.get('host_tools'), hosts)
        self.assertEqual(result.get('runtimes'), runtimes)
        self.assertIsInstance(result.get('reason'), str)
        self.assertTrue(result['reason'])
        # Temporary compatibility for the still-live historical v1 workflow.
        self.assertEqual(result.get('should_build'), bool(hosts or runtimes))


class ProducerChangeClassifierTest(SelectionAssertions):
    def changed_lock(self, dependency: str, field: str, value: object) -> dict:
        before = lock_document()
        after = copy.deepcopy(before)
        after['dependencies'][dependency][field] = value
        return module.classify(['deps.lock.json'], lock_before=before, lock_after=after)

    def test_protobuf_version_source_and_edition_select_complete_set(self):
        for field, value in [('version', '37.0'), ('source_sha256', 'a' * 64),
                             ('source_url', 'https://example.invalid/protobuf.tar.gz'), ('edition', '2025')]:
            with self.subTest(field=field):
                self.assertSelection(self.changed_lock('protobuf', field, value), HOSTS, RUNTIMES)

    def test_source_sha_requalification_preserves_host_identities(self):
        before, profile = lock_document(), profile_document()
        after = copy.deepcopy(before)
        after['dependencies']['protobuf']['source_sha256'] = 'a' * 64
        result = module.classify(['deps.lock.json'], lock_before=before, lock_after=after)
        self.assertSelection(result, HOSTS, RUNTIMES)
        self.assertIn('requalif', result['reason'])
        for host in HOSTS:
            self.assertEqual(make_host_tool_identity(before, profile, host),
                             make_host_tool_identity(after, profile, host))

    def test_each_host_asset_change_selects_only_its_host(self):
        for host, upstream in [('linux-x86_64', 'linux-x86_64'), ('windows-x64', 'windows-x64'),
                               ('macos-universal', 'darwin-universal')]:
            for field, value in [('sha256', 'a' * 64), ('url', 'https://example.invalid/protoc.zip')]:
                with self.subTest(host=host, field=field):
                    before = lock_document()
                    after = copy.deepcopy(before)
                    after['dependencies']['protobuf']['protoc_assets'][upstream][field] = value
                    self.assertSelection(module.classify(['deps.lock.json'], lock_before=before,
                                                         lock_after=after), [host], [])

    def test_unselected_upstream_asset_does_not_invalidate_existing_cells(self):
        before = lock_document()
        after = copy.deepcopy(before)
        after['dependencies']['protobuf']['protoc_assets']['future-host'] = {
            'url': 'https://example.invalid/future.zip', 'sha256': 'a' * 64}
        self.assertSelection(module.classify(['deps.lock.json'], lock_before=before, lock_after=after), [], [])

    def test_abseil_changes_select_runtimes_not_hosts(self):
        for field, value in [('version', '20260101.0'), ('source_sha256', 'b' * 64),
                             ('source_url', 'https://example.invalid/abseil.tar.gz')]:
            with self.subTest(field=field):
                self.assertSelection(self.changed_lock('abseil', field, value), [], RUNTIMES)

    def test_locked_toolchains_select_only_their_target_families(self):
        for dep, field, value, selected in [
            ('windows_llvm', 'sha256', 'a' * 64, ['windows-x64-msvc-static']),
            ('android_ndk', 'api_level', 27, ANDROID),
            ('emscripten', 'version', '6.0.7', ['web-wasm32']),
        ]:
            with self.subTest(dependency=dep):
                self.assertSelection(self.changed_lock(dep, field, value), [], selected)
        before = lock_document()
        after = copy.deepcopy(before)
        after['dependencies']['apple_toolchain'] = {'xcode': 'future-qualified-version'}
        self.assertSelection(module.classify(['deps.lock.json'], lock_before=before, lock_after=after), [], APPLE)

    def test_runtime_contract_code_selects_all_runtimes(self):
        for path in ['build_runtime.py', 'package_runtime.py', 'toolchain.py', 'qualify_runtime.py',
                     'smoke_consumer.py', 'cmake_consumer.py', 'smoke/CMakeLists.txt', 'smoke/main.cpp']:
            with self.subTest(path=path):
                self.assertSelection(module.classify(['tools/semantic/' + path]), [], RUNTIMES)

    def test_host_packager_selects_only_hosts(self):
        self.assertSelection(module.classify(['tools/semantic/package_host.py']), HOSTS, [])

    def test_windows_launcher_is_explicitly_windows_only(self):
        self.assertSelection(module.classify(['tools/semantic/windows_compile.py']), [], ['windows-x64-msvc-static'])

    def test_shared_byte_contracts_select_both_kinds(self):
        for path in ['tools/semantic/contract.py', 'tools/semantic/package_common.py',
                     'tools/semantic/sources.py', 'tools/sdk/archive.py',
                     '.github/workflows/semantic-sdk-producer.yml']:
            with self.subTest(path=path):
                self.assertSelection(module.classify([path]), HOSTS, RUNTIMES)

    def test_historical_recipe_remains_conservative_until_v1_retirement(self):
        for path in ['tools/bootstrap_deps.py', 'tools/semantic_sdk.py',
                     'verification/tests/test_semantic_sdk.py', '.github/workflows/semantic-toolchain-producer.yml']:
            with self.subTest(path=path):
                self.assertSelection(module.classify([path]), HOSTS, RUNTIMES)

    def test_profile_runtime_entries_are_compared_by_exact_key(self):
        for targets in [['windows-x64-msvc-static'], ANDROID, ['web-wasm32'], *[[key] for key in APPLE]]:
            before = profile_document()
            after = copy.deepcopy(before)
            for key in targets:
                after['runtimes'][key]['toolchain']['qualificationRevision'] = 2
            with self.subTest(targets=targets):
                self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                                     profile_after=after), [], targets)

    def test_abi_policy_change_still_scopes_to_its_profile_entry(self):
        before = profile_document()
        after = copy.deepcopy(before)
        after['runtimes']['windows-x64-msvc-static']['abi']['crt'] = 'dynamic-release'
        # Scope selection is not acceptance of a new ABI by the producer.
        self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                             profile_after=after), [], ['windows-x64-msvc-static'])

    def test_host_profile_changes_and_remapping_are_not_filename_heuristics(self):
        before = profile_document()
        after = copy.deepcopy(before)
        after['hostTools']['macos-universal']['runner'] = 'macos-new'
        self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                             profile_after=after), ['macos-universal'], [])
        before['hostTools']['linux-x86_64']['upstreamKey'] = 'custom-asset'
        lock_before = lock_document()
        assets = lock_before['dependencies']['protobuf']['protoc_assets']
        assets['custom-asset'] = assets.pop('linux-x86_64')
        lock_after = copy.deepcopy(lock_before)
        lock_after['dependencies']['protobuf']['protoc_assets']['custom-asset']['sha256'] = 'a' * 64
        self.assertSelection(module.classify(['deps.lock.json'], lock_before=lock_before,
                                             lock_after=lock_after, profile_before=before,
                                             profile_after=before), ['linux-x86_64'], [])

    def test_global_and_effective_per_target_contract_versions(self):
        before = profile_document()
        after = copy.deepcopy(before)
        after['hostContractVersion'] += 1
        self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                             profile_after=after), HOSTS, [])
        after = copy.deepcopy(before)
        after['runtimeContractVersions']['windows-x64-msvc-static'] += 1
        self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                             profile_after=after), [], ['windows-x64-msvc-static'])
        # Overrides retain their effective identity when the default catches up.
        after = copy.deepcopy(before)
        after['runtimeContractVersion'] = 2
        self.assertSelection(module.classify([PROFILE_PATH], profile_before=before, profile_after=after),
                             [], [key for key in RUNTIMES if key != 'windows-x64-msvc-static'])
        after = copy.deepcopy(before)
        after['runtimeContractVersions']['linux-x86_64'] = before['runtimeContractVersion']
        self.assertSelection(module.classify([PROFILE_PATH], profile_before=before, profile_after=after), [], [])

    def test_all_input_scopes_are_unioned_without_short_circuit(self):
        before, after = lock_document(), lock_document()
        after['dependencies']['abseil']['source_sha256'] = 'a' * 64
        self.assertSelection(module.classify(['deps.lock.json', 'tools/semantic/package_host.py'],
                                             lock_before=before, lock_after=after), HOSTS, RUNTIMES)
        pb, pa = profile_document(), profile_document()
        pa['runtimes']['ios-arm64']['toolchain']['deploymentTarget'] = '18.0'
        self.assertSelection(module.classify(['tools/semantic/package_host.py', PROFILE_PATH],
                                             profile_before=pb, profile_after=pa), HOSTS, ['ios-arm64'])
        after = copy.deepcopy(before)
        after['dependencies']['protobuf']['protoc_assets']['windows-x64']['sha256'] = 'a' * 64
        self.assertSelection(module.classify(['deps.lock.json', PROFILE_PATH], lock_before=before,
                                             lock_after=after, profile_before=pb, profile_after=pa),
                             ['windows-x64'], ['ios-arm64'])

    def test_consumer_store_business_and_classifier_paths_select_nothing(self):
        paths = ['tools/semantic/provider.py', 'tools/semantic_fetch.py', 'tools/setup_build_environment.py',
                 'tools/update_semantic_lock.py', 'tools/sdk/store.py', 'tools/sdk/resolver.py',
                 'tools/sdk/transport.py', 'tools/sdk/model.py', 'tools/sdk/new_family.py',
                 'tools/semantic/reuse_release.py', 'tools/semantic/publish_release.py',
                 'tools/semantic/aggregate.py', 'semantic-sdk.lock.json', 'semantic-toolchain.lock.json',
                 'verification/tests/test_semantic_lock.py', 'runtime/semantic/CMakeLists.txt',
                 'runtime/semantic/src/Document.cpp', 'schema/axiom/v1/proto/operation.proto',
                 'tools/semantic/classify_producer_changes.py',
                 'tools/semantic/tests/test_producer_change_classifier.py',
                 '.github/workflows/semantic-sdk-producer-contract.yml']
        for path in paths:
            with self.subTest(path=path):
                self.assertSelection(module.classify([path]), [], [])

    def test_skia_only_and_unrelated_dependency_changes_do_not_start_semantic(self):
        before, after = lock_document(), lock_document()
        after['dependencies']['skia']['commit'] = 'changed'
        after['skia_builds']['windows_d3d12']['is_official_build'] = False
        after['dependencies']['future_sdk'] = {'version': '1'}
        self.assertSelection(module.classify(['deps.lock.json'], lock_before=before, lock_after=after), [], [])
        self.assertSelection(module.classify(['tools/skia/build.py', 'tools/skia/profiles/r1-full-v1.json',
                                             'r1-full-skia-sdk.lock.json']), [], [])
        for dependency in ['protobuf', 'abseil']:
            after = copy.deepcopy(before)
            after['dependencies'][dependency]['version'] = 'changed'
            self.assertEqual(classify_skia(['deps.lock.json'], lock_before=before, lock_after=after)['mode'], 'none')
        self.assertEqual(classify_skia(['tools/semantic/package_host.py', PROFILE_PATH])['mode'], 'none')

    def test_empty_unchanged_and_duplicate_inputs_are_stable(self):
        self.assertSelection(module.classify([]), [], [])
        self.assertSelection(module.classify(['deps.lock.json', PROFILE_PATH], lock_before=lock_document(),
                                             lock_after=lock_document(), profile_before=profile_document(),
                                             profile_after=profile_document()), [], [])
        paths = ['tools/semantic/package_host.py', 'tools/semantic/windows_compile.py']
        a = module.classify(paths)
        b = module.classify(['./' + paths[1], paths[0], paths[1]])
        self.assertEqual(a, b)
        self.assertSelection(a, HOSTS, ['windows-x64-msvc-static'])

    def test_unavailable_or_malformed_inputs_fall_back_to_full(self):
        for paths in [['deps.lock.json'], [PROFILE_PATH], ['../tools/semantic/package_host.py'], [''], [None]]:
            with self.subTest(paths=paths):
                self.assertSelection(module.classify(paths), HOSTS, RUNTIMES)
        for before in [None, [], {}, {'dependencies': []}, {'dependencies': {'protobuf': None}}]:
            with self.subTest(before=before):
                self.assertSelection(module.classify(['deps.lock.json'], lock_before=before,
                                                     lock_after=lock_document()), HOSTS, RUNTIMES)
        for broken in ['missing-runtime', 'bad-versions', 'bad-host', 'unknown-field']:
            before, after = profile_document(), profile_document()
            if broken == 'missing-runtime':
                del after['runtimes']['ios-arm64']
            elif broken == 'bad-versions':
                after['runtimeContractVersions'] = []
            elif broken == 'bad-host':
                after['hostTools']['windows-x64'] = None
            else:
                after['unrecognizedAuthority'] = True
            with self.subTest(broken=broken):
                self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                                     profile_after=after), HOSTS, RUNTIMES)
        after = lock_document()
        del after['dependencies']['protobuf']['protoc_assets']['windows-x64']
        self.assertSelection(module.classify(['deps.lock.json'], lock_before=lock_document(),
                                             lock_after=after), HOSTS, RUNTIMES)

    def test_incomplete_nested_abi_and_toolchain_are_malformed_diffs(self):
        for key, section, field in [('windows-x64-msvc-static', 'abi', 'crt'),
                                    ('linux-x86_64', 'abi', 'cxxStandard'),
                                    ('android-x86_64', 'toolchain', 'apiLevel'),
                                    ('ios-arm64', 'toolchain', 'sdk')]:
            before, after = profile_document(), profile_document()
            del after['runtimes'][key][section][field]
            with self.subTest(key=key, section=section, field=field):
                self.assertSelection(module.classify([PROFILE_PATH], profile_before=before,
                                                     profile_after=after), HOSTS, RUNTIMES)

    def test_non_posix_paths_do_not_silently_skip_recipe_detection(self):
        for path in [r'tools\semantic\package_host.py', 'C:/tools/semantic/package_host.py']:
            with self.subTest(path=path):
                self.assertSelection(module.classify([path]), HOSTS, RUNTIMES)

    def test_malformed_profile_is_not_masked_by_a_known_producer_path(self):
        self.assertSelection(module.classify(['tools/semantic/package_host.py', PROFILE_PATH]), HOSTS, RUNTIMES)

    def test_workflow_does_not_list_consumer_only_inputs(self):
        workflow = (ROOT / '.github/workflows/semantic-toolchain-producer.yml').read_text(encoding='utf-8')
        for consumer in ['tools/semantic_fetch.py', 'tools/update_semantic_lock.py',
                         'semantic-toolchain.lock.json', 'verification/tests/test_semantic_lock.py']:
            self.assertNotIn(f'      - "{consumer}"', workflow)


class ProducerMatrixTest(SelectionAssertions):
    def test_matrices_keep_overlapping_host_and_runtime_keys_separate(self):
        matrices = module.build_matrices(['linux-x86_64', 'windows-x64', 'macos-universal'], RUNTIMES)
        self.assertEqual(set(matrices), {'host_tools', 'runtimes'})
        expected = [
            ('linux-x86_64', 'ubuntu-24.04', 'linux'), ('windows-x64-msvc-static', 'windows-2025', 'windows'),
            ('macos-arm64', 'macos-15', 'apple'), ('macos-x64', 'macos-15', 'apple'),
            ('ios-arm64', 'macos-15', 'apple'), ('ios-simulator-arm64', 'macos-15', 'apple'),
            ('android-arm64-v8a', 'ubuntu-24.04', 'android'), ('android-x86_64', 'ubuntu-24.04', 'android'),
            ('web-wasm32', 'ubuntu-24.04', 'web')]
        self.assertEqual(matrices['runtimes'], {'include': [
            {'key': key, 'kind': 'runtimes', 'os': runner, 'family': family} for key, runner, family in expected]})
        self.assertEqual(matrices['host_tools'], {'include': [
            {'key': key, 'kind': 'host-tools', 'os': runner, 'family': family}
            for key, runner, family in [('linux-x86_64', 'ubuntu-24.04', 'linux'),
                                        ('windows-x64', 'windows-2025', 'windows'),
                                        ('macos-universal', 'macos-15', 'apple')]]})

    def test_empty_and_partial_matrices_do_not_add_smoke_host_builds(self):
        self.assertEqual(module.build_matrices([], []), {'host_tools': {'include': []}, 'runtimes': {'include': []}})
        matrices = module.build_matrices([], ['web-wasm32', 'android-x86_64', 'web-wasm32'])
        self.assertEqual(matrices['host_tools'], {'include': []})
        self.assertEqual([cell['key'] for cell in matrices['runtimes']['include']], ['android-x86_64', 'web-wasm32'])

    def test_runner_comes_from_selected_profile(self):
        profile = profile_document()
        profile['runtimes']['macos-x64']['runner'] = 'macos-new'
        self.assertEqual(module.build_matrices([], ['macos-x64'], profile=profile)['runtimes']['include'][0]['os'], 'macos-new')

    def test_unknown_keys_and_invalid_selection_types_are_rejected(self):
        for hosts, targets in [(['web-wasm32'], []), ([], ['windows-x64']), ('linux-x86_64', []), ([], [None])]:
            with self.subTest(hosts=hosts, targets=targets):
                with self.assertRaises(ValueError):
                    module.build_matrices(hosts, targets)


class ProducerClassifierGitCliTest(SelectionAssertions):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='axiom classifier ')
        self.addCleanup(self.directory.cleanup)
        self.repo = Path(self.directory.name)
        self.env = {key: value for key, value in os.environ.items() if not key.startswith('GIT_')}
        self.env.update(GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull)
        self.git('init', '-q')
        self.git('config', 'user.email', 'classifier-test@example.invalid')
        self.git('config', 'user.name', 'Classifier Test')
        self.git('config', 'core.autocrlf', 'false')
        self.write('deps.lock.json', lock_document())
        self.write(PROFILE_PATH, profile_document())
        self.write('tools/semantic/package_host.py', '# fixture\n')
        self.base = self.commit()

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.repo, env=self.env, text=True).strip()

    def write(self, path, content):
        destination = self.repo / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(content) if isinstance(content, dict) else content, encoding='utf-8')

    def commit(self):
        self.git('add', '.')
        self.git('-c', 'commit.gpgsign=false', 'commit', '-qm', 'fixture')
        return self.git('rev-parse', 'HEAD')

    def cli(self, *args, input_text=''):
        return subprocess.run([sys.executable, str(SCRIPT), *args], cwd=self.repo, env=self.env,
                              input=input_text, text=True, capture_output=True, timeout=30)

    def classify_refs(self, head):
        result = self.cli('--base-ref', self.base, '--head-ref', head)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_ref_comparison_reads_decoded_lock_and_profile_at_exact_revisions(self):
        lock = lock_document()
        lock['dependencies']['protobuf']['protoc_assets']['windows-x64']['sha256'] = 'a' * 64
        self.write('deps.lock.json', lock)
        profile = profile_document()
        profile['runtimes']['ios-arm64']['toolchain']['deploymentTarget'] = '18.0'
        self.write(PROFILE_PATH, profile)
        head = self.commit()
        # Dirty working files must not override committed authority.
        self.write('deps.lock.json', 'broken working lock')
        self.write(PROFILE_PATH, 'broken working profile')
        self.assertSelection(self.classify_refs(head), ['windows-x64'], ['ios-arm64'])

    def test_profile_mapping_is_read_even_when_only_lock_changes(self):
        profile, lock = profile_document(), lock_document()
        profile['hostTools']['linux-x86_64']['upstreamKey'] = 'custom-asset'
        assets = lock['dependencies']['protobuf']['protoc_assets']
        assets['custom-asset'] = assets.pop('linux-x86_64')
        self.write(PROFILE_PATH, profile)
        self.write('deps.lock.json', lock)
        self.base = self.commit()
        assets['custom-asset']['sha256'] = 'a' * 64
        self.write('deps.lock.json', lock)
        self.assertSelection(self.classify_refs(self.commit()), ['linux-x86_64'], [])

    def test_formatting_only_lock_and_profile_changes_select_nothing(self):
        self.write('deps.lock.json', json.dumps(lock_document(), indent=4, sort_keys=True))
        self.write(PROFILE_PATH, json.dumps(profile_document(), indent=4, sort_keys=True))
        self.assertSelection(self.classify_refs(self.commit()), [], [])

    def test_renaming_a_recipe_keeps_deleted_source_in_scope(self):
        self.git('mv', 'tools/semantic/package_host.py', 'renamed.py')
        self.assertSelection(self.classify_refs(self.commit()), HOSTS, [])

    def test_invalid_refs_return_conservative_json_not_an_uncaught_error(self):
        self.assertSelection(self.classify_refs('missing-reference'), HOSTS, RUNTIMES)

    def test_deleted_or_malformed_authority_returns_full(self):
        for content in ['[]', '{invalid', '{"dependencies": {}, "dependencies": {}}',
                        '{"dependencies": {}, "n": NaN}']:
            with self.subTest(content=content):
                self.write('deps.lock.json', content)
                self.assertSelection(self.classify_refs(self.commit()), HOSTS, RUNTIMES)
        (self.repo / PROFILE_PATH).unlink()
        self.assertSelection(self.classify_refs(self.commit()), HOSTS, RUNTIMES)

    def test_positional_and_stdin_interfaces_match(self):
        paths = ['tools/semantic/package_host.py', 'tools/semantic/windows_compile.py']
        positional = self.cli(*paths)
        stdin = self.cli(input_text='\n'.join(paths) + '\n')
        self.assertEqual(positional.returncode, 0, positional.stderr)
        self.assertEqual(stdin.returncode, 0, stdin.stderr)
        self.assertEqual(positional.stdout, stdin.stdout)
        self.assertSelection(json.loads(stdin.stdout), HOSTS, ['windows-x64-msvc-static'])

    def test_matrix_cli_emits_separate_matrices(self):
        selection = json.dumps({'host_tools': ['windows-x64'], 'runtimes': ['android-x86_64']})
        result = self.cli('--matrix', selection)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), module.build_matrices(['windows-x64'], ['android-x86_64']))

    def test_invalid_or_conflicting_cli_inputs_fail_without_matrix_output(self):
        for args in [('--base-ref', self.base), ('--matrix', '[]'), ('--matrix', '{bad'),
                     ('--matrix', '{"host_tools":[],"runtimes":["unknown"]}'),
                     ('--matrix', '{"host_tools":[],"runtimes":[]}', 'tools/semantic/package_host.py'),
                     ('--base-ref', self.base, '--head-ref', self.base, 'ignored.py')]:
            with self.subTest(args=args):
                result = self.cli(*args)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(result.stdout)


if __name__ == '__main__':
    unittest.main()
