#!/usr/bin/env python3
"""Check the bounded C++-first bridge package before modifying product sources."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',required=True,type=Path)
    parser.add_argument('--check',required=True,choices=['PackageBindingPreflight','EvidenceContractPreflight','DesignPreflight','ScopePreflight','all'])
    a=parser.parse_args();root=a.repo.resolve();pkg=root/'.aegis/packages/AXIOM-SDK-CPP-MULTI-BRIDGE-v0.1'
    def git(*argv):return subprocess.run(['git','-C',str(root),*argv],capture_output=True,check=False)
    try:
        remote=git('remote','get-url','origin');url=remote.stdout.decode().strip().lower().removesuffix('.git')
        if remote.returncode or url not in ('https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom'):raise ValueError('repository identity mismatch')
        entry=json.loads((pkg/'package.json').read_text(encoding='utf-8'))
        if entry['repository']!={'provider':'github','full_name':'Mostorm-Labs/axiom'} or not entry['p32_authorized']:
            raise ValueError('package identity or user authorization missing')
        if entry['task_anchor']['relation']!='ancestor' or git('merge-base','--is-ancestor',entry['task_anchor']['revision'],'HEAD').returncode:
            raise ValueError('anchor mismatch')
        for key in ('authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref','preflight_ref'):
            ref=entry[key];path=(pkg/ref['path']).resolve()
            if not path.is_relative_to(pkg.resolve()) or not path.is_file() or sha(path)!=ref['sha256']:raise ValueError('package reference mismatch: '+key)
        authority=json.loads((pkg/'authority.lock.json').read_text(encoding='utf-8'))
        verification=json.loads((pkg/'verification.lock.json').read_text(encoding='utf-8'))
        contract=json.loads((pkg/'execution-contract.json').read_text(encoding='utf-8'))
        for ref in [entry['old_cpp_abi_package'],*authority['bound_sources']]:
            path=(root/ref['path']).resolve()
            if not path.is_relative_to(root) or not path.is_file() or sha(path)!=ref['sha256']:raise ValueError('worktree source mismatch: '+ref['path'])
            blob=git('show',ref['revision']+':'+ref['path'])
            if blob.returncode or hashlib.sha256(blob.stdout).hexdigest()!=ref['sha256']:raise ValueError('source revision mismatch: '+ref['path'])
        red=(root/verification['red']['path']).read_text(encoding='utf-8-sig',errors='replace')
        for expected in ("'readMultiSelection': is not a member", "'submitMultiSelectionProperty': is not a member", "'setMultiSelectionPermission': is not a member"):
            if expected not in red:raise ValueError('expected C++ API RED not found: '+expected)
        if verification['red']['sha256']!=sha(root/verification['red']['path']):raise ValueError('RED hash mismatch')
        required={x['id'] for x in verification['required']}
        if required!=set(contract['tests']['required']) or len(required)!=6:raise ValueError('required tests mismatch')
        if contract['return_policy']['continue_until_terminal_state'] is not True:raise ValueError('terminal return policy missing')
        if not entry['acceptance_basis'].startswith('User explicitly directed C++-first'):raise ValueError('user instruction basis missing')
        if authority['status'] != 'USER_DIRECTION_AUTHORIZES_CPP_MULTI_SLICE_ONLY':raise ValueError('scope basis mismatch')
        if a.check in ('EvidenceContractPreflight','all'):
            for row in verification['required']:
                if not row.get('id') or not row.get('oracle'):raise ValueError('missing real test oracle')
            if any(item not in verification['separate_pending'] for item in ('D07 protobuf/Abseil OOM','Real physical Windows/Web input')):
                raise ValueError('historical limitations not preserved')
        if a.check in ('DesignPreflight','all'):
            source=(root/'runtime/composition/src/multi_selection_port.cpp').read_text(encoding='utf-8')
            if 'MultiSelectionPort::read()' not in source or 'MultiSelectionPort::submitProperty(' not in source:
                raise ValueError('existing real owner methods missing')
            context=(pkg/'implementation-context.md').read_text(encoding='utf-8')
            if 'same composition' not in context or 'not change the existing mixed-stroke limitation' not in context:
                raise ValueError('one-owner or mixed-stroke design missing')
        if a.check in ('ScopePreflight','all'):
            scopes=contract['implementation']['mutation_scope']
            if any(path.startswith('docs/api') or path.startswith('sdk/') for path in scopes):raise ValueError('unrelated ABI scope')
            if not any('C ABI' in item for item in contract['implementation']['forbidden']):raise ValueError('V1 preservation missing')
        print('PRECONDITIONS_OK: identity, immutable refs, user C++ direction, actual RED, exact tests and scope')
        print(a.check+': PASS for this bounded slice; not SDK Gate or physical PASS')
        return 0
    except (OSError,KeyError,ValueError,TypeError,json.JSONDecodeError) as exc:
        print('BLOCKED:',exc);return 2

if __name__=='__main__':sys.exit(main())
