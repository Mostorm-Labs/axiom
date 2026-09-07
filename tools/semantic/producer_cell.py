#!/usr/bin/env python3
"""Thin identity/reuse/probe adapter for the reusable producer workflow.

Package formats and build recipes stay in their existing owners. Reuse failures
are fatal; only absent accepted authority or a different identity is a miss.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys

if __package__ in {None, ''}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import SdkError, detect_host_platform, validate_digest, validate_component
from tools.semantic.contract import (ROOT, HOST_KEYS, RUNTIME_KEYS, load_profile, read_json,
                                     make_host_tool_identity, make_runtime_identity, validate_v2_lock)
from tools.semantic.package_host import probe_host, verify_host_archive
from tools.semantic.package_runtime import verify_runtime_archive
from tools.semantic.producer_plan import write_outputs
from tools.semantic.reuse_release import reuse_asset
from tools.semantic.smoke_consumer import smoke_consumer
from tools.semantic.toolchain import identify_toolchain


def runtime_options(key: str) -> dict:
    family = load_profile()['runtimes'][key]['platform']
    return {'ndk': Path(os.environ['ANDROID_NDK_ROOT']) if family == 'android' else None,
            'emscripten': ROOT / '.deps/emsdk/upstream/emscripten' if family == 'web' else None,
            'cc': str(ROOT / '.deps/llvm/bin/clang-cl.exe') if family == 'windows' else None,
            'cxx': str(ROOT / '.deps/llvm/bin/clang-cl.exe') if family == 'windows' else None}


def describe_cell(kind: str, key: str) -> dict:
    lock, profile = read_json(ROOT / 'deps.lock.json'), load_profile()
    toolchain = None
    if kind == 'hostTools':
        identity, identifier = make_host_tool_identity(lock, profile, key)
    elif kind == 'runtimes':
        toolchain = identify_toolchain(profile, key, **runtime_options(key)).record
        identity, identifier = make_runtime_identity(lock, profile, key, toolchain)
    else:
        raise SdkError('semantic: unknown producer kind')
    result = {'kind': kind, 'key': key, 'identity': identity, 'identifier': identifier}
    if toolchain is not None:
        result['toolchain'] = toolchain
    return result


def _check_plan(plan: dict) -> tuple[str, object]:
    kind, key = plan.get('kind'), plan.get('key')
    if kind not in {'hostTools', 'runtimes'} or key not in (HOST_KEYS if kind == 'hostTools' else RUNTIME_KEYS):
        raise SdkError('semantic: unsupported cell plan')
    identifier = validate_digest(plan.get('identifier'))
    if (not isinstance(plan.get('identity'), dict) or plan['identity'].get('key') != key
            or hashlib.sha256(canonical_bytes(plan['identity'])).hexdigest() != identifier):
        raise SdkError('semantic: producer plan identity mismatch')
    return ('hostToolId', verify_host_archive) if kind == 'hostTools' else ('runtimeId', verify_runtime_archive)


def verify_planned_package(plan: dict, directory: Path) -> dict:
    field, verifier = _check_plan(plan)
    record = read_json(directory / 'record.json')
    if record.get(field) != plan['identifier'] or record.get('identity') != plan['identity']:
        raise SdkError('semantic: package differs from pre-build expected identity')
    asset = validate_component(record['asset'])
    prefix = 'protoc' if plan['kind'] == 'hostTools' else 'runtime'
    if asset != f"axiom-semantic-{prefix}-{plan['key']}-{plan['identifier']}.zip":
        raise SdkError('semantic: package asset name differs from expected identity')
    verifier(directory / asset, record)
    return record


def reuse_planned_package(plan: dict, accepted_lock: Path, output: Path, **options) -> dict:
    _check_plan(plan)
    record = reuse_asset(accepted_lock, plan['kind'], plan['key'], plan['identifier'], output, **options)
    if record is not None:
        verify_planned_package(plan, output)
    return {'reused': record is not None, 'kind': plan['kind'], 'key': plan['key'],
            'identifier': plan['identifier'], 'record': record}


def accepted_lock_path(directory: Path) -> Path:
    facts = read_json(directory / 'authority.json')
    lock = directory / 'semantic-sdk.lock.json'
    if (facts.get('format') != 'axiom-semantic-producer-authority-v2'
            or type(facts.get('lockPresent')) is not bool):
        raise SdkError('semantic: missing producer authority snapshot')
    if facts['lockPresent']:
        if lock.is_symlink() or not lock.is_file() or file_sha256(lock) != facts.get('lockSha256'):
            raise SdkError('semantic: accepted lock snapshot is missing or damaged')
        validate_v2_lock(read_json(lock))
    elif lock.exists() or lock.is_symlink() or facts.get('lockSha256') is not None:
        raise SdkError('semantic: unexpected lock in absent-authority snapshot')
    return lock


def compare_packages(plan: dict, first: Path, second: Path) -> dict:
    if first.resolve() == second.resolve():
        raise SdkError('semantic: comparison requires distinct package paths')
    a, b = verify_planned_package(plan, first), verify_planned_package(plan, second)
    if a != b or file_sha256(first / a['asset']) != file_sha256(second / b['asset']):
        raise SdkError('semantic: producer packages are not byte-identical')
    return {'byteIdentical': True, 'identifier': plan['identifier'], 'sha256': a['sha256']}


def support_host(directory: Path, authority: Path, *, store_root: Path | None = None) -> dict:
    host = detect_host_platform()
    key = 'macos-universal' if host.os == 'macos' else host.key
    plan = describe_cell('hostTools', key)
    lock = accepted_lock_path(authority)
    if directory.exists():
        source = 'selected-host-cell'
    else:
        result = reuse_planned_package(plan, lock, directory, store_root=store_root)
        source = 'accepted-release' if result['reused'] else 'official-upstream-package'
        if not result['reused']:
            other = directory.with_name(directory.name + '-second')
            for output in (directory, other):
                subprocess.run([sys.executable, str(ROOT / 'tools/semantic/package_host.py'),
                                '--host-key', key, '--output', str(output)], check=True)
            compare_packages(plan, directory, other)
    record = verify_planned_package(plan, directory)
    probe_host(directory / record['asset'], record)
    return {'hostToolId': plan['identifier'], 'host': key, 'source': source, 'sourceBuild': False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('identity', 'reuse', 'verify', 'compare', 'support-host', 'smoke'))
    parser.add_argument('--kind', choices=('hostTools', 'runtimes'))
    parser.add_argument('--key')
    parser.add_argument('--plan', type=Path)
    parser.add_argument('--directory', type=Path)
    parser.add_argument('--second', type=Path)
    parser.add_argument('--authority-directory', type=Path)
    parser.add_argument('--store', type=Path)
    parser.add_argument('--host-directory', type=Path)
    parser.add_argument('--probe', action='store_true')
    parser.add_argument('--facts-output', type=Path)
    parser.add_argument('--github-output', type=Path)
    args = parser.parse_args()
    if args.action == 'identity':
        if not args.kind or not args.key or args.plan is None:
            parser.error('identity requires --kind, --key and --plan')
        result = describe_cell(args.kind, args.key)
        args.plan.parent.mkdir(parents=True, exist_ok=True)
        args.plan.write_bytes(canonical_bytes(result) + b'\n')
    elif args.action == 'support-host':
        if args.directory is None or args.authority_directory is None:
            parser.error('support-host requires --directory and --authority-directory')
        result = support_host(args.directory, args.authority_directory, store_root=args.store)
    else:
        if args.plan is None or args.directory is None:
            parser.error('package actions require --plan and --directory')
        plan = read_json(args.plan)
        _check_plan(plan)
        current = describe_cell(plan['kind'], plan['key'])
        if current['identity'] != plan['identity'] or current['identifier'] != plan['identifier']:
            raise SdkError('semantic: producer inputs changed after identity planning')
        if args.action == 'reuse':
            if args.authority_directory is None:
                parser.error('reuse requires --authority-directory')
            result = reuse_planned_package(plan, accepted_lock_path(args.authority_directory),
                                           args.directory, store_root=args.store)
        elif args.action == 'compare':
            if args.second is None:
                parser.error('compare requires --second')
            result = compare_packages(plan, args.directory, args.second)
        else:
            result = verify_planned_package(plan, args.directory)
            if args.action == 'smoke':
                if plan['kind'] != 'runtimes' or args.host_directory is None:
                    parser.error('smoke requires a runtime plan and --host-directory')
                result = smoke_consumer(plan['key'], args.directory, args.host_directory,
                                        profile=load_profile(), **runtime_options(plan['key']))
            elif args.probe:
                if plan['kind'] != 'hostTools':
                    parser.error('--probe requires a host tool')
                probe_host(args.directory / result['asset'], result)
    if args.facts_output is not None:
        args.facts_output.parent.mkdir(parents=True, exist_ok=True)
        args.facts_output.write_bytes(canonical_bytes(result) + b'\n')
    if args.github_output is not None:
        write_outputs(args.github_output, {k: result[k] for k in ('identifier', 'reused') if k in result})
    print(canonical_bytes(result).decode())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
