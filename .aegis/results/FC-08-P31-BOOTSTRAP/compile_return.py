"""Compile the blocked bootstrap return from actual preflights/source reads.

This is evidence tooling, not a product runner or an acceptance oracle.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path

GIT='D:/Softwares/Git/cmd/git.exe'
BASE='427580ae13b60a39e88959b282cbffc695c9aabb'
PKG_REV='f67b9db29090449799bd3e05ef7f1d269dab6d31'

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--attempt',required=True);a=p.parse_args()
    root=a.repo.resolve();out=root/'.aegis/results/FC-08-P31-BOOTSTRAP/attempts'/a.attempt
    def load(path):return json.loads(path.read_text(encoding='utf-8'))
    def save(name,value):
        path=out/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
    def ref(path):
        path=path if isinstance(path,Path) else root/path
        return {'path':path.relative_to(root).as_posix(),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    def git(*args):return subprocess.check_output([GIT,*args],cwd=root,text=True,encoding='utf-8').strip()
    package=root/'.aegis/packages/FC-08-PRODUCT-P31-v0.1'
    check=load(out/'package-preflight.json');evidence=load(out/'evidence-preflight.json')
    assert check['status']=='BLOCKED_VERIFICATION' and evidence['status']=='BLOCKED_VERIFICATION'
    assert check['package_byte_bindings_ok'] and check['frozen_input_bytes_ok'] and check['production_source_unchanged']
    assert load(out/'checker-tests.json')['exit_code']==0
    contract=load(package/'execution-contract.json');verification=load(package/'verification.lock.json')
    corpus=load(root/verification['design']['corpus_manifest']['path'])
    protected=['runtime','schema','apps','debug_ui','CMakeLists.txt','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json',
        '.aegis/materialized/FC-08','.aegis/results/FC-08-VERIFY-MATERIALIZE','.aegis/packages/FC-08-PRODUCT-P31-v0.1',
        'tests/fc08/reference']
    protected_diff=git('diff','--name-only',PKG_REV,'--',*protected)
    protected_new=git('ls-files','--others','--exclude-standard','--',*protected)
    assert not protected_diff and not protected_new,'protected data/source changed'
    bindings={}
    def bind(role,path,needles):
        text=(root/path).read_text(encoding='utf-8');lines=text.splitlines();anchors={}
        for needle in needles:
            hits=[i for i,line in enumerate(lines,1) if needle in line]
            assert hits,role+' missing actual anchor '+needle
            anchors[needle]=hits
        bindings[role]={**ref(path),'source_revision':BASE,'git_blob':git('rev-parse',BASE+':'+path),'anchors':anchors}
    bind('public_facade','runtime/foundation/include/canvas/runtime/runtime_facade.hpp',['class RuntimeFacade'])
    bind('owner_admission','runtime/composition/src/canvas_runtime_facade.cpp',['CanvasRuntimeFacade::submitCanvasControl','service_->enqueue'])
    bind('owner_pump','runtime/composition/src/runtime_composition.cpp',['RuntimeComposition::ownerPump','controlFacade_->process'])
    bind('canonical_engine','runtime/semantic/src/operation_engine.cpp',['OperationEngine::prepare','OperationEngine::apply','prepareChangeSet','recordApplied','commitSuccessor'])
    bind('atomic_apply','runtime/semantic/src/atomic_apply.cpp',['applyPreparedPlanInternal','if (!preflight','ObjectStoreMutator::insertFresh','ObjectStoreMutator::replaceExisting','ObjectStoreMutator::eraseExisting'])
    bind('real_History','runtime/interaction/src/editor_history.cpp',['EditorHistory::prepareRecord','entries_.reserve','EditorHistory::commitPrepared','EditorHistory::undo','EditorHistory::redo'])
    bind('existing_command_seam','runtime/interaction/src/canvas_editing_runtime.cpp',['CanvasEditingRuntime::applyLocalHistoryOperation','history_.prepareRecord','history_.commitPrepared','sceneCoordinator_->apply'])
    bind('Scene_publication','runtime/scene/src/incremental_runtime_coordinator.cpp',['IncrementalRuntimeCoordinator::apply','runtimeScene_.prepareIncremental','IncrementalRuntimeCoordinator::publishPending','IncrementalRuntimeCoordinator::recover','abortPublication'])
    bind('frame_presentation','runtime/composition/src/runtime_composition.cpp',['RuntimeComposition::serviceFrameOpportunity','RuntimeComposition::finishFrameOpportunity','pendingPreviewIdentity_'])
    bind('preview_handoff','runtime/interaction/src/canvas_editing_runtime.cpp',['CanvasEditingRuntime::acknowledgePreviewPresentation','canonicalVisibilitySink_->canonicalVisible','previewController_->retireSession'])
    bind('existing_selection','runtime/interaction/src/selection_session.cpp',['SelectionSession::click','SelectionSession::marquee'])
    bind('control_queue','runtime/control/src/canvas_control_service.cpp',['CanvasControlService::enqueue'])
    bind('snapshot_ingress','runtime/semantic/src/snapshot.cpp',['SnapshotCodec::decode'])
    bind('bootstrap_owner','runtime/semantic/src/snapshot_bootstrap.cpp',['SnapshotBootstrapper::restore'])
    bind('replay_without_History','runtime/semantic/src/replay.cpp',['ReplayCoordinator::replayAndFinalize'])
    bind('Windows_host','apps/minimal_runtime_host/windows_main.cpp',['state.host->composition()','composition.finishFrameOpportunity'])
    bind('Web_host','apps/axiom_canvas_demo/platform/web/bridge.cpp',['lifecycle.composition()','serviceFrameOpportunity'])
    role_map={'FC08-E01':['canonical_engine','snapshot_ingress'],'FC08-E02':['canonical_engine','atomic_apply'],
        'FC08-E03':['existing_command_seam','real_History','canonical_engine'],'FC08-E04':['existing_command_seam','Scene_publication'],
        'FC08-E05':['existing_selection','Scene_publication'],'FC08-E06':['Scene_publication','frame_presentation'],
        'FC08-E07':['preview_handoff','frame_presentation','Scene_publication'],'FC08-E08':['control_queue','owner_admission','owner_pump'],
        'FC08-E09':['snapshot_ingress','bootstrap_owner','replay_without_History'],'FC08-E10':['public_facade','Windows_host','Web_host']}
    design={'format':'fc08-bootstrap-implementation-design-preflight-v1','status':'BLOCKED_VERIFICATION',
        'basis':'Read-only existing-owner/source binding plus frozen P13/P15 target contract; no architecture redesign or product implementation',
        'production_subject_revision':BASE,'source_bindings':bindings,
        'current_flow':['RuntimeFacade -> CanvasRuntimeFacade control admission -> RuntimeComposition ownerPump',
            'CanvasEditingRuntime::applyLocalHistoryOperation prepares F, captures images, accepts supplied inverse and prepares original EditorHistory entry',
            'OperationEngine::apply performs real prepare again, stages actual ChangeSet, atomic apply, ledger/generation/clock/commit record',
            'EditorHistory::commitPrepared consumes its prepared entry; Scene coordinator derives/publishes or recovers from authoritative committed poststate',
            'RuntimeComposition frame opportunity/finish and Interaction canonical-visibility acknowledgement retire compatible preview'],
        'authority_bound_target_sequence':['Construct one immutable F with accepted guards using existing Interaction owner',
            'Prepare F on coherent S -> read-only S1 -> construct real U from exact pre/postimages',
            'Normalize/meter actual F and U and validate U applicability/restoration on S1; reserve actual History/terminal/ChangeSet resources',
            'Bind F/U/resources to same real owner and basis; revalidation invalidates old admission and U; no automatic semantic rebase',
            'Reject/cancel before first write with canonical/index/manifest/ledger/generation/clock/History/cursor/redo unchanged',
            'Publish canonical once; consume reserved History and receipt without postcommit allocation/failure injection',
            'Scene prepare/publication/recovery preserves canonical success; no resubmission; compatible preview retired at coherent publication handoff'],
        'target_authority_refs':[ref('.aegis/materialized/FC-08/sources/accepted-exports/P13.md'),ref('.aegis/materialized/FC-08/sources/accepted-exports/P15.md')],
        'existing_vs_future_findings':['Current prepareRecord reserves actual entries capacity; this is not proof of accepted FC08 real F/U budget/admitted-plan reservation',
            'Existing applyLocalHistoryOperation supplies an inverse and re-prepares at apply; future owner-bound admitted F/U seam is deliberately absent',
            'Existing atomic writes/ledger/clock/Scene and preview anchors are resolved; future Section/subtree publication assertions have not been run',
            'Qualification checkpoints remain unchanged/default OFF and cannot substitute for functional RED'],
        'obligation_to_owner_bindings':{o['id']:{'case_ids':o['case_ids'],'roles':role_map[o['id']],'test_binding':'Frozen corpus and expected refs; actual product adapters not established due blockers'} for o in verification['obligations']},
        'unresolved_semantic_decisions':[],'unresolved_verification_decisions':check['exact_blockers'],'authority_conflict':False,
        'verification_conflict':True,'package_scope_conflict':False,'resolved_existing_owner_bindings':True,'may_continue_product_P32':False}
    save('implementation-design-preflight.json',design)
    by={r['variant_id']:r for r in corpus['variants']};red_cases=[]
    for row in contract['red_oracle_precondition']['representative_variants']:
        r=by[row['id']]
        red_cases.append({'case_id':r['case_id'],'variant_id':r['variant_id'],'input':r['input'],'expected':r['expected'],
            'result':'NOT_RUN','failure_boundary':'blocked preflight before product runner establishment/SUT invocation',
            'sut_invocation':None,'raw_SUT_observation':None,'blocker_ids':['BOOT-VER-01','BOOT-PKG-02']})
    save('red-oracle-preflight.json',{'id':'FC08-RED-01','status':'NOT_RUN_BLOCKED_VERIFICATION','qualification_subject_revision':BASE,
        'frozen_red_subject_revision':contract['red_oracle_precondition']['baseline_subject_revision'],
        'baseline_production_equivalence_observed':check['materialization_and_product_baseline_production_bytes_equal'],
        'product_runner_established':False,'valid_product_RED_observed':False,'cases':red_cases,
        'profiles':{p:{'result':'NOT_RUN','reason':'upstream verification binding conflict; no environment-failure or product-RED inference','SUT_invocation':None} for p in ['windows-native','web-wasm','headless']},
        'historical_baseline_records':'Preserved by exact revision; not repurposed as a new product RED run',
        'not_valid_RED':['checker unit failures or passes','descriptor inspection','qualification hook observations','synthetic traces','missing runner/environment']})
    allowed=['.aegis/packages/FC-08-P31-BOOTSTRAP-v0.1/','.aegis/results/FC-08-P31-BOOTSTRAP/','tools/fc08/bootstrap_preflight.py','tests/fc08/bootstrap_preflight_test.py']
    changed=git('diff','--name-only',PKG_REV).splitlines()+git('ls-files','--others','--exclude-standard').splitlines()
    bad=[x for x in changed if not any(x==v or v.endswith('/') and x.startswith(v) for v in allowed)]
    assert not bad,'scope violation '+str(bad)
    save('preservation-audit.json',{'source_package_revision':PKG_REV,'production_subject_revision':BASE,
        'protected_tracked_diff':protected_diff,'protected_untracked':protected_new,'out_of_scope_changes':bad,
        'frozen_refs_verified_by':ref(out/'package-preflight.json'),'all_frozen_input_expected_wire_hashes_match':check['frozen_input_bytes_ok'],
        'no_product_runtime_schema_registry_or_hook_semantics_changed':True,'no_old_results_rewritten':True})
    save('blocked-handoff.json',{'type':'ownership_handoff','from_owner':'aegis-implementation / P31 verification bootstrap',
        'to_owner':'aegis','next_specialist':'P20 aegis-verification for input/oracle truth; P31 aegis-implementation for exact bindings',
        'return_surface':'CONTROL_REVIEW','terminal_state':'BLOCKED_VERIFICATION','exact_blockers':check['exact_blockers'],
        'earliest_required_owner':'P20 Verification for frozen input/oracle consistency, then P31 exact profile/runner bindings',
        'required_resolution':['Review geometry truth/input delta for the recorded variants; no adapter override or silent golden rewrite',
            'Bind exact accepted per-variant Native/Web/headless applicability and regenerate only explicitly authorized successor locks',
            'Resume separately bounded verification bootstrap with accepted identities; implement real SUT adapters and tests, then run all required FC08-RED-01 observations'],
        'product_P32_authorized':False,'P34_PASS':False,'DEV_PASS':False,'PHYS_PASS':False})
    save('output-index.json',{'bootstrap_package':ref('.aegis/packages/FC-08-P31-BOOTSTRAP-v0.1/package.json'),
        'frozen_product_package':{**ref(package/'package.json'),'revision':PKG_REV},
        'preflight_commands_and_exit_codes':ref(out/'preflight-invocations.json'),
        'PackageBindingPreflight':ref(out/'package-preflight.json'),'EvidenceContractPreflight':ref(out/'evidence-preflight.json'),
        'ImplementationDesignPreflight':ref(out/'implementation-design-preflight.json'),'FC08_RED_01':ref(out/'red-oracle-preflight.json'),
        'checker_tests_not_product_tests':ref(out/'checker-tests.json'),'preservation':ref(out/'preservation-audit.json'),
        'control_handoff':ref(out/'blocked-handoff.json'),'product_P32_authorized':False})
    print('Blocked return compiled; no SUT product RED or P32 execution claimed')

if __name__=='__main__':main()
