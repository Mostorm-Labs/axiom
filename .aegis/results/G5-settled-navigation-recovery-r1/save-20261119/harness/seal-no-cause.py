from pathlib import Path
import json,hashlib,subprocess,datetime,os,importlib.util
W=Path(__file__).resolve().parents[2];O=W/'out/G5-settled-navigation-recovery-r1';E=W/'.aegis/results/G5-settled-navigation-recovery-r1';P=W/'.aegis/packages/G5-settled-navigation-recovery-r1';B=Path('/Users/qing/.codex/worktrees/g5-erase-feedback-frame')
def ref(p):p=Path(p);b=p.read_bytes();return {'path':str(p),'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def write(n,x):p=E/n;p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n');return ref(p)
assert ref(P/'package.json')['bytes']==3238 and ref(P/'package.json')['sha256']=='04096ca6b1498ce9888228f22e0ca754374083c6fac5369106024542a2fdb32b'
causal=json.loads((E/'causal-classification.json').read_text());assert causal['status']=='ROOT_CAUSE_NOT_ESTABLISHED'and not causal['productionRepairSelected']
ledger=json.loads((E/'sampling-ledger.json').read_text());assert len(ledger['attempts'])==5 and ledger['chargedSeconds']==590
assert [a['status']for a in ledger['attempts']]==['COMPLETED','COMPLETED','COMPLETED','FAILED','COMPLETED']
assert all(a['guard']['cleanup_complete']and not a['guard']['remaining_owned_pids']for a in ledger['attempts'])
integrity=json.loads((E/'diagnostic-integrity.json').read_text());assert len(integrity['rows'])==16 and integrity['requiredDataDropped']==0
progress=json.loads((E/'preparation-progress-qualified.json').read_text());assert len(progress['rows'])==4 and sum(x['zeroUnexplainedAttempts']for x in progress['rows'])==0
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=W,text=True).strip();branch=subprocess.check_output(['git','branch','--show-current'],cwd=W,text=True).strip();assert head=='c3a84c289d5b3dcb258fcbe7f481febf227df0c0'and branch=='codex/g5-settled-navigation-recovery'
assert not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=W,text=True).strip()
(E/'README.md').write_text('''# 稳定后导航恢复：因果未确立

本轮按冻结任务包完成四个B诊断模式：原顺序同页、每case新context/page、逆序同页、原顺序私有progress。第四模式第一次因pretty JSON文件达到64MiB限制失败，原始partial/guard/error保留；唯一允许的harness修正只改为无损紧凑JSON，在独立controller-v2目录保留全部事件字段，完成同角色的替代窗口。没有降低采样上限或删事件。

最终返回 BLOCKED / ROOT_CAUSE_NOT_ESTABLISHED。旧EFF-W-C1稳定后平移5.125秒失败仍未关闭；四种B模式的稳定后平移收敛约428/448/531/545ms，均未复现旧9回调/2秒的稀疏模式。不能据此归因为环境噪声、证明旧测量无效或签性能PASS。

新request/callback/lifecycle观察与native游标显示本次的准备有实际进展。最终progress的53次prepare前后配对中52次有work/cursor增长，1次已是cursor40/residents40；没有证实同basis重置循环、orphanWorker或GPU退休死锁。registry销毁和队列计数不证明物理GPU内存即时释放，default窗口无绘制也不代表没有offscreen进度。

没有选择或实现生产行为修复，没有生成normalC/4350，没有执行Cfinal、普通B/C/C/B、C像素或60native候选回归。本包no-cause终态分支不要求用这些窗口碰运气。只保留defaultOFF私有progress诊断和验证辅助代码；HEAD仍为c3a84c，未提交/推送/tag/release。原4348/4349、旧证据、SDK与primary42保留。

同host绝对路径证据由result.json、artifact-manifest.json及其bytes/SHA绑定，原失败和替代窗口都在账本。5次实际启动累计ceil590秒，所有owned family cleanup。40份诊断snapshot缓冲均核对gzip和解压哈希；无新pixel readback。物理触控板、浏览器GPU内部驻留和原慢样本缺失的request/native状态仍不可从新正常样本重构。
''')
write('readiness.json',{'status':'BLOCKED','classification':'ROOT_CAUSE_NOT_ESTABLISHED','branchTaken':'Frozen no_cause_policy','allFourBRoleDiagnosticsCompleted':True,'formatCorrectiveUsed':True,'causeEstablished':False,'productionRepair':False,'SNR2to4Satisfied':False,'newNormalC':False,'previewCallbackRequired':False,'remainingAuthorizedSteps':'exact terminal materialization and callback only; no speculative fix/additional sampling','originalEFF4':'UNMET unchanged'})
# Capture all exact source/driver/raw bindings without certifying entire old history.
refs={}
def add(p):r=ref(p);refs[r['path']]=r
for root in [P,E]:
 for p in root.rglob('*'):
  if p.is_file()and p.name not in ['artifact-manifest.json','result.json','terminal-callback.json','storage-ledger.json','storage-ledger.lock']:add(p)
