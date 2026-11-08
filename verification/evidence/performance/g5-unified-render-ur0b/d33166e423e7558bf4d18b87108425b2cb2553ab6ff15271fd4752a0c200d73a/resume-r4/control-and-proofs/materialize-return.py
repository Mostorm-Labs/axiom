"""Seal the r4 guarded stop and retain the distinct accepted recorded inputs."""
import hashlib,json,shutil,subprocess,sys,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b/resume-r4'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import remaining_runner as runner
import source_identity
load=runner.load;sha=runner.sha;write=runner.atomic
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
def copy(p,to):to=Path(to);to.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,to);assert sha(to)==sha(p)
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
p,c,history,inputs=runner.authority();reused=runner.accepted_reuse(inputs);provenance=load(RESULT/'artifact-provenance.json');tiers=provenance['tiers']
proof=runner.verify_proof(RESULT/'mechanism-proofs.json',p,c,Path(tiers['64-192']['sdk_directory']),Path(tiers['32-96']['sdk_directory']))
source=source_identity.identity(ROOT);tree=source['source_tree_sha256'];assert tree==proof['source_tree_sha256']
assert load(RESULT/'reused-prerequisites.json')==reused
window=load(RESULT/'window.json');analysis=load(RESULT/'terminal-analysis.json');assert window['status']=='CLOSED' and window['terminal_status']=='FROZEN_VERIFICATION_FAILURE'
assert analysis['history']['new_starts']==3 and analysis['history']['new_missing']==35 and analysis['history']['cumulative_charged_seconds']==1251
assert load(RESULT/'cleanup-verification.json')['status']=='ALL_OBSERVED_PROCESSES_GONE'
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
preserved=[]
for name in ['result.json','p36-r1/result.json','p36-r2/result.json','resume-r3/result.json','init-repair-r1/result.json']:
 f=ROOT/'.aegis/results/G5-UR0b'/name;old=load(f);bundle=Path(old['materialized_ref']['path']);m=load(bundle/'manifest.json');assert sha(bundle/'manifest.json')==old['materialized_ref']['manifest_sha256']
 assert all(sha(bundle/n)==r['sha256'] for n,r in m['files'].items())
 preserved.append({'result_ref':ref(f),'manifest_ref':ref(bundle/'manifest.json'),'file_count':len(m['files']),'all_bytes_unchanged':True})
write(RESULT/'historical-preservation.json',preserved)
for r in provenance['native_executable_sha256']:
 assert sha(r)==provenance['native_executable_sha256'][r]
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree/'resume-r4';assert not BUNDLE.exists(),'never replace a sealed return';BUNDLE.mkdir(parents=True)
write(BUNDLE/'source/full-source-identity.json',source);copy(RESULT/'source-scope-audit.json',BUNDLE/'source/source-scope-audit.json')
(BUNDLE/'source/tracked.patch').write_bytes(git('diff','--binary','HEAD'))
untracked=[n for n in git('ls-files','--others','--exclude-standard','-z').decode().split('\0') if n and not n.startswith(source_identity.EXCLUDED)]
with tarfile.open(BUNDLE/'source/untracked-source.tar.gz','w:gz') as archive:
 for n in untracked:archive.add(ROOT/n,arcname=n,recursive=False)
write(BUNDLE/'source/git-status.json',{'revision':source['revision'],'branch':git('branch','--show-current').decode().strip(),'porcelain':git('status','--porcelain=v1').decode(),'untracked_non_output_sources':untracked})
for source_path in c['authorized_source_paths']:copy(ROOT/source_path,BUNDLE/'changed-source'/source_path)
for f in runner.PACKAGE.parent.iterdir():
 if f.is_file():copy(f,BUNDLE/'authority/r4-package'/f.name)
for directory in ['control-continuation-r4','control-review-init-repair-r1']:
 for f in (ROOT/'.aegis/results/G5-UR0b'/directory).iterdir():
  if f.is_file():copy(f,BUNDLE/'control-inputs'/directory/f.name)
for f in RESULT.iterdir():
 if f.is_file() and f.name not in ['window.lock','result.json','validation.json','output-sha256.json']:copy(f,BUNDLE/'control-and-proofs'/f.name)
for f in OUT.rglob('*'):
 if f.is_file() and f.name!='materialization.log':copy(f,BUNDLE/'execution-and-checks'/f.relative_to(OUT))
