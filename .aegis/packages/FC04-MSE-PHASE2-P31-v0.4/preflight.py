#!/usr/bin/env python3
"""Pre-implementation baseline and verification-binding checks for FC04 MSE."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import xml.etree.ElementTree as ET

P = pathlib.Path(__file__).resolve().parent
ROOT = P.parents[2]
CURSOR = '6bcf2d4bbf3314e5252951190c3c475403f52665'
REVIEW = 'e2fb9be07cd596df4193062c2fed27b6316fd8c5'
BRANCH = 'codex/fc04-mse-p31-v02-binding-fix'

def need(value, reason):
    if not value:
        raise ValueError(reason)

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()

def read(name):
    return json.loads((P / name).read_text(encoding='utf-8'))

def sha(data):
    return hashlib.sha256(data).hexdigest()

def verify_locked_file(e, head):
    path=e['path']; data=subprocess.check_output(['git','show',CURSOR+':'+path],cwd=ROOT)
    need(git('rev-parse',CURSOR+':'+path)==e['git_blob_sha'] and sha(data)==e['sha256'], 'cursor lock mismatch '+path)
    current=subprocess.check_output(['git','show',head+':'+path],cwd=ROOT)
    need(sha(current)==e['sha256'], 'HEAD differs from baseline '+path)
    need(sha((ROOT/path).read_bytes())==e['sha256'], 'worktree differs from baseline '+path)

def binding():
    p=read('package.json');head=git('rev-parse','HEAD')
    need(git('branch','--show-current')==BRANCH,'execution branch mismatch')
    remote=git('remote','get-url','origin').lower().removesuffix('.git')
    need(remote in ('https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom'),'repository mismatch')
    for rev in (CURSOR,REVIEW):
        need(subprocess.run(['git','merge-base','--is-ancestor',rev,head],cwd=ROOT).returncode==0,'missing ancestry '+rev)
    need(not git('status','--porcelain=v1','--untracked-files=all'),'tracked or untracked worktree changes')
    need(p['p32_authorized'] is False and p['requires_control_review'] is True,'package authorization state altered')
    old=subprocess.check_output(['git','show',p['prior_package_revision']+':.aegis/packages/FC04-MSE-PHASE2-P31-v0.3/package.json'],cwd=ROOT)
    need(sha(old)==p['prior_package_sha256'],'v0.3 package mismatch')
    for name in ('source-lock.json','input-lock.json'):
        lock=read(name);need(lock['subject_revision']==CURSOR,name+' subject mismatch')
        for e in lock['files']:verify_locked_file(e,head)
    for e in read('integrity.json')['files']:
        actual=(P/e['path']).read_bytes(); tracked=subprocess.check_output(['git','show',head+':'+P.relative_to(ROOT).as_posix()+'/'+e['path']],cwd=ROOT)
        need(sha(actual)==e['sha256'] and actual==tracked,'package mismatch '+e['path'])
    manifest=subprocess.check_output(['git','show',head+':'+P.relative_to(ROOT).as_posix()+'/integrity.json'],cwd=ROOT)
    need(manifest==(P/'integrity.json').read_bytes(),'integrity manifest differs from HEAD')
    print('PASS PackageBindingPreflight: exact source/input at cursor, HEAD and worktree; clean baseline; HEAD='+head)

def evidence():
    m=read('test-matrix.json');rows=m['test_cases']
    need([r['id'] for r in rows]==['MSE-%02d'%i for i in range(1,28)],'incorrect MSE ID set')
    for r in rows:
        n=r['id'][-2:]
        for key in ('fixture','expected','test_source','test_selector','ctest_name','command','evidence','oracle'):
            need(isinstance(r.get(key),str) and bool(r[key].strip()),r['id']+' missing '+key)
        need(r['test_source']=='verification/fc04_mse/mse_01_27_test.cpp',r['id']+' source mismatch')
        need(r['test_selector']=='MultiSelectionMSE.MSE_'+n and r['ctest_name']=='FC04.MSE-'+n,r['id']+' selector mismatch')
        need('MSE-'+n+'.xml' in r['evidence'] and '--no-tests=error' in r['command'],r['id']+' evidence/zero match mismatch')
    need('Escape' in rows[24]['case'] and 'owning' in rows[24]['case'],'MSE-25 semantics lost')
    need(len(m['required_property_rows'])==8,'property rows incomplete')
    need(m['web_runner']['case_ids']==['WEB-MSE-'+x for x in ('03','07','19','20','24','25','26','27')],'web case map mismatch')
    for r in m['inherited_regressions']:
        path=ROOT/r['source'];need(path.is_file(),'missing historical evidence '+str(path))
        if path.suffix=='.xml':
            tree=ET.parse(path);names=[n.get('name','') for n in tree.iter('testcase')]
            need(not any(n.tag in ('failure','error','skipped') for n in tree.iter()),'historical failure '+r['id'])
        else:names=[s.strip() for s in path.read_text(encoding='utf-8').splitlines() if s.strip()]
        need(len(names)==r['expected_count'] and len(set(names))==len(names),'wrong historical IDs '+r['id'])
    for name,count in (('d07',14),('selection',160)):
        ids=(ROOT/f'.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/{name}-names.txt').read_text(encoding='utf-8').splitlines()
        tree=ET.parse(ROOT/f'.aegis/results/FC04-D07-FUNCTIONAL-P32-001/raw/final-551/{name}_{count}.xml')
        need({n.get('name','') for n in tree.iter('testcase')}==set(filter(None,ids)),'historical ID mismatch '+name)
        need(not any(n.tag in ('failure','error','skipped') for n in tree.iter()),'historical failure '+name)
    need(m['oom_blocker']['status']=='BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG','OOM blocker removed')
    print('PASS EvidenceContractPreflight: 27 explicit fixtures, expected outcomes, selectors and evidence; inherited cases; implementation NOT_RUN')

def design():
    d=(P/'design.md').read_text(encoding='utf-8');v=(P/'verification.md').read_text(encoding='utf-8')
    for s in ('RuntimeComposition','owner-thread-only','CanvasControlService','explicit eligible cohort the user confirms','C ABI','preimages','typed changed/skipped'):
        need(s in d,'design missing '+s)
    for s in ('preparer self-review','does not waive oracle independence','real RuntimeComposition','actual built','--gtest_fail_if_no_test_selected','unchanged'):
        need(s in v,'verification missing '+s)
    print('PASS DesignPreflight: selected port, concrete oracle and explicit review boundary')

def scope():
    c=read('implementation-context.json');m=read('test-matrix.json');runner=m['runner']
    for path in ('runtime/composition/CMakeLists.txt','runtime/composition/src/canvas_runtime_facade.cpp','runtime/control/src/canvas_control_service.cpp',
                 'runtime/control/tests/canvas_control_service_test.cpp','runtime/composition/tests/selection_input_integration_test.cpp',
                 'debug_ui/include/canvas/debug_ui/controller.hpp','apps/minimal_runtime_host/windows_main.cpp','apps/axiom_canvas_demo/platform/web/web_debug_ui.inl'):
        need(path in c['allowed_modification_paths'],'required source out of scope '+path)
    for path in ('runtime/composition/include/canvas/runtime/internal/multi_selection_port.hpp','verification/fc04_mse/CMakeLists.txt',
                 'verification/fc04_mse/mse_01_27_test.cpp','apps/axiom_canvas_demo/tests/web_fc04_mse.mjs'):
        need(path in c['allowed_new_paths'],'required new source out of scope '+path)
    for path in ('runtime/foundation/include/canvas/runtime/runtime_facade.hpp','runtime/foundation/include/canvas/runtime/canvas_control_types.hpp',
                 'schema/axiom/v1/registry/field_registry_v1.yaml','semantic-sdk.lock.json'):
        need(path in c['forbidden_paths'],'protected boundary missing '+path)
    need(runner['parent_cmake']=='runtime/composition/CMakeLists.txt' and runner['ctest_count']==27 and
         runner['test_filter_zero_behavior']=='--gtest_fail_if_no_test_selected','test discovery binding mismatch')
    print('PASS FunctionalScopePreflight: source/ABI boundaries and nonzero 27-case runner entry specified')

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--check',required=True,choices=['PackageBindingPreflight','EvidenceContractPreflight','DesignPreflight','FunctionalScopePreflight','all']);a=p.parse_args()
    need(pathlib.Path(a.repo).resolve()==ROOT.resolve(),'--repo must identify real worktree')
    checks={'PackageBindingPreflight':binding,'EvidenceContractPreflight':evidence,'DesignPreflight':design,'FunctionalScopePreflight':scope}
    for name in checks if a.check=='all' else [a.check]:checks[name]()

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError,ET.ParseError) as ex:
        print(str(ex),file=sys.stderr);sys.exit(2)
