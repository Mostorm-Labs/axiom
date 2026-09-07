"""Provider qualification consumes a complete candidate, never builds SDKs."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProviderWorkflowTest(unittest.TestCase):
    def test_real_provider_consumption_follows_release_set_on_three_hosts(self):
        workflow = (ROOT / '.github/workflows/semantic-sdk-producer.yml').read_text()
        self.assertIn('  provider-consumer:\n', workflow)
        job = re.split(r'^  [a-zA-Z0-9_-]+:', workflow.split('  provider-consumer:\n', 1)[1],
                       maxsplit=1, flags=re.MULTILINE)[0]
        self.assertIn('needs: release-set', job)
        for item in ('ubuntu-24.04', 'windows-2025', 'macos-15', '${{ inputs.artifact_prefix }}-release-set',
                     'verification/tools/qualify_semantic_provider.py'):
            self.assertIn(item, job)
        for forbidden in ('bootstrap_deps.py', 'build_runtime.py', 'publish_release.py', 'contents: write'):
            self.assertNotIn(forbidden, job)

    def test_entrypoint_contracts_run_in_cheap_three_host_lane(self):
        workflow = (ROOT / '.github/workflows/build-environment-contract.yml').read_text()
        job = workflow.split('  sdk-infrastructure:', 1)[1]
        self.assertIn('tools.semantic.tests.test_provider_v2', job)
        self.assertIn('verification.tests.test_build_environment_v2', job)
        self.assertEqual(workflow.count('      - "tools/semantic/provider.py"'), 2)
        self.assertEqual(workflow.count('      - "semantic-sdk.lock.json"'), 2)


if __name__ == '__main__':
    unittest.main()
