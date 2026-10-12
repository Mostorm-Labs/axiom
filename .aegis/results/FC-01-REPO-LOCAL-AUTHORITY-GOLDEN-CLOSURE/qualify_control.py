from __future__ import annotations
import hashlib,json,pathlib,re,struct,subprocess
ROOT=pathlib.Path(__file__).parents[3]; EV=pathlib.Path(__file__).parent
BASE=pathlib.Path(r'C:/Users/Chado/.codex/worktrees/fc-00-fc-track-authority/canvas')
def J(p): return json.loads(p.read_text(encoding='utf-8'))
def H(b): return hashlib.sha256(b).hexdigest()
def V(n):
 o=bytearray()
 while n>127:o.append((n&127)|128);n>>=7
 o.append(n);return bytes(o)
def F(t,w,b):return V(t<<3|w)+b
def U(t,n):return F(t,0,V(n))
def M(t,b):return F(t,2,V(len(b))+b)
def D(t,x):return F(t,1,struct.pack('<d',x))
def P(b):
 out=[];i=0
 while i<len(b):
  k=0;s=0
  while 1:
   c=b[i];i+=1;k|=(c&127)<<s
   if c<128:break
   s+=7
  t,w=k>>3,k&7
  if w==0:
   x=0;s=0
   while 1:
    c=b[i];i+=1;x|=(c&127)<<s
    if c<128:break
    s+=7
   v=x
  elif w==1:v=b[i:i+8];i+=8
  elif w==2:
   n=0;s=0
   while 1:
    c=b[i];i+=1;n|=(c&127)<<s
    if c<128:break
    s+=7
   v=b[i:i+n];i+=n
  else:raise ValueError(w)
  out.append((t,w,v))
 return out
def only(x,t):assert [a[0] for a in x]==t
allowed={'.aegis/authorities.json','verification/corpus/semantic/shape2-triangle/v1/authoring/cases.json','verification/corpus/semantic/shape2-triangle/v1/authoring/expected.json','verification/corpus/semantic/shape2-triangle/v1/manifest.json','verification/corpus/semantic/shape2-triangle/v1/wire/SH2-G15-insertoperation.hex','verification/corpus/semantic/shape2-triangle/v1/wire/SH2-G15-objectrecord.hex','verification/corpus/semantic/shape2-triangle/v1/wire/SH2-G15-shapecontent.hex'}
BASE_SHA='2942c63b375366ed977b834aa0a48fe2fb551c6e'; HEAD_SHA='19ec3483240ab03152678b3ea7fb9f568da39852'
assert subprocess.check_output(['git','merge-base',BASE_SHA,HEAD_SHA],cwd=ROOT,text=True).strip()==BASE_SHA
assert set(subprocess.check_output(['git','diff','--name-only',BASE_SHA,HEAD_SHA],cwd=ROOT,text=True).splitlines())==allowed
history={}
for n,key,count in [('.aegis/gates.json','decisions',25),('.aegis/integrations.json','integrations',29),('.aegis/evidence.json','evidence',34)]:
 b=(BASE/n).read_bytes();h=(ROOT/n).read_bytes();assert b==h
 assert len(J(ROOT/n)[key])==count
 history[n]={'byte_equal':True,'count':count,'sha256':H(b)}
a=J(ROOT/'.aegis/authorities.json')['authorities'];by={x['id']:x for x in a}
assert by['fc-feature-completion-execution-v0.1']['status']=='Superseded'
assert by['fc-feature-completion-execution-v0.1']['superseded_by']=='fc-feature-completion-execution-v0.2'
assert by['fc-feature-completion-execution-v0.2']['status']=='Current'
assert by['fc01-shape-triangle-semantic-v0.1']['status']=='Current'
cur=set()
for x in a:
 if x['status']=='Current':
  k=(x['scope'],x['kind']);assert k not in cur;cur.add(k)
 for d in x.get('depends_on',[]):assert d in by
for x in ['g2-canonical-main','g3-capability-authority','g4-capability-authority','g4-capability-authority-v0.3']:
 assert by[x]=={z['id']:z for z in J(BASE/'.aegis/authorities.json')['authorities']}[x]
