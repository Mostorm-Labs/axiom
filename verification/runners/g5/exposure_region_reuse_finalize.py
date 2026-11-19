from pathlib import Path
import json,hashlib,subprocess,statistics,gzip,math
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');O=R/'out/G5-exposure-region-reuse-r1';E=R/'.aegis/results/G5-exposure-region-reuse-r1';P=R/'.aegis/packages/G5-exposure-region-reuse-r1'
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def ref(p):p=Path(p);return{'path':str(p),'bytes':p.stat().st_size,'sha256':h(p)}
def put(name,v):p=E/name;p.write_text(json.dumps(v,indent=2)+'\n');return ref(p)
assert read(E/'sampling-ledger.json')['stop']['id']=='WH-OPT-C-D2'
freeze=read(E/'browser-freeze.json')
for v in freeze['drivers']+freeze['sources']+list(freeze['SDKs'].values()):assert ref(v['path'])==v
# Source patch is task copies versus exact B, including common instrumentation/tests.
changed=[];patch=[];sourceFiles=[]
for parent in ['runtime','cmake','sdk','apps']:
 for p in (O/'source-C'/parent).rglob('*'):
  if not p.is_file()or p.is_symlink():continue
  n=p.relative_to(O/'source-C');b=O/'source-B'/n
  if not b.exists()or b.read_bytes()!=p.read_bytes():
   changed.append(str(n));sourceFiles.append(ref(p));q=subprocess.run(['diff','-u','--label','a/'+str(n),'--label','b/'+str(n),str(b)if b.exists()else'/dev/null',str(p)],capture_output=True,text=True);assert q.returncode in[0,1];patch.append(q.stdout)
(O/'B-to-C.patch').write_text(''.join(patch));put('source-delta.json',{'status':'ISOLATED_TASKCOPIES_ONLY','baseline':ref(R/'.aegis/results/G5-quality-candidate-continuation-r1/normal-source-C.json'),'changed':changed,'patch':ref(O/'B-to-C.patch'),'sources':sourceFiles,'publicSDKAPI_OperationHistoryBrush_changes':False,'noPrimaryApplyCommitPush':True})
# Full production build input manifests and matching artifacts.
compileFiles={}
for kind in ['normal-C','optical-B','optical-C']:
 b=O/('build-'+kind);cmds=read(b/'compile_commands.json');items=[]
 for c in cmds:
  p=Path(c['file']);items.append({'source':ref(p),'command':c['command'],'directory':c['directory']})
 (O/(kind+'-compile-inputs.json')).write_text(json.dumps(items,indent=2)+'\n');compileFiles[kind]=ref(O/(kind+'-compile-inputs.json'))
put('SDK-source-input-manifest.json',{'normalSource':ref(E/'normal-source-C.json'),'normalSDK':ref(E/'normal-SDK-C.json'),'diagnosticSources':{r:ref(E/f'optical-source-{r}.json')for r in['B','C']},'diagnosticSDKs':{r:ref(E/f'optical-SDK-{r}.json')for r in['B','C']},'actualCompilation':compileFiles,'configurations':{k:ref(O/(k+'-build-steps.json'))for k in['normal-C','optical-B','optical-C']},'normalMacros':'allOFF, qualificationOFF, noWH/private sampling exports','diagnosticMacro':'only AXIOM_CONTINUOUS_ZOOM_OBSERVATION=1; actualsuccessfulpresent commonB/C hook','profileMiB':[512,1024,128],'logicalVersion':'0.4.3.0 unchanged','matchingResources':True})
put('dependency-facts.json',{'status':'LOCKED_PREBUILT_CONSUMER_ONLY','locks':[ref(R/n)for n in['semantic-sdk.lock.json','r1-full-skia-sdk.lock.json']],'WebStoreFacts':ref(R/'out/G5-temporary-retirement-wait-r1/web-environment.json'),'configurations':[ref(O/n)for n in['build-steps-C.json','normal-C-build-steps.json','optical-B-build-steps.json','optical-C-build-steps.json']],'noSemanticSkiaSDKRebuild':True})
# Ordinary absolute and paired p95/settle guards.
l=read(E/'sampling-ledger.json');ordinary=[a for a in l['attempts']if a['mode']=='ordinary'];routes=[]
for dpr in [2,4]:
 for route in ['wheel','touch']:
  B=[next(m for m in read(O/'web'/f'WH-W-D{dpr}-B{repeat}'/'metrics.json')if m['route']==route)['proxy']for repeat in[1,2]]
  bp95=statistics.median(x['gaps']['p95']for x in B);settle=statistics.median(x['settle_ms']for x in B);limit=max(33.4,bp95*1.15+2)
  for repeat in[1,2]:
   c=next(m for m in read(O/'web'/f'WH-W-D{dpr}-C{repeat}'/'metrics.json')if m['route']==route)['proxy'];routes.append({'DPR':dpr,'route':route,'repeat':repeat,'BpairedMedianP95':bp95,'BpairedMedianSettle':settle,'C':c,'p95Limit':limit,'settleLimit':settle*1.15+100,'absolutePass':c['gaps']['maximum']<=100 and all(x['gap_ms']<=100 for x in c['direction_boundaries']),'p95Pass':c['gaps']['p95']<=limit,'settlePass':c['settle_ms']<=settle*1.15+100})
