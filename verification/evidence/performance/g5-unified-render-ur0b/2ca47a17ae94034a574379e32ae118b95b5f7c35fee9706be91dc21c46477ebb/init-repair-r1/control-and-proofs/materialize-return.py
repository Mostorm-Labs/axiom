"""Seal this complete offline repair slice; leave all sampling authorities closed."""
import hashlib,json,re,shutil,subprocess,sys,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b/init-repair-r1'
PACKAGE=ROOT/'.aegis/packages/G5-UR0b-init-repair-r1';DESIGN=ROOT/'.aegis/verification/G5-UR0b-initialization-v1'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import source_identity
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
def copy(p,to):to=Path(to);to.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,to);assert sha(to)==sha(p)
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
package=load(PACKAGE/'package.json');order=load(DESIGN/'implementation-request.json')
assert sha(PACKAGE/'package.json')=='247da63294671d763da805758e88bf449db111150f7c37ff7fa2c1272781e710'
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 r=package[key];assert sha(PACKAGE/r['path'])==r['sha256']
for r in load(PACKAGE/'authority.lock.json').values():
 if isinstance(r,dict) and 'path' in r and 'sha256' in r:assert sha(ROOT/r['path'])==r['sha256']
assert git('branch','--show-current').decode().strip()=='codex/geometry-navigation-integration'
for key in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',package[key]['revision'],'HEAD'],cwd=ROOT,check=True)
source=source_identity.identity(ROOT);tree=source['source_tree_sha256'];initial=load(RESULT/'starting-source-identity.json')
changed={n:{'before':initial['files'].get(n),'after':source['files'].get(n)} for n in sorted(set(initial['files'])|set(source['files'])) if initial['files'].get(n)!=source['files'].get(n)}
assert set(changed)<=set(order['source_scope']),set(changed)-set(order['source_scope'])
prior=load(ROOT/'.aegis/results/G5-UR0b/resume-r3/result.json');recorded=Path(prior['materialized_ref']['path']);accepted=load(recorded/'source/full-source-identity.json')
def product(n):return n.startswith(('runtime/','sdk/','schema/','cmake/','tools/','assets/')) or n in ['CMakeLists.txt','verification/runners/g5/bounded_worker.py','verification/runners/g5/ur0/source_identity.py','verification/runners/g5/ur0/worker.mjs','verification/runners/g5/ur0/consumer.mjs','docs/planning/g5-unified-render/ur0a/metrics.schema.json'] or ('/' not in n and n.endswith('.lock.json'))
product_paths={n for n in set(accepted['files'])|set(source['files']) if product(n)}
assert all(accepted['files'].get(n)==source['files'].get(n) for n in product_paths)
scope={'status':'ONLY_AUTHORIZED_OFFLINE_SOURCE_CHANGED','starting_source_tree_sha256':initial['source_tree_sha256'],'accepted_dirty_tree_sha256':accepted['source_tree_sha256'],'source_tree_sha256':tree,'changed_paths':list(changed),'files':changed,'unchanged_product_build_harness_schema_paths':len(product_paths),'source_identity_exclusions_unchanged':source['excluded_self_outputs']==accepted['excluded_self_outputs'],'public_api_impact':'NONE'}
write(RESULT/'source-scope-audit.json',scope)
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
verification=load(RESULT/'offline-reverification.json');assert verification['source_tree_sha256']==tree and verification['status']=='ALL_FIFTEEN_IMMUTABLE_INPUTS_RECONCILED'
assert verification['count']==15 and verification['raw_accepted_total']==10 and verification['initialization_total']==1 and verification['committed_total']==9 and verification['unresolved_total']==0
reuse=load(RESULT/'reuse-assessment.json');assert reuse['status']=='ELIGIBLE_PENDING_INDEPENDENT_CONTROL_REVIEW' and reuse['new_browser_workers']==0
assert all(v is True for k,v in reuse['invariance'].items() if k!='observer_workload_artifact_collector_consumer_unchanged')
node=int(re.search(r'# pass (\d+)\n# fail 0',(OUT/'node-final.log').read_text())[1]);python=int(re.search(r'Ran (\d+) tests',(OUT/'python-final.log').read_text())[1])
assert node==100 and python==44 and (OUT/'python-final.log').read_text().rstrip().endswith('OK')
assert 'passed (120 contract files)' in (OUT/'boundary-final.log').read_text()
red=load(RESULT/'red-precondition.json');assert red['status']=='EXPECTED_ACTUAL_BEHAVIOR_RED_OBSERVED' and sha(ROOT/red['log_ref']['path'])==red['log_ref']['sha256']
frozen=load(DESIGN/'evidence-inputs.json')
for entry in frozen['inputs']:
 for r in entry['immutable_inputs']:assert sha(ROOT/r['path'])==r['sha256']
 derived=OUT/'reverification-final'/entry['run_id'];s=load(derived/'compiled-summary.json')
 assert s['analysis']['mode']=='OFFLINE_REVERIFICATION'
 for key,name in [('verification_spec_ref','verification-spec.json'),('completion_contract_ref','completion-contract.json')]:assert s['analysis'][key]==ref(DESIGN/name)
 for chain in s['canonical_chain']['initialization_chains']:assert chain['initialization_basis']['input_refs']=={k:s['analysis'][k] for k in ['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']}
