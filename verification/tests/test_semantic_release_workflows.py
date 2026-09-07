"""PR release-set qualification is read-only and follows successful producer cells."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SemanticReleaseWorkflowTest(unittest.TestCase):
    def test_aggregation_waits_for_all_cells_and_only_dry_runs_publication(self):
        workflow = (ROOT / '.github/workflows/semantic-sdk-producer.yml').read_text()
        self.assertIn('  release-set:\n', workflow)
        job = workflow.split('  release-set:\n', 1)[1]
        self.assertIn('needs: [prepare, host-tools, runtimes]', job)
        self.assertIn('tools/semantic/aggregate.py', job)
        self.assertIn('tools/update_semantic_lock.py', job)
        self.assertIn('tools/semantic/publish_release.py', job)
        self.assertIn('--dry-run', job)
        self.assertIn('${{ inputs.artifact_prefix }}-release-set', job)
        self.assertNotIn('contents: write', workflow)
        self.assertNotIn('--clobber', workflow)
        self.assertNotIn('gh release create', workflow)

    def test_release_contracts_cover_reuse_publish_and_v1_compatibility(self):
        workflow = (ROOT / '.github/workflows/semantic-release-set-contract.yml').read_text()
        for test in ('test_aggregate_v2', 'test_reuse_release_v2', 'test_publish_release_v2', 'test_semantic_lock'):
            self.assertIn(test, workflow)
        self.assertNotIn('contents: write', workflow)
        self.assertNotIn('bootstrap_deps.py', workflow)
        self.assertNotIn('build_runtime.py --target', workflow)


if __name__ == '__main__':
    unittest.main()
