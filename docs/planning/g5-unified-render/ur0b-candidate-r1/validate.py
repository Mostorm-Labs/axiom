#!/usr/bin/env python3
"""Static candidate validation only. --execution is a fail-closed admission probe.

No browser, Runtime, build, dependency resolver, activation writer or Gate verdict.
"""
import argparse
import copy
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
PACKAGE = ROOT/'.aegis/packages/G5-UR0b-candidate-r1'
RESULT = ROOT/'.aegis/results/G5-UR0b-candidate-r1'
ANCHOR = 'a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e'


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def load(path):
    def pairs(items):
        d = {}
        for key,value in items:
            require(key not in d,'duplicate JSON key: '+key)
            d[key]=value
        return d
    def invalid_constant(value):
        raise ValueError('non-JSON number: '+value)
    return json.loads(Path(path).read_text(),object_pairs_hook=pairs,parse_constant=invalid_constant)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git',*args],cwd=ROOT).decode().strip()


def root_ref(ref):
    path=(ROOT/ref['path']).resolve()
    require(path.is_relative_to(ROOT),'root reference escaped repository')
    require(path.is_file() and sha(path)==ref['sha256'],'missing/mismatched ref: '+ref['path'])
    if ref.get('identity_kind')=='git-and-content':
        require(ref.get('revision')==ANCHOR,'wrong source anchor')
        source=subprocess.check_output(['git','show',ANCHOR+':'+ref['path']],cwd=ROOT)
        require(hashlib.sha256(source).hexdigest()==ref['sha256'],'source does not match Git anchor')


def policy(package,scope,execution):
    require(package['status']=='FROZEN_CANDIDATE_PENDING_CONTROL_REVIEW','candidate status')
    require(package['authorization']['production_execution'] is False,'premature production authorization')
    require(package['authorization']['performance_sampling'] is False,'premature sampling authorization')
    require(package['authorization']['commit_push'] is False,'unrequested publication')
    require(scope['private_build']['default'] is False,'diagnostics default enabled')
    require(scope['private_build']['root_facade_changes'] is False,'public SDK change')
    require(scope['default_optimization_or_policy_change'] is False,'optimization mixed into baseline')
    require(scope['diagnostic_capacity_bytes']==16*1024**2 and scope['critical_reserve_bytes']==1024**2,
            'unbounded/mismatched diagnostic capacity')
    perf=execution['EXECUTION_CLOSURE_CONTRACT']['performance_execution']
    require(perf['budgets_mib']==[[32,96],[64,192]],'baseline budget changed')
    require(perf['concurrency']==1,'overlapping performance workers')
    require(perf['main_including_overhead_max_seconds']==6480 and perf['optional_diagnostic_max_seconds']==1080,
            'UR0a primary/diagnostic budget changed')
    require(perf['all_workers_max_seconds']==6480+1080+3*90+6*90==8370,'unbounded/miscalculated worker batch')
    require(perf['all_candidates_not_required_to_execute'] is True,'artificial full route obligation')
    require(execution['candidate_only'] is True,'candidate pretending active')
    returns=execution['EXECUTION_CLOSURE_CONTRACT']['return_policy']
    require(returns['official_gate_pass'] is False and returns['stop_after']=='UR0b', 'Gate/phase expansion')


