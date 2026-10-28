#!/usr/bin/env python3
"""Read-only preflight for a draft correction; never authorizes P32."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

P = pathlib.Path(__file__).resolve().parent
ROOT = P.parents[2]
CURSOR = '6bcf2d4bbf3314e5252951190c3c475403f52665'
REVIEW = '0fc9462af3b6ee73aa7c378cd3917ce2886acca0'
OLD = 'eecee8f24a952a93f96ee9a4f946f46bbdcf51a4'
BRANCH = 'codex/fc04-mse-p31-v02-binding-fix'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode().strip()

def load(name):
    return json.loads((P / name).read_text(encoding='utf-8'))

def need(value, message):
    if not value:
        raise ValueError(message)

def package_binding():
    p = load('package.json')
    need(git('branch', '--show-current') == BRANCH, 'execution branch mismatch')
    remote = git('remote', 'get-url', 'origin').lower().removesuffix('.git')
    need(remote in ('https://github.com/mostorm-labs/axiom', 'git@github.com:mostorm-labs/axiom'), 'repository identity mismatch')
    head = git('rev-parse', 'HEAD')
    for revision in (CURSOR, REVIEW, OLD):
        need(subprocess.run(['git','merge-base','--is-ancestor',revision,head],cwd=ROOT).returncode == 0, 'required ancestry missing: '+revision)
    original = subprocess.check_output(['git','show',OLD+':'+p['superseded_candidate']['path']],cwd=ROOT)
    need(hashlib.sha256(original).hexdigest() == p['superseded_candidate']['sha256'], 'original package mismatch')
    for entry in load('integrity.json')['files']:
        need(hashlib.sha256((P / entry['path']).read_bytes()).hexdigest() == entry['sha256'], 'draft integrity mismatch: '+entry['path'])
    for entry in load('source-delta.json')['files']:
        blob = git('rev-parse', CURSOR+':'+entry['path'])
        data = subprocess.check_output(['git','show',CURSOR+':'+entry['path']],cwd=ROOT)
        need(blob == entry['git_blob_sha'] and hashlib.sha256(data).hexdigest() == entry['sha256'], 'source lock mismatch: '+entry['path'])
    need(p['p32_authorized'] is False and p['requires_control_review'] is True, 'draft was incorrectly authorized')
    print('PASS PackageBindingPreflight: exact repository, cursor, review, historical package and source locks; HEAD='+head)

def evidence_contract():
    old=json.loads(subprocess.check_output(['git','show',OLD+':.aegis/packages/FC04-MSE-PHASE2-P31-v0.1/test-matrix.json'],cwd=ROOT))
    ids=[x['id'] for x in old['test_cases']]
    need(ids == ['MSE-%02d'%n for n in range(1,28)], 'inherited MSE ID set changed')
    delta=load('verification-delta.json')
    need('Escape' in delta['MSE-25']['required'] and 'owning root' in delta['MSE-25']['additional_assertion'], 'MSE-25 correction missing')
    need(delta['protected_regression']['ctest_id']=='FC01ASelectionInputIntegration', 'selection regression binding missing')
    print('PASS EvidenceContractPreflight: historical 27 IDs and scoped Escape/regression correction preserved; implementation tests NOT_RUN')

def design():
    p=load('package.json')
    need(p['projection_binding'] == 'RESOLVED_AND_REVIEWED', 'BLOCKED_PROJECTION_BINDING: exact ABI-preserving Windows/Web projection and per-target receipt owner/path are not yet frozen or reviewed')
    print('PASS DesignPreflight')

def scope():
    data=load('source-delta.json')
    changes={x['path'] for x in data['files'] if x['role']=='proposed_modify'}
    need(changes=={'runtime/composition/src/canvas_runtime_facade.cpp','runtime/composition/tests/selection_input_integration_test.cpp'}, 'proposed owner delta mismatch')
    print('PASS FunctionalScopePreflight: exact missing Facade and regression-test owners identified; other candidates research-only')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',required=True)
    parser.add_argument('--check',required=True,choices=['PackageBindingPreflight','EvidenceContractPreflight','DesignPreflight','FunctionalScopePreflight','all'])
    args=parser.parse_args()
    need(pathlib.Path(args.repo).resolve()==ROOT.resolve(), '--repo must identify this worktree')
    checks={'PackageBindingPreflight':package_binding,'EvidenceContractPreflight':evidence_contract,'DesignPreflight':design,'FunctionalScopePreflight':scope}
    for name in checks if args.check=='all' else [args.check]:
        checks[name]()

if __name__ == '__main__':
    try:
        main()
    except (ValueError, subprocess.CalledProcessError) as exc:
        print(str(exc),file=sys.stderr)
        sys.exit(2)