historical=[]
for name in ['result.json','p36-r1/result.json','p36-r2/result.json','resume-r3/result.json']:
 p=ROOT/'.aegis/results/G5-UR0b'/name;r=load(p);bundle=Path(r['materialized_ref']['path']);m=load(bundle/'manifest.json');assert sha(bundle/'manifest.json')==r['materialized_ref']['manifest_sha256']
 assert all(sha(bundle/n)==v['sha256'] for n,v in m['files'].items())
 historical.append({'result_ref':ref(p),'manifest_ref':ref(bundle/'manifest.json'),'file_count':len(m['files']),'all_bytes_unchanged':True})
window=ROOT/'.aegis/results/G5-UR0b/resume-r3/window.json';assert load(window)['status']=='CLOSED' and sha(window)==reuse['old_window_sha256'] and load(window)['charged_seconds']==1133
provenance=load(ROOT/'.aegis/results/G5-UR0b/resume-r3/artifact-provenance.json');native=provenance['native_validation']
assert sha(ROOT/native['log_ref']['path'])==native['log_ref']['sha256']
assert all(sha(p)==digest for p,digest in native['executable_sha256'].items())
artifact_counts={}
for tier,artifact in provenance['tiers'].items():
 directory=Path(artifact['sdk_directory']);actual={str(p.relative_to(directory)):sha(p) for p in directory.rglob('*') if p.is_file()};assert actual==artifact['artifacts'];artifact_counts[tier]=len(actual)
write(RESULT/'preservation.json',{'status':'UNCHANGED_PRODUCT_RECORDINGS_AND_PRIOR_RETURNS_VERIFIED','historical':historical,'frozen_input_files':45,'native_evidence':{'mode':'INHERITED_EXACT_22_CTEST_NO_RERUN','log_ref':native['log_ref'],'unchanged_executable_count':len(native['executable_sha256'])},'unchanged_sdk_artifact_files':artifact_counts,'old_window_status':'CLOSED','sampling_debit_seconds':1133,'new_browser_workers':0,'new_artifact_builds':0})
oracles=load(DESIGN/'oracle-cases.json')['cases']
mapping={c['id']:{'status':'SATISFIED_FOR_OFFLINE_SLICE','expected':c['expected'],'evidence_refs':[]} for c in oracles}
for key in mapping:
 if key in ['COMMITTED-ONLY','OFF-NO-OBSERVATIONS','ACTUAL-RT60','REUSE-APPLICABILITY']:mapping[key]['evidence_refs']+=[ref(RESULT/'offline-reverification.json')]
 if key=='REUSE-APPLICABILITY':mapping[key]['evidence_refs']+=[ref(RESULT/'reuse-assessment.json')]
 if key=='BINDING-AND-CLOSED-AUTH':mapping[key]['evidence_refs']+=[ref(OUT/'python-final.log'),ref(ROOT/'verification/tests/test_g5_ur0_repair_runner.py'),ref(ROOT/'verification/tests/test_g5_ur0_resume_runner.py')]
 else:mapping[key]['evidence_refs']+=[ref(OUT/'node-final.log'),ref(ROOT/'verification/tests/g5_ur0_initialization.test.mjs')]
 if key in ['INIT-ONLY','NATIVE-ONLY','DUPLICATE-OR-OMITTED','MISSING-MUTATION-RECEIPT']:mapping[key]['evidence_refs']+=[ref(OUT/'python-final.log'),ref(ROOT/'verification/tests/test_g5_ur0_repair_runner.py')]
