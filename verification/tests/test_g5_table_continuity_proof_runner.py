import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1]/'runners/g5/table_continuity_proof_runner.py'
spec = importlib.util.spec_from_file_location('table_guard', path)
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

class GuardTests(unittest.TestCase):
    def req(self, name='G5TP1-PAIR'):
        return dict(id=name, cases=[dict(id=n, dpr=2, css=[1431,1010], seed=324508639) for n in m.ROSTERS[name]])
    def ledger(self): return dict(charged_seconds=0, attempts=[])
    def test_exact_roster_and_repeated_id(self):
        ledger=self.ledger(); m.reserve(ledger,self.req())
        self.assertEqual(ledger['attempts'][0]['limit_seconds'],240)
        with self.assertRaises(ValueError): m.reserve(ledger,self.req())
    def test_foreign_case_and_dimensions_and_allowance(self):
        req=self.req();req['cases'][0]['id']='foreign'
        with self.assertRaises(ValueError):m.reserve(self.ledger(),req)
        req=self.req();req['cases'][0]['dpr']=4
        with self.assertRaises(ValueError):m.reserve(self.ledger(),req)
        ledger=self.ledger();ledger['charged_seconds']=181
        with self.assertRaises(ValueError):m.reserve(ledger,self.req())
    def test_localization_cannot_precede_pair(self):
        with self.assertRaises(ValueError):m.reserve(self.ledger(),self.req('G5TP1-LOCALIZE'))

if __name__=='__main__':unittest.main()
