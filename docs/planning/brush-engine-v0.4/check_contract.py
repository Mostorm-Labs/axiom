#!/usr/bin/env python3
"""Dependency-free P31 contract/golden checks; does not test production code."""
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
# Frozen protobuf field table, independently explicit instead of production codec.
TYPES = {
 'Id128': [(1,'value','bytes')],
 'OrderKey': [(1,'value','bytes')],
 'Vec2': [(1,'x','double'),(2,'y','double')],
 'Transform2D': [(i+1,k,'double') for i,k in enumerate(['a','b','c','d','tx','ty'])],
 'Placement': [(1,'parent_id','Id128?'),(2,'order_key','OrderKey')],
 'PropertyBag': [],
 'BrushStageBinding': [(1,'stage_id','uint'),(2,'declared_mode','uint'),(3,'active','bool'),(4,'node_id','uint'),(5,'node_version','uint')],
 'BrushVectorParameters': [(i+1,k,t) for i,(k,t) in enumerate([('size','double'),('thinning','double'),('smoothing','double'),('streamline','double'),('pressure_source','uint'),('missing_pressure','uint'),('easing_id','uint'),('start_cap','bool'),('end_cap','bool'),('start_taper','double'),('end_taper','double')])],
 'BrushSolidPaint': [(i+1,k,'double' if i<5 else 'uint') for i,k in enumerate(['red','green','blue','alpha','opacity','blend','color_space'])],
 'BrushExecutionSnapshot': [(1,'snapshot_version','uint'),(2,'package_id','Id128'),(3,'package_revision','uint'),(4,'pipeline_version','uint'),(5,'defaults_version','uint'),(6,'profile_id','uint'),(7,'stages','BrushStageBinding*'),(8,'vector','BrushVectorParameters'),(9,'paint','BrushSolidPaint'),(10,'resources','BrushResourceBinding*'),(11,'seed','fixed64'),(12,'signal_schema_version','uint')],
 'BrushResourceBinding': [(1,'resource_id','Id128'),(2,'content_sha256','bytes')]+[(i+3,k,'uint') for i,k in enumerate(['kind','decode_version','channel','color_space','sampling','wrap'])],
 'BrushConfirmedSample': [(1,'position','Vec2'),(2,'pressure','double?')],
 'BrushVectorOutput': [(1,'outline','Vec2*'),(2,'fill_rule','uint'),(3,'closed','bool')],
 'BrushStrokeRecord': [(1,'snapshot','BrushExecutionSnapshot'),(2,'confirmed_samples','BrushConfirmedSample*'),(3,'vector_output','BrushVectorOutput')],
 'BrushStrokeContent': [(1,'stroke','BrushStrokeRecord')],
 'ObjectContent': [(10,'brush_stroke','BrushStrokeContent')],
 'ObjectRecord': [(1,'id','Id128'),(2,'kind_id','uint'),(3,'kind_version','uint'),(4,'placement','Placement'),(5,'transform','Transform2D'),(6,'properties','PropertyBag'),(7,'content','ObjectContent')],
 'AddStrokeOp': [(1,'object','ObjectRecord')],
 'OperationPayload': [(10,'add_stroke','AddStrokeOp')],
 'Operation': [(1,'operation_id','Id128'),(2,'document_id','Id128'),(3,'schema_version','uint'),(4,'payload_version','uint'),(5,'payload','OperationPayload')],
 'DocumentSnapshot': [(1,'document_id','Id128'),(2,'schema_version','uint'),(3,'objects','ObjectRecord*')],
}

def varint(n):
 assert 0 <= n < 2**64
 b=bytearray()
 while n>127: b.append((n&127)|128);n>>=7
 b.append(n);return bytes(b)

def encode(typ, obj):
 assert not set(obj)-{k for _,k,_ in TYPES[typ]}, (typ,'unknown fields')
 out=b''
 for tag,key,kind in TYPES[typ]:
  repeated=kind.endswith('*');optional=kind.endswith('?');kind=kind.rstrip('*?')
  if optional and key not in obj: continue
  values=obj.get(key,[]) if repeated else [obj[key]]
  for value in values:
   if kind in ('uint','bool'): wire=0;data=varint(int(value))
   elif kind=='fixed64': wire=1;data=struct.pack('<Q',value)
   elif kind=='double':
    assert math.isfinite(value);wire=1;data=struct.pack('<d',0.0 if value==0 else value)
   else:
    wire=2;raw=bytes.fromhex(value) if kind=='bytes' else encode(kind,value)
    data=varint(len(raw))+raw
   out+=varint(tag*8+wire)+data
 return out

def decode(typ, buf):
 pos=0;out={k:[] for _,k,t in TYPES[typ] if t.endswith('*')};seen=set()
 def takevar():
  nonlocal pos
  n=shift=0
  while True:
   x=buf[pos];pos+=1;n|=(x&127)<<shift
   if x<128:return n
   shift+=7
   assert shift<70
 fields={tag:(k,t) for tag,k,t in TYPES[typ]}
 while pos<len(buf):
  word=takevar();tag,wire=word>>3,word&7;key,kind=fields[tag];rep=kind.endswith('*');kind=kind.rstrip('*?')
  assert rep or tag not in seen;seen.add(tag)
  if wire==0: value=takevar();value=bool(value) if kind=='bool' else value
  elif wire==1: value=struct.unpack('<d' if kind=='double' else '<Q',buf[pos:pos+8])[0];pos+=8
  else:
   assert wire==2;n=takevar();data=buf[pos:pos+n];pos+=n
   value=data.hex() if kind=='bytes' else decode(kind,data)
  if rep:out[key].append(value)
  else:out[key]=value
 assert encode(typ,out)==buf
 return out

