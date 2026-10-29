#!/usr/bin/env python3
"""Verify a draft P31 package; passing does not constitute control acceptance."""
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
REVIEW = '53bbf2a96abca24230403797a12394af96d1f2ac'
BRANCH = 'codex/fc04-mse-p31-v02-binding-fix'

def need(value, reason):
    if not value:
        raise ValueError(reason)

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()

def read(name):
    return json.loads((P / name).read_text(encoding='utf-8'))

def binding():
    p = read('package.json')
    need(git('branch','--show-current') == BRANCH, 'wrong execution branch')
    remote = git('remote','get-url','origin').lower().removesuffix('.git')
    need(remote in ('https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom'), 'wrong origin')
    head = git('rev-parse','HEAD')
    for rev in (CURSOR, REVIEW):
        need(subprocess.run(['git','merge-base','--is-ancestor',rev,head],cwd=ROOT).returncode == 0, 'missing ancestry '+rev)
    need(p['p32_authorized'] is False and p['requires_control_review'] is True, 'draft falsely authorizes P32')
    historical = subprocess.check_output(['git','show','eecee8f24a952a93f96ee9a4f946f46bbdcf51a4:.aegis/packages/FC04-MSE-PHASE2-P31-v0.1/package.json'],cwd=ROOT)
    need(hashlib.sha256(historical).hexdigest()==p['historical_p31_package_sha256'], 'historical package identity mismatch')
    for e in read('source-lock.json')['files']:
        data = subprocess.check_output(['git','show',CURSOR+':'+e['path']],cwd=ROOT)
        need(git('rev-parse',CURSOR+':'+e['path'])==e['git_blob_sha'], 'blob mismatch '+e['path'])
        need(hashlib.sha256(data).hexdigest()==e['sha256'], 'source hash mismatch '+e['path'])
    prior=json.loads((ROOT/'.aegis/packages/FC04-MSE-PHASE2-P31-v0.1/input-lock.json').read_text(encoding='utf-8'))
    need(prior['subject_revision']==CURSOR, 'prior input lock subject mismatch')
    for e in prior['files']:
        need(git('rev-parse',CURSOR+':'+e['path'])==e['git_blob_sha'], 'prior input blob mismatch '+e['path'])
    for e in read('integrity.json')['files']:
        need(hashlib.sha256((P/e['path']).read_bytes()).hexdigest()==e['sha256'], 'package hash mismatch '+e['path'])
        tracked=subprocess.check_output(['git','show',head+':'+P.relative_to(ROOT).as_posix()+'/'+e['path']],cwd=ROOT)
        need(hashlib.sha256(tracked).hexdigest()==e['sha256'], 'package differs from HEAD '+e['path'])
    tracked_manifest=subprocess.check_output(['git','show',head+':'+P.relative_to(ROOT).as_posix()+'/integrity.json'],cwd=ROOT)
    need(tracked_manifest==(P/'integrity.json').read_bytes(), 'package integrity manifest differs from HEAD')
    print('PASS PackageBindingPreflight: repository, anchors, source and package identities; HEAD='+head)

def evidence():
    m = read('test-matrix.json')
    need([r['id'] for r in m['test_cases']]==['MSE-%02d'%i for i in range(1,28)], 'MSE ID set differs')
    need('Escape' in m['test_cases'][24]['case'] and 'owning' in m['test_cases'][24]['case'], 'MSE-25 missing either accepted assertion')
    need(len(m['required_property_rows'])==8, 'property row set differs')
    for r in m['inherited_regressions']:
        path=ROOT/r['source']; need(path.is_file(), 'missing input '+r['source'])
        if path.suffix=='.xml':
            tree=ET.parse(path); names=[n.get('name','') for n in tree.iter('testcase')]
            need(not any(n.tag in ('failure','error','skipped') for n in tree.iter()), 'historical results not passing '+r['id'])
        else:
            names=[s.strip() for s in path.read_text(encoding='utf-8').splitlines() if s.strip()]
        need(len(names)==r['expected_count'] and len(set(names))==len(names), 'wrong frozen test IDs '+r['id'])
    for prefix, expected in (('d07',14),('selection',160)):
        names=(ROOT/f'.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/{prefix}-names.txt').read_text(encoding='utf-8').splitlines()
        tree=ET.parse(ROOT/f'.aegis/results/FC04-D07-FUNCTIONAL-P32-001/raw/final-551/{prefix}_{expected}.xml')
        actual=[n.get('name','') for n in tree.iter('testcase')]
        need(len(actual)==expected and set(actual)==set(filter(None,names)), 'frozen ID mismatch '+prefix)
        need(not any(n.tag in ('failure','error','skipped') for n in tree.iter()), 'historical results not passing '+prefix)
    need(m['oom_blocker']['status']=='BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG','OOM blocker lost')
    print('PASS EvidenceContractPreflight: MSE-01..27, eight rows, inherited cases and OOM status; implementation NOT_RUN')

def design():
    text=(P/'design.md').read_text(encoding='utf-8')
    for phrase in ('RuntimeComposition','owner-thread-only','CanvasControlService','explicit eligible cohort the user confirms',
                   'C ABI','C++ consumers','displayed token','preimages','typed changed/skipped',
                   'P31 control review and P32 acceptance remain separate'):
        need(phrase in text, 'design missing '+phrase)
    print('PASS DesignPreflight: selected internal C++ boundary, lifetime, typed queue and outcomes documented; control review pending')

def scope():
    c=read('implementation-context.json')
    for path in ('runtime/composition/src/canvas_runtime_facade.cpp','runtime/control/src/canvas_control_service.cpp',
                 'runtime/control/tests/canvas_control_service_test.cpp','runtime/composition/tests/selection_input_integration_test.cpp',
                 'debug_ui/include/canvas/debug_ui/controller.hpp','apps/minimal_runtime_host/windows_main.cpp',
                 'apps/axiom_canvas_demo/platform/web/web_debug_ui.inl'):
        need(path in c['allowed_modification_paths'], 'required owner outside proposed scope '+path)
    for path in ('runtime/foundation/include/canvas/runtime/runtime_facade.hpp','runtime/foundation/include/canvas/runtime/canvas_control_types.hpp',
                 'schema/axiom/v1/registry/field_registry_v1.yaml','semantic-sdk.lock.json'):
        need(path in c['forbidden_paths'],'missing protected boundary '+path)
    need(c['new_internal_port']=='runtime/composition/include/canvas/runtime/internal/multi_selection_port.hpp', 'port path unbound')
    print('PASS FunctionalScopePreflight: proposed owner and protected-boundary allowlists')

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True)
    p.add_argument('--check',required=True,choices=['PackageBindingPreflight','EvidenceContractPreflight','DesignPreflight','FunctionalScopePreflight','all']);a=p.parse_args()
    need(pathlib.Path(a.repo).resolve()==ROOT.resolve(), '--repo must be this real worktree')
    checks={'PackageBindingPreflight':binding,'EvidenceContractPreflight':evidence,'DesignPreflight':design,'FunctionalScopePreflight':scope}
    for name in checks if a.check=='all' else [a.check]:checks[name]()

if __name__=='__main__':
    try: main()
    except (ValueError, OSError, subprocess.CalledProcessError, ET.ParseError) as ex:
        print(str(ex),file=sys.stderr);sys.exit(2)
