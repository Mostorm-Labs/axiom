"""Audit exact FC08 product P31 bindings without product mutation or fake RED.

Exit0 is a complete package/evidence preflight only. Exit2 is a precise frozen
binding/verification blocker. Neither result grants product P32 permission.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

PACKAGE_REVISION='f67b9db29090449799bd3e05ef7f1d269dab6d31'
BASELINE='427580ae13b60a39e88959b282cbffc695c9aabb'
MATERIALIZATION='0d3b10bba1ac4a31d669dd4a45f94975ec24f3c3'
PACKAGE_PATH='.aegis/packages/FC-08-PRODUCT-P31-v0.1'
GIT='D:/Softwares/Git/cmd/git.exe'

def digest(data):return hashlib.sha256(data).hexdigest()
def matches_digest(data,expected):return digest(data)==expected
def safe_ref(root,relative):
    path=(root/relative).resolve()
    if not path.is_relative_to(root.resolve()):raise ValueError('reference escapes repository: '+relative)
    return path
def load(path):return json.loads(path.read_text(encoding='utf-8'))
def explicit_profile_bindings(rows):
    result={}
    for row in rows:
        for key in ['profiles','profile_applicability','applicable_profiles']:
            if key in row:result[row['variant_id']]=row[key];break
    return result
def shape_conflicts(scenario,identities,kind_names):
    result=[]
    records={r['id']['value']:r for r in scenario.get('records',[]) if r.get('kind_id')==1}
    for name,geometry in scenario.get('kind_geometry',{}).items():
        r=records.get(identities.get(name))
        if r is None:continue
        kind=r.get('content',{}).get('shape',{}).get('shape_kind')
        actual=kind_names.get(kind)
        if actual is not None and actual!=geometry:
            result.append({'object_name':name,'object_id':r['id']['value'],'canonical_shape_kind':kind,
                'canonical_geometry':actual,'reference_geometry':geometry,
                'canonical_pointer':f"/records/{scenario['records'].index(r)}/content/shape/shape_kind",
                'reference_pointer':'/kind_geometry/'+name})
    return result

def audit(root,phase):
    commands=[];binding_errors=[];verified=[]
    def command(argv):
        r=subprocess.run(argv,cwd=root,capture_output=True,text=True,encoding='utf-8',errors='replace')
        commands.append({'argv':argv,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        return r
    def git(*args):return command([GIT,*args])
    remote=git('remote','get-url','origin')
    normalized=remote.stdout.strip().lower().removesuffix('.git').rstrip('/')
    repo_ok=remote.returncode==0 and normalized in ['https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom']
    github=command(['gh','api','repos/Mostorm-Labs/axiom','--jq','{full_name:.full_name,id:.id}'])
    try:identity=json.loads(github.stdout)
    except json.JSONDecodeError:identity={}
    repo_ok=repo_ok and identity.get('full_name','').lower()=='mostorm-labs/axiom' and identity.get('id')==1306396623
    if not repo_ok:binding_errors.append('BLOCKED_REPOSITORY_IDENTITY')
    head=git('rev-parse','HEAD').stdout.strip()
    dirty=git('status','--porcelain').stdout
    for revision in [BASELINE,MATERIALIZATION,'a1e8819abe245ab7487f18771970721f480ff487',PACKAGE_REVISION]:
        if git('merge-base','--is-ancestor',revision,'HEAD').returncode:binding_errors.append('ancestry: '+revision)
    production_paths=['runtime','schema','apps','debug_ui','CMakeLists.txt','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json']
    production_diff=git('diff','--name-only',BASELINE,'--',*production_paths)
    baseline_equivalence=git('diff','--name-only',MATERIALIZATION,BASELINE,'--',*production_paths)
    if production_diff.returncode or production_diff.stdout.strip():binding_errors.append('unmodified production subject violated')
    files={}
    for name in ['package.json','authority.lock.json','verification.lock.json','execution-contract.json','evidence-contract.json','implementation-context.md']:
        relative=PACKAGE_PATH+'/'+name
        frozen=git('show',PACKAGE_REVISION+':'+relative)
        # Git's text conversion above is only an observed diagnostic; byte
        # identity uses the raw blob, with no newline/encoding normalization.
        raw=subprocess.check_output([GIT,'show',PACKAGE_REVISION+':'+relative],cwd=root)
        local=safe_ref(root,relative).read_bytes()
        if local!=raw:binding_errors.append('package changed: '+relative)
        files[name]={'path':relative,'sha256':digest(raw),'bytes':len(raw)}
    package=load(root/PACKAGE_PATH/'package.json')
    authority=load(root/PACKAGE_PATH/'authority.lock.json')
    verification=load(root/PACKAGE_PATH/'verification.lock.json')
    evidence=load(root/PACKAGE_PATH/'evidence-contract.json')
    for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
        row=package[key];data=safe_ref(root/PACKAGE_PATH,row['path']).read_bytes()
        if not matches_digest(data,row['sha256']):binding_errors.append('package local SHA: '+row['path'])
    def refs(value,label):
        if isinstance(value,dict):
            if 'path' in value and ('sha256' in value or 'git_blob_sha' in value):
                path=safe_ref(root,value['path']);ok=path.is_file()
                if ok and 'sha256' in value:ok=matches_digest(path.read_bytes(),value['sha256'])
                if 'git_blob_sha' in value:
                    r=git('rev-parse',value.get('revision',BASELINE)+':'+value['path'])
                    ok=ok and r.returncode==0 and r.stdout.strip()==value['git_blob_sha']
                verified.append({'label':label,'path':value['path'],'matched':ok})
                if not ok:binding_errors.append('frozen ref mismatch: '+label)
            for key,v in value.items():refs(v,label+'.'+key)
        elif isinstance(value,list):
            for i,v in enumerate(value):refs(v,label+'.'+str(i))
    refs(authority,'authority');refs(verification,'verification')
    for name in ['semantic','execution_policy']:
        if authority['current_authorities'][name]['status']!='Current':binding_errors.append('noncurrent authority: '+name)
    if package['P32_authorized'] is not False or package['may_start_code_execution'] is not False:
        binding_errors.append('product permission unexpectedly changed')
    vdesign=verification['design'];manifest=load(root/vdesign['corpus_manifest']['path']);rows=manifest['variants']
    index=load(root/vdesign['variant_index']['path']);catalog=load(root/vdesign['catalog_path'])
    ids=[r['variant_id'] for r in rows]
    if len(ids)!=302 or len(set(ids))!=302 or set(ids)!={r['variant_id'] for r in index['variants']}:
        binding_errors.append('variant identity/completeness mismatch')
    if len(catalog['cases'])!=60 or len(verification['obligations'])!=10:binding_errors.append('logical/family identity mismatch')
    input_errors=[];conflicts=[]
    names=load(root/'.aegis/materialized/FC-08/sources/accepted-seed/verification/fixtures/identity-map.json')
    registry_path='schema/axiom/v1/registry/shape_kind_registry_v1.yaml'
    registry=(root/registry_path).read_text(encoding='utf-8')
    assert re.search(r'id: 1\s+symbol: RECTANGLE',registry) and re.search(r'id: 2\s+symbol: ELLIPSE',registry)
    kind_names={1:'rectangle',2:'ellipse'}
    for row in rows:
        for field in ['input','expected']:
            ref=row[field]
            if not matches_digest(safe_ref(root,ref['path']).read_bytes(),ref['sha256']):input_errors.append(row['variant_id']+': '+field)
        for wire in row['wire']:
            if 'path' in wire and not matches_digest(safe_ref(root,wire['path']).read_bytes(),wire['sha256']):input_errors.append(row['variant_id']+': wire')
        scenario=load(root/row['input']['path'])
        for found in shape_conflicts(scenario,names,kind_names):
            conflicts.append({'variant_id':row['variant_id'],'input':row['input'],'expected':row['expected'],**found})
    bindings=explicit_profile_bindings(rows)
    absent=[r['variant_id'] for r in rows if r['variant_id'] not in bindings]
    design_file=load(root/'.aegis/materialized/FC-08/executable-verification.json')
    # The package explicitly refers to these frozen files as the exact source
    # of applicability. A global list of three profiles is not a variant map.
    top_matrix={k:manifest[k] for k in ['profile_applicability','profile_matrix'] if k in manifest}
    result_contract=load(root/verification['design']['result_contract']['path'])
    for name in ['required_run_fields','required_case_fields','result_values']:
        if name in result_contract and result_contract[name]!=evidence[name]:binding_errors.append('result-contract '+name)
    blockers=[]
    if conflicts:blockers.append({'id':'BOOT-VER-01','classification':'VERIFICATION_DESIGN_DEFECT','owner':'P20 Verification / control review',
        'finding':'Frozen reference geometry and canonical shape record disagree. Cannot repair in SUT adapter or silently rewrite an accepted fixture.',
        'affected':conflicts,'resolution_required':'Decide the intended canonical geometry and review the exact input/oracle delta. Preserve current bytes until authorized.'})
    if absent and not top_matrix:blockers.append({'id':'BOOT-PKG-02','classification':'TASK_PACKAGE_DEFECT','owner':'P31 control review, P20 if applicability truth is unresolved',
        'finding':'verification.lock.profile_policy requires exact headless applicability from variant-index/corpus manifest; neither file supplies that mapping.',
        'missing_profile_variant_ids':absent,'declared_global_profiles':design_file.get('profiles'),
        'profile_policy':verification['profile_policy'],
        'resolution_required':'Bind an exact per-variant profile applicability table with reasons/authority. Do not infer headless coverage from wire presence or mark absent entries PASS.'})
    if binding_errors or input_errors:blockers.append({'id':'BOOT-BIND-03','classification':'TASK_PACKAGE_DEFECT','owner':'P31 control review','binding_errors':binding_errors,'input_errors':input_errors})
    runner=root/'tools/fc08/run_product_verification.py'
    if not runner.is_file():pending='Authorized repair target, not by itself the terminal blocker. Real runner establishment is stopped by the unresolved frozen verification bindings above.'
    else:pending='An entrypoint exists; no claim it implements the required frozen SUT adapters until qualified.'
    return {'format':'fc08-product-bootstrap-preflight-v1','phase':phase,
        'preflight_name':'PackageBindingPreflight' if phase=='package' else 'EvidenceContractPreflight',
        'status':'BLOCKED_VERIFICATION' if blockers else ('PASS_PACKAGE_BINDING' if phase=='package' else 'PASS_EVIDENCE_CONTRACT'),
        'package_revision':PACKAGE_REVISION,'package_files':files,'observed_head':head,
        'checker_identity':{'path':'tools/fc08/bootstrap_preflight.py','sha256':digest(Path(__file__).read_bytes())},
        'source_tree_dirty':bool(dirty),'git_status':dirty,'production_source_unchanged':not production_diff.stdout.strip(),
        'materialization_and_product_baseline_production_bytes_equal':not baseline_equivalence.stdout.strip(),
        'qualification_subject_revision':BASELINE,'materialization_subject':MATERIALIZATION,
        'repository_identity':identity,'repository_identity_ok':repo_ok,'package_byte_bindings_ok':not binding_errors,
        'frozen_input_bytes_ok':not input_errors,'verified_refs':verified,'source_shape_registry':{'path':registry_path,'sha256':digest((root/registry_path).read_bytes())},
        'product_runner_present':runner.is_file(),'product_runner_diagnosis':pending,'exact_profile_bindings_present':bool(bindings or top_matrix),
        'exact_blockers':blockers,'actual_commands':commands,'environment':{'platform':platform.platform(),'python':sys.version,'executable':sys.executable},
        'product_tests_executed':False,'valid_product_RED_observed':False,'product_P32_authorized':False,'P34_PASS':False}

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True)
    p.add_argument('--package-revision',required=True);p.add_argument('--phase',choices=['package','evidence'],required=True)
    p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    if a.package_revision!=PACKAGE_REVISION:raise ValueError('this bounded checker only accepts the exact authorized product package revision')
    result=audit(a.repo.resolve(),a.phase)
    a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'preflight':result['preflight_name'],'status':result['status'],'package_byte_bindings_ok':result['package_byte_bindings_ok'],
        'frozen_input_bytes_ok':result['frozen_input_bytes_ok'],'blockers':[b['id'] for b in result['exact_blockers']],
        'product_tests_executed':False,'report':str(a.report)},indent=2))
    return 2 if result['status']=='BLOCKED_VERIFICATION' else 0
if __name__=='__main__':sys.exit(main())
