"""Orchestration tests use actual Git commits and verified fixture archives.

Fixtures prove control/identity boundaries, not native SDK execution.
"""
import copy
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import SdkError
from tools.semantic.aggregate import aggregate
from tools.semantic.contract import HOST_KEYS, RUNTIME_KEYS, load_profile, read_json
from tools.semantic.tests.release_fixtures import release_fixture
from tools.update_semantic_lock import make_v2_lock
from tools.semantic import producer_plan, producer_cell


class ProducerPlanTest(unittest.TestCase):
    def test_cell_changes_requalify_both_kinds_but_plan_only_changes_do_not(self):
        from tools.semantic.classify_producer_changes import classify
        self.assertEqual(classify(['tools/semantic/producer_cell.py'])['mode'], 'full')
        self.assertEqual(classify(['tools/semantic/producer_plan.py'])['mode'], 'none')

    def test_empty_plan_has_no_cells_or_implicit_host_tools(self):
        plan = producer_plan.make_plan([], [], False)
        self.assertEqual(plan['host_matrix'], {'include': []})
        self.assertEqual(plan['runtime_matrix'], {'include': []})
        self.assertFalse(plan['has_hosts'])
        self.assertFalse(plan['has_runtimes'])
        self.assertFalse(plan['aggregate'])

    def test_runtime_only_plan_gets_support_host_not_host_build(self):
        plan = producer_plan.make_plan([], ['android-x86_64'], False)
        self.assertFalse(plan['has_hosts'])
        self.assertEqual(plan['runtime_matrix']['include'], [
            {'key': 'android-x86_64', 'kind': 'runtimes', 'os': 'ubuntu-24.04',
             'family': 'android', 'host': 'linux-x86_64'}])

    def test_complete_aggregate_is_explicit_and_has_all_keys(self):
        plan = producer_plan.make_plan(list(HOST_KEYS), list(RUNTIME_KEYS), True)
        self.assertTrue(plan['aggregate'])
        self.assertEqual(len(plan['host_matrix']['include']), 3)
        self.assertEqual(len(plan['runtime_matrix']['include']), 9)
        self.assertFalse(producer_plan.make_plan(list(HOST_KEYS), list(RUNTIME_KEYS), False)['aggregate'])

    def test_partial_aggregate_duplicate_and_bad_keys_fail_before_jobs(self):
        for hosts, runtimes, aggregate_requested in [([], [], True),
                (list(HOST_KEYS), list(RUNTIME_KEYS[:-1]), True),
                (['windows-x64'] * 2, [], False), ([], ['bad'], False),
                ('[]', [], False), ([], [], 'false')]:
            with self.subTest(hosts=hosts, runtimes=runtimes, aggregate=aggregate_requested):
                with self.assertRaises((ValueError, SdkError)):
                    producer_plan.make_plan(hosts, runtimes, aggregate_requested)

    def test_runtime_runner_without_matching_host_is_rejected(self):
        profile = load_profile()
        profile['runtimes']['linux-x86_64']['runner'] = 'unknown-runner'
        with self.assertRaises(SdkError):
            producer_plan.make_plan([], ['linux-x86_64'], False, profile=profile)


class AcceptedAuthorityTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        (self.repo / 'README').write_text('fixture\n')
        self.commit()
        self.before = self.git('rev-parse', 'HEAD').strip()
        assets, _ = release_fixture(self.root / 'fixtures')
        release = aggregate(assets, self.root / 'release')
        self.data = canonical_bytes(make_v2_lock(self.root / 'release/semantic-sdk-index.json', release['tag']))
        (self.repo / 'semantic-sdk.lock.json').write_bytes(self.data)
        self.commit()
        self.accepted = self.git('rev-parse', 'HEAD').strip()

    def git(self, *args):
        return subprocess.run(['git', *args], cwd=self.repo, check=True, capture_output=True,
                              text=True, encoding='utf-8').stdout

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'fixture')

    def test_accepted_ref_ignores_dirty_and_candidate_head_lock(self):
        (self.repo / 'semantic-sdk.lock.json').write_text('{"candidate":true}')
        self.commit()
        (self.repo / 'semantic-sdk.lock.json').write_text('dirty')
        out = self.root / 'authority'
        facts = producer_plan.stage_accepted_lock(self.accepted, out, repo_root=self.repo)
        self.assertEqual((out / 'semantic-sdk.lock.json').read_bytes(), self.data)
        self.assertEqual(facts['revision'], self.accepted)
        self.assertTrue(facts['lockPresent'])
        self.assertEqual(facts['lockSha256'], file_sha256(out / 'semantic-sdk.lock.json'))

    def test_absence_at_valid_base_is_only_clean_transition_miss(self):
        out = self.root / 'authority'
        facts = producer_plan.stage_accepted_lock(self.before, out, repo_root=self.repo)
        self.assertFalse(facts['lockPresent'])
        self.assertFalse((out / 'semantic-sdk.lock.json').exists())
        self.assertEqual(read_json(out / 'authority.json'), facts)

    def test_missing_ref_invalid_lock_and_symlink_do_not_become_absence(self):
        with self.assertRaises((SdkError, subprocess.CalledProcessError)):
            producer_plan.stage_accepted_lock('not-a-ref', self.root / 'missing', repo_root=self.repo)
        (self.repo / 'semantic-sdk.lock.json').write_text('bad JSON')
        self.commit()
        with self.assertRaises(SdkError):
            producer_plan.stage_accepted_lock('HEAD', self.root / 'bad', repo_root=self.repo)
        # git index mode is portable even when the host cannot create symlinks.
        sha = subprocess.run(['git', 'hash-object', '-w', '--stdin'], cwd=self.repo,
                             input=b'elsewhere', capture_output=True, check=True).stdout.decode().strip()
        self.git('update-index', '--add', '--cacheinfo', '120000,' + sha + ',semantic-sdk.lock.json')
        self.git('commit', '-qm', 'symlink fixture')
        with self.assertRaises(SdkError):
            producer_plan.stage_accepted_lock('HEAD', self.root / 'symlink', repo_root=self.repo)


class ProducerReuseTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        assets, self.records = release_fixture(self.root / 'fixture')
        self.mirror = self.root / 'mirror'
        release = aggregate(assets, self.root / 'release')
        self.remote = self.mirror / release['tag']
        shutil.copytree(self.root / 'release', self.remote)
        self.lock = self.root / 'accepted.json'
        self.lock.write_bytes(canonical_bytes(make_v2_lock(self.remote / 'semantic-sdk-index.json', release['tag'])))
        self.options = {'store_root': self.root / 'store', 'mirror': str(self.mirror)}

    def plan(self, kind, key):
        record = self.records[kind, key]
        field = 'hostToolId' if kind == 'hostTools' else 'runtimeId'
        return {'kind': kind, 'key': key, 'identity': record['identity'], 'identifier': record[field]}

    def test_verified_reuse_precedes_any_build_and_remains_offline(self):
        for kind, key in [('hostTools', 'windows-x64'), ('runtimes', 'linux-x86_64')]:
            plan = self.plan(kind, key)
            with mock.patch('subprocess.run', side_effect=AssertionError('unexpected build')):
                result = producer_cell.reuse_planned_package(plan, self.lock, self.root / kind, **self.options)
            self.assertTrue(result['reused'])
            record = producer_cell.verify_planned_package(plan, self.root / kind)
            self.assertEqual(record, self.records[kind, key])
        shutil.rmtree(self.mirror)
        with mock.patch('subprocess.run', side_effect=AssertionError('unexpected build')):
            result = producer_cell.reuse_planned_package(self.plan('runtimes', 'linux-x86_64'), self.lock,
                        self.root / 'second worktree', store_root=self.options['store_root'], offline=True)
        self.assertTrue(result['reused'])

    def test_absent_lock_and_identity_change_are_explicit_misses(self):
        plan = self.plan('hostTools', 'windows-x64')
        self.assertFalse(producer_cell.reuse_planned_package(plan, self.root / 'absent', self.root / 'out')['reused'])
        changed = copy.deepcopy(plan)
        changed['identity']['contractVersion'] += 1
        import hashlib
        changed['identifier'] = hashlib.sha256(canonical_bytes(changed['identity'])).hexdigest()
        self.assertFalse(producer_cell.reuse_planned_package(changed, self.lock, self.root / 'out', **self.options)['reused'])
        self.assertFalse((self.root / 'out').exists())

    def test_corrupted_assets_and_invalid_authority_abort_instead_of_rebuild(self):
        plan = self.plan('runtimes', 'linux-x86_64')
        record = self.records['runtimes', 'linux-x86_64']
        (self.remote / record['asset']).write_bytes(b'corrupt')
        with self.assertRaises(SdkError):
            producer_cell.reuse_planned_package(plan, self.lock, self.root / 'out', **self.options)
        self.assertFalse((self.root / 'out').exists())
        self.lock.write_text('{}')
        with self.assertRaises(SdkError):
            producer_cell.reuse_planned_package(plan, self.lock, self.root / 'out', **self.options)

    def test_record_cannot_reference_an_archive_outside_its_cell_directory(self):
        plan = self.plan('hostTools', 'windows-x64')
        out = self.root / 'out'
        producer_cell.reuse_planned_package(plan, self.lock, out, **self.options)
        record = read_json(out / 'record.json')
        shutil.copyfile(out / record['asset'], self.root / 'outside.zip')
        record['asset'] = '../outside.zip'
        (out / 'record.json').write_bytes(canonical_bytes(record))
        with self.assertRaises(SdkError):
            producer_cell.verify_planned_package(plan, out)

    def test_authority_snapshot_loss_is_not_a_clean_miss(self):
        out = self.root / 'authority'
        out.mkdir()
        facts = {'format': 'axiom-semantic-producer-authority-v2', 'revision': 'a' * 40,
                 'lockPresent': True, 'lockSha256': file_sha256(self.lock)}
        (out / 'authority.json').write_bytes(canonical_bytes(facts))
        with self.assertRaises(SdkError):
            producer_cell.accepted_lock_path(out)
        shutil.copyfile(self.lock, out / 'semantic-sdk.lock.json')
        self.assertEqual(producer_cell.accepted_lock_path(out), out / 'semantic-sdk.lock.json')
        (out / 'semantic-sdk.lock.json').write_text('{}')
        with self.assertRaises(SdkError):
            producer_cell.accepted_lock_path(out)

    def test_package_from_other_identity_and_changed_plan_are_rejected(self):
        plan = self.plan('hostTools', 'windows-x64')
        out = self.root / 'out'
        producer_cell.reuse_planned_package(plan, self.lock, out, **self.options)
        altered = copy.deepcopy(plan)
        altered['identity']['contractVersion'] += 1
        with self.assertRaises(SdkError):
            producer_cell.verify_planned_package(altered, out)
        other = self.plan('hostTools', 'linux-x86_64')
        with self.assertRaises(SdkError):
            producer_cell.verify_planned_package(other, out)


if __name__ == '__main__':
    unittest.main()
