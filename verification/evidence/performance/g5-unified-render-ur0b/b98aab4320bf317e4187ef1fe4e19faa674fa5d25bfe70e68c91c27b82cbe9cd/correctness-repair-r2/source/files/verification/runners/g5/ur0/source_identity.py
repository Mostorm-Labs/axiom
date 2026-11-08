"""Content identity of the complete tracked + nonignored dirty repository.

Only this task's self-referential result/evidence outputs are excluded. Historic
evidence, authority, all product sources and runner/tests remain bound. No index
or branch mutation is used. An exact patch archive accompanies the final result.
"""
import hashlib,json,subprocess,sys
from pathlib import Path

EXCLUDED=('.aegis/results/G5-UR0b/','verification/evidence/performance/g5-unified-render-ur0b/')
def identity(root):
    root=Path(root)
    def git(*args):return subprocess.run(['git','-C',str(root),*args],capture_output=True,check=True).stdout
    names=sorted(set(git('ls-files','-z','--cached','--others','--exclude-standard').decode().split('\0'))-{''})
    files={}
    for name in names:
        if name.startswith(EXCLUDED):continue
        p=root/name
        if p.is_symlink():files[name]={'kind':'symlink','target':str(p.readlink())};continue
        if not p.is_file():files[name]={'kind':'deleted'};continue
        h=hashlib.sha256()
        with p.open('rb') as f:
            for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
        files[name]={'sha256':h.hexdigest(),'executable':bool(p.stat().st_mode&0o111)}
    revision=subprocess.run(['git','-C',str(root),'rev-parse','HEAD'],capture_output=True,text=True)
    encoded=json.dumps(files,sort_keys=True,separators=(',',':')).encode()
    return {'schema':'g5-ur0b-full-source-identity-v1','revision':revision.stdout.strip() if revision.returncode==0 else None,
            'source_tree_sha256':hashlib.sha256(encoded).hexdigest(),'excluded_self_outputs':list(EXCLUDED),'files':files}

if __name__=='__main__':print(json.dumps(identity(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[4])))
