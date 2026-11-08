import copy,hashlib,importlib.util,json,multiprocessing,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[2]
def module():
 p=ROOT/'verification/runners/g5/ur0/reverified_remaining_runner.py'
 if not p.is_file():raise AssertionError('r5 bounded continuation runner missing')
 spec=importlib.util.spec_from_file_location('reverified_remaining_runner',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def competitor(state,out,queue):
 try:
  with module().Window(Path(state),Path(out),'proof'):pass
 except RuntimeError:queue.put('rejected')
 else:queue.put('accepted')
def guard(reason='completed',code=0,elapsed=1.1,cleanup=True):
 return {'reason':reason,'returncode':code,'elapsed_s':elapsed,'cleanup_complete':cleanup,'remaining_owned_pids':[],'rss_limit_kib':3145728,'peak_process_family_rss_kib':100,'timeout_s':90}
class ReverifiedRemainingTest(unittest.TestCase):
 def test_exact36_order_and_limits_do_not_repeat_RT_or_expand_allowance(self):
  m=module();rows=m.plan();self.assertEqual(len(rows),36);self.assertEqual(sum(r['limit_seconds'] for r in rows),5940);self.assertTrue(all(r['mode']=='correctness' for r in rows[:6]));self.assertTrue(all(r['phase']=='main' for r in rows[6:]));self.assertFalse(any(r['run_id'].startswith('realtime-') for r in rows))
  m.validate_plan(rows)
  for change in [lambda r:r.reverse(),lambda r:r.pop(),lambda r:r.append(copy.deepcopy(r[0])),lambda r:r[0].update(limit_seconds=91),lambda r:r[6]['budget_mib'].update(navigation=128)]:
   bad=copy.deepcopy(rows);change(bad)
   with self.assertRaises(RuntimeError):m.validate_plan(bad)
 def test_prior33_and1251_are_bound_to_closed_r4_and_real_guard_charges(self):
  m=module();h=m.verify_history();self.assertEqual(h['starts'],33);self.assertEqual(h['seconds'],1251);self.assertEqual(len(h['rows']),33)
  old=m.bound(m.request()['historical_ledger_ref']);old['charged_seconds']=0
  with self.assertRaises(RuntimeError):m.verify_history(old)
 def test_new_exact_acceptance_keeps_all17_original_sources_and_OFFLINE_modes(self):
  m=module();a=m.acceptance();r=m.accepted_reuse(a)
  self.assertEqual(len(r['records']),17);self.assertEqual(len({x['run_id'] for x in r['records']}),17);self.assertEqual({x['mode'] for x in r['records']},{'OFFLINE_REVERIFICATION'})
  sources={x['run_id']:x['recording_source_tree_sha256'] for x in r['records']}
  self.assertEqual(sources['realtime-120'],'d33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a');self.assertNotEqual(sources['realtime-120'],sources['realtime-60'])
  stale=m.load(ROOT/'.aegis/packages/G5-UR0b-resume-r4/reuse-inputs.json')
  with self.assertRaises(RuntimeError):m.accepted_reuse(stale)
 def test_record_source_input_artifact_compiler_counts_and_proof_are_not_interchangeable(self):
  m=module();a=m.acceptance();row=next(r for r in a['records'] if r['run_id']=='realtime-120');summary=m.bound(row['summary_ref']);chain=m.bound(row['proof_ref']);m.admit_record(row,summary,chain,a)
  for change in [lambda s:s['analysis'].update(mode='WORKER_COMPILE'),lambda s:s['analysis'].update(input_source_tree_sha256=a['records'][0]['recording_source_tree_sha256']),lambda s:s['analysis'].update(raw_trace_sha256='0'*64),lambda s:s['analysis']['compiler_files'].update({'decode_trace.mjs':'f'*64}),lambda s:s.update(artifact_identity={}),lambda s:s['canonical_chain'].update(metric_receipt_count=0),lambda s:s.update(clock='MISSING')]:
   bad=copy.deepcopy(summary);change(bad)
   with self.assertRaises(RuntimeError):m.admit_record(row,bad,chain,a)
  bad=copy.deepcopy(row);bad['recording_source_tree_sha256']=a['records'][0]['recording_source_tree_sha256']
  with self.assertRaises(RuntimeError):m.admit_record(bad,summary,chain,a)
  bad=copy.deepcopy(a);bad['records'].pop()
  with self.assertRaises(RuntimeError):m.accepted_reuse(bad)
 def test_foreign_compiler_and_worker_profile_fail_without_starting_a_browser(self):
  m=module();a=m.acceptance();old=m.HERE
  with tempfile.TemporaryDirectory() as tmp:
   m.HERE=Path(tmp)
   for name in a['compiler_file_sha256']:(m.HERE/name).write_bytes((old/name).read_bytes())
   (m.HERE/'initialization_basis.mjs').write_text('foreign compiler')
   with self.assertRaisesRegex(RuntimeError,'compiler'):m.verify_profiles(a)
  m.HERE=old;bad=copy.deepcopy(a);bad['repaired_worker_and_helpers']['verification/runners/g5/ur0/worker.mjs']='f'*64
  with self.assertRaisesRegex(RuntimeError,'profile'):m.verify_profiles(bad)
 def test_guard_cause_precedes_missing_summary_and_blocks_all_hard_failures(self):
  m=module();row=m.plan()[0]
  for g in [guard('nonzero_exit',124),guard('timeout',-15),guard('rss_limit',-9),guard(cleanup=False),None]:
   with self.assertRaisesRegex(RuntimeError,'guard'):m.require_fresh(row,{'guard':g},{})
 def test_recorded_recompile_and_foreign_source_never_become_a_fresh_worker(self):
  m=module();a=m.acceptance();s=m.bound(a['records'][-1]['summary_ref']);row=m.plan()[0]
  with self.assertRaisesRegex(RuntimeError,'WORKER_COMPILE'):m.require_fresh(row,{'guard':guard(),'summary':s},{'source_tree_sha256':'current'})
  s=copy.deepcopy(s);s['analysis']['mode']='WORKER_COMPILE'
  with self.assertRaisesRegex(RuntimeError,'current source'):m.require_fresh(row,{'guard':guard(),'summary':s},{'source_tree_sha256':'current'})
 def test_complete_pair_rules_preserve_product_RED_and_reject_partial_or_forged_pairs(self):
  m=module();entries=[{'toggle':t,'step':n,'case_id':'lower_z_translucent_drag','comparison':{'status':'QUALIFIED'},'candidate_sha256':'a'*64,'reference_snapshot_sha256':'b'*64} for t in ['OFF','ON'] for n in [0,1]]
  self.assertEqual(m.correctness_verdict(entries,'lower_z_translucent_drag'),'QUALIFIED')
  for e in entries:
   if e['step']==0:e['comparison']['status']='FAILED'
  self.assertEqual(m.correctness_verdict(entries,'lower_z_translucent_drag'),'PRODUCT_RED_IN_BOTH_OFF_ON')
  bad=copy.deepcopy(entries);bad[-2]['candidate_sha256']='c'*64;self.assertEqual(m.correctness_verdict(bad,'lower_z_translucent_drag'),'BLOCKED_EVIDENCE')
  for bad in [entries[:-1],entries+[entries[0]],entries[:2]+[entries[0]]+entries[3:]]:
   with self.assertRaises(RuntimeError):m.correctness_verdict(bad,'lower_z_translucent_drag')
 def test_durable_full_reservation_and_unknown_interruption_charge_before_any_new_start(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state'
   with m.Window(state,Path(tmp)/'out','proof') as w:
    w.begin(m.plan()[0]['run_id']);j=m.load(state/'window.json');self.assertEqual(j['reserved_seconds'],90);self.assertEqual(j['new_starts'],1);self.assertEqual(j['historical_starts'],33);self.assertEqual(j['charged_seconds'],1251)
    with self.assertRaises(RuntimeError):w.begin(m.plan()[1]['run_id'])
   j=m.load(state/'window.json');self.assertEqual(j['new_spent_seconds'],90);self.assertEqual(j['charged_seconds'],1341);self.assertEqual(j['status'],'CLOSED');self.assertEqual(sum(r['status']=='MISSING' for r in j['rows']),35)
 def test_actual_elapsed_overrun_is_charged_honestly_without_clipping(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state'
   with m.Window(state,Path(tmp)/'out','proof') as w:
    key=m.plan()[0]['run_id'];w.begin(key)
    with self.assertRaisesRegex(RuntimeError,'limit|overrun|budget'):w.finish(key,{'guard':guard(elapsed=90.01)})
    self.assertEqual(w.data['new_spent_seconds'],91);self.assertEqual(w.data['charged_seconds'],1342)
   self.assertEqual(m.load(state/'window.json')['rows'][0]['charged_seconds'],91)
 def test_claim_rejects_concurrency_restart_relocation_and_unknown_or_reordered_rows(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state';out=Path(tmp)/'out'
   with m.Window(state,out,'proof') as w:
    ctx=multiprocessing.get_context('spawn');q=ctx.Queue();p=ctx.Process(target=competitor,args=(str(state),str(out.parent/'other'),q));p.start();p.join(10);self.assertEqual(p.exitcode,0);self.assertEqual(q.get(timeout=1),'rejected')
    with self.assertRaises(RuntimeError):w.begin('hidden-smoke')
    with self.assertRaises(RuntimeError):w.begin(m.plan()[1]['run_id'])
   for target in [out,out.parent/'other']:
    with self.assertRaises(RuntimeError):
     with m.Window(state,target,'new-proof'):pass
 def test_main_waits_for_all_six_admitted_correctness_rows_and_exact_reuse(self):
  m=module();rows=m.plan();reused=m.accepted_reuse(m.acceptance())
  with self.assertRaises(RuntimeError):m.require_phase('main',rows,reused)
  for row in rows[:6]:row.update(status='COMPLETED',admission_status='ADMITTED',correctness_status='QUALIFIED')
  rows[0]['correctness_status']='PRODUCT_RED_IN_BOTH_OFF_ON';m.require_phase('main',rows,reused)
  for change in [lambda r:r[5].update(admission_status='REJECTED'),lambda r:r[5].update(status='MISSING'),lambda r:r[5].update(correctness_status='BLOCKED_EVIDENCE')]:
   bad=copy.deepcopy(rows);change(bad)
   with self.assertRaises(RuntimeError):m.require_phase('main',bad,reused)
  with self.assertRaises(RuntimeError):m.require_phase('main',rows,{'status':'unaccepted'})
 def test_closed_entry_cannot_read_new_proof_or_claim_relocated_output(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   m.STATE=Path(tmp)/'state';m.STATE.mkdir();p=m.STATE/'window.json';p.write_text('{"status":"CLOSED"}');before=p.read_bytes();out=Path(tmp)/'elsewhere'
   with self.assertRaisesRegex(RuntimeError,'claimed|closed'):m.run(SimpleNamespace(output=out))
   self.assertEqual(p.read_bytes(),before);self.assertFalse(out.exists())
if __name__=='__main__':unittest.main()
