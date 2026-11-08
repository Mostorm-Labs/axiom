import copy,hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class RepairRunnerTest(unittest.TestCase):
 def mixed_v2(self):
  s=self.summary();p=s['canonical_chain'];initial=copy.deepcopy(p['chains'][0]);initial.update(work_id='2',draw_job='2',draw_basis='2',attempt_jobs=['2'],draw_seq='1',provider_seq='2',capture_seq='3',completion_seq='4')
  initial['basis'].update(committed_generation='0',scene_generation='0',scene_revision='0',commit_revision='0',operation_id=None)
  initial['operation_reason']='NOT_APPLICABLE_INITIALIZATION'
  initial['initialization_basis']={'status':'PROVEN_FRESH_EMPTY_INITIALIZATION','public_state':{'documentId':'0'*32,'generation':'0','objects':0,'lastCommit':{'ordinal':'0','operationId':'0'*32},'target':{'runtimeEpoch':'1','attachmentEpoch':'1','viewId':'1'}},
   'source_profile':{'verification/runners/g5/ur0/consumer.mjs':'4512faa07223db9a9a761eeccdc2210e8fef65becf65a9b3e46135553117aee7','verification/runners/g5/ur0/worker.mjs':'a483fcc8763eb8ed8a47005e81d12316ad4e9fa04fa08165eec42b8a100425f1'},
   'recording_source_tree_sha256':'d'*64,'input_refs':{'raw_trace_sha256':'a'*64,'worker_result_sha256':'b'*64,'run_inputs_sha256':'c'*64}}
  p.update(schema='g5-ur0b-canonical-chain-proof-v2',accepted_raw_count=2,qualified_initialization_count=1,qualified_committed_count=1,unresolved_accepted_count=0,initialization_chains=[initial],accepted_records=[{'completion_seq':'4','work_id':'2','classification':'INITIALIZATION'},{'completion_seq':'10','work_id':'1','classification':'COMMITTED'}]);return s
 def summary(self):
  basis={k:'1' for k in ['runtime_instance','lifetime_generation','attachment_epoch','document_epoch','resource_generation','view_id','committed_generation','scene_generation','scene_revision','camera_generation','surface_generation','metrics_generation','frame_id','commit_revision']}
  basis.update(document_id='0'*32,operation_id='1'*32)
  return {'critical_loss':0,'identity_errors':[],'canonical_chain':{'schema':'g5-ur0b-canonical-chain-proof-v1','scope':'NATIVE_AND_METRICS','status':'QUALIFIED','accepted_raw_count':1,'qualified_decoded_count':1,'metric_receipt_count':1,'unresolved':[],
   'chains':[{'work_id':'1','basis':basis,'draw_job':'43','draw_basis':'43','attempt_jobs':['20','43'],'draw_seq':'7','provider_seq':'8','capture_seq':'9','completion_seq':'10'}]}}
 def module(self):
  path=ROOT/'verification/runners/g5/ur0/repair_runner.py';self.assertTrue(path.is_file(),'P36 bounded continuation runner missing')
  spec=importlib.util.spec_from_file_location('repair_runner',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
 def test_original_and_new_worker_envelopes_share_one_unchanged_cap(self):
  m=self.module();plan=m.plan();self.assertEqual(sum(r['limit_seconds'] for r in plan),7740)
  self.assertEqual(sum(r['phase']=='diagnostic' for r in plan),2);self.assertEqual(sum(r['phase']=='qualification' for r in plan),12)
  self.assertEqual(len({r['run_id'] for r in plan}),len(plan));self.assertLessEqual(sum(r['limit_seconds'] for r in plan),8370)
 def test_new_qualification_order_and_no_replacement_of_a_failed_pair(self):
  m=self.module();rows=m.plan();q=[r for r in rows if r['phase']=='qualification']
  self.assertEqual([r['toggle'] for r in q],['OFF','ON','ON','OFF','ON','OFF','OFF','ON','OFF','ON','ON','OFF'])
  with tempfile.TemporaryDirectory() as tmp:
   path=Path(tmp)/'ledger.json';m.claim(path,rows,'diagnostic');saved=json.loads(path.read_text());saved[3]['status']='RSS_LIMIT';path.write_text(json.dumps(saved))
   with self.assertRaisesRegex(RuntimeError,'preserve|claimed|retry'):m.claim(path,rows,'diagnostic')
 def test_hard_guard_stop_cannot_be_interpreted_as_product_red(self):
  m=self.module();self.assertTrue(m.hard_stop({'reason':'rss_limit','cleanup_complete':True,'returncode':0}))
  self.assertTrue(m.hard_stop({'reason':'completed','cleanup_complete':True,'returncode':124}))
  self.assertFalse(m.hard_stop({'reason':'completed','cleanup_complete':True,'returncode':0}))
 def test_schema_and_zero_loss_cannot_admit_a_missing_canonical_chain(self):
  m=self.module();self.assertTrue(callable(getattr(m,'require_canonical_chain',None)),'positive canonical admission gate missing')
  with self.assertRaisesRegex(RuntimeError,'canonical'):m.require_canonical_chain({'critical_loss':0,'identity_errors':[],'schema_status':'VALID'},require_positive=True)
  m.require_canonical_chain(self.summary(),require_positive=True)
  for change in [lambda s:s['canonical_chain'].update(chains=[]),lambda s:s['canonical_chain'].update(qualified_decoded_count=0),lambda s:s['canonical_chain'].update(unresolved=[{'reason':'missing completion'}]),lambda s:s.update(critical_loss=1)]:
   summary=self.summary();change(summary)
   with self.assertRaises(RuntimeError):m.require_canonical_chain(summary,require_positive=True)
 def test_no_observed_completion_is_distinct_from_a_positive_chain(self):
  m=self.module();self.assertTrue(callable(getattr(m,'require_canonical_chain',None)),'positive canonical admission gate missing')
  s=self.summary();s['canonical_chain'].update(status='NOT_OBSERVED',accepted_raw_count=0,qualified_decoded_count=0,metric_receipt_count=0,chains=[])
  with self.assertRaisesRegex(RuntimeError,'positive|canonical'):m.require_canonical_chain(s,require_positive=True)
  m.require_canonical_chain(s,require_positive=False)
 def test_native_only_proof_cannot_claim_metrics_or_an_unknown_scope(self):
  m=self.module();s=self.summary();s['canonical_chain'].update(scope='UNKNOWN')
  with self.assertRaisesRegex(RuntimeError,'canonical'):m.require_canonical_chain(s,require_metrics=False)
  s['canonical_chain'].update(scope='NATIVE_ONLY',metric_receipt_count=None)
  m.require_canonical_chain(s,require_metrics=False)
  with self.assertRaises(RuntimeError):m.require_canonical_chain(s,require_metrics=True)
 def test_old_qualified_label_without_a_bound_positive_diagnostic_never_claims_a_cohort(self):
  m=self.module();self.assertTrue(callable(getattr(m,'require_qualified_diagnostic',None)),'canonical prerequisite check missing')
  with tempfile.TemporaryDirectory() as tmp:
   out=Path(tmp);(out/'diagnostic-result.json').write_text('{"status":"QUALIFIED"}')
   with self.assertRaisesRegex(RuntimeError,'canonical|diagnostic'):m.require_qualified_diagnostic(out,{'qualified_sdk_artifacts':{}})
   self.assertFalse((out/'worker-ledger.json').exists(),'prerequisite failure claimed a new cohort')
 def test_frozen_sampling_stop_applies_even_to_a_fresh_output_directory(self):
  m=self.module();self.assertTrue(callable(getattr(m,'assert_sampling_authority_open',None)),'consumed authorization guard missing')
  with tempfile.TemporaryDirectory() as tmp:
   previous=Path(tmp)/'result.json';previous.write_text(json.dumps({'status':'BLOCKED_EVIDENCE','continue_execution':False,'authorization_ref':{'sha256':'abc'}}))
   with self.assertRaisesRegex(RuntimeError,'control review|closed|exhausted'):m.assert_sampling_authority_open(previous,'abc')
 def test_compilation_binding_rejects_changed_raw_proof_compiler_and_offline_promotion(self):
  m=self.module();self.assertTrue(callable(getattr(m,'require_bound_compilation',None)),'compiled provenance binding missing')
  with tempfile.TemporaryDirectory() as tmp:
   directory=Path(tmp);summary=self.summary()
   for name in ['raw-trace.jsonl','worker-result.json','run-inputs.json']:(directory/name).write_text('{}\n')
   (directory/'canonical-chain.json').write_text(json.dumps(summary['canonical_chain']))
   digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
   summary['canonical_chain_ref']={'path':'canonical-chain.json','sha256':digest(directory/'canonical-chain.json')}
   summary['analysis']={'mode':'WORKER_COMPILE','input_source_tree_sha256':'tree','raw_trace_sha256':digest(directory/'raw-trace.jsonl'),'worker_result_sha256':digest(directory/'worker-result.json'),'run_inputs_sha256':digest(directory/'run-inputs.json'),
    'compiler_files':{name:digest(m.HERE/name) for name in ['compile_run.mjs','decode_trace.mjs','metrics_compiler.mjs','canonical_chain.mjs','external_metrics.mjs','schedule.mjs','validate_metrics.py']}}
   m.require_bound_compilation(directory,summary,{'source_tree_sha256':'tree'})
   for change in [lambda s:s['analysis'].update(mode='OFFLINE_REVERIFICATION'),lambda s:s['analysis'].update(raw_trace_sha256='0'*64),lambda s:s['analysis']['compiler_files'].update({'decode_trace.mjs':'0'*64}),lambda s:s['canonical_chain_ref'].update(sha256='0'*64)]:
    bad=copy.deepcopy(summary);change(bad)
    with self.assertRaises(RuntimeError):m.require_bound_compilation(directory,bad,{'source_tree_sha256':'tree'})
   (directory/'raw-trace.jsonl').write_text('changed\n')
   with self.assertRaisesRegex(RuntimeError,'input binding'):m.require_bound_compilation(directory,summary,{'source_tree_sha256':'tree'})
 def test_v2_complete_partition_keeps_initialization_out_of_positive_mutation_gate(self):
  m=self.module();s=self.mixed_v2();m.require_canonical_chain(s,require_positive=True)
  p=s['canonical_chain'];p.update(accepted_raw_count=1,qualified_committed_count=0,qualified_decoded_count=0,metric_receipt_count=0,chains=[]);p['accepted_records']=p['accepted_records'][:1]
  m.require_canonical_chain(s,require_positive=False)
  with self.assertRaises(RuntimeError):m.require_canonical_chain(s,require_positive=True)
  p.update(scope='NATIVE_ONLY',metric_receipt_count=None);m.require_canonical_chain(s,require_positive=False,require_metrics=False)
 def test_v2_reader_rejects_population_membership_and_initial_state_exemptions(self):
  m=self.module()
  changes=[lambda p:p.update(accepted_raw_count=1),lambda p:p.update(qualified_decoded_count=2),lambda p:p.update(metric_receipt_count=2),lambda p:p.update(unresolved_accepted_count=1),lambda p:p.update(accepted_records=p['accepted_records'][1:]),lambda p:p['accepted_records'][0].update(classification='COMMITTED'),lambda p:p['initialization_chains'][0]['basis'].pop('operation_id'),lambda p:p['initialization_chains'][0]['initialization_basis']['public_state']['target'].update(runtimeEpoch='0'),lambda p:p['initialization_chains'][0]['initialization_basis'].pop('input_refs'),lambda p:p['initialization_chains'][0]['initialization_basis']['source_profile'].update({'verification/runners/g5/ur0/worker.mjs':'0'*64})]
  for change in changes:
   s=self.mixed_v2();change(s['canonical_chain'])
   with self.subTest(change=change),self.assertRaises(RuntimeError):m.require_canonical_chain(s)
  legacy=self.summary();legacy['canonical_chain']['schema']='g5-ur0b-canonical-chain-proof-v2'
  with self.assertRaises(RuntimeError):m.require_canonical_chain(legacy)
if __name__=='__main__':unittest.main()
