"""Expand E01 raw/model variants with independently written bytes.

Authoring values are independent of the old codec's responses. This file does
not assert that an unsupported baseline has executed the future contracts.
"""
from copy import deepcopy
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from wire import Writer, section, record, identity, operation, introduce_empty, content_edit, structural, placement, transform, field
from materialize_sources import MAT, immutable, save_json, digest
from variants import VARIANTS


def make_variant(case, name, model, root, expected, raw=None, direction='wire-ingress'):
    writer = Writer()
    data = raw if raw is not None else writer.encode(root,model)
    dest = ROOT/'tests/fc08/corpus/variants'/f'FC08-{case}'/name
    immutable(dest/'input.pb',data)
    save_json(dest/'input.json',{'message_type':root,'direction':direction,'value':model})
    save_json(dest/'expected.json',expected)
    return {'case_id':f'FC08-{case}','variant_id':f'FC08-{case}/{name}',
            'input_path':(dest/'input.pb').relative_to(ROOT).as_posix(),'input_sha256':digest(data),
            'model_path':(dest/'input.json').relative_to(ROOT).as_posix(),
            'expected_path':(dest/'expected.json').relative_to(ROOT).as_posix(),
            'expected_sha256':digest((dest/'expected.json').read_bytes())}


def main():
    writer = Writer()
    rows = []
    seeds = json.loads((MAT/'sources/accepted-seed/verification/fixtures/section-content-wire.json').read_text())
    for name in VARIANTS['W02']:
        model = section()
        raw = None
        expected = {'root_accepted':True,'value_preserved':True}
        direction = 'wire-ingress'
        if name.startswith('SCW'):
            seed = next(c for c in seeds['cases'] if c['id']==name)
            raw = bytes.fromhex(seed['wire_hex'])
            if name == 'SCW06':
                direction = 'model-to-canonical-writer'
                model['boundary']['y'] = -0.0
                expected = {'canonical_wire_hex':seeds['cases'][0]['wire_hex']}
            else:
                expected = {'root_accepted':seed['expected']=='ACCEPT','logical_result':seed['expected']}
        elif name.startswith('missing-'):
            key = name.removeprefix('missing-')
            del (model['boundary'] if key in model['boundary'] else model)[key]
            expected = {'root_accepted':False,'logical_result':'required-presence'}
        elif name in ('invalid-utf8','whitespace','combining'):
            model['title'] = {'hex':'c080'} if name=='invalid-utf8' else (' \t\n ' if name=='whitespace' else 'e\u0301 中文')
            expected = {'root_accepted':name!='invalid-utf8','title_hex':
                        ('c080' if name=='invalid-utf8' else model['title'].encode().hex())}
        else:
            key,what = name.rsplit('-',1)
            target = model['boundary'] if key in model['boundary'] else model
            target[key] = {'nan':math.nan,'posinf':math.inf,'neginf':-math.inf,'zero':0.0,'negative':-1.0}[what]
            raw = writer.encode('SectionContent',model)
            # JSON never relies on nonstandard NaN literals for input identity.
            if not math.isfinite(target[key]): target[key] = {'f64_bits':struct.pack('>d',target[key]).hex()}
            expected = {'root_accepted':False,'logical_result':'finite/domain'}
        rows.append(make_variant('W02',name,model,'SectionContent',expected,raw,direction))
    original = introduce_empty()
    for name in VARIANTS['W03']:
        model = deepcopy(original)
        topology = model['payload']['edit_container_topology']
        raw = None
        accepted = name in ('root-absent','relocate-empty-with-move')
        if name.startswith('parent-'):
            topology['guard']['child_sets'][0]['parent_id'] = {'value':'' if name=='parent-empty' else '00'*16}
        elif name == 'effect-absent': del topology['introduce_wrappers']
        elif name.startswith('relocate-empty'):
            del topology['introduce_wrappers']
            topology['relocate_objects'] = {}
            if name == 'relocate-empty-with-move':
                shape = record(20,'Shape')
                topology['guard']['nodes'] = [structural(shape)]
                topology['guard']['child_sets'][0]['complete_members'] = [{'object_id':shape['id'],'order_key':{'value':'10'}}]
                topology['relocations'] = [{'object_id':shape['id'],'placement_after':placement(key='20'),'transform_after':transform()}]
        elif name == 'duplicate-effect':
            topology['release_wrappers'] = {'containers_before':[record(10)]}
        elif name == 'duplicate-singular':
            raw = writer.encode('Operation',model) + field(3,0,b'\x01')
        rows.append(make_variant('W03',name,model,'Operation',{'root_accepted':accepted,'zero_canonical_effect_on_reject':True},raw))
    # Illegal new semantic field bytes do not get repaired by the typed writer.
    for name in VARIANTS['W05']:
        model = deepcopy(original)
        topology = model['payload']['edit_container_topology']
        raw = None
        if name=='introduce-shape': topology['introduce_wrappers']['containers']=[record(10,'Shape')]
        elif name=='release-shape':
            del topology['introduce_wrappers']
            topology['release_wrappers']={'containers_before':[record(10,'Shape')]}
        elif name=='multiple-effects': topology['release_wrappers']={'containers_before':[record(10)]}
        else:
            payload = writer.encode('EditContainerTopologyOp',topology)+field(99,2,b'forbidden-content' if name=='leaf-content-field' else b'forbidden-resource')
            raw = b''.join([field(1,2,writer.encode('Id128',model['operation_id'])),
                field(2,2,writer.encode('Id128',model['document_id'])),field(3,0,b'\x01'),field(4,0,b'\x01'),
                field(5,2,field(18,2,payload))])
        rows.append(make_variant('W05',name,model,'Operation',{'root_accepted':False,'zero_canonical_effect_on_reject':True},raw))
    for name in VARIANTS['W06']:
        ids = {'fill':256,'opacity':3,'locked':2,'visible':1,'stroke':257,'blend':4}
        if name=='erase-masks':
            # Nonempty Section masks are rejected for applicability regardless of
            # a valid inherited mask's geometry; source-bound fixture is required.
            continue
        which = name.split('-')[0]
        patch = {'object_id':identity(10),'field_id':ids[which],'action':2 if name.endswith('clear') else 1}
        if patch['action']==1:
            patch['value'] = {'fill_style':{'solid':{'color':{'r':1.0,'g':0.0,'b':0.0,'a':1.0}}}} if which=='fill' else \
                {'f32_value':0.25} if which=='opacity' else {'bool_value':True} if which in ('locked','visible') else \
                {'stroke_style':{'none':{}}} if which=='stroke' else {'blend_mode':1}
        rows.append(make_variant('W06',name,operation({'patch_properties':{'patches':[patch]}}),'Operation',
            {'root_accepted':which in ('fill','opacity'),'descendant_property_bytes_unchanged':True}))
    save_json(MAT/'wire-variant-index.partial.json',{'draft_generated_case_arrays':['FC08-W02','FC08-W03','FC08-W05'],
        'partial_cases':['FC08-W06'],'variants':rows,'status':'PARTIAL_M02_NOT_READY'})
    print(f'{len(rows)} concrete raw/model variants written; remaining variants not claimed complete')


if __name__ == '__main__': main()
