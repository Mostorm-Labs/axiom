"""Static graph guards complement real producer-plan/reuse and hosted probes."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / '.github/workflows'


def job(source, name):
    return re.split(r'^  [a-zA-Z0-9_-]+:', source.split('  ' + name + ':\n', 1)[1],
                    maxsplit=1, flags=re.MULTILINE)[0]


class SemanticSdkWorkflowTest(unittest.TestCase):
    def producer(self):
        return (WORKFLOWS / 'semantic-sdk-producer.yml').read_text(encoding='utf-8')

    def test_reusable_inputs_prepare_and_empty_matrices_are_explicit(self):
        source = self.producer()
        for value in ('workflow_call:', 'host_tools:', 'runtimes:', 'aggregate:', 'accepted_ref:'):
            self.assertIn(value, source)
        prepare = job(source, 'prepare')
        self.assertIn('tools/semantic/producer_plan.py', prepare)
        self.assertIn('persist-credentials: false', prepare)
        self.assertIn('fetch-depth: 0', prepare)
        for name, flag, matrix in [('host-tools', 'has_hosts', 'host_matrix'),
                                  ('runtimes', 'has_runtimes', 'runtime_matrix')]:
            cell = job(source, name)
            self.assertIn("needs.prepare.outputs." + flag + " == 'true'", cell)
            self.assertIn('fromJSON(needs.prepare.outputs.' + matrix + ')', cell)
        runtime = job(source, 'runtimes')
        self.assertIn("needs.host-tools.result == 'skipped'", runtime)
        self.assertIn('!cancelled()', runtime)

    def test_reuse_is_verified_before_any_producer_build(self):
        source = self.producer()
        for name, command in [('host-tools', 'tools/semantic/package_host.py'),
                              ('runtimes', 'tools/semantic/qualify_runtime.py')]:
            cell = job(source, name)
            self.assertLess(cell.index('producer_cell.py identity'), cell.index('producer_cell.py reuse'))
            self.assertLess(cell.index('producer_cell.py reuse'), cell.index(command))
            self.assertIn("if: steps.reuse.outputs.reused != 'true'", cell)
            self.assertIn('producer_cell.py verify', cell)
            self.assertIn('actions/cache@v4', cell)
            self.assertIn('producer-authority', cell)
            self.assertNotIn('continue-on-error', cell)
        host = job(source, 'host-tools')
        self.assertIn('--probe', host)
        self.assertIn('package-a', host)
        self.assertIn('package-b', host)
        self.assertIn('producer_cell.py compare', host)

    def test_runtime_always_probes_reused_packages_and_preserves_windows_rebuild(self):
        runtime = job(self.producer(), 'runtimes')
        self.assertIn('producer_cell.py support-host', runtime)
        self.assertIn('producer_cell.py smoke', runtime)
        self.assertIn("if: steps.reuse.outputs.reused == 'true'", runtime)
        self.assertIn('--verify-clean-rebuild', runtime)
        self.assertIn('tools.semantic.tests.test_runtime_reproducibility', runtime)
        self.assertIn('cmake==3.30.5 ninja==1.11.1.4', runtime)
        self.assertIn('27.2.12479018', runtime)
        self.assertIn('--web', runtime)
        self.assertLess(runtime.index('--verify-clean-rebuild'), runtime.index('actions/upload-artifact'))

    def test_aggregate_is_optional_and_follows_successful_matrices(self):
        source = self.producer()
        aggregate = job(source, 'release-set')
        self.assertIn('needs: [prepare, host-tools, runtimes]', aggregate)
        self.assertIn("needs.prepare.outputs.aggregate == 'true'", aggregate)
        self.assertIn('tools/semantic/aggregate.py', aggregate)
        self.assertIn('tools/update_semantic_lock.py', aggregate)
        self.assertIn('--dry-run', aggregate)
        self.assertIn('${{ inputs.artifact_prefix }}-release-set', aggregate)
        self.assertIn('needs.prepare.result', job(source, 'result'))

    def test_pr_contract_only_calls_selected_reusable_producer(self):
        source = (WORKFLOWS / 'semantic-sdk-producer-contract.yml').read_text()
        classify = job(source, 'classify')
        self.assertIn('fetch-depth: 0', classify)
        self.assertIn('test_producer_change_classifier', classify)
        self.assertIn('--base-ref "$BASE_REF"', classify)
        self.assertIn('--head-ref "$HEAD_REF"', classify)
        self.assertIn('github.event.pull_request.base.sha', classify)
        self.assertIn('github.event.pull_request.head.sha', classify)
        producer = job(source, 'producer')
        self.assertIn("needs.classify.outputs.should_build == 'true'", producer)
        self.assertIn('uses: ./.github/workflows/semantic-sdk-producer.yml', producer)
        self.assertIn('needs.classify.outputs.host_tools', producer)
        self.assertIn('needs.classify.outputs.runtimes', producer)
        self.assertIn("needs.classify.outputs.mode == 'full'", producer)
        self.assertIn('accepted_ref: ${{ github.event.pull_request.base.sha }}', producer)
        self.assertNotIn('qualify_runtime.py --target', source)
        self.assertNotIn('package_host.py --host-key', source)

    def test_hostless_runtime_graph_has_a_scoped_hosted_regression(self):
        source = (WORKFLOWS / 'semantic-sdk-producer-contract.yml').read_text()
        proof = job(source, 'partial-runtime-proof')
        self.assertIn("needs.classify.outputs.verify_partial == 'true'", proof)
        self.assertIn("host_tools: '[]'", proof)
        self.assertIn("runtimes: '[\"linux-x86_64\"]'", proof)
        self.assertIn('aggregate: false', proof)
        self.assertIn('artifact_prefix: semantic-sdk-partial-proof', proof)

    def test_package_discovery_preflight_remains_source_free(self):
        source = (WORKFLOWS / 'semantic-sdk-producer-contract.yml').read_text()
        preflight = job(source, 'runtime-discovery')
        self.assertIn('os: [ubuntu-24.04, windows-2025, macos-15]', preflight)
        self.assertIn('tools.semantic.tests.test_runtime_cmake_discovery', preflight)
        self.assertIn('verification.tests.test_semantic_cmake_contract', preflight)
        self.assertIn('tools.semantic.tests.test_producer_orchestration', preflight)
        for forbidden in ('bootstrap_deps.py', 'build_runtime.py --target', 'qualify_runtime.py --target'):
            self.assertNotIn(forbidden, preflight)

    def test_release_promotion_is_main_only_complete_and_immutable(self):
        source = (WORKFLOWS / 'semantic-sdk-release.yml').read_text(encoding='utf-8')
        self.assertIn('workflow_dispatch:', source)
        self.assertIn('refs/heads/main', job(source, 'guard'))
        self.assertIn('permissions:\n  contents: read', source)
        producer = job(source, 'producer')
        self.assertIn('uses: ./.github/workflows/semantic-sdk-producer.yml', producer)
        self.assertIn('aggregate: true', producer)
        for key in ('linux-x86_64', 'windows-x64', 'macos-universal',
                    'windows-x64-msvc-static', 'macos-arm64', 'macos-x64',
                    'ios-arm64', 'ios-simulator-arm64', 'android-arm64-v8a',
                    'android-x86_64', 'web-wasm32'):
            self.assertIn(key, producer)
        promote = job(source, 'promote')
        self.assertIn('permissions:', promote)
        self.assertIn('contents: write', promote)
        self.assertIn('Verify complete Release Set before publication', promote)
        self.assertIn('len(zip_assets) != 12', promote)
        self.assertIn('semantic-sdk-index.json', promote)
        self.assertIn('tools/semantic/publish_release.py', promote)
        self.assertNotIn('--dry-run', promote)
        self.assertIn('tools/update_semantic_lock.py', promote)
        self.assertIn('automation/semantic-sdk-lock-', promote)
        self.assertIn('releaseSetId', promote)
        self.assertIn('git diff --name-only', promote)
        self.assertNotIn('--force', promote)
        self.assertNotIn('--clobber', promote)

    def test_release_qualifies_exact_candidate_before_opening_lock_only_pr(self):
        source = (WORKFLOWS / 'semantic-sdk-release.yml').read_text(encoding='utf-8')
        validation = job(source, 'candidate-consumer')
        self.assertIn('uses: ./.github/workflows/semantic-sdk-consumer-validation.yml', validation)
        self.assertIn('checkout_ref: ${{ needs.promote.outputs.candidate_ref }}', validation)
        self.assertIn('require_v2_lock: true', validation)
        open_pr = job(source, 'open-lock-pr')
        self.assertIn('needs: [guard, promote, candidate-consumer]', open_pr)
        self.assertIn('permissions:', open_pr)
        self.assertIn('pull-requests: write', open_pr)
        self.assertIn('git diff --name-only', open_pr)
        self.assertIn('semantic-sdk.lock.json', open_pr)
        self.assertIn('gh pr create', open_pr)
        self.assertIn('GITHUB_RUN_ID', open_pr)
        self.assertNotIn('workflow_run', source)
        self.assertNotIn('pull_request_target', source)

    def test_candidate_consumer_can_fail_closed_when_release_lock_is_missing(self):
        source = (WORKFLOWS / 'semantic-sdk-consumer-validation.yml').read_text(encoding='utf-8')
        self.assertIn('require_v2_lock:', source)
        self.assertIn('type: boolean', source)
        self.assertIn('SEMANTIC_V2_LOCK_REQUIRED', job(source, 'preflight'))
        self.assertIn('inputs.require_v2_lock', job(source, 'preflight'))

    def test_pr_and_reusable_producer_cannot_publish_or_accept_authority(self):
        for filename in ('semantic-sdk-producer-contract.yml', 'semantic-sdk-producer.yml'):
            source = (WORKFLOWS / filename).read_text()
            for forbidden in ('contents: write', 'gh release', '--clobber', 'pull_request_target',
                              '--semantic-codec', 'secrets: inherit', 'git push'):
                self.assertNotIn(forbidden, source)
            for line in source.splitlines():
                if 'publish_release.py' in line:
                    command = source[source.index(line):].split('- uses:', 1)[0]
                    self.assertIn('--dry-run', command)


if __name__ == '__main__':
    unittest.main()
