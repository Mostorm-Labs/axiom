import importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
PATH=ROOT/'verification/runners/g5/ur0/run_matrix.py'

class MatrixPlanTest(unittest.TestCase):
    def module(self):
        self.assertTrue(PATH.is_file(),'bounded UR0b matrix runner unavailable')
        spec=importlib.util.spec_from_file_location('ur0_matrix',PATH);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
    def test_each_frozen_row_has_three_independent_planned_outcomes(self):
        m=self.module();matrix=json.loads((ROOT/'docs/planning/g5-unified-render/ur0a/matrix.json').read_text())
        rows=m.plan_rows(matrix);self.assertEqual(len(rows),30);self.assertEqual(len({r['run_id'] for r in rows}),30)
        self.assertTrue(all(sum(r['row_id']==entry['row_id'] for r in rows)==3 for entry in matrix['rows']))
    def test_a_hard_stop_does_not_replace_the_failed_run_or_hide_skips(self):
        m=self.module();matrix=json.loads((ROOT/'docs/planning/g5-unified-render/ur0a/matrix.json').read_text());rows=m.plan_rows(matrix)
        rows[0]['status']='TIMEOUT';m.stop_tier(rows,rows[0]['budget_mib'],'first timeout')
        self.assertEqual(rows[0]['status'],'TIMEOUT');self.assertEqual(sum(r['status']=='MISSING' for r in rows),14)
        self.assertEqual(sum(r['status']=='PLANNED' for r in rows),15)

if __name__=='__main__':unittest.main()