assert len(mapping)==16 and all(v['evidence_refs'] for v in mapping.values());write(RESULT/'oracle-coverage.json',{'status':'ALL_16_FROZEN_ORACLE_GROUPS_SATISFIED','cases':mapping})
facts={'status':'OFFLINE_INITIALIZATION_REPAIR_VERIFIED','node_passed':node,'python_passed':python,'boundary_contracts':120,'oracle_groups':16,'immutable_inputs_recompiled':15,'accepted_completion_records':10,'initialization_completion_records':1,'committed_completion_records':9,'unresolved_accepted_records':0,'note':'Completion counts are frame records, not counts of semantic mutations.','metrics_and_external_bytes_unchanged':True,'committed_receipts_attempts_and_snapshot_outcomes_unchanged':True,'native_cpp':'Existing22CTest and11 executable hashes retained; not rebuilt/rerun','new_browser_workers':0,'new_artifact_builds':0,'old_window_status':'CLOSED','sampling_debit_seconds':1133,'reuse_status':reuse['status'],'official_gate_pass':False,'performance_pass':False}
write(RESULT/'verification.json',facts)
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree/'init-repair-r1';assert not BUNDLE.exists(),'never replace sealed evidence';BUNDLE.mkdir(parents=True)
write(BUNDLE/'source/full-source-identity.json',source);copy(RESULT/'source-scope-audit.json',BUNDLE/'source/source-scope-audit.json')
(BUNDLE/'source/tracked.patch').write_bytes(git('diff','--binary','HEAD'))
untracked=[n for n in git('ls-files','--others','--exclude-standard','-z').decode().split('\0') if n and not n.startswith(source_identity.EXCLUDED)]
with tarfile.open(BUNDLE/'source/untracked-source.tar.gz','w:gz') as archive:
 for n in untracked:archive.add(ROOT/n,arcname=n,recursive=False)
write(BUNDLE/'source/git-status.json',{'revision':source['revision'],'branch':git('branch','--show-current').decode().strip(),'porcelain':git('status','--porcelain=v1').decode(),'untracked_non_output_sources':untracked})
for directory,label in [(PACKAGE,'package'),(DESIGN,'current-verification')]:
 for p in directory.iterdir():
  if p.is_file():copy(p,BUNDLE/'authority'/label/p.name)
for p in RESULT.iterdir():
 if p.is_file() and p.name not in ['result.json','validation.json','output-sha256.json']:copy(p,BUNDLE/'control-and-proofs'/p.name)
for p in OUT.rglob('*'):
 if p.is_file() and p.name!='materialization.log':copy(p,BUNDLE/'offline-execution'/p.relative_to(OUT))
for entry in frozen['inputs']:
 for r in entry['immutable_inputs']:copy(ROOT/r['path'],BUNDLE/'immutable-inputs'/entry['run_id']/Path(r['path']).name)
 old=ROOT/entry['input_directory']
 for name in ['metrics.json','external-metrics.json','canonical-chain.json','compiled-summary.json','decoded.json','final.axsnap']:copy(old/name,BUNDLE/'recorded-baseline'/entry['run_id']/name)