for current,dirs,names in os.walk(O,followlinks=False):
 dirs[:]=[n for n in dirs if not n.startswith(('build-','install-','source-'))and n!='__pycache__'and not(Path(current)/n).is_symlink()]
 for name in names:
  p=Path(current)/name
  if p.is_file()and not p.is_symlink():add(p)
probe=json.loads((E/'source-B-progress.json').read_text());sdk=json.loads((E/'B-progress-SDK.json').read_text())
for n in probe:add(O/'source-B-progress'/n)
for n in sdk['files']:add(Path(sdk['root'])/n)
for r in json.loads((E/'root-dependency-binding.json').read_text())['files']:refs[r['path']]=r
normal=json.loads((B/'.aegis/results/G5-erase-feedback-frame-r1/C-normal-SDK.json').read_text())
for n in normal['files']:add(Path(normal['root'])/n)
fixed=json.loads((P/'fixed-inputs.json').read_text())
def visit(x):
 if isinstance(x,dict):
  if {'path','bytes','sha256'}<=x.keys():
   p=Path(x['path']);p=p if p.is_absolute()else P/p;actual=ref(p);assert all(actual[k]==x[k]for k in ['bytes','sha256']);refs[str(p)]=actual
  for v in x.values():visit(v)
 elif isinstance(x,list):
  for v in x:visit(v)
