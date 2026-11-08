"""Preserve P36 repairs and the newly reproduced positive-join blocker."""
import collections,hashlib,importlib.util,json,re,shutil,subprocess,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b';RUN=OUT/'p36-r1/execution'
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def copy(p,to):to=Path(to);to.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,to)
spec=importlib.util.spec_from_file_location('p36_identity',ROOT/'verification/runners/g5/ur0/source_identity.py');identity=importlib.util.module_from_spec(spec);spec.loader.exec_module(identity)
source=identity.identity(ROOT);proof=load(RESULT/'mechanism-proofs.json');tree=source['source_tree_sha256']
assert tree==proof['source_tree_sha256']
assert load(RESULT/'control-stop-cleanup.json')['status']=='ALL_OBSERVED_OWNED_PIDS_GONE'
assert load(RESULT/'repeated-frame-witness-output.json')['status']=='REPRODUCED_FALSE_NEGATIVE'
assert load(RUN/'execution-stop.json')['status']=='BLOCKED_EVIDENCE'
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree/'p36-r1'
assert not BUNDLE.exists(),'never replace a result bundle'
BUNDLE.mkdir(parents=True)
write(BUNDLE/'source/full-source-identity.json',source)
(BUNDLE/'source/tracked.patch').write_bytes(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT))
scope=load(RESULT/'source-scope-audit.json');write(BUNDLE/'source-scope-audit.json',scope)
with tarfile.open(BUNDLE/'source/untracked-source.tar.gz','w:gz') as archive:
 for p in scope['untracked_source']:archive.add(ROOT/p,arcname=p,recursive=False)
write(BUNDLE/'source/git-status.json',{'head':source['revision'],'branch':subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),'porcelain':subprocess.check_output(['git','status','--porcelain=v1'],cwd=ROOT,text=True),'head_contains_dirty_changes':False})
shutil.copytree(RUN,BUNDLE/'execution')
for p in OUT.glob('p36-*'):
 if p.is_file():copy(p,BUNDLE/'checks'/p.name)
shutil.copytree(OUT/'p36-r1/pixel-mechanism',BUNDLE/'offline-pixel-mechanism')
shutil.copytree(OUT/'p36-r1/recompiled-original-final',BUNDLE/'recompiled-original-without-mutating-original')
for p in RESULT.iterdir():
 if p.is_file():copy(p,BUNDLE/'control-and-diagnosis'/p.name)
for p in (ROOT/'.aegis/packages/G5-UR0b').iterdir():
 if p.is_file():copy(p,BUNDLE/'authority/package'/p.name)
for p in (ROOT/'.aegis/results/G5-UR0b/control-reverification-r1').iterdir():
 if p.is_file():copy(p,BUNDLE/'authority/independent-reverification'/p.name)
boundary=load(OUT/'p36-normal-qualification-boundary.json')
for variant in ['qualified','normal']:
 build=OUT/f'p36-r1/build-{variant}-64-192';sdk=build/'sdk/web';files=[]
 with tarfile.open(BUNDLE/f'{variant}-web-artifacts.tar.gz','w:gz') as archive:
  for entry in boundary['normal_install_manifest']:
   relative=entry['path'].removeprefix('js/');p=sdk/relative
   assert p.is_file(),p
   files.append({'path':relative,'sha256':sha(p),'bytes':p.stat().st_size});archive.add(p,arcname=relative,recursive=False)
 write(BUNDLE/f'{variant}-artifact-manifest.json',files)
 for name in ['CMakeCache.txt','ur0b-build-request.json','configure.log','build.log']:
  copy(build/name,BUNDLE/'build'/variant/name)
copy(ROOT/'out/host/runtime/render/axiom_ur0_fixture_oracle',BUNDLE/'native/axiom_ur0_fixture_oracle')
for name in ['semantic-sdk.lock.json','r1-full-skia-sdk.lock.json','docs/planning/g5-unified-render/ur0a/fixtures/continuous-30s-v1.json','docs/planning/g5-unified-render/ur0a/fixtures/immediate-actions-v1.json']:
 copy(ROOT/name,BUNDLE/'authority/frozen-inputs'/name)
ledger=load(RUN/'worker-ledger.json');diagnostic=[]
for mode in ['OFF','ON']:
 guard=load(RUN/f'guards/diagnostic-{mode}.worker.json');summary=load(RUN/f'raw-runs/diagnostic-{mode}/compiled-summary.json')
 diagnostic.append({'toggle':mode,'guard':guard,'record_count':summary['actual_observations']['raw_records'],'critical_loss':summary['critical_loss'],'statistical_loss':summary['statistical_loss'],'schema_status':summary['schema_status'],'identity_errors':len(summary['identity_errors']),'qualified_completions':summary['actual_observations']['qualified_completions']})