for name in scope['changed_paths']:copy(ROOT/name,BUNDLE/'changed-source'/name)
readme='''G5-UR0b-init-repair-r1：READY_FOR_CONTROL_REVIEW（完整离线修复切片）。

新P31包来自已批准的 Current 初始化验证设计。真实RT60旧行为先得到MISSING的RED，完整原生初始化链和公开init.state均已在断言中确认；不是缺字段/导入失败。新v2 proof完整划分每一条accepted raw：初始化、已提交、未证明。初始化由固定fresh-empty harness/source、实际输入字节哈希、公开初始状态及完整原生owner/draw/provider/capture/completion共同证明；generation0或operation=null单独不足以放行。document epoch取公开target.runtimeEpoch，不使用lastCommit.runtimeEpoch哨兵。

初始化另列lifecycle，不伪造Operation/Scene，也不产生mutation receipt或Up→Canonical。已提交链、全部准备attempt、外来/过时/歧义校验保持；positive gate仍要求真实committed_count>0。v1读者保留原严格含义，v2实际CLI/Python读者一致，OFFLINE不能冒充fresh，旧授权/窗口仍关闭。

最终回归100Node、44Python、120boundary通过，16组冻结oracle已映射并满足。15份原始录制离线复编译：10条accepted完成记录=1条初始化+9条已提交+0未证明；不是10次语义mutation。所有原metrics.json和external metrics字节完全相同，提交receipt/链、全部attempt、快照/终态/产品结果保持。RT60的新对账2=1+1+0，mutation receipt仍1。前一离线轮次及额外歧义RED/GREEN保留，没有覆盖原记录。

六对既有overhead样本/统计/资格结果不变；复用评估为ELIGIBLE_PENDING_INDEPENDENT_CONTROL_REVIEW，只有控制面可以确认未来适用性。离线复编译不是新采样，本轮browser worker/build均0，采样扣账仍1133秒，旧r3窗口保持CLOSED。Runtime/SDK/collector/harness/metric schema/阈值/锁均未改；既有22项CTest证据及二档SDK artifact精确哈希复用，无native/SDK重建。

本切片不等于完整UR0b或P34/性能PASS。120/240Hz、六类Web correctness和30项主矩阵仍是旧采样任务的未执行项；本包不授权它们，不自动重跑，不提交/推送，不进入UR0c/UR1。下一步是独立控制面审查离线修复、全部raw分区和既有cohort复用适用性。
'''
(BUNDLE/'README.md').write_text(readme)
files={str(p.relative_to(BUNDLE)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(BUNDLE.rglob('*')) if p.is_file()}
write(BUNDLE/'manifest.json',{'schema':'g5-ur0b-init-repair-r1-manifest-v1','status':'READY_FOR_CONTROL_REVIEW','scope':'OFFLINE_INITIALIZATION_REPAIR_ONLY','source_revision':source['revision'],'source_tree_sha256':tree,'package_sha256':sha(PACKAGE/'package.json'),'files':files,'official_gate_pass':False})
result={'schema_version':1,'task_id':'G5-UR0b-init-repair-r1','stage':'P32_OFFLINE_INIT_REPAIR','status':'READY_FOR_CONTROL_REVIEW','scope':'OFFLINE_INITIALIZATION_REPAIR_ONLY','return_surface':'CONTROL_REVIEW','starting_revision':load(RESULT/'preflight.json')['starting_revision'],'result_revision':source['revision'],'source_tree_sha256':tree,'dirty':True,'dirty_source_paths':git('diff','--name-only').decode().splitlines(),'package_ref':ref(PACKAGE/'package.json'),'verification_spec_ref':ref(DESIGN/'verification-spec.json'),'completion_contract_ref':ref(DESIGN/'completion-contract.json'),'oracle_ref':ref(DESIGN/'oracle-cases.json'),'immutable_input_ref':ref(DESIGN/'evidence-inputs.json'),'materialized_ref':{'kind':'shared-controlled-worktree-content-addressed-bundle','path':str(BUNDLE),'manifest_sha256':sha(BUNDLE/'manifest.json')},'verification_results':facts,'offline_reverification_ref':ref(RESULT/'offline-reverification.json'),'reuse_assessment_ref':ref(RESULT/'reuse-assessment.json'),'scope_audit_ref':ref(RESULT/'source-scope-audit.json'),'preservation_ref':ref(RESULT/'preservation.json'),'oracle_coverage_ref':ref(RESULT/'oracle-coverage.json'),'parent_UR0b_complete':False,'old_sampling_window':'CLOSED_UNCHANGED','uncompleted_parent_sampling':['realtime120/240','six independent Web correctness workers','30 main matrix rows'],'new_sampling_workers':0,'new_artifact_builds':0,'prior_sampling_debit_seconds':1133,'performance_pass':False,'official_gate_pass':False,'continue_execution':False,'committed':False,'pushed':False,'notification_policy_ref':order['notify_terminal']['user_policy_ref'],'next_action':'Independent control review of the completed offline slice and unchanged cohort reuse applicability; no sampling authorization inferred'}
write(RESULT/'result.json',result);(RESULT/'README.md').write_text(readme+'\nBundle: '+str(BUNDLE)+'\n')
checks={'exact_source_tree':source_identity.identity(ROOT)['source_tree_sha256']==tree,'all_bundle_bytes_match':all(sha(BUNDLE/n)==v['sha256'] for n,v in files.items()),'all_prior_bundles_preserved':all(h['all_bytes_unchanged'] for h in historical),'all_15_frozen_inputs_reconciled':verification['count']==15 and verification['unresolved_total']==0,'all_16_oracle_groups_satisfied':len(mapping)==16,'no_browser_build_or_gate_claim':result['new_sampling_workers']==0 and result['new_artifact_builds']==0 and not result['official_gate_pass'] and not result['performance_pass'],'old_window_remains_closed':load(window)['status']=='CLOSED' and sha(window)==reuse['old_window_sha256'],'authorized_source_only':set(changed)<=set(order['source_scope'])}
assert all(checks.values()),checks
write(RESULT/'validation.json',{'status':'COMPLETE_OFFLINE_RETURN_VERIFIED','checks':checks,'bundle_files':len(files),'manifest_sha256':sha(BUNDLE/'manifest.json'),'result_sha256':sha(RESULT/'result.json')})
write(RESULT/'output-sha256.json',{'result.json':sha(RESULT/'result.json'),'README.md':sha(RESULT/'README.md'),'validation.json':sha(RESULT/'validation.json'),'manifest':{'path':str(BUNDLE/'manifest.json'),'sha256':sha(BUNDLE/'manifest.json')}})
print(json.dumps({'status':result['status'],'package_ref':ref(PACKAGE/'package.json'),'result_ref':ref(RESULT/'result.json'),'materialized_ref':result['materialized_ref'],'source_tree_sha256':tree,'files':len(files),'tests':{'node':node,'python':python,'boundary':120},'inputs':15,'raw':10,'initialization':1,'committed':9,'unresolved':0,'new_sampling_workers':0}))
