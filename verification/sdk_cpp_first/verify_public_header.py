#!/usr/bin/env python3
"""Compile the public Facade header outside the tree; not an installed SDK test."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();root=args.repo.resolve()
    result={'status':'NOT_RUN','note':'Copied-public-header compile only; no library link, installation, product behavior, or ABI-stability claim.'}
    def finish(code):
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(result['status']);return code
    cl=shutil.which('cl.exe')
    if not cl:
        result['status']='BLOCKED_COMPILER';return finish(2)
    source=root/'verification/sdk_cpp_first/public_header_consumer.cpp'
    includes=[root/'runtime/foundation/include',root/'runtime/semantic/include']
    if not source.is_file() or not all(p.is_dir() for p in includes):
        result.update(status='BLOCKED_INPUT',reason='source or public include root missing');return finish(2)
    try:
        with tempfile.TemporaryDirectory(prefix='axiom-cpp-public-') as temporary:
            folder=Path(temporary).resolve()
            if folder.is_relative_to(root):raise ValueError('consumer directory must be outside repository')
            public=folder/'include'
            for include in includes:shutil.copytree(include,public,dirs_exist_ok=True)
            target=folder/'consumer.cpp';shutil.copyfile(source,target)
            command=[cl,'/nologo','/std:c++20','/W4','/WX','/EHsc','/showIncludes','/I'+str(public),'/c',str(target),'/Fo'+str(folder/'consumer.obj')]
            p=subprocess.run(command,cwd=folder,capture_output=True,text=True,check=False)
            lines=(p.stdout+'\n'+p.stderr).replace('\\','/').lower().splitlines()
            public_path=str(public).replace('\\','/').lower()+'/canvas/'
            project_lines=[line for line in lines if public_path in line]
            forbidden=('/canvas/interaction/','/canvas/scene/','/canvas/render/','/canvas/runtime/internal/','/skia/')
            leaks=[line for line in project_lines if any(x in line for x in forbidden)]
            result.update(compiler=cl,command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr,
                          consumer_sha256=sha(source),public_include_count=len(project_lines),forbidden_includes=leaks)
            if p.returncode or not project_lines or leaks:
                result['status']='FAILED_HEADER_BOUNDARY';return finish(1)
            result['status']='PASS_HEADER_COMPILE_ONLY';return finish(0)
    except (OSError,ValueError) as exc:
        result.update(status='BLOCKED_INPUT',reason=str(exc));return finish(2)

if __name__=='__main__':sys.exit(main())