native=int(re.search(r'0 tests failed out of (\d+)',(OUT/'p36-ctest-required.log').read_text())[1])
node=int(re.search(r'# pass (\d+)\n# fail 0',(OUT/'p36-node-required-final.log').read_text())[1])
python=int(re.search(r'Ran (\d+) tests',(OUT/'p36-python-required.log').read_text())[1])
contracts=int(re.search(r'passed \((\d+) contract files\)',(OUT/'p36-runtime-boundaries-r2.log').read_text())[1])
regression={'status':'AFFECTED_REGRESSIONS_VERIFIED','native_tests':native,'node_tests':node,'python_tests':python,'boundary_contracts':contracts,
 'full_ctest':'CORROBORATIVE_ONLY_NOT_RERUN; existing historical failures not repaired or relabeled','positive_browser_chain':'NEW_FAILURE retained separately; passing unit tests do not close it'}
write(BUNDLE/'regression.json',regression);write(BUNDLE/'diagnostic-summary.json',{'status':'GUARD_LOSS_ARTIFACT_SHAPE_CHECKS_PASS_ONLY','canonical_join':'BLOCKED_FALSE_NEGATIVE','rows':diagnostic,'no_primary_timing_adopted':True})
diagnosis=load(RESULT/'canonical-join-diagnosis.json')
write(BUNDLE/'blocking-positive-chain.json',{'classification':'IMPLEMENTATION_DEFECT','owner':'UR0b R4 compiler / canonical demand-attempt joins','facts':[{'work_id':f['work_id'],'frame_id':f['capture_values'][2],'preparation_attempts':f['render_match_count'],'actual_provider_submits':f['provider_match_count'],'accepted_completions':len(f['accepted_completions']),'accepted_provider_job':f['provider_matches'][0]['job']} for f in diagnosis['facts']],
 'current_rule':'draws.length must equal one for full frame identity, so repeated pending attempts are falsely rejected despite one matching provider job','required_repair':'prove successful attempt via actual provider job plus complete owner/frame/Scene/camera/surface/metrics and record order; preserve pending attempts; add actual-compiler repeated-frame positive and foreign/stale negatives',
 'sampling_admission_defect':'qualification was started after guard/loss/schema checks without checking this positive canonical chain; stopped as soon as diagnosed','no_late_code_fix_or_retry':True})
counts={phase:dict(collections.Counter(r['status'] for r in ledger if r['phase']==phase)) for phase in ['original','diagnostic','qualification','separate','main']}
guards=[load(p) for p in (RUN/'guards').glob('*.worker.json')]
write(BUNDLE/'worker-accounting.json',{'outcomes':counts,'planned_upper_bound_seconds':sum(r['limit_seconds'] for r in ledger),'unchanged_all_workers_cap_seconds':8370,
 'new_completed_guard_elapsed_seconds':sum(g['elapsed_s'] for g in guards),'interrupted_worker_elapsed':'MISSING: control SIGINT; bounded by its original 90s and retained monotonic per-PID samples',
 'no_automatic_retry':True,'no_original_pair_reused':True,'all_observed_owned_pids_gone':True})
