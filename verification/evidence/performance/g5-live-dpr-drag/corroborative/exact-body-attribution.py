from pathlib import Path
from collections import defaultdict,Counter
import hashlib,gzip,json

def bodies(path):
 b=Path(path).read_bytes(); p=8; imports=0; result={}
 def u():
  nonlocal p
  v=s=0
  while True:
   x=b[p];p+=1;v|=(x&127)<<s
   if x<128:return v
   s+=7
 def name():
  nonlocal p
  n=u();p+=n
 def limits():
  f=u();u()
  if f&1:u()
 while p<len(b):
  sid=b[p];p+=1;size=u();end=p+size
  if sid==2:
   for _ in range(u()):
    name();name();kind=b[p];p+=1
    if kind==0:imports+=1;u()
    elif kind==1:p+=1;limits()
    elif kind==2:limits()
    elif kind==3:p+=2
    elif kind==4:u();u()
    else:raise ValueError(kind)
  if sid==10:
   for n in range(u()):
    size=u();result[n+imports]=hashlib.sha256(b[p:p+size]).hexdigest();p+=size
  p=end
 return result
old=bodies('out/g5-cumulative-symbols/axiom.wasm'); cur=bodies('out/g5-live-reclaim-clean-off-64-192/js/axiom.wasm'); by=defaultdict(list)
for k,h in old.items():by[h].append(k)
names={int(s.split(':',1)[0]):s.split(':',1)[1] for s in Path('out/g5-cumulative-symbols/axiom.mjs.symbols').read_text().splitlines()}
d=json.loads(gzip.decompress(Path('out/g5-drag-explore-profile-data/drag.cpu.json.gz').read_bytes()));counts=Counter();nodes={v['id']:v for v in d['nodes']}
for i in d['samples']:
 fn=nodes[i]['callFrame']['functionName'];counts[fn]+=1
out=[]
for fn,count in counts.most_common(30):
 found=None
 if fn.startswith('wasm-function['):
  idx=int(fn[14:-1]); candidates=by.get(cur.get(idx),[])
  if len(candidates)==1:found=names.get(candidates[0])
 out.append({'function':fn,'samples':count,'unique_exact_body_match':found})
Path('out/g5-drag-explore-profile-data/leaf-attribution.json').write_text(json.dumps(out,indent=2))
for v in out:print(v)
