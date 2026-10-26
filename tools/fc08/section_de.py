"""Bounded FC08 D/E authorization, preflight and preservation audit."""
import argparse
import fnmatch
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = 'af1aff801369052eb7b94716f6b4ab3e899eb3e9'
PKG = '.aegis/packages/FC-08-SECTION-DE-v0.1'
RESULT = '.aegis/results/FC-08-SECTION-DE'
BRANCH = 'refs/heads/codex/fc08-section-scene-host'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode().strip()

def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    assert not path.exists(), path
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

def ref(path):
    return {'path': path, 'sha256': hashlib.sha256((ROOT / path).read_bytes()).hexdigest()}

def materialize():
    assert git('rev-parse', 'HEAD') == BASE
    assert git('symbolic-ref', 'HEAD') == BRANCH
    authority = json.loads((ROOT / '.aegis/packages/FC-08-SECTION-C-v0.1/authority.lock.json').read_text())
    authority.update(status='Current', source_revision=BASE,
        authorization='Direct user instruction: 继续实现后续的切片D/E',
        previous_C=ref('.aegis/results/FC-08-SECTION-C-H03-v0.1/terminal-result.json'))
    write(ROOT / PKG / 'authority.lock.json', authority)
    tests = [
        {'id':'D-GEOMETRY', 'requirement':'Full ancestor transform once, own Section boundary, descendants outside boundary visible, hierarchy order, coherent publication/recovery', 'oracle':'Literal affine coordinates/bounds and actual Scene/full-incremental equivalence tests', 'acceptance':'Focused native tests pass', 'evidence_required':'RED plus raw native results'},
        {'id':'E-COMMAND', 'requirement':'Create/wrap/release/reparent/resize/rename/select via public facade and existing History; zero-write rejection', 'oracle':'Real Composition/Facade/Semantic/History observations; independent literal world coordinates and preimages', 'acceptance':'Real consumer tests and impacted regressions pass except preserved baseline defect', 'evidence_required':'Native public consumer command/results'},
        {'id':'E-HOST', 'requirement':'Basic Windows/Web Section controls use same public intent/readback with no host semantic construction', 'oracle':'Build actual hosts and actual Chrome/WASM Section execution where environment permits', 'acceptance':'Both builds and scripted common command regressions; physical input explicitly pending', 'evidence_required':'Build/toolchain/browser raw results or precise environment blocker'}]
    write(ROOT / PKG / 'verification.lock.json', {'accepted_P20':ref('.aegis/materialized/FC-08/sources/accepted-exports/P20.md'), 'obligations':tests, 'full_302_runner':'NOT claimed; frozen inputs and expected unchanged'})
    allowed=['runtime/scene/**','runtime/render/**','runtime/interaction/**','runtime/composition/**','runtime/control/**','runtime/foundation/include/canvas/runtime/**','debug_ui/**','apps/minimal_runtime_host/**','apps/canvas_demo_web/**','tools/fc08/section_de*.py',PKG+'/**',RESULT+'/**']
    write(ROOT / PKG / 'execution-contract.json', {'source_revision':BASE,'allowed_files':allowed,
        'required':['D geometry/world hierarchy/Scene publication and recovery','E public facade typed bounded control path through existing planner/History','Basic shared Windows/Web Section controls and source comparison regression','Real RED before production edits; scoped regression and durable pushed evidence'],
        'forbidden':['Schema/descriptors/SDK locks changes','Frozen corpus/oracles/expected or previous package/results mutation','Qualification hooks/defaults/global registries/main changes','Parallel ContainerRuntime/History/membership/selection or generic transaction','Frame/Clipboard/advanced FC04/FC06/FC07/PH01-04','Gate/PHYS/full runner claims'],
        'terminal_success':['Scoped D/E functionality and focused tests pass','Impacted regressions, both host builds and browser evidence recorded','Exact reviewer-accessible result and limitations recorded'],
        'terminal_blockers':['AUTHORITY_CONFLICT','VERIFICATION_ORACLE_CONFLICT','ENVIRONMENT_BLOCKER','IMPLEMENTATION_FAILURE'],
        'hosted_verification':{'required':[],'optional':['CI']},'continue_until_terminal_state':True})
    write(ROOT / PKG / 'evidence-contract.json', {'blocking':['Exact package preflights --repo .','ImplementationDesignPreflight','Real behavioral RED','Raw native/public consumer/build/browser results or classified blockers','Preservation audit','Exact pushed result revision'], 'corroborative':['Physical manual testing pending; no PHYS claim'], 'old_baseline_failure':'StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds remains independently reproduced debt'})
    write(ROOT / PKG / 'implementation-context.json', {'completed':'A/B/C and accepted H03 correction retained unchanged', 'current_flow':'Composition owner -> Interaction planner -> existing EditorHistory.executeUndoable -> OperationEngine staged admission -> existing Scene coordinator', 'next':'Geometry RED; full hierarchical projection/Section rendering then public typed controls; no new canonical model', 'unresolved_semantic_decisions':[]})
    write(ROOT / PKG / 'package.json', {'format':'aegis-repo-local-execution-package-v1','package_id':'FC-08-SECTION-DE-v0.1','execution_authority_mode':'repo_materialized','repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},'source_revision':BASE,'execution_ref':BRANCH,'task_anchor':{'revision':BASE,'relation':'ancestor'},'resume_cursor':None,'authorized_slice':'D/E only','product_P32_authorized':True, **{key+'_ref':ref(PKG+'/'+name) for key,name in [('authority_lock','authority.lock.json'),('verification_lock','verification.lock.json'),('execution_contract','execution-contract.json'),('evidence_contract','evidence-contract.json'),('implementation_context','implementation-context.json')]},'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW'})

def preflight(phase, audit):
    out=ROOT/RESULT/phase
    out.mkdir(parents=True, exist_ok=False)
    errors=[]; checked=[]
    def check(v):
        if isinstance(v,dict):
            if 'path' in v and 'sha256' in v:
                assert (ROOT/v['path']).resolve().is_relative_to(ROOT)
                assert ref(v['path'])==v,v['path']
                checked.append(v)
            for c in v.values(): check(c)
        elif isinstance(v,list):
            for c in v: check(c)
    try:
        assert git('remote','get-url','origin')=='https://github.com/Mostorm-Labs/axiom.git'
        assert git('symbolic-ref','HEAD')==BRANCH
        subprocess.run(['git','merge-base','--is-ancestor',BASE,'HEAD'],cwd=ROOT,check=True)
        for p in (ROOT/PKG).glob('*.json'):check(json.loads(p.read_text(encoding='utf-8')))
        if audit:
            allowed=json.loads((ROOT/PKG/'execution-contract.json').read_text())['allowed_files']
            files=set(git('diff','--name-only',BASE).splitlines()+git('ls-files','--others','--exclude-standard').splitlines())
            assert all(any(fnmatch.fnmatchcase(f,a) for a in allowed) for f in files),sorted(files)
            assert not git('diff',BASE,'--','schema','tests/fc08/reference','.aegis/materialized','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json','runtime/semantic','runtime/interaction/src/qualification_checkpoint.hpp')
    except (AssertionError, OSError, ValueError, subprocess.CalledProcessError) as e: errors.append(str(e))
    write(out/'PackageBindingPreflight.json',{'status':'BLOCKED' if errors else 'PASS','source_head':git('rev-parse','HEAD'),'checked':checked,'errors':errors})
    evidence_errors=[]
    try:
        v=json.loads((ROOT/PKG/'verification.lock.json').read_text());e=json.loads((ROOT/PKG/'evidence-contract.json').read_text());c=json.loads((ROOT/PKG/'execution-contract.json').read_text())
        assert all(all(o.get(k) for k in ['id','requirement','oracle','acceptance','evidence_required']) for o in v['obligations'])
        assert e['blocking'] and c['terminal_success'] and c['terminal_blockers'] and c['continue_until_terminal_state']
    except (AssertionError, OSError, ValueError) as e: evidence_errors.append(str(e))
    write(out/'EvidenceContractPreflight.json',{'status':'BLOCKED' if evidence_errors else 'PASS','errors':evidence_errors})
    print(json.dumps({'binding_errors':errors,'evidence_errors':evidence_errors}))
    return int(bool(errors or evidence_errors))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['materialize','preflight','audit']);p.add_argument('--repo',required=True);p.add_argument('--phase',default='preflight-001');a=p.parse_args();assert Path(a.repo).resolve()==ROOT
    if a.action=='materialize':materialize()
    else:raise SystemExit(preflight(a.phase,a.action=='audit'))