def digest(domain,b):return hashlib.sha256(domain.encode()+b'\0'+b).hexdigest()
def read(name):return json.loads((HERE/'examples'/name).read_text())
def write(name,value):(HERE/'examples'/name).write_text(json.dumps(value,indent=2)+'\n')

def generate():
 corpus=json.loads((ROOT/'.aegis/results/GT-G4-5-R01/corpus-inputs.json').read_text())
 fixture=next(x for x in corpus['fixtures'] if x['id']=='simulated_pressure_speed')
 outlines=json.loads((ROOT/'.aegis/results/GT-G4-5-R01/reference-outlines.json').read_text())['fixtures'][fixture['id']]
 snap={'snapshot_version':1,'package_id':{'value':read('manifest.json')['packageId']},'package_revision':1,'pipeline_version':1,'defaults_version':1,'profile_id':1,'stages':[],'vector':{'size':16,'thinning':0.5,'smoothing':0.5,'streamline':0.5,'pressure_source':1,'missing_pressure':1,'easing_id':1,'start_cap':True,'end_cap':True,'start_taper':0,'end_taper':0},'paint':{'red':0.05,'green':0.1,'blue':0.2,'alpha':1,'opacity':1,'blend':1,'color_space':1},'resources':[],'seed':42,'signal_schema_version':1}
 for i in range(1,8):
  node={1:1,2:2,6:3}.get(i,0);snap['stages'].append({'stage_id':i,'declared_mode':3 if node else 1,'active':bool(node),'node_id':node,'node_version':1 if node else 0})
 stroke={'snapshot':snap,'confirmed_samples':[{'position':{'x':p[0],'y':p[1]}} for p in fixture['points']],'vector_output':{'outline':[{'x':p[0],'y':p[1]} for p in outlines],'fill_rule':1,'closed':True}}
 oid=lambda n:{'value':format(n,'032x')}
 obj={'id':oid(10),'kind_id':5,'kind_version':2,'placement':{'order_key':{'value':'80'}},'transform':{'a':1,'b':0,'c':0,'d':1,'tx':32,'ty':64},'properties':{},'content':{'brush_stroke':{'stroke':stroke}}}
 op={'operation_id':oid(11),'document_id':oid(12),'schema_version':1,'payload_version':2,'payload':{'add_stroke':{'object':obj}}}
 doc={'document_id':oid(12),'schema_version':2,'objects':[obj]}
 for name,value in [('snapshot.json',snap),('stroke.json',stroke),('add-stroke.json',op),('document-snapshot.json',doc)]:write(name,value)
 write('replay.json',{'reference_fixture':fixture['id'],'reference_commit':'176e00f2399f4969e1b0965c5921d96a3e50ce9f','input':'add-stroke.json','expected':'document-snapshot.json','operation_payload_version':2,'snapshot_schema_version':2,'recompute_evaluator_on_document_replay':False,'expected_nonempty_outline_vertices':len(outlines),'semantic_comparison':'exact stored snapshot/samples/outline; R1 reevaluation tolerance is separate'})
 for typ,name in [('BrushExecutionSnapshot','snapshot'),('BrushStrokeRecord','stroke'),('Operation','add-stroke'),('DocumentSnapshot','document-snapshot')]:
  b=encode(typ,read(name+'.json'));(HERE/'examples'/(name+'.pb.hex')).write_text(b.hex()+'\n')
 write('checksums.json',{'snapshot':digest('axiom.brush.snapshot.v1',encode('BrushExecutionSnapshot',snap)),'stroke':digest('axiom.brush.stroke.v1',encode('BrushStrokeRecord',stroke))})

def check():
 for typ,name in [('BrushExecutionSnapshot','snapshot'),('BrushStrokeRecord','stroke'),('Operation','add-stroke'),('DocumentSnapshot','document-snapshot')]:
  obj=read(name+'.json'); b=bytes.fromhex((HERE/'examples'/(name+'.pb.hex')).read_text());assert encode(typ,obj)==b;assert decode(typ,b)==obj
 zero={'position':{'x':0,'y':0},'pressure':0}; absent={'position':{'x':0,'y':0}}
 assert encode('BrushConfirmedSample',zero)!=encode('BrushConfirmedSample',absent)
 negative=copy.deepcopy(zero);negative['position']['x']=-0.0
 assert encode('BrushConfirmedSample',negative)==encode('BrushConfirmedSample',zero)
 op=read('add-stroke.json');doc=read('document-snapshot.json')
 assert op['payload']['add_stroke']['object']==doc['objects'][0]
 assert op['payload_version']==doc['schema_version']==2
 assert len(read('stroke.json')['vector_output']['outline'])>=3
 print('PASS: golden encode/decode, presence, signed-zero, AddStroke/snapshot/replay identity. Production P32 not executed.')

if __name__=='__main__':
 if '--generate' in sys.argv:generate()
 check()
