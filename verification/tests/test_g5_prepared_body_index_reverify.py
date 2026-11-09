import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
path=Path(__file__).resolve().parents[1]/'runners/g5/prepared_body_index_reverify_runner.py'
spec=importlib.util.spec_from_file_location('reverify',path);runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
class Reverification(unittest.TestCase):
    def test_new_window_keeps_closed_history_and_refuses_retry(self):
        ledger=runner.new_ledger();self.assertEqual((ledger['historical_starts'],ledger['historical_charged_seconds']),(48,1770))
        self.assertEqual(runner.IDS,('UR1C1-R1-Q1','UR1C1-R1-N1'))
        with self.assertRaises(ValueError):runner.reserve(ledger,runner.IDS[1])
        for identity in runner.IDS:
            runner.reserve(ledger,identity);runner.complete(ledger,{'elapsed_s':1.2,'reason':'completed','cleanup_complete':True},{'status':'OBSERVED'})
        self.assertEqual(ledger['remaining_status'],'NONE');self.assertEqual([x['charged_seconds'] for x in ledger['attempts']],[2,2])
        with self.assertRaises(ValueError):runner.reserve(ledger,runner.IDS[0])
    def test_hard_failure_closes_second_window(self):
        ledger=runner.new_ledger();runner.reserve(ledger,runner.IDS[0]);runner.complete(ledger,{'elapsed_s':2,'reason':'completed','cleanup_complete':True},{'status':'HARD_FAILURE'})
        self.assertEqual(ledger['remaining_status'],'MISSING')
    def test_only_new_self_outputs_are_added_to_source_exclusions(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);subprocess.run(['git','init','-q',str(root)],check=True)
            old=root/'.aegis/results/G5-UR1c1-prepared-body-index/old';new=root/'.aegis/results'/runner.TASK/'new';package=root/'.aegis/packages'/runner.TASK/'package.json'
            for p in [old,new,package]:p.parent.mkdir(parents=True,exist_ok=True);p.write_text('first')
            first=runner.source_identity(root=root);new.write_text('changed');self.assertEqual(first['source_tree_sha256'],runner.source_identity(root=root)['source_tree_sha256'])
            old.write_text('changed');self.assertNotEqual(first['source_tree_sha256'],runner.source_identity(root=root)['source_tree_sha256'])
            files=runner.source_identity(full=True,root=root)['files'];self.assertIn(str(package.relative_to(root)),files);self.assertIn(str(old.relative_to(root)),files);self.assertNotIn(str(new.relative_to(root)),files)
if __name__=='__main__':unittest.main()
