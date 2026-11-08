"""Seal the stopped r3 evidence without changing raw data or claiming completion."""
import hashlib,json,shutil,subprocess,sys,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
RESULT=Path(__file__).resolve().parent
OUT=ROOT/'out/ur0b/resume-r3'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import resume_runner as runner
import source_identity
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):runner.atomic(p,v)
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
def copy(p,to):
 to=Path(to);to.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,to)
 assert sha(to)==sha(p)
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
package,contract,history=runner.authority()
proof=runner.verify_proof(RESULT/'mechanism-proofs.json',package,contract,ROOT/'out/ur0b/p36-r1/build-qualified-64-192/sdk/web',OUT/'build-qualified-32-96/sdk/web')
source=source_identity.identity(ROOT);tree=source['source_tree_sha256'];assert tree==proof['source_tree_sha256']
analysis=load(RESULT/'terminal-analysis.json');assert analysis['status']=='FROZEN_VERIFICATION_FAILURE' and not analysis['continue_execution']
window=load(RESULT/'window.json');assert window['status']=='CLOSED'
assert git('rev-parse','HEAD').decode().strip()=='f651523a40c9b383b7d8e2115fcd32f75ddc38a7'
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
historical=[]
for name in ['result.json','p36-r1/result.json','p36-r2/result.json']:
 p=ROOT/'.aegis/results/G5-UR0b'/name;old=load(p);bundle=Path(old['materialized_ref']['path']);manifest=load(bundle/'manifest.json')
 assert sha(bundle/'manifest.json')==old['materialized_ref']['manifest_sha256']
 assert all(sha(bundle/n)==v['sha256'] for n,v in manifest['files'].items())
 historical.append({'result_ref':ref(p),'manifest_ref':ref(bundle/'manifest.json'),'file_count':len(manifest['files']),'all_bytes_unchanged':True})
write(RESULT/'historical-preservation.json',historical)
write(RESULT/'resume-position.json',{'cursor_class':'DESCENDANT_CURSOR','accepted_cursor':package['resume_cursor']['revision'],'accepted_checkpoint':'f651523a40c9b383b7d8e2115fcd32f75ddc38a7','starting_revision':'f651523a40c9b383b7d8e2115fcd32f75ddc38a7','result_revision':source['revision'],'source_tree_sha256':tree,'dirty':True,'no_new_commit_or_push':True,'package_sha256':runner.PACKAGE_SHA})
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree/'resume-r3'
assert not BUNDLE.exists(),'never replace a sealed return'
BUNDLE.mkdir(parents=True)
write(BUNDLE/'source/full-source-identity.json',source)
copy(RESULT/'source-scope-audit.json',BUNDLE/'source/source-scope-audit.json')
(BUNDLE/'source/tracked.patch').write_bytes(git('diff','--binary','HEAD'))
untracked=[n for n in git('ls-files','--others','--exclude-standard','-z').decode().split('\0') if n and not n.startswith(source_identity.EXCLUDED)]
with tarfile.open(BUNDLE/'source/untracked-source.tar.gz','w:gz') as archive:
 for n in untracked:archive.add(ROOT/n,arcname=n,recursive=False)
write(BUNDLE/'source/git-status.json',{'revision':source['revision'],'branch':git('branch','--show-current').decode().strip(),'porcelain':git('status','--porcelain=v1').decode(),'dirty_source_paths':[n for n in git('diff','--name-only').decode().splitlines()],'untracked_non_output_sources':untracked})
shutil.copytree(OUT/'execution',BUNDLE/'execution')
for p in OUT.iterdir():
 if p.is_file():copy(p,BUNDLE/'checks'/p.name)
for p in RESULT.iterdir():
 if p.is_file() and p.name not in ['result.json','validation.json','output-sha256.json','window.lock']:copy(p,BUNDLE/'control-and-proofs'/p.name)
for p in runner.PACKAGE.parent.iterdir():
 if p.is_file():copy(p,BUNDLE/'authority/resume-r3'/p.name)
for name in ['package.json','authority.lock.json','verification.lock.json','execution-contract.json','evidence-contract.json','scope-contract.json']:
 copy(ROOT/'.aegis/packages/G5-UR0b'/name,BUNDLE/'authority/parent'/name)
for directory in ['control-continuation-r3','control-review-p36-r2','control-review-resume-r3-checkpoint']:
 for p in (ROOT/'.aegis/results/G5-UR0b'/directory).iterdir():
  if p.is_file():copy(p,BUNDLE/'control-inputs'/directory/p.name)
