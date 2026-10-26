"""Exact revised bootstrap-package preflights; no product/P32 or Gate verdict."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

GIT = 'D:/Softwares/Git/cmd/git.exe'
SUBJECT = '427580ae13b60a39e88959b282cbffc695c9aabb'
CANDIDATE = 'eb12da1acc4516e115d36767073f8bdc607f8a4d'
CANDIDATE_SHA = '52fc554901368ec1e4ece9ef2950f03eefdbd55cbf67def6ff68785b47f85a8c'


def exact_revision(value):
    if not re.fullmatch('[0-9a-f]{40}',value): raise ValueError('Full exact revision required')
    return value


def safe_path(root, value):
    root = root.resolve()
    path = (root/value).resolve()
    if not path.is_relative_to(root): raise ValueError('Reference escapes root: '+value)
    return path


def acceptance_matches(receipt, revision, digest):
    return (receipt.get('accepted') is True and receipt.get('candidate_revision') == revision
            and receipt.get('candidate_package_sha256') == digest
            and set(receipt.get('accepts',[])) == {'source_truth','profile_matrix'}
            and receipt.get('product_P32_authorized') is False)


def preflight(root, revision, package_path, phase):
    root = root.resolve(); commands = []; checked = []
    def command(argv):
        run = subprocess.run(argv,cwd=root,capture_output=True,text=True,encoding='utf-8',errors='replace')
        commands.append({'argv':argv,'cwd':str(root),'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
        if run.returncode: raise ValueError('Command failed: '+str(argv))
        return run.stdout.strip()
    def git(*args): return command([GIT,*args])
    def load(path): return json.loads(safe_path(root,path).read_text(encoding='utf-8'))
    def verify(ref):
        path = safe_path(root,ref['path']);data=path.read_bytes()
        if 'sha256' in ref and hashlib.sha256(data).hexdigest()!=ref['sha256']: raise ValueError('SHA mismatch: '+ref['path'])
        if 'size' in ref and len(data)!=ref['size']: raise ValueError('Size mismatch: '+ref['path'])
        if 'git_blob_sha' in ref and git('rev-parse',ref.get('revision',SUBJECT)+':'+ref['path']) != ref['git_blob_sha']: raise ValueError('Git blob mismatch: '+ref['path'])
        checked.append(ref['path'])
    def refs(value):
        if isinstance(value,dict):
            if 'path' in value and ('sha256' in value or 'git_blob_sha' in value): verify(value)
            for v in value.values(): refs(v)
        elif isinstance(value,list):
            for v in value: refs(v)
    report = {'format':'fc08-accepted-bootstrap-preflight-v1',
              'preflight_name':'PackageBindingPreflight' if phase=='package' else 'EvidenceContractPreflight',
              'package_revision':revision,'package_path':package_path,'qualification_subject_revision':SUBJECT,
              'product_P32_authorized':False,'product_tests_executed':False,'valid_product_RED_observed':False,
              'actual_commands':commands,'environment':{'python':sys.version,'executable':sys.executable,'platform':platform.platform()},
              'verified_paths':checked,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    try:
        exact_revision(revision)
        remote = git('remote','get-url','origin').lower().removesuffix('.git')
        if remote not in ['https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom']: raise ValueError('BLOCKED_REPOSITORY_IDENTITY')
        identity = json.loads(command(['gh','api','repos/Mostorm-Labs/axiom','--jq','{full_name:.full_name,id:.id}']))
        if identity != {'full_name':'Mostorm-Labs/axiom','id':1306396623}: raise ValueError('BLOCKED_REPOSITORY_IDENTITY')
        report['repository_identity']=identity
        report['observed_head']=git('rev-parse','HEAD');report['git_status']=git('status','--porcelain')
        for rev in [SUBJECT,CANDIDATE,revision]: git('merge-base','--is-ancestor',rev,'HEAD')
        package=load(package_path);directory=str(Path(package_path).parent.as_posix())
        if package['product_P32_authorized'] or package['authorized_work']!='verification_bootstrap_only': raise ValueError('Product permission or scope conflict')
        for name in ['package.json','authority.lock.json','verification.lock.json','execution-contract.json','evidence-contract.json','implementation-context.md','acceptance.json']:
            path=directory+'/'+name
            raw=subprocess.check_output([GIT,'show',revision+':'+path],cwd=root)
            if raw!=safe_path(root,path).read_bytes(): raise ValueError('Package differs from exact revision: '+path)
        locks={}
        for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','evidence_contract_ref','implementation_context_ref','acceptance_ref']:
            ref=package[key];local=safe_path(root/directory,ref['path'])
            verify({'path':local.relative_to(root).as_posix(),'sha256':ref['sha256']})
            if key not in ['implementation_context_ref']: locks[key]=json.loads(local.read_text(encoding='utf-8'))
        approval=locks['acceptance_ref']
        if not acceptance_matches(approval,CANDIDATE,CANDIDATE_SHA): raise ValueError('Missing exact human acceptance')
        authority=locks['authority_lock_ref'];verification=locks['verification_lock_ref'];closure=locks['execution_contract_ref'];evidence=locks['evidence_contract_ref']
        refs(authority);refs(verification)
        for axis in ['semantic','execution_policy']:
            if authority['current_authorities'][axis]['status']!='Current': raise ValueError('Authority not Current: '+axis)
        if not closure['continue_until_terminal_state'] or closure['product_P32_authorized']: raise ValueError('Invalid closure policy')
        protected=git('diff','--name-only',SUBJECT,'--','runtime','schema','apps','debug_ui','CMakeLists.txt','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json')
        if protected: raise ValueError('Production subject changed: '+protected)
        # Reuse accepted-candidate audit as one consistency mechanism; it is
        # explicitly not product execution or an independent authority decision.
        spec=importlib.util.spec_from_file_location('candidate_audit',root/'tools/fc08/audit_input_amendment.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        report['corpus_audit']=module.audit(root)
        if verification['design']['corpus_manifest']!=load('.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.2/package.json')['corpus']: raise ValueError('Revised corpus not bound')
        matrix=verification['profile_policy']['exact_matrix']
        verify(matrix);verify(verification['profile_policy']['schema']);verify(verification['profile_policy']['methods'])
        if len(verification['obligations'])!=10: raise ValueError('Obligation family count changed')
        for obligation in verification['obligations']:
            if not all(obligation.get(k) for k in ['requirement','oracle','acceptance','evidence_required']): raise ValueError('Incomplete obligation '+obligation['id'])
        result=load(verification['design']['result_contract']['path'])
        for key in ['required_run_fields','required_case_fields']:
            if evidence[key]!=result[key]: raise ValueError('Evidence contract mismatch: '+key)
        if evidence['product_P32_authorized'] or evidence['result_values']!=result['results']: raise ValueError('Invalid evidence permission/result states')
        report.update(status='PASS_PACKAGE_BINDING' if phase=='package' else 'PASS_EVIDENCE_CONTRACT',exact_blockers=[],production_source_unchanged=True,
                      accepted_input_binding=True,runner_status='Authorized output of bounded bootstrap; not evidence of product coverage until implemented and qualified')
    except (ValueError,AssertionError,KeyError,OSError,subprocess.CalledProcessError) as exc:
        report.update(status='BLOCKED_PREFLIGHT',exact_blockers=[{'classification':'TASK_PACKAGE_DEFECT','finding':str(exc)}])
    return report


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path)
    p.add_argument('--package-revision',required=True);p.add_argument('--package-path',required=True)
    p.add_argument('--phase',choices=['package','evidence'],required=True);p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();report=preflight(a.repo,a.package_revision,a.package_path,a.phase)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('x',encoding='utf-8',newline='\n') as stream:stream.write(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'status':report['status'],'exact_blockers':report['exact_blockers']},ensure_ascii=False))
    raise SystemExit(0 if report['status'].startswith('PASS_') else 2)


if __name__=='__main__': main()
