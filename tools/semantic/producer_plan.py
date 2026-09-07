#!/usr/bin/env python3
"""Validate producer selections and snapshot accepted authority from an exact ref.

This is orchestration, not SDK identity or consumer-acceptance authority.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

if __package__ in {None, ''}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import SdkError
from tools.semantic.classify_producer_changes import build_matrices
from tools.semantic.contract import ROOT, HOST_KEYS, RUNTIME_KEYS, load_profile, parse_json_bytes, validate_v2_lock


def make_plan(host_tools: list[str], runtimes: list[str], aggregate: bool, *, profile: dict | None = None) -> dict:
    if type(aggregate) is not bool:
        raise SdkError('semantic: aggregate must be a boolean')
    profile = profile if profile is not None else load_profile()
    matrices = build_matrices(host_tools, runtimes, profile=profile)
    if any(len(values) != len(set(values)) for values in (host_tools, runtimes)):
        raise SdkError('semantic: duplicate producer keys')
    if aggregate and (set(host_tools) != set(HOST_KEYS) or set(runtimes) != set(RUNTIME_KEYS)):
        raise SdkError('semantic: aggregate requires exactly all three hosts and nine runtimes')
    for cell in matrices['runtimes']['include']:
        matches = [key for key, host in profile['hostTools'].items() if host['runner'] == cell['os']]
        if len(matches) != 1:
            raise SdkError('semantic: runtime runner requires one matching host-tool profile')
        cell['host'] = matches[0]
    return {'host_matrix': matrices['host_tools'], 'runtime_matrix': matrices['runtimes'],
            'has_hosts': bool(host_tools), 'has_runtimes': bool(runtimes), 'aggregate': aggregate}


def stage_accepted_lock(ref: str, output: Path, *, repo_root: Path = ROOT) -> dict:
    def git(*args: str) -> bytes:
        return subprocess.run(['git', *args], cwd=repo_root, check=True, capture_output=True).stdout
    revision = git('rev-parse', '--verify', '--end-of-options', ref + '^{commit}').decode().strip()
    path = 'semantic-sdk.lock.json'
    entry = git('ls-tree', '-z', revision, '--', path)
    data = None
    if entry:
        metadata, name = entry.rstrip(b'\0').split(b'\t', 1)
        mode, kind, _ = metadata.split()
        if name != path.encode() or mode != b'100644' or kind != b'blob':
            raise SdkError('semantic: accepted lock must be a regular Git blob')
        data = git('show', revision + ':' + path)
        validate_v2_lock(parse_json_bytes(data))
    if output.exists() and any(output.iterdir()):
        raise SdkError('semantic: authority output must be empty')
    output.mkdir(parents=True, exist_ok=True)
    lock_path = output / path
    if data is not None:
        lock_path.write_bytes(data)
    facts = {'format': 'axiom-semantic-producer-authority-v2', 'revision': revision,
             'lockPresent': data is not None, 'lockSha256': file_sha256(lock_path) if data is not None else None}
    (output / 'authority.json').write_bytes(canonical_bytes(facts) + b'\n')
    return facts


def write_outputs(path: Path, values: dict) -> None:
    with path.open('a', encoding='utf-8', newline='\n') as stream:
        for key, value in values.items():
            text = json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)
            # Strings are output values, not JSON string literals.
            if isinstance(value, str):
                text = value
            if '\n' in text or '\r' in text:
                raise SdkError('semantic: unsafe multiline workflow output')
            stream.write(f'{key}={text}\n')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-tools', required=True)
    parser.add_argument('--runtimes', required=True)
    parser.add_argument('--aggregate', choices=('true', 'false'), required=True)
    parser.add_argument('--accepted-ref', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--github-output', type=Path)
    args = parser.parse_args()
    plan = make_plan(json.loads(args.host_tools), json.loads(args.runtimes), args.aggregate == 'true')
    if plan['has_hosts'] or plan['has_runtimes']:
        stage_accepted_lock(args.accepted_ref, args.output / 'authority')
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'plan.json').write_bytes(canonical_bytes(plan) + b'\n')
    if args.github_output:
        write_outputs(args.github_output, plan)
    print(canonical_bytes(plan).decode())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