readme='''P36-r1 返回状态：BLOCKED_EVIDENCE，需独立控制面审查。

R1 预算中立 sidecar、R2 外来 owner 拒绝、R3 无损 lease 压缩、冷 snapshot/trace 传输及 RSS 归因已实现并有机制证据。
两次新诊断均完成，没有 RSS/超时硬停止，ON 无普通或关键丢样。此处只证明该诊断，不代表性能或完整观测 PASS。

新阻断：同一 canonical frame 有 24 次准备尝试，只有 1 个成功 provider submit，且 Runtime 已接受 completion；当前解码器因要求唯一 render frame 记录而丢失两条真实 completion。
六对资格验证曾提前启动：完成 3 个 worker，第 4 个在发现缺口后由控制面 SIGINT 停止，其余 8 个 MISSING。全部观察到的工作进程已清理。无 RSS/超时原因被伪造，部分资格结果不得作为开销或主要时序结论。
30 个主矩阵、3 个 realtime、6 个联合 Web correctness worker 全部未准入。独立 CPU 无缓存像素 oracle 与六类 driver 的代码已接入，但联合 Web correctness 尚未验证。

源码是 HEAD 加未提交改动，见 source/full-source-identity.json、tracked.patch 与 untracked-source.tar.gz；不要只以 HEAD 作为本实现身份。
旧结果与旧 bundle 均保留。没有修改产品策略、预算、semantic/Skia lock、公开 API 或旧 P34；没有提交、推送或发消息。下一步应修成功 attempt 的精确关联与采样准入门，再由控制面决定是否授权新的有界资格组。无 G5、性能或 P34 PASS。
'''
(BUNDLE/'README.md').write_text(readme)
files={str(p.relative_to(BUNDLE)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(BUNDLE.rglob('*')) if p.is_file()}
manifest={'schema':'g5-ur0b-p36-r1-manifest-v1','source_revision':source['revision'],'source_tree_sha256':tree,'status':'BLOCKED_EVIDENCE','files':files}
write(BUNDLE/'manifest.json',manifest)
result={'schema_version':1,'task_id':'G5-UR0b','stage':'P36-r1','status':'BLOCKED_EVIDENCE','return_surface':'CONTROL_REVIEW','root_cause_class':'IMPLEMENTATION_DEFECT',
 'reason':'R4 actual compiler cannot associate accepted canonical completions with the successful job among repeated same-frame preparation attempts; conditional sampling halted.',
 'package_ref':{'path':'.aegis/packages/G5-UR0b/package.json','sha256':sha(ROOT/'.aegis/packages/G5-UR0b/package.json')},
 'authorization_ref':{'path':'.aegis/results/G5-UR0b/control-reverification-r1/repair-authorization.json','sha256':sha(ROOT/'.aegis/results/G5-UR0b/control-reverification-r1/repair-authorization.json')},
 'starting_revision':source['revision'],'result_revision':source['revision'],'source_tree_sha256':tree,'head_contains_dirty_changes':False,
 'materialized_ref':{'kind':'shared-controlled-worktree-content-addressed-bundle','path':str(BUNDLE),'manifest_sha256':sha(BUNDLE/'manifest.json')},
 'validation_results':regression,'worker_outcomes':counts,'matrix_ledger_ref':str(BUNDLE/'execution/worker-ledger.json'),
 'correctness_status':'AFFECTED_COMPONENT_REGRESSIONS_PASS; COMBINED_WEB_CORRECTNESS_MISSING','evidence_status':'INCOMPLETE_CANONICAL_POSITIVE_CHAIN; full raw and failure provenance preserved','performance_status':'NOT_EVALUATED; diagnostic and incomplete qualification timings not adopted',
 'scope_audit':scope,'open_boundaries':['Successful canonical attempt/provider/work/completion join remains unresolved','Qualification admission accepted partial guard/loss/shape checks too early; remaining cohort stopped, no retry','30 main, 3 realtime and 6 combined Web correctness outcomes are MISSING','32/96 candidate artifact not built after this control stop','Product input rejection remains preserved; observation task does not repair product editing','No physical display/pure GPU duration or multi-platform acceptance claimed'],
 'original_result_unchanged':True,'official_gate_pass':False,'continue_execution':False,'stop_after':'UR0b','committed':False,'pushed':False}
write(RESULT/'result.json',result);(RESULT/'README.md').write_text(readme+'\nBundle: '+str(BUNDLE)+'\n')
old=load(ROOT/'.aegis/results/G5-UR0b/output-sha256.json')
validation={'status':'VERIFIED_BLOCKED_RETURN','checks':{'exact_source_tree_unchanged_during_sampling':identity.identity(ROOT)['source_tree_sha256']==tree,
 'all_bundle_hashes_match':all(sha(BUNDLE/k)==v['sha256'] for k,v in files.items()),
 'original_return_hashes_unchanged':all(sha(ROOT/'.aegis/results/G5-UR0b'/k)==v if isinstance(v,str) else sha(v['path'])==v['sha256'] for k,v in old.items()),
 'all_56_worker_envelopes_accounted':len(ledger)==56 and all(r['status'] not in ['PLANNED','CLAIMED'] for r in ledger),
 'no_new_worker_over_cap':all(g['peak_process_family_rss_kib']<=g['rss_limit_kib']==3145728 for g in guards),
 'completed_workers_cleaned':all(g['cleanup_complete'] for g in guards),'interrupted_worker_cleaned':load(RESULT/'control-stop-cleanup.json')['status']=='ALL_OBSERVED_OWNED_PIDS_GONE',
 'blocker_reproducer_retained':load(RESULT/'repeated-frame-witness-output.json')['status']=='REPRODUCED_FALSE_NEGATIVE','no_primary_or_gate_pass':not result['official_gate_pass'] and result['performance_status'].startswith('NOT_EVALUATED')},'bundle_file_count':len(files),'manifest_sha256':sha(BUNDLE/'manifest.json'),'result_sha256':sha(RESULT/'result.json')}
assert all(validation['checks'].values()),validation
write(RESULT/'validation.json',validation);write(RESULT/'output-sha256.json',{'result.json':sha(RESULT/'result.json'),'README.md':sha(RESULT/'README.md'),'validation.json':sha(RESULT/'validation.json'),'manifest':{'path':str(BUNDLE/'manifest.json'),'sha256':sha(BUNDLE/'manifest.json')}})
print(json.dumps({'status':result['status'],'bundle':str(BUNDLE),'source_tree_sha256':tree,'manifest_sha256':sha(BUNDLE/'manifest.json'),'validation':validation['status'],'regression':regression,'worker_outcomes':counts}))