visit(fixed)
for r in json.loads((E/'diagnostic-source-delta.json').read_text())['files']:refs[r['path']]=r
manifest=write('artifact-manifest.json',{'task':'G5-settled-navigation-recovery-r1','status':'DIAGNOSTIC_TERMINAL_SHARED_HOST','files':list(refs.values()),'excludes':['self','result','terminal callback','mutable storage ledger/runtime lock','buildarchives explicitly source/SDKbound elsewhere'],'reviewerBoundary':'Same-host shared task worktree exact paths/bytesSHA; noGit/hostedpublicationclaim','rawFailedPreserved':True,'newBrowserStarts':5,'chargedSeconds':590,'noCauseBranch':True})
spec=importlib.util.spec_from_file_location('storage',O/'task-storage.py');storage=importlib.util.module_from_spec(spec);spec.loader.exec_module(storage);quota=storage.update()
result={'schema':'axiom-scoped-implementation-result-v1','task':'G5-settled-navigation-recovery-r1','at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'BLOCKED','classification':'ROOT_CAUSE_NOT_ESTABLISHED','returnSurface':'CONTROL_REVIEW','continue_execution':False,'package':ref(P/'package.json'),'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom','worktree':str(W),'branch':branch,'baseHEAD':head,'currentHEAD':head,'indexEmpty':True,'committedOrPushed':False},'materialized_ref':{'provider':'shared-controlled-worktree','repository':'Mostorm-Labs/axiom','root':str(W),'path':'.aegis/results/G5-settled-navigation-recovery-r1/result.json','artifact_manifest':manifest},'sourceSDKBinding':ref(E/'source-and-SDK-binding.json'),'normalB':ref(B/'.aegis/results/G5-erase-feedback-frame-r1/C-normal-SDK.json'),'sourceB':ref(B/'.aegis/results/G5-erase-feedback-frame-r1/source-C.json'),'probeBSource':ref(E/'source-B-progress.json'),'probeBSDK':ref(E/'B-progress-SDK.json'),'diagnosticDelta':ref(E/'diagnostic-only-delta.patch'),'causalClassification':ref(E/'causal-classification.json'),'terminalIdentity':ref(E/'terminal-identity.json'),'obligations':{
'SNR1':{'status':'FOUR_DIAGNOSTIC_ROLES_COMPLETED_CAUSE_UNESTABLISHED','evidence':[ref(E/'scheduling-timeline.json'),ref(E/'preparation-progress-qualified.json'),ref(E/'lifecycle-and-resources.json'),ref(E/'diagnostic-integrity.json')],'finding':'All16completedcases tail120callbacks, no drops; settledpan0.428–0.545s ratherthanold5.125s. Finalprogress stablejobadvances0..40, no samebasisrestart observed. Newhealthyobservationscannotidentifyoldmissingstate.'},
'SNR2':{'status':'NOT_EXECUTED_NO_CAUSAL_REPAIR_SELECTED','evidence':[ref(E/'diagnostic-source-delta.json')],'finding':'OnlydefaultOFFnativeprogress/lifecycleobservability andverificationhelpers; no productionbehaviorrepair or speculativeroster/bufferpool/schedulerrewrite.'},
'SNR3':{'status':'NOT_EXECUTED_NO_C_CANDIDATE','finding':'No newproductionC ornative60/pixelqualification. Prior4349 correctness evidence preserved, not relabelled asnewPASS.'},
'SNR4':{'status':'NOT_EXECUTED_NO_CAUSE_BRANCH_ORIGINAL_EFF4_UNMET','finding':'No newordinaryqualificationwindows. OldEFF-W-C1settledpanfailure unchanged; no lucky samples or environmentwaiver.'},
'SNR5':{'status':'DIAGNOSTIC_TERMINAL_MATERIALIZED','evidence':[ref(E/'preservation.json'),ref(E/'cleanup.json'),ref(E/'storage-preflight.json'),ref(E/'harness-correction.json'),ref(E/'buffer-audit.json')],'finding':'OldsourceSDK/evidence/listeners/primary42preserved, allactualstarts/failure retained. No newnormalC, previewcallbacknotrequired; terminalcallbackfollows.'}},'observations':{'settledPanDiagnosticCases':[{'run':r['run'],'navigationMs':r['navigationMs'],'convergenceMs':r['metrics']['convergenceMs'],'tailCallbacks':r['tailCallbacks'],'maxTailRequestedToCallbackMs':r['tailRequestDelayMs']['max'],'postInputCallbacks':r['recoveryCallbacks']}for r in json.loads((E/'scheduling-timeline.json').read_text())['rows']if r['run']in selected and r['case']=='settled-pan']if False else causal['fourModes'],'interpretationLimits':['Diagnosticwindowsarecausalobservationsnotordinaryperformanceacceptance.','Generation3committedbefore settledpan; notprovedOperationapplyblock.','Offscreenstep/bodytile/maskprogressindependentfromdefaultwindowwrite.','NewrequestdelaydoesnotreconstructoldunobservedrequestedAt; browserdeliveryvsRuntimeCPUcauseunknown.','Registryclose/retirementfactsnotphysicalGPUreleaseorleakproof.','Native progress detailed body/mask pointee IDs not fullyexposed; observedbasis/cursor/work cannot retrospectivelycertifyoldunavailablejobstate.']},'sampling':{'ledger':ref(E/'sampling-ledger.json'),'starts':5,'fourCompletedDiagnosticRoles':True,'failedFormattingStart':1,'harnessCorrectiveStarts':1,'chargedSeconds':590,'capSeconds':4200,'extraPerformanceRetries':0,'futureQualificationStarts':0},'storage':{'ledger':ref(E/'storage-ledger.json'),'status':quota['status'],'peakRetainedBytes':quota['peakRetainedBytes'],'logicalBytes':quota['logicalBytes'],'retainedCapBytes':quota['retainedCapBytes'],'logicalCapBytes':quota['logicalCapBytes'],'buildSourceSDKExcludedAndHashBound':True},'offlineVerification':ref(E/'offline-verification.json'),'cleanup':ref(E/'cleanup.json'),'preservation':ref(E/'preservation.json'),'preview':{'newNormalCExists':False,'newURL':None,'preservedExistingURL':'http://127.0.0.1:4349/example.html','4350NotCreated':'NoCnormal,plannedpreviewconditionnotreached'},'remaining':{'diagnosticBranchRequiredExecutableItems':[],'blockedReason':'Original scheduling/recovery root cause not established from finite4roleBdiagnostics; noauthorizedproductionguess.','notExecuted':['focusedproductionRED/GREEN','native60candidate','Cfinaldiagnostic','ordinaryB/C/C/B','C20pixels/4CPUrefs/UndoRedo'],'nextRequiresControlScope':True},'claimBoundary':'Producer explicitBLOCKED only; noP34/Gate/overallperformancePASS; no commit/push/tag/release. OriginalEFF4result remainsblocked.'}
rr=write('result.json',result);print(json.dumps({'result':rr,'manifest':manifest,'status':'BLOCKED','classification':'ROOT_CAUSE_NOT_ESTABLISHED','starts':5,'chargedSeconds':590,'storage':result['storage']},ensure_ascii=False,indent=2))