bound=[]
for r in proof['files']:
 p=ROOT/r['path'];destination=BUNDLE/'bound-files'/r['path'];copy(p,destination)
 bound.append({'original_ref':r,'bundle_path':str(destination.relative_to(BUNDLE))})
provenance=load(RESULT/'artifact-provenance.json')
for tier,value in provenance['tiers'].items():
 sdk=Path(value['sdk_directory'])
 for n,expected in value['artifacts'].items():
  assert sha(sdk/n)==expected;copy(sdk/n,BUNDLE/'artifacts'/tier/n)
write(BUNDLE/'bound-file-index.json',bound)
write(BUNDLE/'historical-budget.json',history)
write(BUNDLE/'historical-preservation.json',historical)
write(BUNDLE/'matrix-ledger.json',[r for r in window['rows'] if r['phase']=='main'])
write(BUNDLE/'separate-result.json',{'status':'BLOCKED_EVIDENCE','executed':['realtime-60'],'blocked_run':'realtime-60','missing':[r['run_id'] for r in window['rows'] if r['phase']=='separate' and r['status']=='MISSING'],'reason':window['reason'],'qualified_separate_phase':False})
write(BUNDLE/'baseline-summary.json',{'status':'INCOMPLETE_FROZEN_STOP','diagnostic_status':'QUALIFIED','overhead_status':'QUALIFIED','separate_status':'BLOCKED_EVIDENCE','main_status':'MISSING','accounting':analysis['accounting'],'primary_timing_adopted':False,'performance_pass':False,'official_gate_pass':False})
readme='''G5-UR0b resume-r3：FROZEN_VERIFICATION_FAILURE，返回 CONTROL_REVIEW。

已完成 fresh mechanism proof、32/96 与 64/192 MiB 精确 artifact/provenance 绑定、诊断 OFF/ON 和完整六对 AB/BA 开销资格验证。资格结果 QUALIFIED：p95 配对中位增量 0.05ms，小于 1ms 原门槛；wall 与 OFF MAD 规则通过。这只建立本 cohort 的观测开销资格，不是产品性能 PASS。

60Hz 实时输入 worker 正常结束、零丢样、零身份错误，schema VALID，但 canonical 对账为 raw accepted 2 / qualified decoded 1 / metric receipt 1。缺失的一条是首个 Operation 之前的 generation/revision 0 初始化帧（completion seq47，draw15/provider32/capture42）。当前编译链要求每条 accepted completion 对应已提交的 Scene/Operation，初始化帧没有这条观测依据；不能伪造 Operation，也不能把它静默删除以宣称通过。具体数据在 control-and-proofs/terminal-witness.json。

冻结规则已关闭整个窗口：新 worker 15 个实际运行，14 个准入通过，1 个 proof blocked，38 个 MISSING。历史 597 秒 + 本次逐 worker 向上取整 536 秒 = 1133 秒。剩余额度不授权重试。120/240Hz、六类 correctness 和30行主矩阵未执行。全部15个guard正常退出且清理完成，未触发超时、RSS或进程清理失败。

本轮只改 runner guard/accounting 与测试，未改 Runtime/SDK/Brush/collector/compiler/fixture/锁/阈值。回归：87 Node、40 Python、22 原生、120 boundary。旧结果与585个历史bundle文件逐一复核保持原样。64/192产物保留f6659200 dirty=true构建身份；32/96是f651523a dirty=false的Axiom consumer构建；二者与当前orchestration源码身份分开记录。

实现阶段停止，不提交/推送，不重开本窗口，不进入UR0c/UR1，不签发P34或性能PASS。下一步由独立控制面/Verification评审初始化帧的观测依据与准入范围，随后在明确授权下修复/离线复验；任何新采样需另行核算和授权，不能把本次失败改成成功。
'''
(BUNDLE/'README.md').write_text(readme)
files={str(p.relative_to(BUNDLE)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(BUNDLE.rglob('*')) if p.is_file()}
write(BUNDLE/'manifest.json',{'schema':'g5-ur0b-r3-evidence-manifest-v1','task':'G5-UR0b-resume-r3','status':'FROZEN_VERIFICATION_FAILURE','package_sha256':runner.PACKAGE_SHA,'source_revision':source['revision'],'source_tree_sha256':tree,'files':files,'performance_pass':False,'official_gate_pass':False})
result={'schema_version':1,'task_id':'G5-UR0b-resume-r3','parent_task_id':'G5-UR0b','stage':'P33-r3','status':'FROZEN_VERIFICATION_FAILURE','return_surface':'CONTROL_REVIEW','starting_revision':'f651523a40c9b383b7d8e2115fcd32f75ddc38a7','result_revision':source['revision'],'source_tree_sha256':tree,'dirty':True,'dirty_source_paths':git('diff','--name-only').decode().splitlines(),
 'package_ref':ref(runner.PACKAGE),'mechanism_proof_ref':ref(RESULT/'mechanism-proofs.json'),'terminal_analysis_ref':ref(RESULT/'terminal-analysis.json'),'terminal_witness_ref':ref(RESULT/'terminal-witness.json'),
 'materialized_ref':{'kind':'shared-controlled-worktree-content-addressed-bundle','path':str(BUNDLE),'manifest_sha256':sha(BUNDLE/'manifest.json')},
 'completed':['exact authority/ancestry and historical debit','durable single-use reserve/claim/stop ledger','fresh mechanism regressions','two exact budget artifacts and provenance/export/install boundaries','diagnostic OFF/ON with positive canonical proof','one full six-pair AB/BA overhead cohort QUALIFIED','realtime60 browser execution and guard cleanup'],
 'blocker':{'class':'FROZEN_VERIFICATION_FAILURE','area':'VERIFICATION_INITIALIZATION_CANONICAL_CHAIN','run_id':'realtime-60','detail':'raw accepted2 vs qualified decoded1/metric receipt1; unmatched generation-zero pre-operation initialization frame','owning_followup':'Independent control/Verification review; compiler/observation scope repair is outside this package','not_established':'New product rendering/performance defect'},
 'validation_results':load(RESULT/'regression.json'),'accounting':analysis['accounting'],'guard_summary':analysis['guard_summary'],'phase_counts':analysis['phase_counts'],
 'uncompleted':['realtime120 and240','six independent Web correctness workers','30 main matrix rows','complete UR0b evidence and independent control acceptance'],'history_preservation':historical,'official_gate_pass':False,'performance_pass':False,'primary_timings_adopted':False,'continue_execution':False,'automatic_retry':False,'committed':False,'pushed':False,
 'notification_policy_ref':ref(ROOT/'.aegis/results/G5-UR0b/control-review-resume-r3-checkpoint/notification-policy.md')}
write(RESULT/'result.json',result);(RESULT/'README.md').write_text(readme+'\nBundle: '+str(BUNDLE)+'\n')
checks={'source_tree_unchanged_since_proof':source_identity.identity(ROOT)['source_tree_sha256']==tree,'bundle_hashes_match':all(sha(BUNDLE/n)==entry['sha256'] for n,entry in files.items()),'all_53_ids_accounted':len(window['rows'])==53 and analysis['accounting']['new_started']+analysis['accounting']['new_missing']==53,'budget_charged_exactly':window['charged_seconds']==1133,'all_owned_processes_gone':load(RESULT/'cleanup-verification.json')['status']=='ALL_OBSERVED_PROCESSES_GONE','closed_window_preserved':load(RESULT/'window.json')['status']=='CLOSED','old_returns_and_raw_preserved':all(h['all_bytes_unchanged'] for h in historical),'no_product_gate_or_retry_claim':not result['official_gate_pass'] and not result['continue_execution'] and not result['performance_pass']}
assert all(checks.values()),checks
write(RESULT/'validation.json',{'status':'FROZEN_RETURN_MATERIALIZED_AND_VERIFIED','checks':checks,'bundle_file_count':len(files),'manifest_sha256':sha(BUNDLE/'manifest.json'),'result_sha256':sha(RESULT/'result.json')})
write(RESULT/'output-sha256.json',{'result.json':sha(RESULT/'result.json'),'README.md':sha(RESULT/'README.md'),'validation.json':sha(RESULT/'validation.json'),'manifest':{'path':str(BUNDLE/'manifest.json'),'sha256':sha(BUNDLE/'manifest.json')}})
print(json.dumps({'status':result['status'],'result_ref':ref(RESULT/'result.json'),'materialized_ref':result['materialized_ref'],'files':len(files),'source_tree_sha256':tree,'dirty':True,'new_workers':15,'missing':38,'cumulative_charged_seconds':1133}))
