"""Verification-only protobuf writer. No generated DTOs or production codecs."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[3]
MAT = ROOT / '.aegis/materialized/FC-08'


def varint(n):
    if n < 0: raise ValueError('unsigned field')
    out = bytearray()
    while n > 127:
        out.append((n & 127) | 128)
        n >>= 7
    return bytes(out)+bytes([n])


def field(n,wt,value):
    return varint(n*8+wt) + (varint(len(value)) if wt == 2 else b'') + value


class Writer:
    def __init__(self):
        table = json.loads((MAT/'proposed-descriptor/field-table.json').read_text())
        self.messages = {m['name']:m['fields'] for f in table.values() for m in f['messages']}

    def encode(self,name,value):
        out = bytearray()
        known = {f['name'] for f in self.messages[name]}
        if value.keys() - known: raise ValueError(f'unknown authoring field: {name}: {value.keys()-known}')
        for f in self.messages[name]:
            if f['name'] not in value: continue
            values = value[f['name']] if f['label'] == 3 else [value[f['name']]]
            for item in values:
                t,wt = f['type'],0
                if t == 11:
                    data,wt = self.encode(f['type_name'].split('.')[-1],item),2
                elif t in (9,12):
                    if isinstance(item,dict): data = bytes.fromhex(item['hex'])
                    else: data = item.encode('utf-8') if t == 9 else bytes.fromhex(item)
                    wt = 2
                elif t in (1,2):
                    data,wt = struct.pack('<d' if t==1 else '<f', 0.0 if item == 0 else item),(1 if t==1 else 5)
                elif t in (3,4,5,8,13,14): data = varint(int(item))
                else: raise ValueError(f'unused unsupported authoring primitive {t}')
                out += field(f['number'],wt,data)
        return bytes(out)


def identity(number):
    return {'value':f'{number:032x}'}


def transform(value=None):
    return dict(zip(('a','b','c','d','tx','ty'),value or [1,0,0,1,0,0]))


def section(boundary=None,title=''):
    return {'boundary':dict(zip(('x','y','width','height'),boundary or [-20,0,120,80])),
            'title':title,'padding':8.0,'header_height':20.0}


def placement(parent=None,key='10'):
    return {**({'parent_id':identity(parent)} if parent is not None else {}),'order_key':{'value':key}}


def record(number,kind='Section',parent=None,key='10',local=None,content=None):
    kinds = {'Shape':1,'RichText':4,'Sticky':8,'Group':9,'Section':10}
    bodies = {'Section':{'section':section()},'Group':{'group':{}},
              'Shape':{'shape':{'shape_kind':1,'width':20.0,'height':20.0}},
              'Sticky':{'sticky':{'width':40.0,'height':40.0}}}
    return {'id':identity(number),'kind_id':kinds[kind],'kind_version':1,
            'placement':placement(parent,key),'transform':transform(local),
            'properties':{},'content':content if content is not None else bodies[kind]}


def structural(obj):
    return {'object_id':obj['id'],'kind_id':obj['kind_id'],'kind_version':obj['kind_version'],
            'placement_before':obj['placement'],'transform_before':obj['transform']}


def operation(payload,number=1001):
    return {'operation_id':identity(number),'document_id':identity(100),'schema_version':1,
            'payload_version':1,'payload':payload}


def introduce_empty(number=10):
    return operation({'edit_container_topology':{'guard':{'child_sets':[{'complete_members':[]}]},
        'introduce_wrappers':{'containers':[record(number)]}}})


def content_edit():
    obj = record(10)
    return operation({'set_section_content':{'items':[{'object_id':identity(10),'expected_kind_version':1,
        'expected_content':section(),'replacement_content':section(title='标题 Axiom')}],
        'geometry_basis':[]}})


def snapshot(writer,objects,version=3,caps=None):
    value = {'document_id':identity(100),'schema_version':version,'objects':objects}
    if version < 3: return value
    caps = caps if caps is not None else ['resource-manifest-v1','section-v1']
    capbytes = writer.encode('CapabilityRequirementsV1',{'required_ids':caps})
    value.update({'manifest':{'schema_version':1,'manifest_revision':0},'document_revision':0,
        'recovery_frontier':'080110011800','digest_algorithm':1,'capability_requirements':capbytes.hex()})
    def num(n,w): return n.to_bytes(w,'little')
    domain = b'axiom.semantic.projection.v1'
    projection = num(len(domain),4)+domain+bytes.fromhex(identity(100)['value'])+num(3,4)+num(len(capbytes),4)+capbytes
    projection += num(1,1)+num(len(objects),8)
    for obj in objects:
        data = writer.encode('ObjectRecord',obj)
        projection += num(len(data),4)+data
    projection += num(1,4)+num(0,8)+num(0,8)+num(0,8)
    value['document_digest'] = hashlib.sha256(projection).hexdigest()
    data = writer.encode('DocumentSnapshot',value)
    domain = b'axiom.checkpoint.v1'
    value['checkpoint_digest'] = hashlib.sha256(num(len(domain),4)+domain+num(len(data),8)+data).hexdigest()
    return value
