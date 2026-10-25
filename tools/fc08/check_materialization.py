"""Report exact artifact integrity and readiness without issuing a Gate verdict.

Exit 2 means a recorded terminal blocker. It is deliberately different from a
passing materialization and from a tool/environment crash (exit 1).
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--manifest',type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    mat = repo/'.aegis/materialized/FC-08'
    results = repo/'.aegis/results/FC-08-VERIFY-MATERIALIZE'
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    sources = json.loads((mat/'source-index.json').read_text(encoding='utf-8'))
    errors = []
    for item in sources['accepted_exports']+sources['seed_sources']+sources['schema_sources']+[sources['inherited_math_export']]:
        path = repo/item['path']
        if not path.is_file() or sha(path) != item['sha256']:
            errors.append('source hash mismatch: '+item['path'])
    seed = mat/'sources/accepted-inputs.zip'
    if sha(seed) != '378447a1590bd4f00b563b2025cbd186c564def565f8e323acb282ff1c2d5bfb':
        errors.append('accepted archive hash mismatch')
    descriptor = json.loads((mat/'descriptor-diff.json').read_text())
    for label in ('baseline','proposed'):
        if sha(mat/f'{label}-descriptor/descriptor.pb') != descriptor[label+'_sha256']:
            errors.append(label+' descriptor hash mismatch')
    controls = json.loads((mat/'oracle-controls.json').read_text())
    if not (controls['C01_bad']['first_failure']==1 and controls['C02_bad']['first_failure']==2
            and controls['C01_good'] is None and controls['C02_good'] is None):
        errors.append('R02 fixed control outcome mismatch')
    git = 'D:/Softwares/Git/cmd/git.exe'
    changed = subprocess.check_output([git,'diff','--name-only','HEAD'],cwd=repo,text=True).splitlines()
    unknown = subprocess.check_output([git,'ls-files','--others','--exclude-standard'],cwd=repo,text=True).splitlines()
    prefixes = ('.aegis/materialized/FC-08/','.aegis/results/FC-08-VERIFY-MATERIALIZE/','tests/fc08/','tools/fc08/')
    exception = '.aegis/packages/FC-08-VERIFY-MATERIALIZE/accepted-inputs.zip'
    forbidden = [p for p in changed+unknown if p != 'CMakeLists.txt' and p != exception and not p.startswith(prefixes)]
    if forbidden: errors.append('out-of-scope changes: '+str(forbidden))
    if args.manifest:
        manifest = json.loads(args.manifest.read_text())
        for item in manifest['files']:
            path = repo/item['path']
            actual = hashlib.sha256(subprocess.check_output([git,'show',':'+item['path']],cwd=repo)).hexdigest() if item.get('content_basis') == 'git-index-blob' else sha(path) if path.is_file() else None
            if actual != item['sha256']:
                errors.append('output manifest mismatch: '+item['path'])
    terminal = json.loads((results/'terminal-result.json').read_text())
    corpus = json.loads((mat/'corpus-status.json').read_text())
    state = 'INTEGRITY_FAILURE' if errors else terminal['terminal_state']
    output = {'format':'fc08-materialization-check-v1','status':state,'integrity_errors':errors,
        'M01_artifact_integrity':not errors,'M02_complete':corpus['full_catalog_complete'],
        'M03_runner_exists':(repo/'tools/fc08/run_verification.py').exists(),
        'ready_for_control_review':False,'terminal_blockers':[b['id'] for b in terminal['exact_blockers']],
        'product_P32_authorized':False,'gate_pass_claimed':False}
    text = json.dumps(output,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text)
    print(text,end='')
    return 1 if errors else 2


if __name__ == '__main__': sys.exit(main())
