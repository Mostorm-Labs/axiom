#!/usr/bin/env python3
"""Check exact inputs and user-directed closure; not an independent review."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    args = parser.parse_args()
    root = args.repo.resolve()
    pkg = root / '.aegis/packages/AXIOM-SDK-WINDOWS-COMPOSITION-P31-v0.1'
    data = json.loads((pkg / 'package.json').read_text(encoding='utf-8'))
    git = lambda *x: subprocess.run(['git', '-C', str(root), *x], capture_output=True, check=True).stdout
    remote = git('remote', 'get-url', 'origin').decode().strip().lower().removesuffix('.git')
    if remote not in ('https://github.com/mostorm-labs/axiom', 'git@github.com:mostorm-labs/axiom'):
        raise ValueError('repository identity')
    subprocess.run(['git', '-C', str(root), 'merge-base', '--is-ancestor', data['task_anchor']['revision'], 'HEAD'], check=True)
    sha = lambda x: hashlib.sha256(x).hexdigest()
    if sha((pkg / 'user-plan.md').read_bytes()) != data['user_plan_sha256']:
        raise ValueError('plan hash')
    if sha((pkg / 'preflight.py').read_bytes()) != data['preflight_sha256']:
        raise ValueError('preflight hash')
    for item in data['inputs']:
        path = (root / item['path']).resolve()
        if not path.is_relative_to(root) or sha(path.read_bytes()) != item['sha256']:
            raise ValueError('working tree input: ' + item['path'])
        if sha(git('show', item['revision'] + ':' + item['path'])) != item['sha256']:
            raise ValueError('revision input: ' + item['path'])
    closure = data['closure']
    if not all(closure.get(key) for key in ('required_changes', 'forbidden', 'required_tests', 'evidence',
                                            'terminal_success', 'terminal_blockers', 'continue_until_terminal_state')):
        raise ValueError('incomplete closure')
    print('PASS package/input/evidence inventory; user direction only, not Control or Gate approval')


if __name__ == '__main__':
    main()
