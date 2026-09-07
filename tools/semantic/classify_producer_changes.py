#!/usr/bin/env python3
"""Select Semantic SDK v2 qualification cells, not dependency acceptance.

Decoded lock/profile subtrees determine platform scope. Selection may include
requalification of reusable identities; it never changes IDs or bypasses package
verification. --matrix accepts a JSON object with host_tools/runtimes key arrays
and emits two separate GitHub matrices. Empty matrices must be gated by callers.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
from typing import Any

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.model import SdkError, validate_digest
from tools.semantic.contract import HOST_KEYS, RUNTIME_KEYS, PROFILE_FORMAT, load_profile, parse_json_bytes

PROFILE_PATH = 'tools/semantic/profile-v2.json'
HOST_RECIPE_PATHS = frozenset({'tools/semantic/package_host.py'})
RUNTIME_RECIPE_PATHS = frozenset({
    'tools/semantic/build_runtime.py', 'tools/semantic/package_runtime.py',
    'tools/semantic/toolchain.py', 'tools/semantic/qualify_runtime.py',
    'tools/semantic/smoke_consumer.py', 'tools/semantic/cmake_consumer.py',
})
SHARED_RECIPE_PATHS = frozenset({
    'tools/semantic/contract.py', 'tools/semantic/package_common.py',
    'tools/semantic/sources.py', 'tools/sdk/archive.py',
    '.github/workflows/semantic-sdk-producer.yml', 'tools/semantic/producer_cell.py',
})
# Keep the live v1 recipe conservative until its workflow is explicitly retired.
LEGACY_RECIPE_PATHS = frozenset({
    'tools/bootstrap_deps.py', 'tools/semantic_sdk.py',
    'verification/tests/test_semantic_sdk.py', '.github/workflows/semantic-toolchain-producer.yml',
})
TOOLCHAIN_PLATFORMS = {
    'windows_llvm': {'windows'}, 'android_ndk': {'android'}, 'emscripten': {'web'},
    'apple_toolchain': {'macos', 'ios', 'ios-simulator'},
}
PLATFORM_FAMILY = {'linux': 'linux', 'windows': 'windows', 'android': 'android', 'web': 'web',
                   'macos': 'apple', 'ios': 'apple', 'ios-simulator': 'apple'}
HOST_FAMILY = {'linux-x86_64': 'linux', 'windows-x64': 'windows', 'macos-universal': 'apple'}


def _result(hosts: set[str], runtimes: set[str], reasons: set[str]) -> dict[str, object]:
    mode = ('full' if hosts == set(HOST_KEYS) and runtimes == set(RUNTIME_KEYS)
            else 'partial' if hosts or runtimes else 'none')
    return {'mode': mode, 'host_tools': [key for key in HOST_KEYS if key in hosts],
            'runtimes': [key for key in RUNTIME_KEYS if key in runtimes],
            'reason': '; '.join(sorted(reasons)) or 'consumer-only, classification-only, or unrelated change',
            # Derived compatibility alias, NOT a second classifier/authority.
            'should_build': bool(hosts or runtimes)}


def _full(reason: str) -> dict[str, object]:
    return _result(set(HOST_KEYS), set(RUNTIME_KEYS), {reason + '; conservative full qualification'})



def _runtime_shape(key: str, entry: dict) -> None:
    abi = entry['abi']
    string_fields = {'linkage', 'buildType', 'sanitizer', 'cxxRuntime'}
    bool_fields = {'pic', 'exceptions', 'rtti'}
    int_fields = {'cxxStandard'}
    if key == 'windows-x64-msvc-static':
        string_fields.add('crt')
        int_fields.add('iteratorDebugLevel')
    if key == 'web-wasm32':
        bool_fields.add('pthread')
    if (set(abi) != string_fields | bool_fields | int_fields
            or any(not isinstance(abi[k], str) or not abi[k] for k in string_fields)
            or any(type(abi[k]) is not bool for k in bool_fields)
            or any(type(abi[k]) is not int for k in int_fields)):
        raise ValueError('incomplete or invalid runtime ABI structure')
    fields = {
        'linux': {'compilerFamily'}, 'windows': {'compilerFamily', 'llvm'},
        'macos': {'sdk'}, 'ios': {'sdk', 'deploymentTarget'},
        'ios-simulator': {'sdk', 'deploymentTarget'}, 'android': {'ndk', 'apiLevel'},
        'web': {'emscripten', 'llvm'},
    }.get(entry['platform'])
    toolchain = entry['toolchain']
    if fields is None or not fields <= toolchain.keys():
        raise ValueError('incomplete or unknown runtime toolchain structure')
    for field in fields:
        value = toolchain[field]
        if field == 'apiLevel':
            if type(value) is not int or value <= 0:
                raise ValueError('invalid Android API level')
        elif not isinstance(value, str) or not value:
            raise ValueError('invalid runtime toolchain field')


def _profile_shape(profile: Any) -> dict:
    """Check comparable structure, not acceptance of a proposed toolchain/ABI.

    The producer owns supported ABI policy. Even an unsupported new CRT value
    has a known scope when its complete Windows profile entry is available.
    """
    required = {'format', 'hostContractVersion', 'runtimeContractVersion', 'hostTools', 'runtimes'}
    if not isinstance(profile, dict) or not required <= profile.keys() or profile.keys() - required - {'runtimeContractVersions'}:
        raise ValueError('profile fields unavailable or unknown')
    if profile['format'] != PROFILE_FORMAT:
        raise ValueError('unknown profile format')
    versions = profile.get('runtimeContractVersions', {})
    if not isinstance(versions, dict) or versions.keys() - set(RUNTIME_KEYS):
        raise ValueError('invalid per-target contract versions')
    if any(type(value) is not int or value <= 0 for value in
           [profile['hostContractVersion'], profile['runtimeContractVersion'], *versions.values()]):
        raise ValueError('invalid contract version')
    for field, keys, entry_fields in [
        ('hostTools', HOST_KEYS, {'upstreamKey', 'runner', 'executable'}),
        ('runtimes', RUNTIME_KEYS, {'platform', 'arch', 'runner', 'abi', 'toolchain'}),
    ]:
        entries = profile[field]
        if not isinstance(entries, dict) or set(entries) != set(keys):
            raise ValueError('incomplete or unknown profile matrix')
        for key, entry in entries.items():
            if not isinstance(entry, dict) or set(entry) != entry_fields:
                raise ValueError('invalid profile entry')
            for name in entry_fields:
                value = entry[name]
                if name in {'abi', 'toolchain'}:
                    if not isinstance(value, dict) or not value:
                        raise ValueError('invalid ABI/toolchain structure')
                elif not isinstance(value, str) or not value:
                    raise ValueError('invalid profile string')
            if field == 'runtimes':
                _runtime_shape(key, entry)
    return profile


def _dependencies(lock: Any, profile: dict) -> dict:
    if not isinstance(lock, dict) or not isinstance(lock.get('dependencies'), dict):
        raise ValueError('lock dependencies unavailable')
    dependencies = lock['dependencies']
    for name in ('protobuf', 'abseil'):
        dependency = dependencies.get(name)
        if not isinstance(dependency, dict):
            raise ValueError('missing Semantic dependency')
        for field in ('version', 'source_sha256', 'source_url'):
            if not isinstance(dependency.get(field), str) or not dependency[field]:
                raise ValueError('invalid Semantic source authority')
        validate_digest(dependency['source_sha256'])
    protobuf = dependencies['protobuf']
    if not isinstance(protobuf.get('edition'), str) or not protobuf['edition']:
        raise ValueError('invalid Protobuf edition')
    assets = protobuf.get('protoc_assets')
    if not isinstance(assets, dict):
        raise ValueError('invalid host assets')
    for entry in profile['hostTools'].values():
        asset = assets.get(entry['upstreamKey'])
        if not isinstance(asset, dict) or not isinstance(asset.get('url'), str) or not asset['url']:
            raise ValueError('missing mapped host asset')
        validate_digest(asset.get('sha256'))
    for name in TOOLCHAIN_PLATFORMS:
        if name in dependencies and (not isinstance(dependencies[name], dict) or not dependencies[name]):
            raise ValueError('invalid locked toolchain')
    return dependencies


def _lock_changes(before: Any, after: Any, profiles: tuple[dict, dict],
                  hosts: set[str], runtimes: set[str], reasons: set[str]) -> None:
    old, new = (_dependencies(lock, profile) for lock, profile in zip((before, after), profiles))
    # Descriptive text and Skia-only build configuration are not Semantic inputs.
    ignored = {'dependencies', 'skia_builds', 'generated_for'}
    if {k: v for k, v in before.items() if k not in ignored} != {k: v for k, v in after.items() if k not in ignored}:
        hosts.update(HOST_KEYS)
        runtimes.update(RUNTIME_KEYS)
        reasons.add('lock schema/unknown metadata changed')
    pb_old, pb_new = old['protobuf'], new['protobuf']
    if {k: v for k, v in pb_old.items() if k != 'protoc_assets'} != {k: v for k, v in pb_new.items() if k != 'protoc_assets'}:
        hosts.update(HOST_KEYS)
        runtimes.update(RUNTIME_KEYS)
        reasons.add('Protobuf source/version contract changed; requalify hosts, reuse unchanged verified identities')
    for key in HOST_KEYS:
        assets = [pb['protoc_assets'][profile['hostTools'][key]['upstreamKey']]
                  for pb, profile in zip((pb_old, pb_new), profiles)]
        if assets[0] != assets[1]:
            hosts.add(key)
            reasons.add('host upstream asset changed: ' + key)
    if old['abseil'] != new['abseil']:
        runtimes.update(RUNTIME_KEYS)
        reasons.add('Abseil source/version contract changed')
    for name, platforms in TOOLCHAIN_PLATFORMS.items():
        if old.get(name) != new.get(name):
            runtimes.update(key for profile in profiles for key, entry in profile['runtimes'].items()
                            if entry['platform'] in platforms)
            reasons.add('target toolchain lock changed: ' + name)


def _profile_changes(before: dict, after: dict, hosts: set[str], runtimes: set[str], reasons: set[str]) -> None:
    for key in HOST_KEYS:
        if (before['hostTools'][key] != after['hostTools'][key]
                or before['hostContractVersion'] != after['hostContractVersion']):
            hosts.add(key)
            reasons.add('host profile/contract changed: ' + key)
    for key in RUNTIME_KEYS:
        versions = [p.get('runtimeContractVersions', {}).get(key, p['runtimeContractVersion']) for p in (before, after)]
        if before['runtimes'][key] != after['runtimes'][key] or versions[0] != versions[1]:
            runtimes.add(key)
            reasons.add('runtime profile/contract changed: ' + key)


def classify(paths: list[str], *, lock_before: dict | None = None, lock_after: dict | None = None,
             profile_before: dict | None = None, profile_after: dict | None = None) -> dict[str, object]:
    """Union all changed inputs; unavailable/ambiguous authority fails closed."""
    hosts: set[str] = set()
    runtimes: set[str] = set()
    reasons: set[str] = set()
    try:
        if not isinstance(paths, list):
            raise ValueError('paths must be an array')
        normalized = set()
        for path in paths:
            if not isinstance(path, str) or not path or any(c in path for c in ('\0', '\\', ':')):
                raise ValueError('invalid repository path')
            value = PurePosixPath(path)
            if value.is_absolute() or '..' in value.parts or value.as_posix() == '.':
                raise ValueError('repository-relative paths required')
            normalized.add(value.as_posix())
        for path in sorted(normalized):
            if path in HOST_RECIPE_PATHS:
                hosts.update(HOST_KEYS)
                reasons.add('host package contract changed')
            if path in RUNTIME_RECIPE_PATHS or path.startswith('tools/semantic/smoke/'):
                runtimes.update(RUNTIME_KEYS)
                reasons.add('runtime build/package/qualification contract changed')
            if path == 'tools/semantic/windows_compile.py':
                runtimes.add('windows-x64-msvc-static')
                reasons.add('Windows compile launcher changed')
            if path in SHARED_RECIPE_PATHS or path in LEGACY_RECIPE_PATHS:
                hosts.update(HOST_KEYS)
                runtimes.update(RUNTIME_KEYS)
                reasons.add('shared or historical producer contract changed')
        if 'deps.lock.json' in normalized or PROFILE_PATH in normalized:
            if profile_before is None and profile_after is None and PROFILE_PATH not in normalized:
                profile_before = profile_after = load_profile()
            profiles = (_profile_shape(profile_before), _profile_shape(profile_after))
            if PROFILE_PATH in normalized:
                _profile_changes(*profiles, hosts, runtimes, reasons)
            if 'deps.lock.json' in normalized:
                _lock_changes(lock_before, lock_after, profiles, hosts, runtimes, reasons)
    except (OSError, ValueError, TypeError, KeyError, SdkError) as error:
        return _full(f'unavailable or malformed producer diff: {error}')
    return _result(hosts, runtimes, reasons)


def classify_lock_documents(before: dict, after: dict, **profiles: Any) -> dict[str, object]:
    """Compatibility entry point using the same set-based decision."""
    return classify(['deps.lock.json'], lock_before=before, lock_after=after, **profiles)


def build_matrices(host_tools: list[str], runtimes: list[str], *, profile: dict | None = None) -> dict:
    """Expand selected cells only; smoke-tool acquisition is orchestration work."""
    for values, keys in ((host_tools, HOST_KEYS), (runtimes, RUNTIME_KEYS)):
        if not isinstance(values, list) or any(not isinstance(key, str) or key not in keys for key in values):
            raise ValueError('matrix keys must be known strings of the selected kind')
    profile = _profile_shape(profile if profile is not None else load_profile())
    return {
        'host_tools': {'include': [
            {'key': key, 'kind': 'host-tools', 'os': profile['hostTools'][key]['runner'], 'family': HOST_FAMILY[key]}
            for key in HOST_KEYS if key in host_tools]},
        'runtimes': {'include': [
            {'key': key, 'kind': 'runtimes', 'os': profile['runtimes'][key]['runner'],
             'family': PLATFORM_FAMILY[profile['runtimes'][key]['platform']]}
            for key in RUNTIME_KEYS if key in runtimes]},
    }


def _git_output(*args: str) -> str:
    return subprocess.run(['git', *args], check=True, capture_output=True, text=True, encoding='utf-8').stdout


def _git_document(ref: str, path: str) -> dict:
    return parse_json_bytes(_git_output('show', f'{ref}:{path}').encode('utf-8'))


def _git_classification(base_ref: str, head_ref: str) -> dict[str, object]:
    try:
        # Resolve refs before passing them to diff; no option-like revision input.
        base, head = [_git_output('rev-parse', '--verify', '--end-of-options', ref + '^{commit}').strip()
                      for ref in (base_ref, head_ref)]
        # Include both sides of renames; NUL framing preserves arbitrary names.
        paths = [p for p in _git_output('diff', '--no-renames', '--name-only', '-z', base, head, '--').split('\0') if p]
        arguments: dict[str, dict] = {}
        if 'deps.lock.json' in paths or PROFILE_PATH in paths:
            arguments.update(profile_before=_git_document(base, PROFILE_PATH),
                             profile_after=_git_document(head, PROFILE_PATH))
        if 'deps.lock.json' in paths:
            arguments.update(lock_before=_git_document(base, 'deps.lock.json'),
                             lock_after=_git_document(head, 'deps.lock.json'))
        return classify(paths, **arguments)
    except (OSError, subprocess.CalledProcessError, ValueError, SdkError) as error:
        return _full(f'Git/JSON diff unavailable: {error}')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='*')
    parser.add_argument('--base-ref')
    parser.add_argument('--head-ref')
    parser.add_argument('--matrix', help='JSON object containing host_tools and runtimes key arrays')
    args = parser.parse_args()
    if bool(args.base_ref) != bool(args.head_ref):
        parser.error('--base-ref and --head-ref must be provided together')
    if (args.base_ref and args.paths) or (args.matrix is not None and (args.paths or args.base_ref)):
        parser.error('choose exactly one of paths, Git refs, or --matrix')
    if args.matrix is not None:
        try:
            selected = parse_json_bytes(args.matrix.encode('utf-8'))
            result = build_matrices(selected['host_tools'], selected['runtimes'])
        except (OSError, KeyError, ValueError, SdkError) as error:
            parser.error(str(error))
    elif args.base_ref:
        result = _git_classification(args.base_ref, args.head_ref)
    else:
        paths = args.paths
        if not paths and not sys.stdin.isatty():
            paths = [line.rstrip('\r\n') for line in sys.stdin if line.rstrip('\r\n')]
        result = classify(paths)
    print(json.dumps(result, sort_keys=True, separators=(',', ':'), allow_nan=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