def validate():
    package=load(PACKAGE/'package.json');scope=load(PACKAGE/'scope-contract.json')
    execution=load(PACKAGE/'execution-contract.json');authority=load(PACKAGE/'authority.lock.json')
    verification=load(PACKAGE/'verification.lock.json');evidence=load(PACKAGE/'evidence-contract.json')
    require(git('remote','get-url','origin')=='git@github.com:Mostorm-Labs/axiom.git','repository identity')
    require(git('branch','--show-current')=='codex/geometry-navigation-integration','branch identity')
    require(package['repository']=={'provider':'github','full_name':'Mostorm-Labs/axiom'},'declared repo')
    git('merge-base','--is-ancestor',ANCHOR,'HEAD')
    require(package['task_anchor']=={'revision':ANCHOR,'relation':'ancestor'},'anchor relation')
    require(not git('diff','--name-only',ANCHOR,'--'),'tracked source/design changed outside candidate window')
    local_refs=0
    for key in ('authority_lock_ref','verification_lock_ref','execution_contract_ref',
                'implementation_context_ref','evidence_contract_ref','scope_contract_ref'):
        ref=package[key];path=(PACKAGE/ref['path']).resolve()
        require(path.parent==PACKAGE and path.is_file() and sha(path)==ref['sha256'],'package-local ref '+key)
        local_refs+=1
    root_ref(package['source_plan_ref'])
    for ref in authority['sources']+authority['implementation_reality']:
        root_ref(ref)
    require(authority['status']=='CANDIDATE_PENDING_INDEPENDENT_CONTROL_ACCEPTANCE','authority self-accepted')
    for ref in (verification['verification_spec'],verification['trusted_basis']):
        root_ref(ref)
    perf=execution['EXECUTION_CLOSURE_CONTRACT']['performance_execution']
    for key in ('matrix_ref','protocol_ref'):
        root_ref(perf[key])
    policy(package,scope,execution)
    obligations=verification['obligation_set'];ids=[o['id'] for o in obligations]
    require(len(ids)==len(set(ids))==11,'obligation set')
    for obligation in obligations:
        for key in ('requirement','oracle','acceptance','evidence_required','blocking_reason'):
            require(obligation.get(key),'missing separate obligation field '+key)
    frozen=execution['EXECUTION_CLOSURE_CONTRACT']
    require({t['id'] for t in frozen['tests']['required']}==set(ids)==set(evidence['blocking_obligations']),
            'execution/verification/evidence obligation mismatch')
    require(frozen['hosted_verification']=={'required':[],'optional':[]},'unexpected hosted dependency')
    require(set(ids)==set(evidence['required_artifact_groups']),'evidence artifact mapping')
    require(evidence['required_artifact_groups']=={o['id']:o['evidence_required'] for o in obligations},
            'mismatched artifact names')
    require('BLOCKED_CONTROL_REVIEW' in frozen['terminal_blockers']['explicit_classes'],'no control boundary')
    matrix=load(ROOT/'docs/planning/g5-unified-render/ur0a/matrix.json')
    routes=load(ROOT/'docs/planning/g5-unified-render/ur0a/routes.json')
    expected={x for row in matrix['rows'] for x in row['required_routes']}
    require(set(perf['candidate_route_set'])==expected and expected<={r['route_id'] for r in routes['routes']},
            'candidate routes lost/unknown')
    for path in scope['allowed_existing_source_files']:
        require((ROOT/path).is_file(),'allowed existing source missing: '+path)
    require(not any('lock.json' in x or x.startswith('schema/') for x in scope['allowed_existing_source_files']),
            'dependency/schema source write scope')
    plan=(HERE/'plan.md').read_text()
    for index,title in enumerate(['Role','Authority','Objective','Non-goals','Required Analysis',
                                 'Required Output','Quality / Evidence Gate','Handoff'],1):
        require(f'## {index}. {title}' in plan,'missing stage output field '+title)
    for target in re.findall(r'\]\(([^)]+)\)',plan):
        require((HERE/target.split('#')[0]).exists(),'broken review link '+target)
    manifest=load(ROOT/'.aegis/results/G5-UR0a/output-sha256.json')
    for path,digest in manifest['outputs'].items():
        require(sha(ROOT/path)==digest,'UR0a output rewritten: '+path)
    prior=load(RESULT/'ur0a-reconciliation.json')
    for path,digest in prior['preserved_before_this_turn'].items():
        require(sha(ROOT/path)==digest,'preserved input rewritten: '+path)
    request=load(RESULT/'review-request.json')
    require(request['ur0b_candidate_ref']['sha256']==sha(PACKAGE/'package.json'),'review request package hash')
    require(request['not_a_review_verdict'] is True and request['official_gate_pass'] is False,
            'request is not independent approval')
    require(package['activation_requirements']['status']=='PENDING' and
            prior['independent_control_review_ref'] is None,'fabricated independent review')
    allowed=('.aegis/packages/G5-UR0b-candidate-r1/','.aegis/results/G5-UR0b-candidate-r1/',
             'docs/planning/g5-unified-render/ur0b-candidate-r1/')
    untracked=git('ls-files','--others','--exclude-standard').splitlines()
    fresh=[p for p in untracked if p not in prior['preserved_before_this_turn']]
    require(all(p.startswith(allowed) for p in fresh),'new file outside candidate preparation scope')
    for path in fresh:
        text=(ROOT/path).read_text()
        require(all(line.rstrip()==line for line in text.splitlines()),'trailing whitespace: '+path)
    git('diff','--check')
    negative=[]
    for case in ('premature-write','sampling','public-api','optimization','unbounded-ring',
                 'budget-change','overlapping-workers','self-gate'):
        p,s,e=copy.deepcopy(package),copy.deepcopy(scope),copy.deepcopy(execution)
        if case=='premature-write':p['authorization']['production_execution']=True
        if case=='sampling':p['authorization']['performance_sampling']=True
        if case=='public-api':s['private_build']['root_facade_changes']=True
        if case=='optimization':s['default_optimization_or_policy_change']=True
        if case=='unbounded-ring':s['diagnostic_capacity_bytes']=None
        if case=='budget-change':e['EXECUTION_CLOSURE_CONTRACT']['performance_execution']['budgets_mib']=[[64,192]]
        if case=='overlapping-workers':e['EXECUTION_CLOSURE_CONTRACT']['performance_execution']['concurrency']=2
        if case=='self-gate':e['EXECUTION_CLOSURE_CONTRACT']['return_policy']['official_gate_pass']=True
        try:
            policy(p,s,e)
        except ValueError as error:
            negative.append({'case':case,'result':'REJECTED_AS_EXPECTED','reason':str(error)})
        else:
            raise ValueError('policy counterexample accepted: '+case)
    return {'candidate_structure':'PASS','package_local_hash_refs':local_refs,
            'source_refs_verified':len(authority['sources'])+len(authority['implementation_reality']),
            'ur0a_preserved_outputs':len(manifest['outputs']),'preserved_input_files':len(prior['preserved_before_this_turn']),
            'frozen_obligations':ids,'main_matrix_rows':len(matrix['rows']),
            'main_planned_runs':sum(r['repetitions'] for r in matrix['rows']),
            'conditional_candidate_routes':len(expected),'policy_negative_cases':negative,
            'production_diff_empty':True,'new_browser_or_runtime_runs':0,
            'activation_status':'PENDING_INDEPENDENT_CONTROL_REVIEW','can_execute':False,
            'official_gate_pass':False,'head':git('rev-parse','HEAD'),
            'candidate_package_sha256':sha(PACKAGE/'package.json')}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--execution',action='store_true',help='Dry admission check; candidate cannot authorize execution')
    args=parser.parse_args()
    try:
        report={'status':'READY_FOR_CONTROL_REVIEW','phase':'UR0b_P31_CANDIDATE',**validate()}
        code=0
        if args.execution:
            report['status']='BLOCKED_CONTROL_REVIEW'
            report['reason']='No independently verified activation; no production mutation, build or sampling started.'
            code=3
    except (ValueError,KeyError,TypeError,OSError,subprocess.CalledProcessError) as error:
        report={'status':'INVALID_CANDIDATE','can_execute':False,'error':str(error)};code=1
    payload=json.dumps(report,ensure_ascii=False,indent=2)+'\n'
    if args.output:
        target=args.output.resolve()
        require(target.is_relative_to(RESULT),'output escaped candidate results root')
        target.write_text(payload)
    print(payload,end='')
    return code


if __name__=='__main__':
    raise SystemExit(main())
