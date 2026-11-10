"""Final blocked evidence return. Never starts browsers or changes old results."""
from pathlib import Path
import difflib
import hashlib
import json
import statistics
import subprocess
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[3]
R=Path(__file__).resolve().parent
O=ROOT/'out/G5-erase-exact-canonical-r1'


def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()


def read(p):return json.loads(Path(p).read_text())


def ref(p):
    p=Path(p)
    return dict(path=str(p.relative_to(ROOT)),sha256=sha(p),bytes=p.stat().st_size)


def write(name,value):
    p=R/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(value,indent=2)+'\n');return ref(p)


def require(ok,message):
    if not ok:raise ValueError(message)


def main():
    package=ROOT/'.aegis/packages/G5-erase-exact-canonical-r1/package.json'
    ec=read(package.parent/'execution-contract.json')
    pre=read(R/'PackageBindingPreflight.json')['source_before']
    sources={};patch=[]
    allowed=ec['scope']['allowed_existing']+ec['scope']['allowed_new']+ec['scope']['conditional_scene']+[
        'verification/runners/g5/erase_undo_readiness.mjs','verification/tests/g5_erase_undo_readiness.test.mjs',
        'verification/runners/g5/post_erase_interaction_readiness.mjs','verification/runners/g5/post_erase_interaction_readiness_runner.py',
        'verification/tests/g5_post_erase_interaction_readiness.test.mjs','verification/tests/test_g5_post_erase_interaction_readiness.py']
    for name in sorted(set(allowed)):
        current=ROOT/name
        if not current.is_file():continue
        before=R/'preimages'/name
        if before.exists() and sha(before)==sha(current):continue
        post=R/'new-source-postimages'/name;post.parent.mkdir(parents=True,exist_ok=True);post.write_bytes(current.read_bytes())
        sources[name]={'current':ref(current),'postimage':ref(post),'preimage':ref(before) if before.exists() else None}
        patch.extend(difflib.unified_diff(before.read_text().splitlines(keepends=True) if before.exists() else [],
            current.read_text().splitlines(keepends=True),fromfile='a/'+name if before.exists() else '/dev/null',tofile='b/'+name))
    (R/'source-delta.patch').write_text(''.join(patch))
    product=[name for name in sources if name.startswith('runtime/')]
    pair=R/'local-web/G5EC1-FINAL-PAIRS'
    frozen=read(pair/'freeze.json')['inputs']
    for name,digest in frozen['source'].items():
        if name.startswith(('runtime/','sdk/','cmake/')):require(sha(ROOT/name)==digest,'product changed after sampling: '+name)
    write('source-manifest.json',{'schema':'g5-erase-exact-canonical-source-v1','head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'branch':'codex/geometry-navigation-integration','worktree':str(ROOT),'files':sources,'product_files':product,
        'delta':ref(R/'source-delta.patch'),'accepted_dirty_baseline':ref(R/'PackageBindingPreflight.json'),
        'no_public_SDK_schema_Operation_History_contract_change':True,'product_frozen_through_all_windows':True})
    worker=read(pair/'worker-result.json');require(worker['status']=='OBSERVED' and len(worker['rows'])==6,'final pairing incomplete')
    timings=[];functional=[];pixels=[]
    for row in worker['rows']:
        directory=pair/(row['variant']+'-'+str(row['repetition']));raw=directory/'qualified-row.json'
        require(row['roundtrip']['status']=='QUALIFIED','paired pixel failure')
        phases=[]
        for phase in row['rounds']:
            main=phase['main'];events=phase['events'];after=phase['after'];ops=phase['observation']['colorOps']
            content=[x for x in ops if x['kind']=='content'];after_up=[x for x in content if x['started']>=main['end']]
            phases.append({'round':phase['round'],'main_Up_ms':main['end']-main['start'],
                'next_Down_ms':events[-3]['end']-events[-3]['start'],'tiny_Up_ms':events[-1]['end']-events[-1]['start'],
                'all_Move_ms':[e['end']-e['start'] for e in events if e['phase']=='move'],
                'after_main_Up_first_content_ms':after_up[0]['started']-main['start'] if after_up else None,
                'last_content_after_main_Up_ms':content[-1]['ended']-main['start'] if content else None,
                'raf_callback_wall_ms':sum(x['ended']-x['started'] for x in phase['observation']['rafCallbacks']),
                'content_writes':len(content),'active_contacts':after['activeContacts'],'generation':after['generation'],
                'presented_generation':after['lastCommit']['presentedGeneration'],'pending_at_6s':phase['activity']['pending'],
                'operation_delta':phase['operationDelta']})
            require(after['activeContacts']==0 and after['generation']==after['lastCommit']['presentedGeneration'] and phase['activity']['pending']==0,'paired lifecycle')
        key={'variant':row['variant'],'pair':row['repetition'],'raw':ref(raw)}
        timings.append({**key,'rounds':phases})
        functional.append({**key,'rounds_operation_delta':[p['operation_delta'] for p in phases],
            'hit_Up_then_immediate_wheel':row['functionalHit'],'cancel_noop':True,
            'final':row['final']['state'],'undo':row['undo']['state'],'redo':row['redo']['state']})
        pixels.append({**key,'qualification':row['roundtrip'],
            'final_snapshot':ref(directory/'S2-final.axsnap'),'redo_snapshot':ref(directory/'S2-redo.axsnap'),
            'final_rgba':ref(directory/'S2-final.rgba'),'redo_rgba':ref(directory/'S2-redo.rgba')})
    summary={}
    for variant in ('old','candidate'):
        selected=[x for x in timings if x['variant']==variant]
        ups=[p['main_Up_ms'] for x in selected for p in x['rounds']]
        downs=[p['next_Down_ms'] for x in selected for p in x['rounds']]
        moves=[v for x in selected for p in x['rounds'] for v in p['all_Move_ms']]
        summary[variant]={'all_main_Up_ms':ups,'median_main_Up_ms':statistics.median(ups),'max_main_Up_ms':max(ups),
            'all_next_Down_ms':downs,'median_next_Down_ms':statistics.median(downs),'Move_p50_ms':statistics.median(moves),
            'Move_p95_ms':sorted(moves)[int((len(moves)-1)*.95)],'Move_max_ms':max(moves)}
    write('paired-web-timing.json',{'classification':'ALL3_ACTUAL_PAIRS_RETAINED_PERFORMANCE_FINDINGS','rows':timings,'summary':summary,
        'median_speedup':summary['old']['median_main_Up_ms']/summary['candidate']['median_main_Up_ms'],
        'scope':'Original160Move40copies3roundnormalSDK. NoCPUprofile/readback duringprimary. GLwritesnotGPUcompletion/physicalfeedback.','discarded_samples':[]})
    stress=R/'local-web/G5EC1-STRESS-CONTINUE-R2'
    longrows=[]
    for variant in ('old','candidate'):
        p=stress/(variant+'-undo-1600.json');row=read(p)
        longrows.append({'variant':variant,'raw':ref(p),'main_Up_ms':row['up']['end']-row['up']['start'],
            'successor_Up_ms':row['successor']['up']['end']-row['successor']['up']['start'],
            'feedback_from_T0_ms':row['actionFeedbackAt']-row['up']['start'],
            'dark_to_white_before_Up':row['actionPositivePixels'],'Undo_generation':row['undo']['generation'],
            'Redo_generation':row['redo']['generation'],'classification':'SYNC_UP_BLOCK_AND_LATE_POST_ERASE_FEEDBACK'})
    write('undo-readiness/undo-readiness-candidate.json',{'status':'ACTUAL_UI_LONG1600_OBSERVED','rows':longrows,
        'independent_prefix':ref(R/'undo-readiness/successor-prefix-independent-proof.json'),
        'limitation':'Shared exact/canonical optimization barely improves long1600/nextshortUp. User10sbusy notclaimedresolved.'})
    write('undo-readiness/undo-readiness-localization.json',{'status':'SYNC_RUNTIME_CRITICAL_PATH_NOT_BUTTON_ONLY',
        'baseline':ref(R/'undo-readiness/undo-readiness-baseline.json'),'candidate':ref(R/'undo-readiness/undo-readiness-candidate.json'),
        'async_harness_race':ref(R/'stress-failure-classification.json'),
        'source':'RuntimecanUndo requires noactiveerasertrace/historySceneReady. actualDemo projects publicstate; successor activefalsecanUndoexpected.',
        'missing':'Full10roundterminal proof, stickyactivepixelwitness/table/image scenarios remain unverified.'})
    write('undo-readiness/offline-observer-tests.json',{'logs':[ref(R/'undo-readiness/async-Undo-RED.log'),ref(R/'undo-readiness/async-Undo-GREEN.log'),
        ref(R/'undo-readiness/browser-helper-GREEN.log'),ref(R/'post-erase-readiness/observer-final-offline.log')],
        'browser_dependency_preflight':ref(R/'offline-driver-preflight.json')})
    ready=[];invalid=[]
    for window in ('G5EC1-READINESS-ALL','G5EC1-READINESS-CONTINUE-R1'):
        base=R/'local-web'/window;w=read(base/'worker-result.json')
        for row in w['rows']:
            name=row['variant']+'-'+row['kind'];raw=base/(name+'.json')
            facts={'variant':row['variant'],'kind':row['kind'],'raw':ref(raw),'feedback':row['feedback'],
                'timing':row['timing'],'snapshot_roundtrip':row['snapshotRoundtrip'],'pose_correct':row.get('actionPoseCorrect'),
                'positive_pixels':row.get('actionPositivePixels'),'canonical_generation':row['after']['generation'],
                'presented_generation':row['after']['lastCommit']['presentedGeneration'],'active_contacts':row['after']['activeContacts']}
            if row['status']=='QUALIFIED':ready.append(facts)
            else:invalid.append(facts)
    require(len(ready)==8,'unexpected qualified readiness count')
    for filename,value in [
        ('post-erase-action-matrix.json',{'status':'PARTIAL_8_OF_11_QUALIFIED','qualified':ready,'invalid_original_rows':invalid,
            'missing':['candidateSticky actualcorrectactiveROI','candidateTable','candidateImage'],
            'invalid_sticky_reference':ref(R/'sticky-witness-classification.json'), 'invalid_zoom_early_probe':ref(R/'readiness-failure-classification.json')}),
        ('active-feedback-pixel-proof.json',{'status':'PARTIAL_NOT_FULLY_VERIFIED','qualified':ready,
            'oldCPU_prefix':ref(R/'undo-readiness/successor-prefix-independent-proof.json'),
            'analytic_nonink':'Preseedblue rectangle expectedcamera position; rectangle newfootprint outsideold actualblue +translation80/30. FullcanonicalUndoRedo equality.',
            'missing':['Stickytyped200x200/yellowactiveROI:invalidtest','Table/image:notexecuted'],
            'no_claim':'Finalpixels/transform cannotbackfillmissingactivefeedback; genericFBOwrite notproof.'}),
        ('admission-commit-presentation.json',{'qualified_rows':ready,'paired':functional,'invariants':'Nooperationforpan/zoom; create/drag publiccommit; finalRedo fullcanonicalobjects/masks/manifest/digest equal. rawIDs/ordinalsretain.',
            'missing_stress_terminal':'8of10 only; nofinalstressreceipt/pixels'}),
        ('interaction-recovery-timing.json',{'policy_ref':ref(ROOT/'.aegis/results/G5-erase-exact-canonical-control-r1/timing-target-3s-5s-supersession.json'),
            'time_origin':'PredecessorlongUpdispatchstart includingblocking','qualified_rows':ready,'long_old_candidate':longrows,
            'scope':'FirstROIcorrectobservation upperbound, notphysicalinput-to-photon. Oldfixedearlyprobe markedinvalid preserved.'})]:
        write('post-erase-readiness/'+filename,value)
    write('web-functional.json',{'status':'PRIMARY3PAIRS_QUALIFIED_WHOLE_CLOSURE_INCOMPLETE','primary_rows':functional,
        'snapshot_checks':ref(R/'final-paired-snapshot-checks.json'),'pixels':pixels,
        'readiness':ref(R/'post-erase-readiness/post-erase-action-matrix.json'),
        'continuous_stress':ref(R/'stress-timeout-classification.json')})
    write('idle-and-progress.json',{'primary':{'all6runs_idle6s_generation_presented_pending0':True},
        'readiness':ready,'longUI':longrows,'continuous_stress':ref(R/'stress-timeout-classification.json'),
        'classification':'Eightof10roundscompletedbeforeguard, noindependentbrowsercrashproof. Monotonicgrowthafterround4 observed.','video10s_resolved':False})
    write('receipt-history.json',{'primary6':functional,'canonical_full_roundtrips':ref(R/'final-paired-snapshot-checks.json'),
        'readiness':ready,'stress_limit':'Full10roundterminalHistory/pixelreceipt missing. All8recordedroundscontactsclosed/knownoperations retained.'})
    write('commit-history-scene.json',{'source_path':'InteractionpartialEraseTargets -> perobjectID -> prepareRecord -> publicationreservation -> OperationEngine -> finalize/observer -> Scene unchanged',
        'actual_native_work':ref(R/'A-runtime-work.log'),'frozen_old':ref(R/'baseline-native-run.log'),
        'canonical_web':ref(R/'final-paired-snapshot-checks.json'),'regression56':ref(R/'final-native-junit.xml'),
        'invariant':'PerobjectmaskIDs/orderedpayload remain canonical; derivedgeometry equality alone ignoresID.'})
    write('identity-invalidation.json',{'status':'EXACT_NATIVE_FOCUSED_GREEN','proofs':[ref(R/'A-finite-focused.log'),ref(R/'A-history-focused.log'),ref(R/'A-172-run.log'),ref(R/'A-exact-parity.json')],
        'prefix_caveat':'Prefixcomparesallfloatcoords/radii/completegeometry; unknownowner/forks fallback; sequentialOporder retained. 1600adversarialnonlinearcontroloriginalInvalid preserved.'})
    write('lease-accounting.json',{'status':'NATIVE_RESOURCE_KEEPERS_GREEN','native56':ref(R/'final-native-junit.xml'),
        'prefix8001600':ref(R/'prefix-cost-oracle.log'),'history_local_cap_bytes':8*1024*1024,'desktop_mib':[256,512,64],
        'body_snapshot':'Onlyper-tileownedsurface/snapshot, nofullscratchseed/COW. TilebuildingRasterJobretainedandcharged. GPUepochsretireatrealcompletion.',
        'limitations':'PathOps internaltemporarymemory is conservatively reserved; SDK/browserwholeRSSnotclaimedexact. Costfulltracecopy remainsmeasured.'})
    write('B-pixel-parity.json',{'status':'SCOPED_NATIVE_PIXELS_AND_PRIMARY_WEB_QUALIFIED','newmask_only':ref(R/'B-bounded-work.log'),
        'mask_sorted_insertion_locality':ref(R/'B-region-work.log'),'native56':ref(R/'final-native-junit.xml'),
        'desktop_area30':'out/G5-object-interaction-continuity-r2/build-native/runtime/render/desktop-budget-area/dense-mixed-counters.json',
        'primary6pixels':pixels,'no_candidate_difference_band':True,'whole_readiness':'Partial; table/image andstickyactive notqualified.'})
    write('B-progress-and-recovery.json',{'status':'NATIVE_KEEPERS_GREEN_CONTINUOUS_WEB_STRESS_INCOMPLETE',
        'regression56':ref(R/'final-native-junit.xml'),'two_initial_keeper_regressions':ref(R/'regression.log'),
        'coldcooperative_fix':ref(R/'B-bounded-keeper.log'),'bounded_tilejob':'32primitiveRasterJob acrossallowance/tilebuilding persists; notdrainedinonehiddenleaf.',
        'locality_limit':'Sortedmaskinsertion affects2of8tiles inproof. AcrossyieldunchangedoldEpochtilesmayresetfully; correctness retained.',
        'capacity':'stabletilecardinalityandbytesgate fallsbackoriginalboundedrasterjob, noquota expansion.',
        'stress_timeout':ref(R/'stress-timeout-classification.json')})
    write('performance-classification.json',{'status':'PERFORMANCE_FINDINGS_LONG_ERASE_NOT_RESOLVED','primary':summary,'longUI':longrows,
        'continuous8round':read(R/'stress-timeout-classification.json'),'readiness':ready,
        'policy':ref(ROOT/'.aegis/results/G5-erase-exact-canonical-control-r1/timing-target-3s-5s-supersession.json'),
        'attribution':{'proven':'Nativegeometry-equivalenthistory/memo reducesrepeatedexact; Bmask-onlyresidentbodycoverage0 andlocalreplay reduction.',
            'not_proven':'NativegainsdidnotturnintomeaningfulnormalWeblong1600gains. Whole40dense deviceworking-set canexceed512tiles; stablegatekeepsfallback. Longsequentialqueryunionstillcostly, history8MiBmaydeclineplans; exactmaincauseinactualWebnotinstrumented.',
            'no_foreign_symbols':'Rejected6789indexmapneverusedasidentity.'},
        'missing_functional_is_not_noise':True,'official_G5_P34_performance_PASS':False})
    junit=ET.parse(R/'final-native-junit.xml').getroot();require(junit.attrib['tests']=='56' and junit.attrib['failures']=='0','native56')
    final_native=read(R/'final-native-source-binding.json')
    require(final_native['test_exit']==0,'final source focused checks failed')
    write('regressions.json',{'status':'56_CHECKPOINT_KEEPERS_PLUS_FINAL_AFFECTED10_GREEN_SCOPE_NOT_FULLY_VERIFIED','native':{'tests':56,'failures':0,'junit':ref(R/'final-native-junit.xml'),'log':ref(R/'final-native.log'),
        'source_limit':'Before finalAcommontransformprefix/temporaryquota changes; full56 is checkpoint, not claimed all56 onlastsource.'},
        'final_source_binding':ref(R/'final-native-source-binding.json'),'final_affected10':ref(R/'final-source-focused-junit.xml'),
        'Node':ref(R/'final-offline-node.log'),'Python':ref(R/'final-offline-python.log'),'boundaries120':ref(R/'final-boundary.log'),
        'diff_check':ref(R/'final-diff-check.log'),'native_initial_failure':ref(R/'regression.log'),
        'exact634':ref(R/'A-original634.json'),'finite_and_unknownowner':ref(R/'A-finite-focused.log'),
        'old55':ref(ROOT/'.aegis/results/G5-UR2d-selection-up-continue-r1/selection-parity.json'),
        'new172':ref(R/'A-172-run.log'),'prior_broad_baseline_failures':'Retained as parentauthority, noallprojectbuildandnotwaived.'})
    ledger=read(R/'sampling-ledger.json');require(ledger['charged_seconds']==851,'ledger changed')
    write('cleanup.json',{'status':'ALL8_OWNED_ATTEMPTS_CLEAN_USER_DEMOS_PRESERVED','attempts':[
        {'id':x['id'],'status':x['status'],'charged_seconds':x['charged_seconds'],'reason':x['guard']['reason'],
         'cleanup_complete':x['guard']['cleanup_complete'],'remaining_owned_pids':x['guard']['remaining_owned_pids']} for x in ledger['attempts']],
        'charged_seconds':ledger['charged_seconds'],'combined_cap_seconds':1200,'no_more_sampling_IDs':True,
        'final_demo':ref(R/'final-demo.json'),'preserved_demo_ports':[4318,4317,4316,4298,4297],
        'user_documents_untouched':True,'no_commit_push':True,'index_empty':not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=ROOT,text=True).strip(),
        'live_logs_excluded':['out/G5-erase-exact-canonical-r1/demo-server.log','out/G5-erase-exact-canonical-r1/final-demo-server.log']})
    write('blocking-items.json',{'status':'BLOCKED_EVIDENCE','items':[
        {'obligation':'EC-B2/EC-PERF','blocker':'CONTINUOUS_STRESS_INCOMPLETE','actual':'8of10roundsterminatedby151sownedguard; nofinalstresspixels/History/receipt'},
        {'obligation':'EC-READY-ALL/CORRECT','blocker':'INVALID_STICKY_WITNESS','actual':'testreadunsupportedwidth0 andwrongbluematerial; activeROIinvalid; finalcorrectnotreplacement'},
        {'obligation':'EC-READY-ALL/CORRECT','blocker':'UNRUN_TABLE_IMAGE','actual':'candidateTable/Image neverexecutedafterlastcontrolledhardstop'}],
        'performance_separate':'LongUp4.13s/nextshortUp9.13snotresolved;3/5targetssupersededhardstopandarefindings.',
        'no_independent_product_crash_proven':True,'remaining_action_owner':'Control classify/repairverification beforeauthorizingnewwork; noextrasampling underthispackage.'})
    obligations={
        'EC-A1':['baseline-phase-attribution.json','A-exact-parity.json','A-work-comparison.json','milestone-A.json'],
        'EC-A2':['A-exact-parity.json','identity-invalidation.json','commit-history-scene.json','lease-accounting.json'],
        'EC-B1':['B-canonical-RED.json','B-mask-only-work.json','B-pixel-parity.json','B-progress-and-recovery.json'],
        'EC-B2':['web-functional.json','idle-and-progress.json','receipt-history.json','blocking-items.json'],
        'EC-PERF':['paired-web-timing.json','performance-classification.json','blocking-items.json'],
        'EC-REG':['regressions.json','final-normal-artifacts.json','source-manifest.json','cleanup.json'],
        'EC-UNDO-LONG':['undo-readiness/undo-readiness-baseline.json','undo-readiness/undo-readiness-candidate.json','undo-readiness/undo-readiness-localization.json'],
        'EC-READY-ALL':['post-erase-readiness/post-erase-action-matrix.json','post-erase-readiness/active-feedback-pixel-proof.json','blocking-items.json'],
        'EC-READY-3S_SUPERSEDED':['post-erase-readiness/interaction-recovery-timing.json','performance-classification.json']}
    write('obligation-index.json',{'status':'RESOLVABLE_WITH_EXPLICIT_MISSING_PROOFS', 'obligations':{k:[ref(R/n)for n in v]for k,v in obligations.items()},
        'aliases':{'normal-artifacts.json':ref(R/'final-normal-artifacts.json'),'demo.json':ref(R/'final-demo.json')},
        'control_supersession':ref(ROOT/'.aegis/results/G5-erase-exact-canonical-control-r1/timing-target-3s-5s-supersession.json')})
    external=[]
    for p in sorted(O.rglob('*')):
        if p.is_file() and not p.name.endswith('server.log'):external.append(ref(p))
    for base in [ROOT/'out/G5-object-interaction-continuity-r2/build-native/runtime/render/desktop-budget-area']:
        external.extend(ref(p)for p in sorted(base.rglob('*'))if p.is_file())
    write('external-artifacts.json',{'files':external,'notes':'Frozenoldarchives/exactbinaries/nativeexpected/oracle/SDKbytes. Buildnative shared source remains externaltool outputs; explicit hash identitieshere.'})
    manifest_files=[p for p in sorted(R.rglob('*'))if p.is_file() and p.name not in('artifact-manifest.json','result.json') and '__pycache__'not in p.parts]
    write('artifact-manifest.json',{'schema':'g5-erase-exact-canonical-artifacts-v1','files':[ref(p)for p in manifest_files],
        'external':ref(R/'external-artifacts.json'),'excludes':['artifact-manifest.json self','result.json callbackhashowner','__pycache__','allrunningDemoHTTPlogs']})
    write('result.json',{'schema':'g5-erase-exact-canonical-result-v1','status':'BLOCKED_EVIDENCE','verification_state':'NOT_FULLY_VERIFIED',
        'package':ref(package),'supplements':[ref(ROOT/'.aegis/packages/G5-erase-undo-readiness-supplement-r1/package.json'),ref(ROOT/'.aegis/packages/G5-post-erase-interaction-readiness-r1/package.json')],
        'result_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'materialized_ref':{'provider':'shared-controlled-worktree','repository':'Mostorm-Labs/axiom','root':str(ROOT),'path':str(R.relative_to(ROOT))},
        'source':ref(R/'source-manifest.json'),'delta':ref(R/'source-delta.patch'),'manifest':ref(R/'artifact-manifest.json'),
        'SDK':ref(R/'final-normal-artifacts.json'),'driver':ref(ROOT/'verification/runners/g5/erase_exact_canonical.mjs'),
        'readiness_driver':ref(ROOT/'verification/runners/g5/erase_undo_readiness.mjs'),'obligation_index':ref(R/'obligation-index.json'),
        'blocking_items':ref(R/'blocking-items.json'),'ledger':ref(R/'sampling-ledger.json'),'cleanup':ref(R/'cleanup.json'),
        'performance':ref(R/'performance-classification.json'),'final_normalDemoURL':read(R/'final-demo.json')['url'],
        'native_checkpoint_tests':56,'final_source_affected_tests':10,'final_native_binding':ref(R/'final-native-source-binding.json'),
        'primary_pairs':3,'readiness_qualified_unique_rows':8,'readiness_required_rows':11,
        'continuous_rounds_observed':8,'continuous_rounds_required':10,'allrawfailures_retained':True,
        'no_commit_push':True,'official_G5_P34_performance_PASS':False})
    for x in read(R/'artifact-manifest.json')['files']:require(sha(ROOT/x['path'])==x['sha256'],'manifest changed')
    print(json.dumps({n:ref(R/n)for n in ['result.json','source-manifest.json','source-delta.patch','artifact-manifest.json','final-normal-artifacts.json','sampling-ledger.json']},indent=2))


if __name__=='__main__':main()
