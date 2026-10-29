#!/usr/bin/env python3
"""Check the P32 ingress scope correction against its exact baseline."""
import hashlib
import json
import pathlib
import subprocess
import sys

P=pathlib.Path(__file__).resolve().parent
ROOT=P.parents[2]
BASE='a6b68e36dacd95b64d9e23586c271db87edc901a'
BRANCH='codex/fc04-mse-p31-v02-binding-fix'

def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
def need(value,reason):
    if not value:raise ValueError(reason)
def read(name):return json.loads((P/name).read_text(encoding='utf-8'))
def sha(value):return hashlib.sha256(value).hexdigest()

def main():
    need(len(sys.argv)==3 and sys.argv[1]=='--repo' and pathlib.Path(sys.argv[2]).resolve()==ROOT.resolve(),'--repo must be this worktree')
    head=git('rev-parse','HEAD');need(git('branch','--show-current')==BRANCH,'wrong branch')
    need(git('remote','get-url','origin').lower().removesuffix('.git') in ('https://github.com/mostorm-labs/axiom','git@github.com:mostorm-labs/axiom'),'wrong origin')
    need(subprocess.run(['git','merge-base','--is-ancestor',BASE,head],cwd=ROOT).returncode==0,'baseline not ancestor')
    need(not git('status','--porcelain=v1','--untracked-files=all'),'worktree not clean')
    parent=read('package.json'); lock=read('source-lock.json')
    for k in ('parent_package','parent_authorization'):
        item=parent[k]; data=subprocess.check_output(['git','show',item['revision']+':'+item['path']],cwd=ROOT)
        need(sha(data)==item['sha256'],k+' identity mismatch')
    for e in lock['files']:
        path=e['path']; original=subprocess.check_output(['git','show',BASE+':'+path],cwd=ROOT)
        current=subprocess.check_output(['git','show',head+':'+path],cwd=ROOT)
        need(git('rev-parse',BASE+':'+path)==e['git_blob_sha'] and sha(original)==e['sha256'],'baseline lock mismatch '+path)
        need(sha(current)==e['sha256'] and sha((ROOT/path).read_bytes())==e['sha256'],'executing source mismatch '+path)
    for e in read('integrity.json')['files']:
        actual=(P/e['path']).read_bytes();tracked=subprocess.check_output(['git','show',head+':'+P.relative_to(ROOT).as_posix()+'/'+e['path']],cwd=ROOT)
        need(sha(actual)==e['sha256'] and actual==tracked,'package mismatch '+e['path'])
    tracked=subprocess.check_output(['git','show',head+':'+P.relative_to(ROOT).as_posix()+'/integrity.json'],cwd=ROOT)
    need(tracked==(P/'integrity.json').read_bytes(),'integrity mismatch')
    c=(P/'closure.md').read_text(encoding='utf-8')
    for word in ('canvas_runtime_facade.hpp','canvas_runtime_text.cpp','TextEdit','D07','zero Document/Scene/History/redo','NOT_RUN'):
        need(word in c,'contract missing '+word)
    print('PASS ingress amendment: exact package, parent authority, baseline sources, ordering and no-mutation scope; implementation tests NOT_RUN; HEAD='+head)

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError) as ex:print(str(ex),file=sys.stderr);sys.exit(2)