bound_files=[]
for r in proof['files']:
 copy(ROOT/r['path'],BUNDLE/'bound-mechanism-files'/r['path']);bound_files.append({'original_ref':r,'bundle_path':'bound-mechanism-files/'+r['path']})
write(BUNDLE/'bound-file-index.json',bound_files)
write(BUNDLE/'reused-prerequisites.json',reused);write(BUNDLE/'historical-budget.json',history)
for record in inputs['records']:
 for key in ['accepted_derived_summary_ref','accepted_v2_proof_ref']:
  r=record[key];copy(ROOT/r['path'],BUNDLE/'accepted-recorded-reuse'/record['run_id']/Path(r['path']).name)
write(BUNDLE/'matrix-ledger.json',[r for r in window['rows'] if r['phase']=='main'])
summary={'status':'INCOMPLETE_FROZEN_GUARD_STOP','recorded_prerequisites':'ACCEPTED_RECORDED_REUSE','fresh_admitted':analysis['fresh_admitted_runs'],'correctness':'INCOMPLETE_OFF_PIXEL_FAILURES_AND_PHASE_TIMEOUT; no paired ON verdict','main':'NOT_STARTED_MISSING','history':analysis['history'],'partial_off_pixel_results':analysis['partial_off_pixel_results'],'guard_failure':analysis['actual_guard_failure'],'primary_timings_adopted':False,'performance_pass':False,'official_gate_pass':False}
write(BUNDLE/'baseline-summary.json',summary)
readme='''G5-UR0b-resume-r4：FROZEN_VERIFICATION_FAILURE，返回 CONTROL_REVIEW。

r4新runner/测试及全部机制证明完成：100Node、56Python、120boundary通过；只新增允许的两个Python源码，既有compiler/Runtime/SDK/collector/harness保持已接受字节。旧诊断、六对AB/BA和60Hz通过明确ACCEPTED_RECORDED_REUSE入口引用，原OFFLINE/source身份不重命名；没有重跑旧cohort。

新的120Hz、240Hz真实worker均通过WORKER_COMPILE、当前source/artifact、schema及完整v2 canonical证明：每项raw2=init1+committed1+unresolved0。两项guard分别40.936s、41.928s，逐项ceil扣41s、42s。

第三项lower_z_translucent_drag在冻结correctness-only30秒阶段超时，worker退出124。完整进程guard记录34.214s，ceil35s；RSS峰值1852016KiB低于3145728KiB，清理完成。OFF0/1比较已落盘，内部不一致像素分别2463/16112；但ON配对未完成，没有worker-result、最终trace或完整correctness_status，不能判PRODUCT_RED_IN_BOTH_OFF_ON，也不能通过正确性。

runner原始停止文案为“new worker requires fresh WORKER_COMPILE”，因为阶段超时发生在worker-result/trace/compiler输出前。原文和账本保留；实际原因由原guard退出124及phase-timeout记录证明，不是旧证据被当fresh拒绝。现有stage只定位到OFF两步完成后、ON产物出现前，尚不能确定精确pending await。底层超时与像素差异根因留待控制面/Verification/harness复核，不在此包中改deadline/AA/oracle或重跑。

窗口CLOSED：38计划中3个启动、2个准入通过、1个guard停止、35个MISSING。历史30次1133s + 本次118s = 累计33次1251s。全部3个guard清理完成，独立PID复查无残留。主矩阵30行完全未启动；剩余额度不授权重试。全部旧包/窗口/五份结果及1783个历史bundle文件不变。

当前revision仍f651523a并有受控dirty；未提交、推送、重建或进入UR0c/UR1；没有产品/性能/P34或整体UR0bPASS。后续需要独立审查正确性worker的阶段与配对生命周期、保留OFF像素证据，并另行决定修复及采样授权。
'''
(BUNDLE/'README.md').write_text(readme)
files={str(f.relative_to(BUNDLE)):{'sha256':sha(f),'bytes':f.stat().st_size} for f in sorted(BUNDLE.rglob('*')) if f.is_file()}
write(BUNDLE/'manifest.json',{'schema':'g5-ur0b-r4-evidence-manifest-v1','status':'FROZEN_VERIFICATION_FAILURE','package_sha256':runner.PACKAGE_SHA,'source_revision':source['revision'],'source_tree_sha256':tree,'files':files,'official_gate_pass':False})
result={'schema_version':1,'task_id':'G5-UR0b-resume-r4','status':'FROZEN_VERIFICATION_FAILURE','return_surface':'CONTROL_REVIEW','starting_revision':load(RESULT/'entry-preflight.json')['starting_revision'],'result_revision':source['revision'],'source_tree_sha256':tree,'dirty':True,'package_ref':ref(runner.PACKAGE),'mechanism_proof_ref':ref(RESULT/'mechanism-proofs.json'),'accepted_reuse_ref':ref(RESULT/'reused-prerequisites.json'),'terminal_analysis_ref':ref(RESULT/'terminal-analysis.json'),'cleanup_ref':ref(RESULT/'cleanup-verification.json'),'materialized_ref':{'kind':'shared-controlled-worktree-content-addressed-bundle','path':str(BUNDLE),'manifest_sha256':sha(BUNDLE/'manifest.json')},'validation_results':load(RESULT/'regression.json'),'fresh_admitted_runs':analysis['fresh_admitted_runs'],'blocker':{'class':'FROZEN_VERIFICATION_FAILURE','kind':'CORRECTNESS_PHASE_DEADLINE','run_id':analysis['stop_run_id'],'guard_exit_code':124,'phase_limit_ms':30000,'precise_underlying_await':'UNRESOLVED_FROM_EXISTING_STAGE_MARKERS','partial_OFF_pixel_failures_preserved':True,'paired_ON_complete':False,'not_eligible_for_PRODUCT_RED_IN_BOTH_OFF_ON':True,'owner_followup':'Independent control/Verification/harness review; no timer/threshold/code repair or retry in this window'},'accounting':analysis['history'],'guard_totals':analysis['guard_totals'],'unstarted_remaining_ids':analysis['remaining_ids'],'uncompleted_required_evidence':['lower_z_translucent_drag paired correctness','five unstarted correctness cases','30 unstarted main matrix rows'],'recorded_vs_fresh':'Recorded reuse keeps original OFFLINE/recorded source and accepted compiler identity; two new real-time rows retain WORKER_COMPILE/current source; incomplete guard-stopped worker not promoted.','old_windows_unchanged':True,'history_preservation':preserved,'new_artifact_builds':0,'new_diagnostic_or_overhead_workers':0,'new_browser_starts':3,'official_gate_pass':False,'performance_pass':False,'primary_timings_adopted':False,'continue_execution':False,'automatic_retry':False,'committed':False,'pushed':False,'next_action':'Independent review of bounded stop, paired correctness phase lifecycle and partial OFF oracle results. Any repair or new execution requires separate explicit authority.'}
write(RESULT/'result.json',result);(RESULT/'README.md').write_text(readme+'\nBundle: '+str(BUNDLE)+'\n')
checks={'source_matches_mechanism_proof':source_identity.identity(ROOT)['source_tree_sha256']==tree,'new_bundle_hashes_match':all(sha(BUNDLE/n)==r['sha256'] for n,r in files.items()),'old_bundles_unchanged':all(r['all_bytes_unchanged'] for r in preserved),'all_38_planned_rows_accounted':len(window['rows'])==38 and len(analysis['remaining_ids'])==35,'charges_truthful':window['charged_seconds']==1251 and window['new_spent_seconds']==118 and window['reserved_seconds']==0,'new_window_closed':load(RESULT/'window.json')['status']=='CLOSED','owned_processes_gone':load(RESULT/'cleanup-verification.json')['status']=='ALL_OBSERVED_PROCESSES_GONE','no_retry_gate_or_complete_correctness_claim':not result['automatic_retry'] and not result['official_gate_pass'] and result['blocker']['paired_ON_complete'] is False}
assert all(checks.values()),checks
write(RESULT/'validation.json',{'status':'VERIFIED_FROZEN_RETURN','checks':checks,'bundle_files':len(files),'result_sha256':sha(RESULT/'result.json'),'manifest_sha256':sha(BUNDLE/'manifest.json')})
write(RESULT/'output-sha256.json',{'result.json':sha(RESULT/'result.json'),'README.md':sha(RESULT/'README.md'),'validation.json':sha(RESULT/'validation.json'),'manifest':{'path':str(BUNDLE/'manifest.json'),'sha256':sha(BUNDLE/'manifest.json')}})
print(json.dumps({'status':result['status'],'result_ref':ref(RESULT/'result.json'),'materialized_ref':result['materialized_ref'],'source_tree_sha256':tree,'bundle_files':len(files),'new_starts':3,'missing':35,'cumulative_charged_seconds':1251}))