put('browser-comparison.json',{'status':'EIGHT_ORDINARY_COMPLETED_ABSOLUTE_100MS_MET','candidateRoutes':routes,'allAbsolutePass':all(x['absolutePass']for x in routes),'allP95Pass':all(x['p95Pass']for x in routes),'allSettlePass':all(x['settlePass']for x in routes),'pairedMedianStableGuard':[{'DPR':d,'route':t,'Cmedian':statistics.median(x['C']['settle_ms']for x in routes if x['DPR']==d and x['route']==t),'limit':next(x['settleLimit']for x in routes if x['DPR']==d and x['route']==t),'pass':statistics.median(x['C']['settle_ms']for x in routes if x['DPR']==d and x['route']==t)<=next(x['settleLimit']for x in routes if x['DPR']==d and x['route']==t)}for d in[2,4]for t in['wheel','touch']],'p95ProxyWarning':'Multiple GL content writes inside a single frame can yield zero GL-op p95; this is not display-frame p95 or photon latency. Actualsuccessfulpresent qualified separately.','noPerformancePASS':True})
camera=[]
for case in ['WH-OPT-B-D2','WH-OPT-C-D2']:
 camera.append({'id':case,'raw':ref(O/'web'/case/'raw.json'),'metrics':read(O/'web'/case/'metrics.json')})
put('camera-handoff.json',{'status':'FROZEN_33_4MS_CURRENT_CAMERA_GATE_FAILED','rows':camera,'finding':ref(E/'terminal-finding.json'),'absolute100msPassedForCandidate':True,'candidateWheelRealMissIndex':100,'last360FalseNegativeDisclosedNotBlocking':True,'DPR4':'NOT_RUN_AFTER_TERMINAL_GUARD'})
snapshots=[]
for a in l['attempts']:
 dest=O/'web'/a['id'];index=read(dest/'binary-index.json');assert index['files']['final.axsnap']['sha256']=='17f0f5ceaa10b0de2f808dd5deed40e44295a4179a43a388d14dd731cb949fbe';raw=read(dest/'raw.json');assert not raw['operations']and not raw['errors'];snapshots.append({'id':a['id'],'snapshot':index['files']['final.axsnap'],'zeroNavigationOps':True,'closed':raw['closed'],'finalState':raw['final']['state']})
