"""Freeze output navigation; publish no product authorization or Gate state."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ATT=ROOT/'.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01'
MAT=ROOT/'.aegis/materialized/FC-08'
GIT='D:/Softwares/Git/cmd/git.exe'
def load(p):return json.loads(p.read_text(encoding='utf-8'))
def save(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
def ref(p):
    p=p if isinstance(p,Path) else ROOT/p
    return {'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
def git(*args):return subprocess.check_output([GIT,*args],cwd=ROOT,text=True,encoding='utf-8').strip()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--subject-revision');a=ap.parse_args()
    if a.subject_revision:
        subject=a.subject_revision
        assert git('rev-parse',subject)==subject
        assert load(ATT/'readiness.json')['status']=='READY_FOR_CONTROL_REVIEW'
        save(ATT/'terminal-result.json',{'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},
            'task_id':'FC-08-VERIFY-MATERIALIZE','stage':'P33','return_surface':'CONTROL_REVIEW',
            'package_revision':'a0883474790fb5eb70c49c96907caef80fa6b2ab',
            'package_sha256':'152faa67e265f91f2c131036fd6b73bf2ddb0a8ca8488e94fe5d8d0d5953cc4a',
            'starting_revision':'a0883474790fb5eb70c49c96907caef80fa6b2ab',
            'accepted_cursor':'5f216a8e331a98ccccfc7a0616991bb60e1ea312',
            'task_anchor':'a5af5f6d89e8439cc5f70950ba5f1b7986b6ff7b','result_revision':subject,
            'materialized_ref':'refs/heads/codex/fc08-verification-materialization',
            'materialization_policy':'An evidence-only descendant carries this receipt; result_revision identifies the exact qualified source/artifact subject. Source/corpus/runner bytes are unchanged by the receipt commit.',
            'output_manifest_ref':{'revision':subject,**ref(ATT/'output-manifest.json')},
            'evidence_input_refs':[ref(ROOT/'.aegis/packages/FC-08-VERIFY-MATERIALIZE-v0.2/package.json'),
                ref(MAT/'source-index.json'),ref(MAT/'executable-verification.json'),ref(ATT/'output-index.json')],
            'actual_invocations':[ref(ATT/'profiles.json'),ref(ATT/'runner-manifest.json'),ref(ATT/'qualification/python-tests-final.json'),
                ref(ATT/'package-preflight-final.json'),ref(ATT/'evidence-preflight-final.json')],
            'terminal_state':'READY_FOR_CONTROL_REVIEW','exact_blockers':[],
            'remaining_product_obligations':'Section/1,17/18, typed public/platform container APIs, F/U admission and H06 are deliberately absent and nonpassing. PH01-PH04 remain FC13 debt.',
            'product_P32_authorized':False,'P34_PASS':False,'DEV_PASS':False,'PHYS_PASS':False})
        print('Exact subject receipt written');return
    design=load(MAT/'executable-verification.json');corpus_dir=(ROOT/design['corpus_manifest']).parent
    required={'source-index.json':MAT/'source-index.json','descriptor-diff.json':MAT/'descriptor-diff.json',
        'descriptor-build-log.txt':MAT/'descriptor-build-log.txt','variant-index.json':corpus_dir/'variant-index.json',
        'corpus-manifest.json':corpus_dir/'corpus-manifest.json'}
    for name in ['runner-manifest.json','oracle-controls.json','baseline-probes.json','seam-map.json','profiles.json','profile-observations.json','inherited-regression.json']:
        required[name]=ATT/name
    save(ATT/'output-index.json',{'format':'fc08-materialization-output-navigation-v2','required_artifacts':{k:ref(v) for k,v in required.items()},
        'product_candidate':ref(MAT/'product-package-candidate-v0.2.json'),'readiness':ref(ATT/'readiness.json'),
        'historical_terminal_preserved':'.aegis/results/FC-08-VERIFY-MATERIALIZE/terminal-result.json@5f216a8e331a98ccccfc7a0616991bb60e1ea312',
        'current_corpus_only':design['corpus_manifest'],'earlier_attempt_directories':'Preserved authoring history; not accepted goldens or current readiness',
        'product_P32_authorized':False})
    # Preserve diagnostics that were emitted in the ignored execution area.
    for p in (ROOT/'out/fc08').glob('materialize-corpus*.log'):
        dest=ATT/'authoring-diagnostics'/p.name
        if not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(p.read_bytes())
    contract=load(ROOT/'.aegis/packages/FC-08-VERIFY-MATERIALIZE-v0.2/execution-contract.json')
    allowed=contract['implementation']['qualification_only_runtime_scope']['allowed_runtime_files']+[
        'runtime/semantic/CMakeLists.txt','runtime/interaction/CMakeLists.txt','runtime/composition/CMakeLists.txt','runtime/render/CMakeLists.txt','CMakeLists.txt']
    prefixes=['.aegis/materialized/FC-08/','.aegis/results/FC-08-VERIFY-MATERIALIZE/','tests/fc08/','tools/fc08/']
    changed=git('diff','--name-only','HEAD').splitlines()+git('ls-files','--others','--exclude-standard').splitlines()
    retained='.aegis/packages/FC-08-VERIFY-MATERIALIZE/accepted-inputs.zip'
    forbidden=[p for p in changed if p!=retained and p not in allowed and not any(p.startswith(pre) for pre in prefixes)]
    assert not forbidden,'out-of-scope mutation '+str(forbidden)
    save(ATT/'scope-audit.json',{'package_revision':'a0883474790fb5eb70c49c96907caef80fa6b2ab','source_baseline':git('rev-parse','HEAD'),
        'allowed_runtime_paths_used':sorted(set(changed)&set(allowed)),'out_of_scope_changes':forbidden,
        'retained_untracked_archive_not_for_commit':retained,'product_changes_authorized':False,
        'qualification_switch_default':'OFF','normal_runtime_public_API_changed':False})
    outputs=set()
    for directory in prefixes:
        outputs.update(p for p in (ROOT/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    outputs.update(ROOT/p for p in allowed if (ROOT/p).is_file())
    excluded={ATT/'output-manifest.json',ATT/'terminal-result.json'}
    save(ATT/'output-manifest.json',{'format':'fc08-output-manifest-v2','source_baseline':git('rev-parse','HEAD'),
        'files':[ref(p) for p in sorted(outputs-excluded)],'self_and_final_receipt_excluded_to_avoid_recursive_digest':True,
        'product_P32_authorized':False})
    print('Output index, scope audit and exact digest manifest written')
if __name__=='__main__':main()
