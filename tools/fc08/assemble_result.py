"""Compile reviewer navigation from actual immutable run artifacts.

This is a materialization readiness checker, not an Aegis Gate evaluator.
It never upgrades future product CAPABILITY_ABSENT records to product PASS.
"""
import hashlib,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from variants import VARIANTS
from run_verification import load,save
from check_hook_probe import check_observation
MAT=ROOT/'.aegis/materialized/FC-08'
ATT=ROOT/'.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01'
GIT='D:/Softwares/Git/cmd/git.exe'
def ref(path):
    p=path if isinstance(path,Path) else ROOT/path
    b=p.read_bytes();return {'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(b).hexdigest(),'size':len(b)}
def anchors(path,needle):
    return {**ref(path),'lines':[i for i,line in enumerate((ROOT/path).read_text(encoding='utf-8').splitlines(),1) if needle in line]}
def owner_headers_reachable(source):
    """Check the repository include closure of standalone validator tests.

    These targets deliberately compile validator/manifest sources directly and
    do not link or consume any of the four instrumented owner definitions.
    They are not ODR participants just because a file is under semantic/.
    """
    roots=list((ROOT/'runtime').glob('*/include'));seen=set();pending=[Path(source)]
    owners={'operation_engine.hpp','editor_history.hpp','canvas_editing_runtime.hpp','runtime_composition.hpp'}
    while pending:
        path=pending.pop().resolve()
        if path in seen:continue
        seen.add(path)
        if path.name in owners:return True
        text=path.read_text(encoding='utf-8')
        for include in re.findall(r'^\s*#include\s+"([^"]+)"',text,re.M):
            candidates=[path.parent/include,*[base/include for base in roots]]
            found=next((p for p in candidates if p.is_file()),None)
            if found is not None:pending.append(found)
    return False
def main():
    errors=[]
    package=ROOT/'.aegis/packages/FC-08-VERIFY-MATERIALIZE-v0.2/package.json'
    assert ref(package)['sha256']=='152faa67e265f91f2c131036fd6b73bf2ddb0a8ca8488e94fe5d8d0d5953cc4a'
    for phase in ['package','evidence']:
        pf=load(ATT/f'{phase}-preflight-final.json')
        assert pf['status'].startswith('PASS_MATERIALIZATION_'),phase+' preflight not observed'
    sources=load(MAT/'source-index.json')
    for row in sources['accepted_exports']+sources['seed_sources']+sources['schema_sources']+[sources['inherited_math_export']]:
        assert ref(row['path'])['sha256']==row['sha256'],'M01 source drift'
    desc=load(MAT/'descriptor-diff.json')
    for name in ['baseline','proposed']:assert ref(MAT/f'{name}-descriptor/descriptor.pb')['sha256']==desc[name+'_sha256']
    design=load(MAT/'executable-verification.json');corpus=load(ROOT/design['corpus_manifest'])
    wanted={f'FC08-{c}/{v}' for c,vs in VARIANTS.items() for v in vs}
    assert {r['variant_id'] for r in corpus['variants']}==wanted,'incomplete materialized discovery'
    for row in corpus['variants']:
        for r in [row['input'],row['expected'],*[w for w in row['wire'] if 'path' in w]]:
            assert ref(r['path'])['sha256']==r['sha256'],'corpus identity drift'
    audit=load(ATT/'corpus-authoring-audit.json');assert not audit['errors'],'authoring audit failed'
    assert load(ATT/'qualification/python-tests-final.json')['exit_code']==0,'oracle/source tests failed'
    native=load(ATT/'qualification/live-hooks-final.json')
    assert native['status']=='LIVE_HOOK_BINDING_ACCEPTED','native hooks failed'
    browser_on=ATT/'runner/web-final/browser'
    on=load(browser_on/'live-hooks.json');off=load(ATT/'browser/off-final/passive-off.json')
    hook_errors=check_observation(on,off['passive']);assert not hook_errors,'browser OFF/ON hooks failed'
    save(ATT/'browser/hook-verdict-final.json',{'status':'LIVE_HOOK_BINDING_ACCEPTED','errors':hook_errors,
        'ON':ref(browser_on/'live-hooks.json'),'OFF':ref(ATT/'browser/off-final/passive-off.json'),'product_PASS':False})
    runs={'windows-native':'windows-final','web-wasm':'web-final','headless':'headless-final'}
    profile_observations=[]
    for profile,folder in runs.items():
        result=load(ATT/'runner'/folder/'results.json')
        assert {r['variant_id'] for r in result['results']}==wanted,'profile missing discovery'
        assert result['product_P32_authorized'] is False and not result['Gate_PASS']
        by={r['variant_id']:r for r in result['results']}
        for r in corpus['variants']:
            assert by[r['variant_id']]['input_sha256']==r['input']['sha256'] and by[r['variant_id']]['expected_sha256']==r['expected']['sha256'],'profile observed an earlier fixture'
            assert by[r['variant_id']]['product_positive_passed'] is False,'baseline must not qualify future product'
        inherited=load(ATT/'runner'/folder/'inherited/inherited-regression.json')
        assert inherited['status']=='BASELINE_MATCHED' and not inherited['errors'],'retained regressions failed'
        profile_observations.append({'id':profile,'discovery':ref(ATT/'runner'/folder/'discovery.json'),
            'actual_result':ref(ATT/'runner'/folder/'results.json'),
            'retained_regression':ref(ATT/'runner'/folder/'inherited/inherited-regression.json')})
    definitions=[];builds=[];binaries=[];compilers=[]
    for tag,hooks in [('v02-on',True),('v02-off',False)]:
        for profile in ['windows-native','web-wasm']:
            build=ROOT/'out/fc08'/tag/profile
            compiler_file=next((build/'CMakeFiles').rglob('CMakeCXXCompiler.cmake'))
            compiler_text=compiler_file.read_text(encoding='utf-8')
            compiler_values={k:re.search(r'set\('+k+r' "?([^\n"]+)',compiler_text).group(1)
                for k in ['CMAKE_CXX_COMPILER','CMAKE_CXX_COMPILER_ID','CMAKE_CXX_COMPILER_VERSION']}
            saved_compiler=ATT/'environment'/f'compiler-{tag}-{profile}.cmake'
            saved_compiler.parent.mkdir(parents=True,exist_ok=True);saved_compiler.write_bytes(compiler_file.read_bytes())
            compilers.append({'tag':tag,'profile':profile,**compiler_values,'raw_CMake_fact':ref(saved_compiler)})
            commands=load(build/'compile_commands.json')
            rows=[x for x in commands if any('/runtime/'+m+'/' in x['file'].replace('\\','/') for m in ['semantic','interaction','composition']) or '/tests/fc08/' in x['file'].replace('\\','/')]
            assert rows,'empty qualification build definition audit'
            standalone=['canvas_semantic_payload_structure_test.dir','canvas_semantic_g1_04_c_geometry_overflow_source_test.dir',
                'canvas_semantic_leaf_structure_validation_test.dir']
            excluded=[x for x in rows if any(name in x.get('command',' '.join(x.get('arguments',[]))) for name in standalone)]
            assert all(not owner_headers_reachable(x['file']) for x in excluded),'standalone target unexpectedly consumes an instrumented owner'
            rows=[x for x in rows if x not in excluded]
            bad=[x['file'] for x in rows if ('AXIOM_FC08_QUALIFICATION_HOOKS=1' in x.get('command',' '.join(x.get('arguments',[]))))!=hooks]
            assert not bad,'qualification ODR propagation mismatch'
            save(ATT/'environment'/f'compile-commands-{tag}-{profile}.json',rows)
            definitions.append({'tag':tag,'profile':profile,'compiler_commands_sha256':ref(build/'compile_commands.json')['sha256'],
                'affected_compiler_commands':ref(ATT/'environment'/f'compile-commands-{tag}-{profile}.json'),
                'standalone_validator_exclusions':sorted({x['file'] for x in excluded}),
                'excluded_include_closures_have_no_instrumented_owner':True,
                'all_affected_compilations_consistent':True,'hooks':hooks,'normal_controls_excluded':not hooks})
            logs=ATT/'builds'/tag/profile
            successful=[]
            for file in logs.glob('run-*/invocations.json'):
                inv=load(file)
                if inv and inv[-1]['exit_code']==0 and any(c['argv'][:2]==['cmake','--build'] and c['exit_code']==0 for c in inv):successful.append(file)
            assert successful,'no successful applicable build'
            # The chosen successful invocation is exact and retained; no
            # unqualified/floating "latest build" is passed as authority.
            file=sorted(successful)[-1];builds.append({'tag':tag,'profile':profile,'invocations':ref(file)})
            suffix='exe' if profile=='windows-native' else 'wasm'
            for stem in ['baseline_probe','hook_probe','headless_probe','selection_regression','connector_regression','control_regression']:
                p=build/f'tests/fc08/axiom_fc08_{stem}.{suffix}'
                binary={**ref(p),'tag':tag,'profile':profile,'role':stem,
                    'identity_kind':'observed local binary identity; reproducible from retained exact build invocation, not a repository binary artifact'}
                if stem=='hook_probe' and profile=='windows-native':
                    contains=b'Q-SEM-STAGED' in p.read_bytes()
                    assert contains==hooks,'OFF binary contains qualification checkpoint control strings'
                    binary['checkpoint_string_present']=contains
                binaries.append(binary)
    save(ATT/'qualification/build-definition-audit.json',{'definitions':definitions,'binary_observations':binaries,
        'normal_public_ABI_policy':'normal macro-OFF headers contain no qualification members or API; no RuntimeFacade changes',
        'errors':[],'product_PASS':False})
    # Frozen environment facts are copied, including locked target/host package
    # identities, rather than depending on ignored old build caches.
    facts=[]
    for label in ['native','web']:
        data=load(ROOT/f'out/fc08/{label}-environment-resume.json')
        save(ATT/'environment'/f'{label}-sdk-facts.json',data);facts.append(ref(ATT/'environment'/f'{label}-sdk-facts.json'))
    save(ATT/'profiles.json',{'format':'fc08-real-profile-bindings-v2','builds':builds,'binaries':binaries,'SDK_facts':facts,
        'actual_consumer_compilers':compilers,'machine':ref(ATT/'environment/machine.json'),
        'locked_Skia_producer_toolchains':{p:load(ROOT/'r1-full-skia-sdk.lock.json')['targets'][p]['variants']['release']['toolchain'] for p in ['windows-x64-d3d12','web-wasm-webgl2']},
        'compiler_identity_note':'Actual consumer compiler facts and locked producer metadata are recorded separately; no assertion that their LLVM identities are equal. The selected Web Emscripten version is the lock-declared 6.0.6; real link/load/execution is recorded.',
        'browser_ON':ref(browser_on/'profile.json'),'browser_OFF':ref(ATT/'browser/off-final/profile.json'),
        'profile_realizations':profile_observations,'headless':'semantic-only executable linked only to canvas_runtime_semantic; no EditorHistory owner',
        'old_web_failure_preserved':'.aegis/results/FC-08-VERIFY-MATERIALIZE/builds/web-wasm/build-probe.log@5f216a8e331a98ccccfc7a0616991bb60e1ea312',
        'product_PASS':False,'PHYS_PASS':False})
    save(ATT/'profile-observations.json',{'profiles':profile_observations,'product_PASS':False})
    points={
      'Q-SEM-STAGED':anchors('runtime/semantic/src/operation_engine.cpp','FC08_ENGINE_STAGED'),
      'Q-SEM-RESERVED':anchors('runtime/semantic/src/operation_engine.cpp','FC08_ENGINE_RESERVED'),
      'Q-SEM-PUBLISH':anchors('runtime/semantic/src/atomic_apply.cpp','Point::kSemPublish'),
      'Q-HIST-BEFORE-RESERVE':anchors('runtime/interaction/src/editor_history.cpp','Point::kHistBeforeReserve'),
      'Q-HIST-READY':anchors('runtime/interaction/src/editor_history.cpp','Point::kHistReady'),
      'Q-CMD-ADMITTED':anchors('runtime/interaction/src/canvas_editing_runtime.cpp','Point::kCmdAdmitted'),
      'Q-SEM-DONE':anchors('runtime/semantic/src/operation_engine.cpp','FC08_ENGINE_DONE'),
      'Q-HIST-DONE':anchors('runtime/interaction/src/editor_history.cpp','historyTransition')}
    save(ATT/'seam-map.json',{'format':'fc08-live-observation-seams-v2','points':points,
        'native_live':ref(ATT/'qualification/live-hooks-final.json'),'browser_live':ref(ATT/'browser/hook-verdict-final.json'),
        'definition_propagation':ref(ATT/'qualification/build-definition-audit.json'),
        'owner_identity':'actual const owner address plus full16-byte lifetime registration key/epoch; real full OperationId and DocumentId. The registration key is qualification identity, not a fabricated CanvasTargetKey/pointer fact.',
        'actual_reserved_resources':['real apply plan','actual ChangeSet preparation','actual temporary PreparedEntry + entries capacity'],
        'not_reserved_or_claimed':['future FC08 F/U admission','future E01 outcome slot','future resumable admitted-plan ticket'],
        'mutation_boundary':'no callback between canonical object writes; Q-SEM-DONE after manifest/ledger/generation/clock/record; Q-HIST-DONE after cursor/redo transition; no postcommit rejection',
        'vectors':['canonical record bytes','resource manifest + retired entries','complete fixture parent indices','semantic generation','full fixture ledger entries/fingerprints','runtime epoch/commit ordinal','History entries/cursor/redo'],
        'fixture_bounds':{'objects':16,'History_units':8,'operation_ID_domain_per_lane':64,'trace_events':32},
        'baseline_R01':{k:ref(browser_on/f'{k}.json') for k in ['section-record','section-snapshot3','op17-content','op18-introduce','legacy-insert']},
        'envelope_warning':'Snapshot envelope observation alone can accept new opaque bytes; normal SnapshotCodec.decode is authoritative and remains nonpassing for Section.',
        'future_capability_mapping':{r['variant_id']:{'seam':r['future_seam'],'frozen_case':r['case_id'],
            'actual_records':[x['actual_result'] for x in profile_observations]} for r in corpus['variants']},
        'H06':'NOT_REACHED_BASELINE: no resumable admitted F/U plan boundary; remains mandatory for future product execution',
        'product_PASS':False})
    controls=load(MAT/'oracle-controls.json')
    assert controls['C01_bad']['first_failure']==1 and controls['C02_bad']['first_failure']==2 and controls['C01_good'] is None and controls['C02_good'] is None
    save(ATT/'oracle-controls.json',{'fixed_R02':ref(MAT/'oracle-controls.json'),'independent_tests':ref(ATT/'qualification/python-tests-final.json'),
        'authoring_audit':ref(ATT/'corpus-authoring-audit.json'),'not_product_observations':True})
    save(ATT/'runner-manifest.json',{'runner':ref('tools/fc08/run_verification.py'),'design':ref(MAT/'executable-verification.json'),
        'oracle':ref('tests/fc08/reference/oracles.py'),'corpus':ref(ROOT/design['corpus_manifest']),
        'profiles':profile_observations,'future_product_nonpassing':True})
    save(ATT/'baseline-probes.json',{'profiles':profile_observations,'interpretation':'Each exact R01 wire is executed through real baseline ingress; future unknown-field/tuple failure is capability absence, not product rejection PASS.'})
    save(ATT/'inherited-regression.json',{'plan':ref('.aegis/materialized/FC-08/attempts/v0.2-obs01-regressions/inherited-regression-plan.json'),
        'profiles':profile_observations,'historical_FC01A_Gates_reopened':False})
    save(MAT/'product-package-candidate-v0.2.json',{'status':'NON_AUTHORIZED_CANDIDATE_FOR_CONTROL_REVIEW',
        'product_P32_authorized':False,'implementation_permission':False,'package_basis':ref(package),
        'design':ref(MAT/'executable-verification.json'),'corpus':ref(ROOT/design['corpus_manifest']),
        'runner':ref(ATT/'runner-manifest.json'),'seams':ref(ATT/'seam-map.json'),'profiles':ref(ATT/'profiles.json'),
        'future_required_execution':'All accepted FC08 positives incl H06 and P01/P02 typed platform bindings remain required after actual product implementation. No zero-case or unsupported-as-positive shortcut.'})
    save(ATT/'readiness.json',{'status':'READY_FOR_CONTROL_REVIEW','M01_reused_exact':True,'M02_artifacts_bound':True,
        'M03_real_baseline_hooks_and_runner_bound':True,'M04_profiles_and_retained_regressions_bound':True,
        'errors':errors,'product_P32_authorized':False,'P34_PASS':False,'DEV_PASS':False,'PHYS_PASS':False})
    print('READY_FOR_CONTROL_REVIEW (verification materialization only)')
if __name__=='__main__':main()
