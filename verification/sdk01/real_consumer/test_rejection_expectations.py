import json
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
class RejectionExpectationTests(unittest.TestCase):
    def test_distinct_scenarios_preserve_real_state(self):
        data=json.loads((HERE/'rejection-expectations-v1.json').read_text(encoding='utf-8'))
        rows=data['cases']
        self.assertEqual(14,len(rows))
        self.assertEqual(14,len({r['id'] for r in rows}))
        self.assertEqual(14,len({r['trigger'] for r in rows}))
        for row in rows:
            with self.subTest(id=row['id']):
                self.assertEqual(['unchanged']*4,[row[k] for k in ('canonical','scene','history','redo')])
                self.assertIn(row['expected_receipt_state'],('stale','rejected','queue_full'))
                self.assertNotEqual('not_applicable',row['expected_failure'])
                self.assertEqual('NOT_IMPLEMENTED',row['driver_status'])

if __name__=='__main__': unittest.main()