for n,pairs in [('schema/axiom/v1/proto/auditoryworks/axiom/v1/object.proto',[('id',1),('kind_id',2),('kind_version',3),('placement',4),('transform',5),('properties',6),('content',7)]),('schema/axiom/v1/proto/auditoryworks/axiom/v1/geometry.proto',[('a',1),('b',2),('c',3),('d',4),('tx',5),('ty',6)]),('schema/axiom/v1/proto/auditoryworks/axiom/v1/operation.proto',[('operation_id',1),('document_id',2),('schema_version',3),('payload_version',4),('payload',5)])]:
 t=(ROOT/n).read_text()
 assert 'edition = "2024"' in t
 for name,num in pairs:assert re.search(r'\b'+name+r'\s*=\s*'+str(num)+r'\s*;',t)
desc=ROOT/'schema/axiom/v1/descriptor/descriptor.lock.pb';assert H(desc.read_bytes())=='8c1da1c19b754f9345472f96452e58505ebd18c0874b04480e378eb95a38654d'
id1=bytes.fromhex('00'*15+'01');id2=bytes.fromhex('00'*15+'02');id3=bytes.fromhex('00'*15+'03')
shape=U(1,3)+D(2,100)+D(3,80)
ident=lambda b:M(1,b)
placement=M(2,ident(bytes.fromhex('80')))
transform=D(1,1)+D(2,0)+D(3,0)+D(4,1)+D(5,0)+D(6,0)
obj=M(1,ident(id1))+U(2,1)+U(3,2)+M(4,placement)+M(5,transform)+M(6,b'')+M(7,M(1,shape))
insert=M(1,obj);operation=M(1,ident(id2))+M(2,ident(id3))+U(3,1)+U(4,1)+M(5,M(1,insert))
vec=[('SH2-G15-shapecontent.hex',shape,20,'c882e59071d419f7db2d7209bb674979beca61dea5cb39a7bc6fd272a8f334c1'),('SH2-G15-objectrecord.hex',obj,113,'b375c7c0c6731ecbbc5137809b212e46af967d3856f35bbb228e85a74d900afc'),('SH2-G15-insertoperation.hex',operation,163,'64232960c062798d18919528a0468a2eac70384650a4320171b8c49d48ddbb24')]
wr={}
for fn,want,l,h in vec:
 b=bytes.fromhex(''.join((ROOT/'verification/corpus/semantic/shape2-triangle/v1/wire'/fn).read_text().split()));assert b==want and len(b)==l and H(b)==h
 wr[fn]={'bytes':l,'sha256':h,'independent_wire_match':True}
sf=P(shape);only(sf,[1,2,3]);assert sf[0][2]==3 and struct.unpack('<d',sf[1][2])[0]==100 and struct.unpack('<d',sf[2][2])[0]==80
of=P(obj);only(of,[1,2,3,4,5,6,7]);assert P(of[5][2])==[]
pf=P(operation);only(pf,[1,2,3,4,5]);assert P(pf[0][2])[0][2]==id2 and P(pf[1][2])[0][2]==id3
cases=J(ROOT/'verification/corpus/semantic/shape2-triangle/v1/authoring/cases.json');expected=J(ROOT/'verification/corpus/semantic/shape2-triangle/v1/authoring/expected.json')
ids=[f'SH2-G{i:02d}' for i in range(1,26)]
assert [x['id'] for x in cases['cases']]==ids and list(expected['cases'])==ids
assert expected['cases']['SH2-G24']['status']=='NO_SUBMISSION' and expected['cases']['SH2-G06']['status']=='REJECT'
g15=expected['cases']['SH2-G15']['wire'];assert (g15['shapecontent']['byte_length'],g15['objectrecord']['byte_length'],g15['insertoperation']['byte_length'])==(20,113,163)
old=ROOT/'verification/corpus/semantic/v1/suites/shape-kind-v1.json';assert old.read_bytes()==(BASE/'verification/corpus/semantic/v1/suites/shape-kind-v1.json').read_bytes() and J(old)['vectors']==[]
report={'schema':'fc01-control-qualification-v0.1','status':'READY_FOR_CONTROL_REVIEW_NO_INDEPENDENT_ACCEPTANCE','base_revision':BASE_SHA,'result_revision':HEAD_SHA,'historical_control':history,'authority_current_scope_kind_count':len(cur),'authority_key_statuses':{k:by[k]['status'] for k in ['fc-feature-completion-execution-v0.1','fc-feature-completion-execution-v0.2','fc01-shape-triangle-semantic-v0.1']},'descriptor_sha256':H(desc.read_bytes()),'golden':{'case_count':25,'g24':'NO_SUBMISSION','g06':'REJECT','g15':wr,'independent_acceptance':'PENDING_CONTROL_REVIEW'},'nonclaims':['no product/runtime changes','no FC-01 P31/P32','no Gate PASS','no Golden acceptance by producer']}
(EV/'qualification-report.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))

