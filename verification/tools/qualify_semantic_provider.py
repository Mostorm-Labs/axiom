#!/usr/bin/env python3
"""Exercise real candidate packages through the Store; never build or publish SDKs."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from unittest import mock

if __package__ in {None, ''}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import SdkError, detect_host_platform
from tools.semantic.aggregate import verify_release_directory
from tools.semantic.contract import ROOT, RUNTIME_KEYS, read_json
from tools.semantic.provider import select_keys
from tools.setup_build_environment import setup_environment
from tools.update_semantic_lock import make_v2_lock


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SdkError('provider qualification: ' + message)


def qualify(release_directory: Path) -> dict:
    release = verify_release_directory(release_directory)
    lock = make_v2_lock(release_directory / 'semantic-sdk-index.json', release['tag'])
    host = detect_host_platform()
    _, native = select_keys(host, 'native')
    targets = [native]
    for key in RUNTIME_KEYS:
        if key == native:
            continue
        try:
            select_keys(host, key)
        except SdkError:
            continue
        targets.append(key)
    with tempfile.TemporaryDirectory(prefix='semantic-provider-') as directory:
        base = Path(directory)
        mirror, store = base / 'mirror', base / 'shared SDK Store'
        shutil.copytree(release_directory, mirror / release['tag'])
        projects = [base / 'project-one', base / 'another/deeper project']
        for repo in projects:
            repo.mkdir(parents=True)
            shutil.copyfile(ROOT / 'deps.lock.json', repo / 'deps.lock.json')
            (repo / 'semantic-sdk.lock.json').write_bytes(canonical_bytes(lock) + b'\n')
        results = []
        # This is verification code: fail the entire test on any HTTP attempt.
        with mock.patch('tools.sdk.transport._open', side_effect=AssertionError('unexpected network')):
            for target in targets:
                start = time.monotonic()
                facts = setup_environment(core=False, semantic=True, target=target, repo_root=projects[0],
                                          store_root=store, mirror=str(mirror))
                elapsed = time.monotonic() - start
                require(not facts['semantic']['resolution']['networkUsed'], 'filesystem mirror used network')
                results.append({'target': target, 'coldSeconds': elapsed, 'cold': facts})
                if target == native:
                    require(len(list((store / 'archives/sha256').glob('*/*.zip'))) == 2,
                            'native request downloaded more than one host/runtime pair')
            manifest_times = {str(p): p.stat().st_mtime_ns for p in (store / 'packages').rglob('manifest.json')}
            shutil.rmtree(mirror)
            shutil.rmtree(store / 'archives')
            for result in results:
                start = time.monotonic()
                warm = setup_environment(core=False, semantic=True, target=result['target'], repo_root=projects[1],
                                         store_root=store, offline=True)
                result['warmSeconds'] = time.monotonic() - start
                result['warm'] = warm
                for name in ('AXIOM_PROTOC', 'AXIOM_SEMANTIC_HOST_ROOT', 'AXIOM_SEMANTIC_RUNTIME_ROOT'):
                    require(warm['environment'][name] == result['cold']['environment'][name], 'worktree path drift')
                resolution = warm['semantic']['resolution']
                require(not resolution['networkUsed'], 'offline request used network')
                require(all(item['source'] == 'store' for item in resolution['artifacts']), 'offline Store miss')
        require(manifest_times == {str(p): p.stat().st_mtime_ns for p in (store / 'packages').rglob('manifest.json')},
                'warm resolve reinstalled SDKs')
        environment = results[0]['warm']['environment']
        version = subprocess.check_output([environment['AXIOM_PROTOC'], '--version'], text=True).strip()
        expected_version = 'libprotoc ' + read_json(release_directory / 'semantic-sdk-index.json')['dependencies']['protobuf']['version']
        require(version == expected_version, 'host protoc version mismatch')
        cli_env = {**os.environ, 'AXIOM_SDK_STORE': str(store), 'AXIOM_SDK_MIRROR': str(mirror)}
        candidate_lock = projects[1] / 'semantic-sdk.lock.json'
        command = [sys.executable, str(ROOT / 'tools/setup_build_environment.py'), '--semantic',
                   '--lock', str(candidate_lock), '--offline', '--status',
                   '--github-env', str(base / 'github-env'), '--facts-output', str(base / 'facts.json')]
        cli = subprocess.run(command, env=cli_env, capture_output=True, text=True, check=True)
        cli_facts = json.loads(cli.stdout)
        require(cli_facts == read_json(base / 'facts.json'), 'CLI facts output mismatch')
        require(release['tag'] in cli.stderr, 'status omits release identity')
        exported = dict(line.split('=', 1) for line in (base / 'github-env').read_text().splitlines())
        require(exported['AXIOM_PROTOC'] == environment['AXIOM_PROTOC'], 'CLI export mismatch')
        fetched = subprocess.run([sys.executable, str(ROOT / 'tools/semantic_fetch.py'), '--lock',
                                  str(candidate_lock), '--offline'], env=cli_env, capture_output=True,
                                 text=True, check=True)
        require(json.loads(fetched.stdout)['environment']['AXIOM_PROTOC'] == environment['AXIOM_PROTOC'],
                'compatibility CLI selected another host tool')
        require(all(not (repo / '.deps').exists() for repo in projects), 'v2 created repository .deps')
    return {'format': 'axiom-semantic-provider-qualification-v2', 'status': 'PASS', 'host': host.key,
            'releaseSetId': lock['releaseSetId'], 'indexSha256': lock['indexSha256'],
            'sourceFree': True, 'networkUsed': False, 'nativeProtocVersion': version,
            'compiledDependencies': False, 'targetCount': len(results), 'targets': results,
            'cliAndEnvironmentExports': 'PASS', 'secondWorktreeOfflineWithoutArchives': 'PASS'}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-directory', type=Path, required=True)
    parser.add_argument('--facts-output', type=Path, required=True)
    args = parser.parse_args()
    facts = qualify(args.release_directory)
    args.facts_output.parent.mkdir(parents=True, exist_ok=True)
    args.facts_output.write_bytes(canonical_bytes(facts) + b'\n')
    print(canonical_bytes({key: value for key, value in facts.items() if key != 'targets'}).decode())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