put('final-pixels-state.json',{'status':'ALL10_COMPLETED_SNAPSHOTS_BYTEEXACT_ZERO_NAV_OPERATIONS','rows':snapshots,'nativeOneStepPixels':ref(O/'scalar-pixels.log'),'nativePixelPolicy':'Existing independent2devicepixelAA policy; unchanged nonAA tolerance; DPR2affine12channels/max22 withinAA, maskvariant dpr1/dpr4 one-level rounding disclosed.','currentCaptureRGBA':48,'optical':ref(E/'optical-comparison.json'),'uncompleted':'RemainingDPR4optical and reference-unavailable rows not qualified; no blanketAA/image waiver.'})
put('web-smoke.json',{'status':'DPR2_DPR4_FUNCTIONAL_PASS','ledger':ref(E/'smoke-ledger.json'),'rows':[ref(O/f'smoke-D{d}/result.json')for d in[2,4]],'covers':['publicrootround4draw','duplicate','drag','3partialeraseimmediatepanzoom','UndoRedo','SnapshotthroughCommitfreshrestore','contactsPending0','close','blankexample'],'noAdditionalStarts':True})
# Completed native work versus exact verification obligations; disclose unclosed specifics.
put('native-regressions.json',{'status':'REQUIRED14_PLUS6CAMERA_PLUSMASK_21_PASS','ctest':ref(O/'final-checks.log'),'nativeSource':ref(O/'source-C/runtime/composition/tests/exposure_region_reuse_cache_test.cpp'),'focusedCache':ref(O/'correctness/scalar-format-cache.json'),'cacheLog':ref(O/'scalar-format-cache.log'),'oneStepPixels9':ref(O/'scalar-pixels.log'),'boundedService':ref(O/'final-bounded-service.log'),'frontierFairness':ref(O/'fair-preempt-fairness.log'),'samePresentBeforeFinalSchedulingCorrection':ref(O/'same-present.log'),'densityAttempts':[ref(O/f'density{i}.log')for i in[2,3]],'node6':ref(O/'all-node-tests.log'),'pythonSyntax':ref(O/'python-syntax.log'),'boundary122':ref(O/'closure-boundary.log'),'limits':'Density attempts under concurrentbuild had failures; serial density3 convergedDPR2/4. Finalsource sparse samepresent/density was not rerun after lastpreemption correction. Dedicated fine-line64block seam oracle and complete mixed interleave projection matrix not separatelyclosed; terminalWebguard preventsready claim.'})
put('reuse-and-region-work.json',{'status':'ACTUAL_CANONICAL_SHARED_FULLBODY_AND_FIXEDREGION_CONSUMER_IMPLEMENTED','native45':ref(O/'correctness/scalar-format-handoff.json'),'early4':ref(E/'early-ledger.json'),'bodyProducer':'SkiaRenderCache->sameSkiaInkLayers fullsupport immutableN32 white coverage using existingCompactStrokeRasterJob32primitive advances/canonical allowance','consumer':'SkiaNavigationTiles projects readycompletebody withoutupscale, orderedperobjectmask/material; no selfsnapshotcopy; geometry fallback unchanged','regionIdentity':'parent256+region1..16 fixed64interior/gutter2, query/cache/observer/publication identity','dirtyBootstrap':'Fullcurrentviewport mustcover.1active+1suspended; fulllegacyqualitydoesnotownfrontier merelybyoverlap','earlyCausalFindings':['wholequalityjobregion0 heldexposure','WebsRGBprovider versus scalarN32 whitebodyformatmiss'],'geometrySourceUnchanged':True,'noUnknownPixels':True})
put('coverage-lifetimes.json',{'status':'FOCUSED_PHASE_FORMAT_GPULEASE_AND_RESOURCEBASIS_VERIFIED','testSource':ref(O/'source-C/runtime/composition/tests/exposure_region_reuse_cache_test.cpp'),'testResult':ref(O/'correctness/scalar-format-cache.json'),'cacheLimits':'combinedtiles+navigationBodies+fullBodies<=min1024,options.maxEntries;combinedoptionalbytes,perEntry;sharedResourceBudget unchanged','staging':'separatekScratchtemporary lease reset into realGPUepochpending; residentgeometrypixelpins held; nofenceproduction','bodyMaskSeparation':'snapshotbeforemask; independentresult scratch/currentorderedhistories','caveat':'nofullER4platformopticalornewseammatrixqualification'})
# Actual preservation, including primary dirty lineage.
fixed=read(P/'fixed-inputs.json');primary=read(R/fixed['primary_preservation']['path']);errors=[]
for n,v in primary.items():
 if not(R/n).exists()or h(R/n)!=v:errors.append(n)
oldSDK=[]
for v in fixed['SDK_preservation']:
 m=read(R/v['path']);bad=[n for n,s in m['files'].items()if h(Path(m['root'])/n)!=s];oldSDK.append({'manifest':ref(R/v['path']),'changed':bad});assert not bad
