#!/usr/bin/env python3
"""Verify v0.4 draft integrity; never imply draft execution authorization."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--check', required=True, choices=['integrity', 'PackageBindingPreflight', 'EvidenceContractPreflight', 'DesignPreflight', 'all'])
    args = parser.parse_args()
    root = args.repo.resolve()
    pkg = root / '.aegis/packages/AXIOM-SDK-01-v0.4'
    def git(*cmd):
        return subprocess.run(['git', '-C', str(root), *cmd], capture_output=True, check=False)
    try:
        remote = git('remote', 'get-url', 'origin')
        if remote.returncode or remote.stdout.decode().strip().lower().removesuffix('.git') not in ('https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom'):
            raise ValueError('repository origin mismatch')
        entry = json.loads((pkg/'package.json').read_text(encoding='utf-8'))
        if entry['repository'] != {'provider':'github','full_name':'Mostorm-Labs/axiom'}:
            raise ValueError('package repository mismatch')
        anchor = entry['task_anchor']
        if anchor['relation'] != 'ancestor' or git('merge-base','--is-ancestor',anchor['revision'],'HEAD').returncode:
            raise ValueError('anchor is not ancestor of HEAD')
        for key in ('authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref','preflight_ref'):
            ref = entry[key]
            path = (pkg/ref['path']).resolve()
            if not path.is_relative_to(pkg.resolve()) or not path.is_file() or digest(path) != ref['sha256']:
                raise ValueError(f'package ref mismatch: {key}')
        authority = json.loads((pkg/'authority.lock.json').read_text(encoding='utf-8'))
        verification = json.loads((pkg/'verification.lock.json').read_text(encoding='utf-8'))
        contract = json.loads((pkg/'execution-contract.json').read_text(encoding='utf-8'))
        refs = [entry['predecessor'],authority['existing_authority_lock'],verification['prior_verification_lock'],*authority['candidate_bindings'],*authority['owner_sources']]
        for ref in refs:
            path = (root/ref['path']).resolve()
            if not path.is_relative_to(root) or not path.is_file() or digest(path) != ref['sha256']:
                raise ValueError(f'worktree ref mismatch: {ref["path"]}')
            blob = git('show',f'{ref["revision"]}:{ref["path"]}')
            if blob.returncode or hashlib.sha256(blob.stdout).hexdigest() != ref['sha256']:
                raise ValueError(f'revision ref mismatch: {ref["path"]}')
        ids = [o['id'] for o in verification['obligations']]
        if ids != [f'P{i:02d}' for i in range(1,11)] or ids != contract['tests']['required_obligations']:
            raise ValueError('obligation ID binding mismatch')
        for case in verification['obligations']:
            if any(not case.get(k) for k in ('requirement','oracle_and_acceptance','candidate_fixture','fixture_status','evidence_required')):
                raise ValueError(f'incomplete case: {case["id"]}')
            if not (root/case['candidate_fixture']).is_file():
                raise ValueError(f'fixture missing: {case["id"]}')
        print('INTEGRITY_OK: repository, anchor, package bytes, locked revisions, owners and P01-P10')
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as exc:
        print('BLOCKED_INTEGRITY:', exc)
        return 2
    if args.check == 'integrity':
        print('DRAFT_ONLY: integrity does not imply P20 acceptance, P32 authorization or product pass')
        return 0
    failures=[]
    if args.check in ('PackageBindingPreflight','all'):
        if not entry.get('p32_authorized') or authority['not_accepted']:
            failures.append('PackageBindingPreflight: accepted exact ABI and P32 authorization are absent')
    if args.check in ('EvidenceContractPreflight','all'):
        if verification['unresolved']:
            failures.append('EvidenceContractPreflight: P20 expected values, remaining cases and real driver contract remain incomplete or unaccepted; product execution is an exit requirement')
    if args.check in ('DesignPreflight','all'):
        if authority['not_accepted']:
            failures.append('DesignPreflight: public ABI/lifecycle and per-root owner path require acceptance')
    for failure in failures:
        print('BLOCKED:',failure)
    return 2 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
