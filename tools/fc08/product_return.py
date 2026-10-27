"""Compile truthful bootstrap return from actual exact-revision observations."""
from collections import Counter
import json
from pathlib import Path
import subprocess
from product_runner import ROOT,PACKAGE,PACKAGE_REVISION,ref,save,load

def main():
    out=ROOT/'.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-003'
    run=out/'qualified-001';invocations=load(run/'invocations.json')
    assert invocations['all_commands_exit_zero']
    result_revision=invocations['runner_revision']
    methods=load(ROOT/'.aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/profile-method-bindings.json')
    bindings={b['method_id']:b for b in methods['bindings']}
    coverage={};unavailable=[];red_refs={}
    for profile in ['windows-native','web-wasm','headless']:
        result=load(run/'profiles'/profile/'results.json');report=load(run/'profiles'/profile/'coverage.json')
        assert result['runner_revision']==result_revision and len(result['cases'])==302
        assert result['FC08_RED_01']['status']=='REAL_R01_RED_OBSERVED'
        red_refs[profile]={'result':ref(run/'profiles'/profile/'results.json'),
                           'raw_red_observations':[r for r in result['FC08_RED_01']['rows']],
                           'interpretation':'The four independently valid Section/17/18 positives fail at actual normal codec/ingress; inherited controls succeed. This is feature-absence R01 RED, not every future workflow executed.'}
        coverage[profile]={'coverage_artifact':ref(run/'profiles'/profile/'coverage.json'),
                           'case_results':report['case_results'],
                           'applicability':dict(Counter(c['applicability'] for c in result['cases'])),
                           'raw_artifacts_present':sum(c['observed_artifact'] is not None for c in result['cases'])}
        for case in result['cases']:
            if case['applicability']=='APPLICABLE' and case['result']=='NOT_RUN':
                binding=bindings[case['variant_id']]
                unavailable.append({'profile':profile,'variant_id':case['variant_id'],'case_id':case['case_id'],
                                    'method':binding['method'],'accepted_method':binding['rationale'],
                                    'owner':binding['owner_binding'],'evidence_role':binding['evidence_role'],
                                    'expected_fields':[p.get('path',p.get('before',p['op'])) for p in binding['frozen_predicates']],
                                    'missing_observations':case['predicate_evaluation']['missing_observations'] if case['predicate_evaluation'] else 'Whole owner/method observation not supplied',
                                    'basis':case['observation_basis'],'input':binding['input'],'expected':binding['expected']})
    unavailable_ref=save(out/'unexecuted-applicable-variants.json',{
        'format':'fc08-real-runner-coverage-gaps-v1','rows':unavailable,
        'policy':'No extra exclusion is added to the accepted matrix. These are applicable NOT_RUN entries, never PASS or NOT_APPLICABLE. Shared controls have no product execution credit.',
        'not_all_gaps_have_the_same_cause':'Some methods need further verification tooling; Section command/History/selection/Scene/host workflow execution also depends on absent production types/entry points. Do not infer that every listed row independently needs product changes.'})
    source_paths=['runtime/semantic/include/canvas/semantic/object_record.hpp',
                  'runtime/semantic/include/canvas/semantic/object_content.hpp',
                  'runtime/semantic/include/canvas/semantic/operation.hpp',
                  'runtime/semantic/include/canvas/semantic/operation_payload.hpp',
                  'runtime/foundation/include/canvas/runtime/runtime_facade.hpp',
                  '.aegis/materialized/FC-08/sources/accepted-exports/P15.md']
    blocker={
        'id':'BOOT-RUNNER-03','classification':'TASK_PACKAGE_DEFECT / P31_PHASE_DEPENDENCY',
        'owner':'P31 control review; Verification owner if changing the accepted observation sequence',
        'status':'BLOCKED_FULL_PRODUCT_RUNNER_BINDING','source_bindings':[ref(ROOT/p) for p in source_paths],
        'finding':'The new runner genuinely executes normal codec and explicit legacy-bootstrap observations, but does not materialize all accepted product observation seams. Full product-runner completion must not be inferred from R01 success or302-ID enumeration.',
        'concrete_dependency':'427580ae ObjectKind ends at Group9 and OperationKind at16; ObjectContent/OperationPayload have no Section/17/18 representation, and existing RuntimeFacade has no concrete accepted container command/readback entry. Therefore actual Section command F/U, History, scoped selection, Scene/preview and host workflows cannot be instantiated on this subject. Test adapters may not supply substitute semantics.',
        'not_a_method_name_test':'P15 expressly calls its suggested public method names non-stable; absence of submitContainerIntent alone is not used as functional RED or the sole blocker. Real rejected bytes and absent canonical types/payloads establish the dependency.',
        'examples':['FC08-H01/empty-Section requires actual forward/Undo/Redo and History timeline',
                    'FC08-S01/boundary-full-child-inside requires actual Section producer and scoped selection',
                    'FC08-V04 real Scene prepare failure/recovery needs a committed Section workflow',
                    'FC08-P02/confirmed-utf8-title requires actual native/WASM host container-intent path'],
        'coverage_gap_manifest':unavailable_ref,
        'decision_required':'Bind a viable complete real-SUT adapter/phase sequence at P31. If full adapters require Section product types/owners to exist, control must explicitly version that bootstrap/product sequencing decision; do not silently waive applicable variants or authorize product changes inside this task.',
        'continue_execution':False,'product_P32_authorized':False}
    blocker_ref=save(out/'blocked-handoff.json',blocker)
    preservation=subprocess.check_output(['D:/Softwares/Git/cmd/git.exe','diff','--name-only','0d13b63702facc1534e815643c3bc6085d9c6618','--diff-filter=CDMRTUXB'],cwd=ROOT,text=True)
    assert not preservation.strip()
    preservation_ref=save(out/'preservation-audit.json',{'status':'ALL_PREEXISTING_TRACKED_BYTES_PRESERVED',
        'comparison_revision':'0d13b63702facc1534e815643c3bc6085d9c6618','modified_deleted_renamed_paths':[],
        'production_subject_revision':'427580ae13b60a39e88959b282cbffc695c9aabb',
        'frozen_expected_files_preserved':302,'old_oracles_unchanged':True,'old_qualification_hooks_unchanged':True,
        'deps_protobuf_present':(ROOT/'.deps/protobuf').exists(),'main_merged':False})
    # Preserve the real prequalification observer diagnostic as corroborative,
    # never as a final product RED or a change to historical frozen results.
    diag=ROOT/'out/fc08-bootstrap-003/smoke-native/native/r01-section-snapshot3.json'
    if diag.is_file():
        save(out/'diagnostics/prequalification-snapshot-observer.json',{
            'status':'PREQUALIFICATION_RUNNER_DIAGNOSTIC_ONLY','raw':load(diag),
            'finding':'Golden acceptance true and actual normal_ingress_ok false; runner initially read the wrong boolean. Qualified runner uses normal ingress and retains both raw values.',
            'correction':'Observer extraction only; frozen expected/oracle and production source unchanged. Covered by real failing-then-passing runner tests.',
            'valid_product_RED_claimed_by_this_diagnostic':False})
    save(out/'red-oracle-preflight.json',{'status':'REAL_R01_RED_OBSERVED_ALL_THREE_PROFILES',
         'qualification_subject_revision':'427580ae13b60a39e88959b282cbffc695c9aabb','runner_revision':result_revision,
         'profiles':red_refs,'R03':'No claim of executed Section temporal/UI/History predecessor workflows when the feature is absent; all missing current observations stay explicit NOT_RUN.',
         'product_P32_authorized':False})
    manifest=save(out/'evidence-manifest.json',{'format':'fc08-bootstrap-evidence-manifest-v1','result_revision':result_revision,
        'artifacts':[ref(p) for p in sorted(out.rglob('*')) if p.is_file() and p.name not in ['terminal-result.json','evidence-manifest.json']],
        'package':dict(ref(ROOT/PACKAGE/'package.json'),revision=PACKAGE_REVISION)})
    terminal={
        'task_id':'FC-08-P31-BOOTSTRAP','terminal_state':'BLOCKED_FULL_PRODUCT_RUNNER_BINDING','return_surface':'CONTROL_REVIEW',
        'result_revision':result_revision,'materialized_ref':'refs/heads/codex/fc08-p20-p31-closure',
        'receipt_policy':'An evidence-only descendant stores exact-revision qualified001 runs and this receipt. Candidate and runner identities remain distinct from receipt storage revision.',
        'bootstrap_package':dict(ref(ROOT/PACKAGE/'package.json'),revision=PACKAGE_REVISION),
        'parent_product_package_revision':'f67b9db29090449799bd3e05ef7f1d269dab6d31',
        'accepted_input_revision':'eb12da1acc4516e115d36767073f8bdc607f8a4d','acceptance':ref(ROOT/PACKAGE/'acceptance.json'),
        'prior_blockers':{'BOOT-VER-01':'RESOLVED_BY_EXACT_ACCEPTED_VERSIONED_CORRECTION','BOOT-PKG-02':'RESOLVED_BY_EXACT_ACCEPTED_302_ID_MATRIX'},
        'preflights':{name:ref(run/path) for name,path in [('PackageBindingPreflight','package-preflight.json'),('EvidenceContractPreflight','evidence-preflight.json'),('ImplementationDesignPreflight','implementation-design-preflight.json')]},
        'preflight_scope':'Both exact package preflights pass for revised verification-only bootstrap package6497; this is not acceptance of full product P31 or its complete product runner.',
        'actual_invocations':ref(run/'invocations.json'),'environment_and_toolchain':ref(run/'toolchain-and-reuse.json'),
        'verification_tool_tests':invocations['verification_tool_tests'],'verification_tool_tests_are_not_Axiom_tests':True,
        'profiles':coverage,'FC08_RED_01':ref(out/'red-oracle-preflight.json'),'valid_real_R01_RED_observed':True,
        'full_302_variant_product_runner_qualified':False,'exact_blockers':[blocker],
        'preservation':preservation_ref,'evidence_manifest':manifest,
        'resume_cursor':{'execution_ref':'refs/heads/codex/fc08-p20-p31-closure','revision':result_revision,
                         'completed_through':['Exact acceptance and source/matrix preserved','Two exact bootstrap preflights and design binding pass','Real Native/Chrome-WASM/headless R01 and inherited controls run','Complete302-ID discovery and exact applicable NOT_RUN gaps published'],
                         'next_action':'Return BOOT-RUNNER-03 to P31 control for complete real-adapter/phase binding; preserve qualified001. Product P32 still requires separate independent authorization.'},
        'ownership_handoff':{'to':'aegis','root_owner':'P31 implementation control','reason':'Full product-runner/production-phase dependency exceeds verification-only bootstrap execution authority.'},
        'product_P32_authorized':False,'DEV_PASS':False,'P34_PASS':False,'PHYS_PASS':False,'release_ready':False}
    terminal_ref=save(out/'terminal-result.json',terminal)
    print(json.dumps({'terminal_state':terminal['terminal_state'],'result_revision':result_revision,'terminal_result':terminal_ref,'coverage':coverage},ensure_ascii=False,indent=2))

if __name__=='__main__':main()
