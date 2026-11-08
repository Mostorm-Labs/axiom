import copy,hashlib,importlib.util,json,multiprocessing,subprocess,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[2]
def module():
 p=ROOT/'verification/runners/g5/ur0/remaining_runner.py'
 if not p.is_file():raise AssertionError('r4 remaining runner missing')
 spec=importlib.util.spec_from_file_location('remaining_runner',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def competitor(state,out,queue):
 try:
  m=module()
  with m.Window(Path(state),Path(out),'proof'):pass
 except RuntimeError:queue.put('rejected')
 else:queue.put('accepted')
class RemainingRunnerTest(unittest.TestCase):
 def test_only_38_frozen_unstarted_rows_in_order_and_with_fixed_budgets(self):
  m=module();rows=m.plan();self.assertEqual(len(rows),38);self.assertEqual(sum(r['limit_seconds'] for r in rows),6120)
  self.assertEqual([r['run_id'] for r in rows[:2]],['realtime-120','realtime-240']);self.assertEqual(sum(r['phase']=='main' for r in rows),30)
  for alter in [lambda r:r.append(copy.deepcopy(r[0])),lambda r:r.reverse(),lambda r:r[0].update(run_id='realtime-60'),lambda r:r[8].update(limit_seconds=181),lambda r:r[8]['budget_mib'].update(navigation=128)]:
   bad=copy.deepcopy(rows);alter(bad)
   with self.assertRaises(RuntimeError):m.validate_plan(bad)
 def test_history_cannot_omit_early_or_unknown_guard_start_or_reset_debit(self):
  m=module();history=m.load(m.PACKAGE.parent/'historical-budget.json');self.assertEqual(m.verify_history(history),1133);self.assertEqual(len(history['rows']),30)
  for change in [lambda h:h['rows'].pop(0),lambda h:h.update(historical_charged_seconds=0),lambda h:h['rows'].__setitem__(3,copy.deepcopy(h['rows'][0]))]:
   bad=copy.deepcopy(history);change(bad)
   with self.assertRaises(RuntimeError):m.verify_history(bad)
  self.assertEqual(m.resume.charge(None,90),90)
 def test_explicit_accepted_reuse_keeps_old_modes_and_sources(self):
  m=module();packet=m.PACKAGE.parent/'reuse-inputs.json';original_packet=packet.read_bytes();inputs=m.load(packet);current_here=m.HERE
  # This positive proves the frozen historical acceptance, whose compiler was
  # checkpointed at this exact revision. It must not borrow ambient new hashes.
  revision='df214581c1c80c2d546939271f3db2dc0e77a38b'
  with tempfile.TemporaryDirectory(prefix='ur0b-historical-compiler-') as temporary:
   historical=Path(temporary)
   for name,expected in inputs['compiler_file_sha256'].items():
    data=subprocess.check_output(['git','show',revision+':verification/runners/g5/ur0/'+name],cwd=ROOT)
    self.assertEqual(hashlib.sha256(data).hexdigest(),expected,name)
    (historical/name).write_bytes(data)
   try:
    m.HERE=historical
    result=m.accepted_reuse(inputs)
   finally:m.HERE=current_here
  self.assertIs(m.HERE,current_here);self.assertEqual(packet.read_bytes(),original_packet)
  self.assertEqual(result['status'],'ACCEPTED_RECORDED_REUSE');self.assertEqual(len(result['records']),15);self.assertTrue(result['overhead_qualified'])
  self.assertEqual({r['mode'] for r in result['records']},{'OFFLINE_REVERIFICATION'});self.assertEqual(result['recording_source_tree_sha256'],'dade34ea89c46625291c9aa39a8cbffb56b3df72fa12eac07f4048cb011af2a5')
 def test_actual_current_compiler_rejects_the_unchanged_historical_reuse_packet(self):
  m=module();inputs=m.load(m.PACKAGE.parent/'reuse-inputs.json')
  self.assertEqual(m.HERE,ROOT/'verification/runners/g5/ur0')
  self.assertTrue(any(m.sha(m.HERE/name)!=expected for name,expected in inputs['compiler_file_sha256'].items()),'current compiler must actually differ for this rejection case')
  with self.assertRaisesRegex(RuntimeError,'current compiler differs from accepted reuse compiler'):m.accepted_reuse(inputs)
 def test_changed_or_foreign_review_input_compiler_and_proof_fail_before_claim(self):
  m=module();inputs=m.load(m.PACKAGE.parent/'reuse-inputs.json')
  for change in [lambda r:r.update(status='UNACCEPTED'),lambda r:r['independent_review_ref'].update(sha256='0'*64),lambda r:r['records'][0]['recorded_input_refs'][0].update(sha256='0'*64),lambda r:r['compiler_file_sha256'].update({'decode_trace.mjs':'0'*64}),lambda r:r['records'][0]['accepted_v2_proof_ref'].update(sha256='0'*64)]:
   bad=copy.deepcopy(inputs);change(bad)
   with self.assertRaises(RuntimeError):m.accepted_reuse(bad)
 def test_recorded_proof_cannot_hide_wrong_source_mode_or_artifact(self):
  m=module();inputs=m.load(m.PACKAGE.parent/'reuse-inputs.json');row=inputs['records'][1];summary=m.bound(row['accepted_derived_summary_ref']);chain=m.bound(row['accepted_v2_proof_ref']);artifacts=m.bound(inputs['existing_artifact_provenance_ref'])['tiers']['64-192']['artifacts']
  m.admit_record(row,summary,chain,inputs,artifacts)
  for change in [lambda s:s['analysis'].update(mode='WORKER_COMPILE'),lambda s:s['analysis'].update(input_source_tree_sha256='0'*64),lambda s:s.update(artifact_identity={}),lambda s:s['analysis']['compiler_files'].update({'decode_trace.mjs':'0'*64}),lambda s:s['canonical_chain'].update(metric_receipt_count=0)]:
   bad=copy.deepcopy(summary);change(bad)
   with self.assertRaises(RuntimeError):m.admit_record(row,bad,chain,inputs,artifacts)
 def test_sdk_bytes_and_new_worker_fresh_source_checks_are_independent_of_reuse(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   sdk=Path(tmp);p=sdk/'axiom.wasm';p.write_bytes(b'accepted');expected={'axiom.wasm':hashlib.sha256(p.read_bytes()).hexdigest()};m.verify_sdk(sdk,expected);p.write_bytes(b'changed')
   with self.assertRaises(RuntimeError):m.verify_sdk(sdk,expected)
  inputs=m.load(m.PACKAGE.parent/'reuse-inputs.json');summary=m.bound(inputs['records'][1]['accepted_derived_summary_ref']);outcome={'summary':summary}
  with self.assertRaisesRegex(RuntimeError,'fresh|WORKER'):m.require_fresh(m.plan()[0],outcome,{'source_tree_sha256':'current'})
  summary=copy.deepcopy(summary);summary['analysis']['mode']='WORKER_COMPILE'
  with self.assertRaisesRegex(RuntimeError,'source'):m.require_fresh(m.plan()[0],{'summary':summary},{'source_tree_sha256':'current'})
 def test_single_fixed_window_rejects_concurrency_relocation_and_restart(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state';out=Path(tmp)/'out'
   with m.Window(state,out,'proof'):
    ctx=multiprocessing.get_context('spawn');q=ctx.Queue();p=ctx.Process(target=competitor,args=(str(state),str(out.parent/'other'),q));p.start();p.join(10);self.assertEqual(p.exitcode,0);self.assertEqual(q.get(timeout=1),'rejected')
   for new_out in [out,out.parent/'relocated']:
    with self.assertRaises(RuntimeError):
     with m.Window(state,new_out,'different-proof'):pass
 def test_full_reservation_is_durable_before_spawn_and_unknown_interruption_charges_full(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state'
   with m.Window(state,Path(tmp)/'out','proof') as w:
    w.begin('realtime-120');j=m.load(state/'window.json');self.assertEqual(j['reserved_seconds'],90);self.assertEqual(j['rows'][0]['status'],'RUNNING');self.assertEqual(j['historical_charged_seconds'],1133)
    with self.assertRaises(RuntimeError):w.begin('realtime-240')
   saved=m.load(state/'window.json');self.assertEqual(saved['status'],'CLOSED');self.assertEqual(saved['charged_seconds'],1223);self.assertEqual(saved['rows'][0]['charged_seconds'],90);self.assertEqual(sum(r['status']=='MISSING' for r in saved['rows']),37)
 def test_actual_ceil_missing_guard_and_overbudget_are_not_clipped_or_retried(self):
  m=module()
  for guard,want in [(None,90),({'reason':'completed','elapsed_s':1.01},2),({'reason':'timeout','elapsed_s':90.2},91)]:
   with self.subTest(guard=guard),tempfile.TemporaryDirectory() as tmp:
    with m.Window(Path(tmp)/'state',Path(tmp)/'out','proof') as w:
     w.begin('realtime-120');self.assertEqual(w.finish('realtime-120',{'guard':guard}),want);self.assertEqual(w.data['charged_seconds'],1133+want)
     with self.assertRaises(RuntimeError):w.begin('realtime-120')
     w.data['new_spent_seconds']=6120
     with self.assertRaises(RuntimeError):w.begin('realtime-240')
 def test_main_requires_accepted_reuse_and_all_eight_admitted_separate_rows(self):
  m=module();rows=m.plan();accepted={'status':'ACCEPTED_RECORDED_REUSE','overhead_qualified':True,'diagnostic_qualified':True,'realtime60_qualified':True}
  with self.assertRaises(RuntimeError):m.require_phase('separate',rows,{'status':'QUALIFIED'})
  m.require_phase('separate',rows,accepted)
  for r in rows[:7]:r.update(status='COMPLETED',admission_status='QUALIFIED')
  with self.assertRaises(RuntimeError):m.require_phase('main',rows,accepted)
  rows[7].update(status='COMPLETED',admission_status='QUALIFIED');m.require_phase('main',rows,accepted)
  rows[7]['admission_status']='REJECTED'
  with self.assertRaises(RuntimeError):m.require_phase('main',rows,accepted)
 def test_nonzero_exit_or_failed_cleanup_remains_a_frozen_stop(self):
  m=module();inputs=m.load(m.PACKAGE.parent/'reuse-inputs.json');s=m.bound(inputs['records'][1]['accepted_derived_summary_ref']);s['analysis']['mode']='WORKER_COMPILE'
  for guard in [{'reason':'nonzero_exit','returncode':1,'cleanup_complete':True},{'reason':'completed','returncode':0,'cleanup_complete':False}]:
   with self.subTest(guard=guard),self.assertRaisesRegex(RuntimeError,'guard|cleanup|stop'):
    m.require_fresh(m.plan()[0],{'summary':s,'guard':guard},{'source_tree_sha256':s['analysis']['input_source_tree_sha256']})
 def test_closed_entry_does_not_consume_proof_or_create_relocated_output(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   m.STATE=Path(tmp)/'state';m.STATE.mkdir();p=m.STATE/'window.json';p.write_text('{"status":"CLOSED"}');before=p.read_bytes();out=Path(tmp)/'other'
   with self.assertRaisesRegex(RuntimeError,'claimed|closed'):m.run(SimpleNamespace(output=out))
   self.assertEqual(p.read_bytes(),before);self.assertFalse(out.exists())
if __name__=='__main__':unittest.main()