assert not errors
put('preservation.json',{'status':'PRIMARY1083_AND_OLD_SDKS_UNCHANGED','primaryFiles':len(primary),'changed':errors,'oldSDKs':oldSDK,'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'branch':subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip(),'oldPIDs':[20503,20508,91128,97416],'noOldPagesAccessedRefreshed':True,'userDocumentsPreserved':True,'newDemoPort':4340})
ledgers={'early':read(E/'early-ledger.json'),'functional':read(E/'smoke-ledger.json'),'formal':l};cleanup=[]
for name,ledger in ledgers.items():
 for a in ledger['attempts']:
  assert a['guard']['cleanup_complete']and not a['guard']['remaining_owned_pids'];cleanup.append({'pool':name,'id':a.get('id',a.get('DPR')),'cleanup':True,'chargedSeconds':a['charged_seconds'],'peakRSSKiB':a['guard']['peak_process_family_rss_kib']})
put('cleanup.json',{'status':'ALL16_BROWSER_FAMILIES_CLEANED_PREVIEW_RETAINED','attempts':cleanup,'spend':{n:{'starts':len(v['attempts']),'chargedSeconds':v['charged_seconds'],'cap':v['cap_seconds']}for n,v in ledgers.items()},'remainingUnexecuted':l['remaining'],'preview':read(E/'preview-ready.json'),'oldPIDsPreserved':True,'newOracleStarts':0})
put('raw-refs.json',{'formal':[{'id':a['id'],'files':[ref(p)for p in(O/'web'/a['id']).iterdir()if p.is_file()]}for a in l['attempts']],'early':[{'id':a['id'],'raw':ref(O/'early'/a['id']/'raw.json'),'worker':ref(O/'early'/a['id']/'process.worker.json')}for a in ledgers['early']['attempts']],'unexecuted':l['remaining'],'offlineReferenceReuse':ref(O/'reference-reuse-candidates.json')})
ready=read(E/'preview-ready.json');ready['status']='EXPERIMENTAL_TRIAL_RETAINED_TERMINAL_33_4MS_HANDOFF_BLOCKER';ready['formalOrdinary']='8completed,Cmax34..50ms';ready['terminalFinding']=ref(E/'terminal-finding.json');put('preview-ready.json',ready)
doc=R/'docs/planning/G5_EXPOSURE_REGION_REUSE_20261117.md';doc.write_text('''# G5 exposure region reuse r1\n\n隔离实现位于 out/G5-exposure-region-reuse-r1/source-C，主生产工作树没有应用这些修改。\n\n已实现 fixed64 子块及2texel gutter、完整区域发布与显式region身份、Query callback成本边界、canonical/native共享的本体coverage、独立mask结果、相位/格式/lease和预算校验。Web sRGB目标与标量白coverage格式已收敛，整块quality任务不再以AABB重叠阻塞区域frontier。\n\n原49对象native45/48新present；21指定相关回归通过；DPR2/4公共SDK功能通过。正式ordinary8全部执行，候选最长GL内容提交空窗约34–50ms，配对基线约1133ms；GLop p95不是显示帧p95。\n\n终态 FROZEN_WEB_HANDOFF_OR_QUALITY_FAILURE：DPR2光学wheel input100后47.7ms才出现新相机，未满足33.4ms窗口。停止剩余2个DPR4光学starts。离线完整比较另发现DPR4稳定恢复配对中位数门槛失败：wheel约1082ms、touch约860ms，限额约558/573ms。最后input360的分析截断误报另列，不作为阻断根因。48当前画面RGBA已保存，14个完全匹配旧独立reference采样可测且与基线一致，34不可用。没有签整体Gate/P34/performance PASS。\n\n试用 http://127.0.0.1:4340/example.html，PID44610，仍属实验版。旧4336–4339和文档保持。未commit/push。精确结果见 .aegis/results/G5-exposure-region-reuse-r1/result.json。\n''')
# Review inventory excludes dependency/checkout duplication and live server log.
paths=[p for p in E.rglob('*')if p.is_file()and p.name not in['result.json','artifact-manifest.json']]
for p in O.rglob('*'):
 if not p.is_file()or p.is_symlink():continue
 rel=p.relative_to(O)
 if any(part.startswith(('source-','build-','install-'))for part in rel.parts):continue
 if p.name in['preview-server.log','finalize.log']or'__pycache__'in rel.parts:continue
 paths.append(p)
for n in ['normal-C','optical-B','optical-C']:
 for p in(O/('install-'+n)/'js').rglob('*'):
  if p.is_file():paths.append(p)
for parent in ['verification/runners/g5','verification/tests']:
 paths+=list((R/parent).glob('exposure_region_reuse*'if'runners'in parent else'g5_exposure_region_reuse*'))
paths+= [doc]+[Path(v['path'])for v in sourceFiles]
manifest=put('artifact-manifest.json',{'schema':'samehost-hashbound-evidence-v1','files':[ref(p)for p in sorted(set(paths))if p.is_file()],'excluded':['livepreviewlog','dependencies','buildintermediates','duplicatecheckouts'],'sourceIndexes':[ref(E/n)for n in['normal-source-C.json','optical-source-B.json','optical-source-C.json']],'compressedEvidenceMiB':sum(p.stat().st_size for p in(O/'web').rglob('*')if p.is_file())/1024**2})
result={'task':'G5-exposure-region-reuse-r1','status':'BLOCKED','classification':'FROZEN_WEB_HANDOFF_OR_QUALITY_FAILURE','continue_execution':False,'package':ref(P/'package.json'),'repository':'Mostorm-Labs/axiom','branch':'codex/g5-cpu-worker','HEAD':'6df8e16c1b1e45654532d2eb4cb8513f335c7a39','worktree':str(R),'materialized_ref':{'provider':'shared-controlled-worktree','repository':'Mostorm-Labs/axiom','root':str(R),'path':'.aegis/results/G5-exposure-region-reuse-r1/result.json'},'obligations':{'ER1':'Queryboundary+oldREDbound','ER2':'Actualnative45/48andsharedproducerconsumer; targetregressionsclosed, specificseam/mixedprojectionmatrixincomplete','ER3':'Ordinary100msall8pass; DPR4pairedmedianstableguardfails; diagnostic33.4mswindowfailswheel100','ER4':'Nativepixels/functionalPASS; partialD2opticalonly;DPR4notrun','ER5':'IsolatedmatchingSDK/hashboundresult/4340preserved'},'achieved':{'nativePresents':45,'nativeCalls':48,'requiredMinimum':42,'noOperationLostTargetAssertions':True,'ctest':{'passed':21,'total':21},'functionalDPR2DPR4':True,'ordinaryStarts':8,'CmaximumGapRangeMs':[34,50],'BmaximumGapApproximatelyMs':1133,'zeroNavigationOpsAndSnapshotByteExactAll10':True},'blocker':ref(E/'terminal-finding.json'),'additionalBlocker':'DPR4 paired median stable recovery exceeds Bmedian*1.15+100ms: wheelCmedian1081.55ms>558.22ms,touchCmedian859.7ms>573.11ms; discovered during terminal offline fullcomparison',
'ordinaryPerformanceOverallPass':False,'browserComparison':ref(E/'browser-comparison.json'),'cameraHandoff':ref(E/'camera-handoff.json'),'optical':ref(E/'optical-comparison.json'),'sourceDelta':ref(E/'source-delta.json'),'SDKsource':ref(E/'SDK-source-input-manifest.json'),'driverFreeze':ref(E/'browser-freeze.json'),'spend':{n:{'starts':len(v['attempts']),'chargedSeconds':v['charged_seconds'],'cap':v['cap_seconds']}for n,v in ledgers.items()},'cleanup':ref(E/'cleanup.json'),'preservation':ref(E/'preservation.json'),'unexecuted':l['remaining']+['DPR4opticalpixels/referenceprofiles','remaining32D2independentposeReferences','dedicated64gutterline/shapeGPUseamoracle','completefullbodyprojectioninterleavedmixed/qualitymatrix','final-source samepresent/density rerun'],'preview':{'URL':ready['URL'],'PID':ready['PID'],'leftRunning':True,'status':ready['status']},'artifactManifest':manifest,'noOfficialGateP34PerformancePASS':True,'verificationRunnerFinding':'Paired p95/stable comparison ran at terminal offline finalization, not online afterordinary8.2D2opticalstarts admitted before discovery ofDPR4stablefailure; allretained/charged. See terminal-finding.json.','primaryProductionNotApplied':True,'noCommitPush':True,'nextOwner':'CONTROL_REVIEW','afterCallback':'STOP'}
put('result.json',result);print(json.dumps(ref(E/'result.json')));print('manifest',json.dumps(manifest))
