"""Materialize explicit user acceptance and a bounded verification-only package."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OLD='.aegis/packages/FC-08-PRODUCT-P31-v0.1'
NEW='.aegis/packages/FC-08-P31-BOOTSTRAP-v0.2'
CANDIDATE='.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.2/package.json'

def load(path):return json.loads((ROOT/path).read_text(encoding='utf-8'))
def ref(path):
    data=(ROOT/path).read_bytes()
    return {'path':path,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)}
def write(name,value):
    path=ROOT/NEW/name;path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('x',encoding='utf-8',newline='\n') as stream:
        stream.write(value if isinstance(value,str) else json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    return {'path':name,'sha256':ref(NEW+'/'+name)['sha256']}

def main():
    amendment=load(CANDIDATE)
    acceptance=write('acceptance.json',{
        'format':'fc08-input-acceptance-v1','source':'Direct human reply in this task',
        'question_item_id':'["request_user_input_async","call_3993d292d55640189e2306136197064d",0]',
        'answer_verbatim':'接受验证修正与矩阵，继续预检（推荐）',
        'accepted':True,'candidate_revision':'eb12da1acc4516e115d36767073f8bdc607f8a4d',
        'candidate_package_sha256':ref(CANDIDATE)['sha256'],
        'accepts':['source_truth','profile_matrix'],'product_P32_authorized':False,
        'scope':'Accept exact source-truth correction and profile matrix; continue previously authorized verification-bootstrap work, not product P32.'})
    authority=load(OLD+'/authority.lock.json')
    authority['accepted_verification_input_amendment']=dict(ref(CANDIDATE),revision='eb12da1acc4516e115d36767073f8bdc607f8a4d')
    authority['human_input_acceptance']=ref(NEW+'/acceptance.json')
    verification=deepcopy(load(OLD+'/verification.lock.json'))
    verification['status']='INPUTS_ACCEPTED_FOR_VERIFICATION_BOOTSTRAP_ONLY'
    verification['design']['corpus_manifest']=amendment['corpus']
    verification['profile_policy'].update(exact_matrix=amendment['matrix'],schema=amendment['matrix_schema'],methods=amendment['method_bindings'])
    verification['profile_policy']['execution_rule']='Use the exact accepted external matrix for every variant/profile; original variant index remains unchanged. NOT_APPLICABLE, NOT_RUN and BLOCKED_ENVIRONMENT are distinct.'
    verification['accepted_input_amendment']=dict(ref(CANDIDATE),revision='eb12da1acc4516e115d36767073f8bdc607f8a4d')
    verification['historical_materialization_only']='Old runner/probe artifacts remain bound as historical observations, never substituted for new product RED.'
    closure={
        'format':'aegis-execution-closure-contract-v1','authorized_work':'verification_bootstrap_only',
        'required_changes':['Persist exact acceptance and revised immutable input/profile bindings',
                            'Run both exact-revision package preflights in actual worktree with --repo .',
                            'Record accepted owner/source design bindings and real F/U, History reservation, atomic rejection, Scene/recovery and preview handoff',
                            'Establish scoped actual-SUT product runner with independent predicate evaluation and sensitivity tests; retain all302 discovery IDs and no false coverage',
                            'Execute FC08-RED-01 against unmodified427580ae production bytes on Windows Native, actual Chrome/WASM and applicable headless boundary',
                            'Persist all commands, hashes, raw results and exact reviewer-accessible revision, or a precise terminal blocker'],
        'allowed_paths':['.aegis/packages/FC-08-P31-BOOTSTRAP-v0.2/**','.aegis/results/FC-08-P20-P31-CLOSURE/**',
                         '.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-003/**',
                         'tools/fc08/accepted_preflight.py','tools/fc08/product_*.py','tools/fc08/run_product_verification.py',
                         'tests/fc08/accepted_preflight_test.py','tests/fc08/product_runner_test.py','tests/fc08/product/**'],
        'forbidden_changes':['Production runtime/schema/registry/hosts/debug UI/root CMake and SDK locks',
                             'All old packages/results/frozen inputs/expected/oracles/qualification-hook semantics',
                             'Global Aegis registries, main merge, product P32 or DEV/P34/PHYS/release verdicts'],
        'red_policy':{'source':'accepted P20 R01/R02/R03 + CR01',
                      'R01':'Actual normal production codec/ingress rejection of independently valid new Section/17/18 positives. Invalid fixture/root, environment/build failure, descriptor/hook/synthetic-only observation never counts as product RED.',
                      'R03':'Missing new workflow/owner is NOT_RUN or NOT_APPLICABLE for predecessor behavior, never advertised as every future temporal behavior observed RED.',
                      'oracle_defect':'Unexpected pass of known-absent positive returns VERIFICATION_DESIGN_DEFECT; no expected rewrite.'},
        'tests':['Accepted input consistency/independence audit','Preflight checker negative controls','Exact PackageBindingPreflight','Exact EvidenceContractPreflight',
                 'ImplementationDesignPreflight','Actual runner unit/sensitivity tests','Actual native/browser/headless RED with normal positive inherited control'],
        'hosted_verification':{'required':[],'optional':['Corroborative CI; no new hosted requirement']},
        'terminal_success':['Both exact package preflights pass','Design preflight has no unresolved conflict','Scoped real runner/tests qualified and real R01 RED recorded on required profiles','Exact reviewer-accessible result; no product P32 permission'],
        'terminal_blockers':['AUTHORITY_CONFLICT','VERIFICATION_DESIGN_DEFECT','TASK_PACKAGE_DEFECT','BLOCKED_ENVIRONMENT','MISSING_REAL_OBSERVATION_SEAM','BLOCKED_EVIDENCE'],
        'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW','product_P32_authorized':False}
    evidence=load(OLD+'/evidence-contract.json')
    evidence['scope']='P31 verification bootstrap only; no product Gate evaluation'
    evidence['product_P32_authorized']=False
    evidence['bootstrap_required_artifacts']=['acceptance identity','exact two preflights','design source bindings','runner tests','actual per-variant raw SUT observations and commands','RED interpretation','preservation','terminal receipt']
    context='''# Accepted FC08 verification bootstrap\n\nThe direct human reply accepts candidate eb12da1a source truth and the exact302-ID profile matrix. Old candidate files remain unchanged; acceptance is a separate receipt.\n\nProduction subject427580ae remains unmodified and has no Section/17/18 implementation. Existing Facade -> mounted owner -> Interaction -> Semantic prepare/atomic commit -> Scene publication/recovery -> preview handoff remains the only target flow. Do not implement missing product semantics in adapters or qualification hooks.\n\nUse exact accepted corpus and independent expected files. Implement actual-SUT runner tooling, not a capability-summary stub. R01 must invoke a real normal codec/ingress with independently valid bytes. R03 permits absent future workflow observations to remain not executed; it does not qualify those product obligations. Bind every available observation and report unavailable seams explicitly. This package does not accept the full product P31 or grant product P32.\n'''
    refs={
        'authority_lock_ref':write('authority.lock.json',authority),
        'verification_lock_ref':write('verification.lock.json',verification),
        'execution_contract_ref':write('execution-contract.json',closure),
        'evidence_contract_ref':write('evidence-contract.json',evidence),
        'implementation_context_ref':write('implementation-context.md',context),
        'acceptance_ref':acceptance}
    package={'format':'aegis-repo-local-execution-package-v1','package_id':'FC-08-P31-BOOTSTRAP-v0.2',
             'task_id':'FC-08-P31-BOOTSTRAP','stage':'P31_VERIFICATION_BOOTSTRAP',
             'status':'AUTHORIZED_BOUNDED_VERIFICATION_BOOTSTRAP','execution_authority_mode':'repo_materialized',
             'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom','repository_id':1306396623},
             'execution_ref':'refs/heads/codex/fc08-p20-p31-closure',
             'task_anchor':{'revision':'427580ae13b60a39e88959b282cbffc695c9aabb','relation':'ancestor'},
             'resume_cursor':{'revision':'0d13b63702facc1534e815643c3bc6085d9c6618','completed_through':['Candidate inputs/matrix audited and published; human acceptance now recorded'],'next_action':'Run both exact revised package preflights, then design binding and actual runner/RED'},
             'authorized_work':'verification_bootstrap_only','product_P32_authorized':False,
             'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW',
             'parent_product_package':dict(ref(OLD+'/package.json'),revision='f67b9db29090449799bd3e05ef7f1d269dab6d31'),
             **refs}
    write('package.json',package)
    print(json.dumps(ref(NEW+'/package.json'),indent=2))

if __name__=='__main__':main()
