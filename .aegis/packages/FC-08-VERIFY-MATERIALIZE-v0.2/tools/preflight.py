#!/usr/bin/env python3
"""Input/evidence-contract preflight for the bounded MATERIALIZATION task, not FC08 DEV."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile


def check_ref(root: Path, ref: dict) -> Path:
    root = root.resolve()
    name = ref.get('path', '')
    path = (root / name).resolve()
    if not name or Path(name).is_absolute() or path == root or root not in path.parents:
        raise ValueError('reference escapes package: ' + name)
    if not path.is_file():
        raise ValueError('required input missing: ' + name)
    digest = ref.get('sha256', '')
    if not re.fullmatch(r'[0-9a-f]{64}', digest):
        raise ValueError('invalid digest binding: ' + name)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise ValueError('hash mismatch: ' + name)
    return path


def check_scope(package: dict) -> None:
    if package.get('authorized_work') != 'verification_materialization_only' or package.get('product_P32_authorized') is not False:
        raise ValueError('not a bounded verification materialization package')


def check_inputs(path: Path) -> dict:
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        if len(names) != len(set(names)) or z.testzip() is not None:
            raise ValueError('duplicate archive member or invalid CRC')
        for name in names:
            p = Path(name)
            if p.is_absolute() or '..' in p.parts or '\\' in name:
                raise ValueError('unsafe archive member')
        m = json.loads(z.read('manifest.json'))
        expected = m['files']
        if set(names) != set(expected) | {'manifest.json'}:
            raise ValueError('unmanifested or missing member')
        for name, digest in expected.items():
            if hashlib.sha256(z.read(name)).hexdigest() != digest:
                raise ValueError('seed member mismatch: ' + name)
        catalog = json.loads(z.read('verification/effective-case-catalog.json'))
        obligations = json.loads(z.read('verification/reviewed-obligations.json'))
        ids = [c['id'] for c in catalog['cases']]
        covered = [c for o in obligations['families'] for c in o['fixture_ids']]
        if len(ids) != 60 or len(set(ids)) != 60 or len(obligations['families']) != 10 or sorted(ids) != sorted(covered):
            raise ValueError('frozen coverage mismatch')
        if catalog['effective_design_id'] != 'FC08-P20-v0.1+CR01':
            raise ValueError('wrong verification revision')
        accepted = json.loads(z.read('authority/execution-sequencing-acceptance.json'))
        if accepted['post_decision']['SEQ-01'] != 'CLOSED_BY_SCOPED_EXECUTION_AMENDMENT':
            raise ValueError('sequence amendment not accepted')
        return {'seed_members':len(expected),'logical_cases':len(ids),'families':len(obligations['families'])}


def run(package_path: Path, phase: str, repo: Path | None) -> dict:
    root = package_path.resolve().parent
    p = json.loads(package_path.read_text(encoding='utf-8'))
    check_scope(p)
    refs = ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref','accepted_inputs_ref','source_acquisition_ref','profile_recipes_ref','tool_ref']
    loaded = {k:check_ref(root,p[k]) for k in refs}
    counts = check_inputs(loaded['accepted_inputs_ref'])
    a = json.loads(loaded['authority_lock_ref'].read_text())
    if a['status'] != 'CURRENT_ACCEPTED_FOR_MATERIALIZATION_SCOPE':
        raise ValueError('untrusted authority lock')
    v = json.loads(loaded['verification_lock_ref'].read_text())
    e = json.loads(loaded['evidence_contract_ref'].read_text())
    c = json.loads(loaded['execution_contract_ref'].read_text())
    if c['return_policy'].get('continue_until_terminal_state') is not True:
        raise ValueError('missing continuous execution contract')
    obligation_ids = set()
    for o in v['obligations']:
        if not all(o.get(k) for k in ['id','requirement','oracle','acceptance','evidence_required']):
            raise ValueError('incomplete obligation binding')
        obligation_ids.add(o['id'])
    if obligation_ids != {'MAT-01','MAT-02','MAT-03','MAT-04'}:
        raise ValueError('wrong materialization completion set')
    if phase == 'evidence':
        if e['gate_pass_authorized'] or not e['durable_return']['materialized_ref_required']:
            raise ValueError('invalid evidence boundary')
        for item in e['blocking']:
            if not item.get('artifact') or item['obligation'] not in obligation_ids:
                raise ValueError('unbound blocking evidence')
    identity = 'NOT_RUN_IN_THIS_COMMAND'
    if repo is not None:
        remote = subprocess.check_output(['git','remote','get-url','origin'],cwd=repo,text=True).strip()
        if not re.search(r'(github\.com[:/])Mostorm-Labs/axiom(?:\.git)?/?$',remote):
            raise ValueError('repository identity mismatch')
        subprocess.run(['git','merge-base','--is-ancestor',p['task_anchor']['revision'],'HEAD'],cwd=repo,check=True)
        identity = 'PASS_REPOSITORY_AND_ANCESTRY'
    return {'phase':phase,'status':'PASS_MATERIALIZATION_INPUT_BINDINGS','task_id':p['task_id'],'repository_preflight':identity,'product_binding_preflight_pass':False,'product_tests_executed':False,**counts}


def main() -> int:
    ap=argparse.ArgumentParser();ap.add_argument('--package',type=Path,default=Path(__file__).resolve().parents[1]/'package.json');ap.add_argument('--phase',choices=['package','evidence'],required=True);ap.add_argument('--repo',type=Path);ap.add_argument('--output',type=Path)
    ns=ap.parse_args()
    try:
        result=run(ns.package,ns.phase,ns.repo);code=0
    except (ValueError,KeyError,OSError,zipfile.BadZipFile,subprocess.SubprocessError) as exc:
        result={'phase':ns.phase,'status':'BLOCKED_INPUT_OR_BINDING','error':str(exc),'product_binding_preflight_pass':False};code=2
    text=json.dumps(result,indent=2)+'\n'
    if ns.output:
        ns.output.parent.mkdir(parents=True,exist_ok=True);ns.output.write_text(text,encoding='utf-8')
    print(text,end='');return code

if __name__=='__main__':sys.exit(main())
