import importlib.util,json,pathlib,sys,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class RssObserver(unittest.TestCase):
    def test_retains_owned_pid_samples_without_changing_guard_or_killing_others(self):
        path=ROOT/'verification/runners/g5/ur0/observed_worker.py'
        self.assertTrue(path.exists(),'bounded per-process observer unavailable')
        spec=importlib.util.spec_from_file_location('ur0_observed',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
        with tempfile.TemporaryDirectory(prefix='ur0-rss-') as tmp:
            prefix=pathlib.Path(tmp)/'worker'
            command=[sys.executable,'-c','import time; bytearray(1000000); print(\'{"stage":"unit-cold-stage"}\',flush=True); time.sleep(.3)']
            result=m.run(command,prefix,3,128*1024)
            self.assertEqual(result['reason'],'completed');self.assertTrue(result['cleanup_complete'])
            rows=[json.loads(s) for s in prefix.with_suffix('.rss.jsonl').read_text().splitlines()]
            self.assertTrue(any(r['owned_processes'] for r in rows))
            self.assertTrue(any(r['stage']=='unit-cold-stage' for r in rows))
            self.assertTrue(all(r['summed_owned_rss_kib']==sum(p['rss_kib'] for p in r['owned_processes']) for r in rows))
            self.assertEqual(result['rss_limit_kib'],128*1024)
