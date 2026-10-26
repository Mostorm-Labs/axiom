"""Record actual local candidate-audit invocations, never a product test run."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


def now():
    return datetime.now(timezone.utc).isoformat()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(value, ensure_ascii=False, indent=2)+'\n')


def ref(root, path):
    data = path.read_bytes()
    return {'path':path.relative_to(root).as_posix(), 'sha256':hashlib.sha256(data).hexdigest(), 'size':len(data)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--run',required=True)
    args = parser.parse_args()
    root = args.repo.resolve()
    out = root / '.aegis/results/FC-08-P20-P31-CLOSURE/attempts/control-001' / args.run
    out.mkdir(parents=True,exist_ok=False)
    git = ['D:/Softwares/Git/cmd/git.exe','-C',str(root)]
    env = dict(os.environ,PYTHONUTF8='1')
    invocations = []
    commands = [
        ('materialize-resume', [sys.executable,'tools/fc08/propose_input_amendment.py','--repo','.']),
        ('audit', [sys.executable,'tools/fc08/audit_input_amendment.py','--repo','.', '--output',str(out/'audit.json')]),
        ('audit-tool-tests',[sys.executable,'-m','unittest','discover','-s','tests/fc08','-p','input_amendment_audit_test.py','-v']),
        ('repository-identity', git + ['remote','get-url','origin']),
        ('ancestry',git + ['merge-base','--is-ancestor','63ea152b2944d21120c968d5e11e166d1096b44f','HEAD']),
        ('preservation',git + ['diff','63ea152b2944d21120c968d5e11e166d1096b44f','--name-only','--diff-filter=CDMRTUXB']),
    ]
    for label, command in commands:
        started = now()
        run = subprocess.run(command,cwd=root,env=env,capture_output=True)
        (out/(label+'.stdout.log')).write_bytes(run.stdout)
        (out/(label+'.stderr.log')).write_bytes(run.stderr)
        invocations.append({'label':label,'argv':command,'cwd':str(root),'start_utc':started,'end_utc':now(),
                            'exit_code':run.returncode,'stdout':ref(root,out/(label+'.stdout.log')),
                            'stderr':ref(root,out/(label+'.stderr.log'))})
    report = {
        'format':'fc08-control-check-invocations-v1', 'observed_at':now(),
        'observed_head':subprocess.check_output(git+['rev-parse','HEAD'],text=True).strip(),
        'observed_branch':subprocess.check_output(git+['branch','--show-current'],text=True).strip(),
        'git_status':subprocess.check_output(git+['status','--short'],text=True),
        'environment':{'python':sys.version,'python_executable':sys.executable,'python_utf8':env['PYTHONUTF8'],
                       'jsonschema':importlib.metadata.version('jsonschema'),'os':platform.platform(),
                       'machine':platform.machine(),'git':subprocess.check_output(git+['--version'],text=True).strip(),
                       'cpp_toolchain':'NOT_INVOKED; no product build authorized before input acceptance',
                       'browser':'NOT_INVOKED; no browser environment qualification claimed'},
        'tools':[ref(root,root/p) for p in ['tools/fc08/propose_input_amendment.py','tools/fc08/audit_input_amendment.py','tests/fc08/input_amendment_audit_test.py']],
        'commands':invocations,'all_commands_exit_zero':all(i['exit_code']==0 for i in invocations),
        'package_preflights':'NOT_RUN_PENDING_INPUT_ACCEPTANCE','ImplementationDesignPreflight':'NOT_RUN_PENDING_INPUT_ACCEPTANCE',
        'FC08-RED-01':'NOT_RUN_PENDING_INPUT_ACCEPTANCE','SUT_invocations':[],
        'nonclaim':'Candidate audit and its unit tests do not establish Axiom product PASS or RED.'}
    write(out/'invocations.json',report)
    print(json.dumps({'run':str(out),'all_commands_exit_zero':report['all_commands_exit_zero'],
                      'exit_codes':{i['label']:i['exit_code'] for i in invocations}},indent=2))
    raise SystemExit(0 if report['all_commands_exit_zero'] else 2)


if __name__ == '__main__':
    main()
