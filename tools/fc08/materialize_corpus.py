"""M02 concrete corpus compiler. No product imports and no production writes.

The fixture protocol is a verification protocol, not a proposed public ABI.
Baseline adapters must report missing product capabilities, never implement
these actions on behalf of the Runtime.
"""
from copy import deepcopy as cp
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from variants import VARIANTS
from wire import Writer,record,identity,operation,section,structural,placement,transform,field,snapshot
from fixtures import (SEED,IDS,I,seed,key,object_,objects,children,topology,transition,wrap,cross_parent,nested,metadata,
                      numeric_sample,budget_recipe,richtext)
from oracles import compose,inverse,point,bits,quality,worlds,release_frontier,c01,c02,rect_contained

DEST=ROOT/'.aegis/materialized/FC-08/attempts/v0.2-obs01-corpus-bound'
W=Writer()
def digest(b):return hashlib.sha256(b).hexdigest()
def safe(v):
    if isinstance(v,float) and not math.isfinite(v):return {'f64_bits':struct.pack('>d',v).hex()}
    if isinstance(v,dict):return {k:safe(x) for k,x in v.items()}
    if isinstance(v,list):return list(map(safe,v))
    return v
def jsonbytes(v):return (json.dumps(safe(v),ensure_ascii=False,indent=2,allow_nan=False)+'\n').encode('utf-8')
def put(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists() and path.read_bytes()!=data:raise ValueError('immutable attempt artifact differs: '+str(path))
    path.write_bytes(data)
    return {'path':path.relative_to(ROOT).as_posix(),'sha256':digest(data),'size':len(data)}
def eq(path,value):return {'op':'equal','path':path,'expected':value}
def unchanged(a,b):return {'op':'unchanged','before':a,'after':b}
def desired(records,**extra):return [eq('canonical.records',records),*[eq(k,v) for k,v in extra.items()]]
def rejected():return [eq('outcome','Rejected'),unchanged('before','after')]
def materialize(case,name,scenario,expected,blobs=None,recipe=None):
    dest=DEST/'corpus'/('FC08-'+case)/name
    ref=put(dest/'input.json',jsonbytes(scenario))
    exp=put(dest/'expected.json',jsonbytes({'predicates':expected,'source_case':'FC08-'+case,'product_expected_only':True}))
    wire=[]
    if blobs is None:
        blobs=[]
        for label in ['forward','inverse','redo']:
            if label in scenario:blobs.append((label,'Operation',W.encode('Operation',scenario[label])))
        if 'wire_model' in scenario:blobs.append(('input',scenario['wire_root'],W.encode(scenario['wire_root'],scenario['wire_model'])))
    for label,root,b in blobs:
        r={'label':label,'root':root,'sha256':digest(b),'size':len(b)}
        if len(b)<=65536:r.update(put(dest/(label+'.pb'),b))
        elif recipe:r['recipe']=recipe
        else:raise ValueError('large fixture must have explicit reconstructible recipe')
        wire.append(r)
    return {'case_id':'FC08-'+case,'variant_id':f'FC08-{case}/{name}','input':ref,'expected':exp,
        'wire':wire,'future_seam':seam(case),'product_status':'REQUIRES_PRODUCT_EXECUTION',
        'author':'independent field table/literal tables/ordered rational binary64; no DUT output'}
def seam(case):
    family=case[0]
    return {'W':'normal semantic ingress and canonical writer','T':'container topology prepare/apply',
        'H':'typed undoable container command and EditorHistory','G':'PoseConstruction/1 and container geometry',
        'S':'Runtime scoped lasso/hit/identity selection','V':'Scene derived projection and resource binding',
        'C':'preview/Scene coherent publication','L':'Runtime owner ingress and request lifecycle',
        'K':'snapshot capability bootstrap and journal replay','P':'ordinary RuntimeFacade container consumer'}[family]

def w_variant(case,name):
    context=[];model=None;raw=None;root='Operation';accepted=True;direction='normal-wire-ingress'
    extra=[]
    if case=='W01':
        return {'source_index':'.aegis/materialized/FC-08/source-index.json',
            'base_descriptor':'.aegis/materialized/FC-08/baseline-descriptor/field-table.json',
            'proposed_descriptor':'.aegis/materialized/FC-08/proposed-descriptor/field-table.json',
            'operation_tags':list(range(1,19)),'Section':{'kind':10,'version':1,'content_tag':12},
            'preserve':{'brush_tags':[10,11],'ShapeContent.label':4},'comparison':name},[eq('source_preservation',True),eq('declared_delta_only',True)],[]
    if case=='W02':
        root='SectionContent';model=section()
        if name.startswith('SCW'):
            s=next(v for v in seed('section-content-wire')['cases'] if v['id']==name)
            raw=bytes.fromhex(s['wire_hex']);accepted=s['expected']=='ACCEPT'
            if name=='SCW06':
                direction='model-to-canonical-writer';model['boundary']['y']=-0.;accepted=True
                extra=[eq('canonical_hex',seed('section-content-wire')['cases'][0]['wire_hex'])]
        elif name.startswith('missing-'):
            f=name[8:];del (model['boundary'] if f in model['boundary'] else model)[f];accepted=False
        elif name in ['invalid-utf8','whitespace','combining']:
            model['title']={'hex':'c080'} if name=='invalid-utf8' else ' \t\n ' if name=='whitespace' else 'e\u0301 中文'
            accepted=name!='invalid-utf8'
            if accepted:extra=[eq('decoded.title_utf8',model['title'].encode().hex())]
        else:
            f,v=name.rsplit('-',1);(model['boundary'] if f in model['boundary'] else model)[f]={
                'nan':math.nan,'posinf':math.inf,'neginf':-math.inf,'zero':0.,'negative':-1.}[v];accepted=False
    elif case=='W03':
        base=record(10);context=[]
        model=topology([], [base]);t=model['payload']['edit_container_topology']
        if name.startswith('parent-'):
            t['guard']['child_sets'][0]['parent_id']={'value':'' if name=='parent-empty' else '00'*16};accepted=False
        elif name=='effect-absent':del t['introduce_wrappers'];accepted=False
        elif name.startswith('relocate-empty'):
            context=[record(20,'Shape')];after=cp(context);after[0]['transform']=transform([1,0,0,1,5,0])
            model=topology(context,after);t=model['payload']['edit_container_topology']
            if name.endswith('without-move'):t['relocations']=[];accepted=False
        elif name=='duplicate-effect':
            # Same oneof field occurs twice: duplicate is the sole wire violation.
            tbytes=W.encode('EditContainerTopologyOp',t)+field(4,2,W.encode('IntroduceWrappersEffect',t['introduce_wrappers']))
            raw=op_with_topology(model,tbytes);accepted=False
        elif name=='duplicate-singular':raw=W.encode('Operation',model)+field(3,0,b'\x01');accepted=False
    elif case=='W05':
        r=record(10,'Shape');model=topology([],[r]);t=model['payload']['edit_container_topology'];accepted=False
        if name=='release-shape':context=[r];model=topology([r],[])
        elif name=='multiple-effects':
            # Two different selected effect tags; every nested effect is legal
            # independently, but the oneof multiplicity is forbidden.
            context=[record(20)];model=topology(context,context+[record(10,key='20')])
            t=model['payload']['edit_container_topology'];t['release_wrappers']={'containers_before':[context[0]]}
        elif name in ['leaf-content-field','resource-field']:
            context=[record(20,'Shape')];model=topology(context,context+[record(10)])
            t=model['payload']['edit_container_topology']
            raw=op_with_topology(model,W.encode('EditContainerTopologyOp',t)+field(99,2,b'leaf' if name.startswith('leaf') else b'resource'))
    elif case=='W06':
        context=[record(10),record(20,'Shape',10)]
        if name=='erase-masks':
            knot=lambda x:{'position':{'x':x,'y':0.},'radius':1.}
            mask={'mask_id':identity(800),'geometry':{'swept_circle':{'segments':[{'p0':knot(0.),'p1':knot(2.),
                'control1':{'x':.5,'y':0.},'control2':{'x':1.5,'y':0.}}]}}}
            model=operation({'add_erase_masks':{'items':[{'object_id':identity(10),'masks':[mask]}]}});accepted=False
        else:
            which=name.split('-')[0];fid={'fill':256,'opacity':3,'locked':2,'visible':1,'stroke':257,'blend':4}[which]
            patch={'object_id':identity(10),'field_id':fid,'action':2 if name.endswith('clear') else 1}
            if patch['action']==1:patch['value']={'fill':{'fill_style':{'solid':{'color':{'r':1.,'g':0.,'b':0.,'a':1.}}}},
                'opacity':{'f32_value':.25},'locked':{'bool_value':True},'visible':{'bool_value':True},
                'stroke':{'stroke_style':{'none':{}}},'blend':{'blend_mode':1}}[which]
            model=operation({'patch_properties':{'patches':[patch]}});accepted=which in ['fill','opacity']
        extra=[unchanged('before_child_properties','after_child_properties')]
    elif case=='W04':return collection_variant(name)
    elif case=='W07':
        return {'immutable_legacy_binding':name,'baseline_root':{'legacy-codec':'Golden','legacy-snapshot':'LegacyImport','legacy-operation-replay':'Replay'}[name],
            'source_revision':'a5af5f6d89e8439cc5f70950ba5f1b7986b6ff7b','reuse_exact_regression_artifacts':True},[eq('immutable_bytes_preserved',True),eq('legacy_state_matched',True)],[]
    else:raise ValueError(case)
    raw=raw if raw is not None else W.encode(root,model)
    return {'before':context,'wire_root':root,'wire_model':safe(model),'direction':direction},[eq('accepted',accepted),*extra,
        *([unchanged('before','after')] if not accepted else [])],[('input',root,raw)]

def op_with_topology(model,raw):
    return b''.join([field(1,2,W.encode('Id128',model['operation_id'])),field(2,2,W.encode('Id128',model['document_id'])),
        field(3,0,b'\x01'),field(4,0,b'\x01'),field(5,2,field(18,2,raw))])

def collection_variant(name):
    before=[record(1,'Section',key='30'),record(2,'Section',key='10'),record(9,'Shape',1,'20'),record(10,'Shape',2,'10')]
    after=cp(before)+[record(6,'Section',key='20')]
    for r in after:
        if r['id'] in [identity(9),identity(10)]:r['placement']=placement(6,'30' if r['id']==identity(9) else '10')
    model=topology(before,sorted(after,key=lambda r:r['id']['value']))
    t=model['payload']['edit_container_topology'];collection=name.rsplit('-',1)[1]
    if 'containers_before' in name:model=topology(after,before);t=model['payload']['edit_container_topology'];before=after
    if 'order_only_edits' in name:
        # Two unaffected roots; disjoint from relocation IDs9/10.
        t['order_only_edits']=[{'object_id':identity(1),'order_key_after':{'value':'40'}},
            {'object_id':identity(2),'order_key_after':{'value':'50'}}]
    if 'items' in name or 'geometry_basis' in name:
        model=operation({'set_section_content':{'items':[{'object_id':r['id'],'expected_kind_version':1,
            'expected_content':r['content']['section'],'replacement_content':section(title='new')} for r in before if r['kind_id']==10],
            'geometry_basis':[structural(r) for r in before]}});t=model['payload']['set_section_content']
    candidates={**t,**t.get('guard',{}),**t.get('introduce_wrappers',{}),**t.get('release_wrappers',{})}
    if 'guard' in t:candidates['complete_members']=t['guard']['child_sets'][0]['complete_members']
    accepted=not name.startswith('duplicate-') and name not in ['wire-duplicate','wire-noncanonical']
    if name.startswith(('model-permutation-','duplicate-')):
        coll=candidates[collection]
        if len(coll)<2:
            # Both wrappers are independent, with distinct root order keys.
            if collection=='containers':coll.append(record(7,'Group',key='60'))
            elif collection=='containers_before':
                # Add a second existing empty wrapper to the release preimage.
                empty=record(7,'Group',key='60');before.append(empty);coll.append(empty)
                t['guard']['nodes']=[structural(r) for r in sorted(before,key=lambda r:r['id']['value'])]
                t['guard']['child_sets']=children(before)
            else:raise ValueError('collection not exercised '+collection)
        if name.startswith('duplicate-'):coll.append(cp(coll[0]))
        else:coll.reverse()
    if name in ['wire-noncanonical','wire-duplicate']:
        coll=t['guard']['nodes'];coll.reverse()
        if name=='wire-duplicate':coll.append(cp(coll[0]))
    if name=='root-first':
        t['guard']['child_sets'].reverse()
    if name=='members-by-id':
        # IDs9<10 but orderkeys30>10, deliberately disagreeing comparators.
        before=[record(9,'Shape',key='30'),record(10,'Shape',key='10')]
        model=topology(before,before+[record(6,'Section',key='20')])
    direction='model-canonicalization' if name.startswith('model-permutation-') or name in ['root-first','members-by-id'] else 'wire-ingress'
    canonical=canonical_collections(cp(model)) if accepted else None
    raw=W.encode('Operation',model)
    expected=[eq('accepted',accepted)]
    if direction=='model-canonicalization':expected.append(eq('canonical_hex',W.encode('Operation',canonical).hex()))
    if not accepted:expected.append(unchanged('before','after'))
    return {'before':before,'wire_root':'Operation','wire_model':model,'direction':direction},expected,[('input','Operation',raw)]

def canonical_collections(value):
    if isinstance(value,list):return [canonical_collections(v) for v in value]
    if not isinstance(value,dict):return value
    result={k:canonical_collections(v) for k,v in value.items()}
    for k,v in result.items():
        if isinstance(v,list) and v and k in ['nodes','complete_members','relocations','order_only_edits','containers','containers_before','items','geometry_basis']:
            v.sort(key=lambda x:x.get('object_id',x.get('id',{}))['value'])
        if k=='child_sets':v.sort(key=lambda x:x.get('parent_id',{}).get('value',''))
    return result

def topology_variant(case,name):
    fixture=wrap();pred=[]
    if case=='T01':
        kind,size,action=name.split('-');fixture=wrap(kind,size=='nonempty')
        if action=='release':fixture=transition(fixture['after'],fixture['before'])
    elif case=='T02':fixture=cross_parent()
    elif case=='T03':
        fixture=nested()
        if name.startswith('inverse'):fixture=transition(fixture['after'],fixture['before'])
    elif case=='T04':
        before=[object_('a','Shape')];after=[object_('A'),object_('B','Group','A'),object_('C','Section','B'),object_('a','Shape','C')]
        if name=='self-parent':after[0]['placement']=placement(1,'10')
        if name=='cycle':after[0]['placement']=placement(3,'10')
        if name=='missing-parent':after[0]['placement']=placement(999,'10')
        if name=='leaf-parent':after[1]['placement']=placement(9,'10');after[-1]=cp(before[0])
        if name.startswith('sticky-'):
            after=[object_('S','Sticky'),richtext(9,[1],6)]
            if name=='sticky-two-children':after.append(richtext(10,[1],6))
            else:after[1]=object_('a','Shape','S')
            # Existing Sticky and leaves; relocate a legal text/shape under it.
            before=cp(after)
            for r in before:
                if r['id']!=identity(6):r['placement']=placement(key=key(int(r['id']['value'],16)))
        fixture=transition(before,after)
        if name!='alternating-chain':pred=rejected()
    elif case=='T05':
        fixture=cross_parent()
        if name=='mutate-wrapper-title':fixture=transition(fixture['after'],fixture['before'])
        fixture['schedule']=[{'at':'prepared-before-apply','owner':'separate-legitimate-command','mutation':name}]
        f=fixture['forward']['payload']['edit_container_topology']
        if name=='omit-ancestor':f['guard']['nodes']=[n for n in f['guard']['nodes'] if n['object_id']!=identity(7)]
        elif name=='omit-child-set':f['guard']['child_sets']=[s for s in f['guard']['child_sets'] if s.get('parent_id')!=identity(7)]
        elif name=='mutate-wrapper-title':
            s=next(r['content']['section'] for r in fixture['before'] if r['id']==identity(6))
            new=cp(s);new['title']='changed while prepared'
            fixture['intervening_operation']=operation({'set_section_content':{'items':[{'object_id':identity(6),'expected_kind_version':1,
                'expected_content':s,'replacement_content':new}],'geometry_basis':[]}},2001)
        elif name=='mutate-members':fixture['intervening_operation']=operation({'insert_objects':{'objects':[object_('X','Shape','P')]}},2001)
        elif name=='mutate-order':fixture['intervening_operation']=operation({'set_placements':{'items':[{'object_id':identity(9),'placement':placement(7,'20')}]}},2001)
        elif name=='unrelated-leaf-content':fixture['intervening_operation']=operation({'set_object_size':{'items':[{'object_id':identity(9),'width':30.,'height':40.}]}},2001)
        else:fixture['intervening_operation']=operation({'set_transforms':{'items':[{'object_id':identity(7),'transform':transform([1,0,0,1,99,0])}]}},2001)
        pred=rejected() if name!='unrelated-leaf-content' else [eq('outcome','Applied'),eq('unrelated_leaf_content_preserved',True)]
    elif case=='T06':
        fixture=cross_parent();t=fixture['forward']['payload']['edit_container_topology']
        t['order_only_edits']=[{'object_id':identity(7),'order_key_after':{'value':'40'}},{'object_id':identity(8),'order_key_after':{'value':'50'}}]
        for r in fixture['after']:
            if r['id'] in [identity(7),identity(8)]:r['placement']['order_key']={'value':'40' if r['id']==identity(7) else '50'}
        fixture['inverse']=topology(fixture['after'],fixture['before'],1002)
        if name=='payload-permutation':t['relocations'].reverse();t['order_only_edits'].reverse()
        elif name=='sibling-key-collision':t['order_only_edits'][1]['order_key_after']={'value':'40'};pred=rejected()
        elif name=='overlapping-write-id':t['order_only_edits'][0]['object_id']=identity(9);pred=rejected()
    elif case=='T07':
        fixture['failure_checkpoint']=name
        fixture['baseline_binding']={'after-staging':'Q-SEM-STAGED','after-reservation':'Q-SEM-RESERVED',
            'before-canonical-publication':'Q-SEM-PUBLISH','before-validation':'normal-invalid-input'}[name]
        pred=rejected()
    else:raise ValueError(case)
    if not pred:pred=desired(fixture['after'],**{'outcome':'Applied','canonical_commit_delta':1})
    return fixture,pred,None

def history_variant(case,name):
    if case in ['H03','H04','H07']:
        recipe_name=name
        if case=='H07':recipe_name=name
        blobs,recipe=budget_recipe(recipe_name)
        return {'recipe_name':recipe_name,'recipe':recipe,'reconstruct':'tests/fc08/reference/fixtures.py:budget_recipe',
            'font_manifest_context':font_manifest() if name.startswith(('record-','restore-')) else None},[
            eq('independent_size_predicate',recipe.get('size_predicate',recipe['command_admitted'] if 'command_admitted' in recipe else None)),
            *([eq('command_admitted',recipe['command_admitted'])] if 'command_admitted' in recipe else [])],blobs,recipe_name
    fixture=wrap()
    if case=='H01':
        if name.startswith(('empty-','nonempty-')):size,kind=name.split('-');fixture=wrap(kind,size=='nonempty')
        elif name=='cross-parent-wrap':fixture=cross_parent()
        elif name=='nested-release':fixture=nested()
        elif name in ['resize','rename','padding','header']:fixture=metadata({'resize':'boundary','rename':'title','padding':'padding','header':'header_height'}[name])
        elif name in ['fill','opacity']:
            scenario,_,blobs=w_variant('W06',name+'-set');fixture={'before':scenario['before'],'forward':scenario['wire_model']}
            fixture['inverse']=operation({'patch_properties':{'patches':[{'object_id':identity(10),'field_id':256 if name=='fill' else 3,'action':2}]}},1002)
            fixture['redo']=cp(fixture['forward']);fixture['redo']['operation_id']=identity(1003)
        if name in ['NoChange','AlreadyApplied','Rejected']:
            fixture['history_before']={'size':0,'cursor':0}
            fixture['schedule']=[{'action':name,'operation_id':identity(1001)}]
            if name=='AlreadyApplied':fixture['setup']=[{'action':'semantic-replay-without-History','operation':fixture['forward']}]
            if name=='NoChange':
                fixture=metadata();fixture['forward']['payload']['set_section_content']['items'][0]['replacement_content']=cp(fixture['forward']['payload']['set_section_content']['items'][0]['expected_content'])
                fixture['typed_intent']={'action':'rename','object_id':identity(6),'confirmed_title':''}
                fixture['after']=cp(fixture['before']);fixture.pop('inverse');fixture.pop('redo')
            if name=='Rejected':fixture['forward']['payload']['edit_container_topology']['guard']['nodes']=[structural(record(999,'Shape'))]
            return fixture,[eq('history.size',0),eq('history.cursor',0),eq('canonical_commit_delta',0)],None,None
        return fixture,[eq('history_timeline',[{'size':1,'cursor':1},{'size':1,'cursor':0},{'size':1,'cursor':1}]),
            eq('canonical_commit_delta',3),eq('emitted_operation_ids',[identity(1001),identity(1002),identity(1003)]),
            *([eq('undo_records',fixture['before'])] if 'before' in fixture else [])],None,None
    if case=='H02':
        fixture=cross_parent();fixture['schedule']=[{'action':'forward'},{'action':'intervening','kind':name},{'action':'undo'}]
        if name=='new-member':fixture['intervening_operation']=operation({'insert_objects':{'objects':[object_('X','Shape','S')]}},2001)
        elif name=='deleted-original-parent':fixture['intervening_operation']=operation({'delete_objects':{'object_ids':[identity(7)]}},2001)
        elif name=='changed-wrapper-metadata':
            old=next(r['content']['section'] for r in fixture['after'] if r['id']==identity(6));new=cp(old);new['title']='changed after F'
            fixture['intervening_operation']=operation({'set_section_content':{'items':[{'object_id':identity(6),'expected_kind_version':1,
                'expected_content':old,'replacement_content':new}],'geometry_basis':[]}},2001)
        else:fixture['intervening_operation']=operation({'set_object_size':{'items':[{'object_id':identity(9),'width':30.,'height':40.}]}},2001)
        pred=[eq('outcome','Applied'),eq('retained_leaf_size',[30.,40.])] if name=='retained-leaf-content' else [eq('outcome','HistoryConflict'),eq('history.cursor',1),eq('history.size',1),unchanged('before_undo','after_undo')]
    elif case=='H05':
        fixture['history_before']={'size':2,'cursor':1,'redo_ids':[identity(1000)]}
        fixture['failure_checkpoint']={'history-allocation-failure':'Q-HIST-BEFORE-RESERVE','outcome-reservation-failure':'future-E01-outcome-reservation',
            'cancel-precommit':'owner-cancel-before-apply','failure-with-redo-tail':'Q-HIST-READY'}[name]
        pred=[unchanged('before','after'),eq('history.size',2),eq('history.cursor',1),eq('reservation_released_once',True)]
    elif case=='H06':
        fixture['schedule']=[{'action':'prepare-admitted-F-U'},{'action':'pause-at-safe-boundary'},
            {'action':'legitimate-owner-mutation','target':identity(9) if name.startswith('guarded') else identity(12),
             'new_transform':transform([1,0,0,1,17,3])},{'action':'resume-original-ticket'}]
        pred=[eq('outcome','Rejected'),unchanged('before_resume','after_resume')] if name.startswith('guarded') else [eq('outcome','Applied'),eq('revalidated_basis_and_U',True),eq('canonical_commit_delta',1)]
    elif case=='H08':
        fixture['schedule']=[{'action':'forward'},{'action':name,'operation_id':identity(1001 if name!='undo-new-operation-id' else 1002)}]
        if name=='same-id-changed-payload':fixture['retry']=metadata()['forward'];fixture['retry']['operation_id']=identity(1001)
        pred=[eq('second_commit_delta',0),eq('second_history_delta',0)]
        if name=='same-id-same-payload-stale-guard':pred.append(eq('outcome','AlreadyApplied'))
        elif name=='same-id-changed-payload':pred.append(eq('outcome','Rejected'))
        elif name=='NoChange':pred.append(eq('outcome','NoChange'))
        else:pred=[eq('history.size',1),eq('history.cursor',0),eq('undo_records',fixture['before']),eq('operation_ids_distinct',True)]
    else:raise ValueError(case)
    return fixture,pred,None,None

def font_manifest():
    return {'schema_version':1,'manifest_revision':0,'entries':[{'resource_id':identity(512),'resource_kind':2,'resource_revision':1,
        'content_hash':'sha256:2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b','byte_length':16437364,
        'decode_version':1,'layout_version':1,'metadata':W.encode('FontResourceMetadataV1',{'schema_version':1,'collection_index':0,'layout_profile':1}).hex()}]}

def geometry_variant(case,name):
    if case=='G01':
        f=seed('affine-reparent');before=cp(f['before']);after=cp(before)
        for k,v in f['after'].items():after[k].update(v)
        b=objects({k:{**v,'kind':'Group' if k=='G' else 'Section' if k=='S' else 'Shape'} for k,v in before.items()})
        a=objects({k:{**v,'kind':'Group' if k=='G' else 'Section' if k=='S' else 'Shape'} for k,v in after.items()})
        return transition(b,a),[eq('S_local_bits',[bits(x) for x in [0,2,-2,0,90,70]]),eq('C_world_origin',[82.,76.]),unchanged('C_local_before','C_local_after')]
    if case=='G02':
        rect={'left':[-20,0,120,80],'top':[0,-10,100,90],'right':[0,0,120,80],'bottom':[0,0,100,90]}[name]
        f=metadata('boundary');f['before'][0]['content']['section']=section([0,0,100,80]);f['after'][0]['content']['section']=section(rect)
        b=f['before'][0];a=f['after'][0]
        f['forward']=operation({'set_section_content':{'items':[{'object_id':identity(6),'expected_kind_version':1,
            'expected_content':b['content']['section'],'replacement_content':a['content']['section']}],
            'geometry_basis':[structural(b)]}})
        f['inverse']=operation({'set_section_content':{'items':[{'object_id':identity(6),'expected_kind_version':1,
            'expected_content':a['content']['section'],'replacement_content':b['content']['section']}],
            'geometry_basis':[structural(a)]}},1002)
        f['redo']=cp(f['forward']);f['redo']['operation_id']=identity(1003)
        f['before'].append(object_('a','Shape','S'));f['after'].append(object_('a','Shape','S'))
        return f,[eq('canonical.records',f['after']),unchanged('descendants_before','descendants_after')]
    if case in ['G03','G04']:
        if name=='literal-reflection-shear':
            f=seed('affine-reparent')['reflection_shear']
            return f,[eq('world_bits',[bits(x) for x in f['expected_world']]),eq('world_point',f['expected_point'])]
        graph={'G':{'kind':'Group','parent':None,'local':[-1,.5,.25,2,100,50]},
            'S':{'kind':'Section','parent':'G','local':[0,1,-1,0,10,5]},
            'a':{'kind':'Shape','parent':'S','local':[1,0,0,1,3,4],'local_rect':[0,0,20,10]},
            'b':{'kind':'Shape','parent':'S','local':[1,0,0,1,5000,5000],'local_rect':[0,0,20,10]}}
        before=objects(graph);delta={'move':[1,0,0,1,10,15],'rotate':[0,1,-1,0,0,0],'scale':[2,0,0,3,0,0]}.get(name,[1,0,0,1,10,15])
        after=cp(graph);after['G']['local']=compose(delta,graph['G']['local'])
        f={'before':before,'after':objects(after),'intent':{'selected':[identity(5),identity(6)] if name=='duplicate-ancestor-hit' else [identity(5)],
            'world_delta':delta,'normalization':'selection roots'},'bounds':{'a':[0,0,20,10],'b':[0,0,20,10]},
            'quality_members':[identity(9),identity(10)],'hidden_members':[identity(10)] if name=='hidden-outside-visible-bounds' else [],
            'bounds_available':name!='bounds-unavailable'}
        literal={k:[bits(v) for v in t] for k,t in worlds(after).items()}
        return f,([eq('outcome','UnsupportedPose'),unchanged('before','after')] if name=='bounds-unavailable' else [eq('world_bits',literal),eq('written_ids',[identity(5)]),unchanged('descendant_local_before','descendant_local_after')])
    if case=='G05':
        if name in ['quality-below','quality-at','quality-above']:
            if name=='quality-at':
                # Exact bound predicate control, not a claim that arbitrary
                # perturbed numbers came from PoseConstruction/1.
                return {'role':'independent-quality-predicate-control','before':0.,'after':2.**-32+2.**-47},[{'op':'quality','before':'before','after':'after'}]
            f=numeric_sample(12 if name=='quality-below' else 24)
            return f,[eq('local_bits',f['local_bits']),eq('quality_ok',name=='quality-below'),eq('outcome','Applied' if name=='quality-below' else 'UnsupportedPose')]
        return {'old_world':[1,0,0,1,0,0],'new_parent':[1,1,1,1,0,0] if name=='singular-parent' else [1e308,0,0,1e308,0,0],
            'local_bounds':[0,0,20,10],'construction':'PoseConstruction/1'},[eq('outcome','UnsupportedPose' if name=='singular-parent' else 'NumericRange'),unchanged('before','after')]
    if case=='G06':
        samples=json.loads((ROOT/'tests/fc08/corpus/numeric/non-dyadic.json').read_text())
        f=cp(samples[1 if name=='non-dyadic-2' else 0]);f['iterations']=16 if name=='repeated-F-U-16' else 1
        return f,[eq('local_bits',f['local_after_bits']),eq('undo_bits',f['undo_expected_bits']),eq('repeat_drift',False)]
    raise ValueError(case)

def selection_variant(case,name):
    graph={'S':{'kind':'Section','parent':None,'boundary':[0,0,100,100]},
        'a':{'kind':'Shape','parent':'S','local':[1,0,0,1,10,10],'local_rect':[0,0,2,2]},
        'b':{'kind':'Shape','parent':'S','local':[1,0,0,1,40,40],'local_rect':[0,0,10,10]}}
    region=[[-1,-1],[101,-1],[101,101],[-1,101]];expected=[identity(6)]
    f={'graph':graph,'kind_geometry':{'a':'rectangle','b':'ellipse'},'before_selection':[],
        'scope':None,'input':{'source':'mouse','pointer':1,'generation':7,'shift_at_down':False}}
    if case=='S01':
        if name.endswith('overflow'):graph['b']['local']=[1,0,0,1,150,150]
    elif case=='S02':
        region=[[9,9],[41,9],[41,41],[9,41]];expected=[identity(9)]
        f['broad_phase_candidates']=[identity(9),identity(10)]
        # The ellipse AABB corner40,40 is in the region, its true curve is not.
    elif case=='S03':
        graph['G']={'kind':'Group','parent':None,'local':I};graph['S']['parent']='G'
        if name=='outer-partial-inner-full':
            graph['G']['kind']='Section';graph['G']['boundary']=[-100,-100,300,300];expected=[identity(6)]
        elif name=='unopened-Group':expected=[identity(5)]
        else:f['scope']=identity(5)
    elif case=='S04':
        literal=seed('concave-region');region=literal['polygon_even_odd'];graph['S']['boundary']=literal['section_boundary']
        graph['b']['local']=[1,0,0,1,50,50];expected=[identity(9)]
    elif case=='S05':
        f['action']='click';f['click']={'child':[11,11],'blank-interior':[70,70],'header':[20,5],'border':[0,70],
            'empty-Group-paint':[325,325],'empty-Group-identity':None}[name]
        if name.startswith('empty-Group'):
            graph={'G':{'kind':'Group','parent':None,'local':I}}
            f['setup']=[{'action':'insert-child','id':identity(9),'rect':[320,320,10,10]},
                {'action':'delete-child','id':identity(9)}]
            expected=[] if name=='empty-Group-paint' else [identity(5)];f['identity_target']=identity(5)
        else:expected=[identity(9)] if name=='child' else [identity(6)]
    elif case=='S06':
        graph['S']['boundary']=[-100,-100,300,300]
        graph['b']['local']=[1,0,0,1,50,50];graph['b']['hidden']=True
        graph['C']={'kind':'Shape','parent':'S','local':[1,0,0,1,80,80],'local_rect':[0,0,5,5],'locked':True}
        graph['X']={'kind':'Shape','parent':None,'local':[1,0,0,1,200,200],'local_rect':[0,0,10,10]}
        f['before_selection']=[identity(12)];f['primary_before']=identity(12);f['input']['shift_at_down']=True
        f['schedule']=['Down+Shift','Move','ShiftReleased','Cancel' if name=='cancel' else 'Up']
        expected=[identity(12)] if name=='cancel' else [identity(12),identity(9)]
    else:raise ValueError(case)
    f['region']=region;f['records']=objects(graph)
    return f,[eq('selected',expected),eq('primary',expected[0] if expected else None),eq('canonical_commit_delta',0)]

def projection_variant(case,name):
    f={'graph':{'S':{'kind':'Section','parent':None,'boundary':[1000,1000,100,100]},
        'a':{'kind':'Shape','parent':'S','local':[1,0,0,1,10,10],'local_rect':[0,0,20,10]}},
        'viewport':[0,0,200,200],'generation':7,'target':target(),'faults':[]}
    if case=='V01':return f,[eq('child_draw_present',True),eq('query_ids',[identity(9)]),eq('Section_own_boundary_visible',False)]
    if case=='V02':
        f['graph']['S']['boundary']=[0,0,100,100];f['parent_opacity']=.25;f['child_rgba']=[0,0,1,1];f['pixel']=[15,15]
        f['change']={'own-opacity-quarter':None,'own-opacity-change':{'opacity':.75},'own-fill-change':{'fill':[1,0,0,1]}}[name]
        return f,[eq('child_effective_rgba',[0,0,1,1]),eq('interior_pixel_rgba',[0,0,1,1]),eq('subtree_clip',False)]
    if case=='V03':
        f['graph']['G']={'kind':'Group','parent':None,'local':I};f['graph']['S']['parent']='G'
        f['external_connector']={'id':identity(50),'start_target':identity(9),'start_port':2,'end_free':[200,15]}
        f['action']={'move-root':{'root':identity(5),'translation':[10,0]},'reparent-leaf':{'leaf':identity(9),'new_parent':None,'preserve_world':True},
            'resize-child':{'leaf':identity(9),'size':[40,10]}}[name]
        f['expected_child_rect']={'move-root':[20,10,20,10],'reparent-leaf':[10,10,20,10],'resize-child':[10,10,40,10]}[name]
        f['required_damage_rects']=[[10,10,20,10],f['expected_child_rect']]
        f['affected_ids']=[identity(n) for n in [5,6,9,50]]
        return f,[eq('child_world_rect',f['expected_child_rect']),eq('damage_covers',f['required_damage_rects']),eq('affected_ids',f['affected_ids']),eq('render_query_geometry_same_basis',True)]
    if case=='V04':
        f['forward']=wrap()['forward'];f['faults']=[{'phase':name,'once':True}]
        f['schedule']=[{'action':'commit-F'},{'action':'projection-prepare-fails','phase':name},
            {'action':'commit-newer-generation'},{'action':'remove-faults'},{'action':'drain-finite-recovery','work_items':64}]
        return f,[eq('canonical_F_commits',1),eq('history_F_units',1),eq('outcome','Committed'),eq('final_generation',9),eq('partial_scene_observed',False)]
    if case=='V05':
        f['title_utf8']='原始标题';f['resource_revision']=1
        f['schedule']=[{'action':'delay-layout','title':'原始标题','target':target(),'resource_revision':1},
            {'action':'change-title','title':'当前标题'} if name=='stale-title-result' else
            {'action':'remount','target':target(attachmentEpoch=8)} if name=='stale-target-result' else
            {'action':'resource-refresh','revision':2},{'action':'deliver-old-layout'},{'action':'deliver-current-layout'}]
        return f,[eq('stale_result_published',False),eq('current_layout_visible',True),eq('refresh_history_delta',0),eq('refresh_document_delta',0)]
    raise ValueError(case)

def target(**overrides):return {'runtimeEpoch':9007199254740993,'viewId':2,'viewEpoch':3,'attachmentEpoch':7,**overrides}

def coherence_variant(case,name):
    controls=json.loads((SEED/'temporal-controls-CR01.json').read_text())
    f={'target':target(),'base_generation':7,'committed_generation':8,'base_leaf_x':10,'preview_dx':10,
        'finite_recovery_work':[{'job':i,'generation':9} for i in range(4)],'observe_every_handoff':True}
    if case in ['C01','C03']:
        f['schedule']=[{'action':'preview','x':20},{'action':'commit','generation':8},{'action':'delay-generation','generation':8},
            {'action':'recover','generation':9},{'action':'drain-declared-work'}]
        good=cp(controls['C01']['good_trace'])
        if case=='C03':good[-1]['scene']='g+2'
        if name=='fault-then-recovery':good.insert(1,{'unavailable':True})
        if name=='all-unavailable':good=[{'unavailable':True}]*3
        if name=='empty':good=[]
        if name=='truncated':good=good[:1]
        f['independent_control_trace']=good
        if name=='late-g1-after-g2':f['schedule'].append({'action':'deliver-stale-generation','generation':8})
        return f,[{'op':'c01','path':'trace'}]
    if case=='C02':
        f['descendant_origins_before']=[[10,5],[30,15]];f['preview']={'translation':[10,0]} if name=='move-whole-subtree' else {'boundary':[0,0,120,90]}
        return f,[eq('descendant_origins',[[20,5],[40,15]] if name=='move-whole-subtree' else [[10,5],[30,15]]),eq('render_query_basis_equal',True)]
    f['schedule']=[{'event':'wrap_committed','selected':['S']},{'event':'user_selection','selected':['T']}]
    if name=='late-callback-after-wrapper-delete':f['schedule'].append({'event':'delete-wrapper','id':identity(6)})
    if name=='view-B':f['callback_target']=target(viewId=4)
    if name=='detach-remount':f['callback_target']=target(attachmentEpoch=8)
    f['schedule'] += [{'event':'late_callback','selected':['T']},{'event':'later_current_read','selected':['T']}]
    return f,[{'op':'c02','path':'trace'},eq('primary','T'),eq('scope',None)]

def lifecycle_variant(case,name):
    f=wrap();f.update({'target':target(),'owner_thread':'qualification-owner','request_key':{'client':'fc08','sequence':1},
        'pointer':{'source':'touch','pointerId':9007199254740995,'generation':4},'selected':[identity(9)],
        'selection_revision':11,'observe_every_handoff':True})
    if case=='L01':
        f['schedule']=['Down','Move','Cancel','late-Up'] if name.startswith('cancel') else \
            ['Down','Move','CaptureLost','late-Up'] if name.startswith('capture') else ['Down','Move','Up-commit','Cancel']
        committed=name=='commit-then-cancel'
        return f,[eq('canonical_commit_delta',int(committed)),eq('history_delta',int(committed)),eq('session_active',False),
            eq('outcome','Committed' if committed else 'Cancelled')]
    if case=='L02':
        f['targets']=[{'value':'1000000000000000000000000000002a'},{'value':'2000000000000000000000000000002a'}]
        f['selected']=f['targets'][:1];f['requested_target']=f['targets'][0]
        mismatch={'stale-target':{'runtimeEpoch':9007199254740994},'stale-view':{'viewEpoch':4},'stale-attachment':{'attachmentEpoch':8},
            'stale-pointer':{'pointer_generation':5},'stale-selection':{'selection_revision':12}}
        f['current_changes']=mismatch.get(name,{})
        return f,([eq('written_ids',[f['targets'][0]]),eq('other_target_unchanged',True)] if name=='same-low64-different-high64' else rejected())
    if case=='L03':
        f['schedule']=[{'action':'submit','key':f['request_key'],'payload':'forward'},{'action':'drain'}]
        if name=='same-key-changed-payload':f['retry_payload']=metadata()['forward']
        if name=='evicted-after-ack':f['schedule'] += [{'action':'ack','high_water':1},{'action':'evict-acknowledged'}]
        if name=='new-target-epoch':f['schedule'].append({'action':'remount','target':target(attachmentEpoch=8)})
        f['schedule'].append({'action':'retry','key':f['request_key'],'target':target()})
        return f,[eq('canonical_commit_delta',1),eq('new_execution_on_retry',False),eq('retry_outcome',
            'Collision' if name=='same-key-changed-payload' else 'OutcomeUnavailable' if name in ['evicted-after-ack','new-target-epoch'] else 'SameOutcome')]
    if case=='L04':
        f['limits']={'pending_queue':256,'receipt_slots':1024,'client_registry':1280}
        f['fill']={'pending_queue':256 if name in ['queue-full','terminal-through-backlog'] else 0,
            'unacknowledged_receipts':1024 if name in ['receipt-full','denied-outcome-slot'] else 0}
        f['admitted_gesture']={'source':'touch','pointerId':42,'generation':8}
        f['schedule']=[{'action':'fill-to-declared-limits'},{'action':'terminal','kind':'Cancel','pointer':f['admitted_gesture']},
            {'action':'submit-new-forward'},{'action':'drain'}]
        return f,[eq('new_request_committed',False),eq('terminal_delivered',True),eq('queue_bounded',True),eq('reservation_released_once',True)]
    if case=='L05':
        f['schedule']=[{'action':name},{'action':'drain-declared-work','count':64}]
        pred={'canonical-reject':[eq('canonicalOutcome','Rejected'),unchanged('before','after')],
            'committed-projection-pending':[eq('canonicalOutcome','Committed'),eq('projectionOutcome','Pending'),eq('canonical_commit_delta',1)],
            'projection-recovered':[eq('canonicalOutcome','Committed'),eq('projectionOutcome','Ready'),eq('canonical_commit_delta',1)],
            'old-ProductControl':[eq('old_state_meaning_preserved',True)],'old-ApplyResult':[eq('old_Applied_meaning_preserved',True)]}[name]
        return f,pred+[eq('saved_claim',False),eq('synced_claim',False)]
    if case=='L06':
        f['caller_payload_bytes']=W.encode('Operation',f['forward']).hex()
        f['schedule']=[{'action':'enqueue-owned-value','thread':'non-owner' if name=='non-owner-ingress' else 'owner'}]
        tail={'non-owner-ingress':[{'action':'owner-drain'}],'mutate-caller-buffer':[{'action':'overwrite-caller-bytes','hex':'ff'},{'action':'owner-drain'}],
            'release-caller-buffer':[{'action':'release-caller-bytes'},{'action':'owner-drain'}],
            'detach-queued':[{'action':'detach'},{'action':'owner-drain'}],'close-queued':[{'action':'close'},{'action':'owner-drain'}],
            'detach-staging':[{'action':'pause-safe-staging'},{'action':'detach'},{'action':'resume'}],
            'close-staging':[{'action':'pause-safe-staging'},{'action':'close'},{'action':'resume'}],
            'write-behind-disabled-UI':[{'action':'close-owner'},{'action':'direct-shared-write-ingress'}]}[name]
        f['schedule']+=tail
        success=name in ['non-owner-ingress','mutate-caller-buffer','release-caller-buffer']
        return f,[eq('canonical_commit_delta',int(success)),eq('caller_memory_borrowed',False),eq('post_close_commit',False)]
    raise ValueError(case)

def compatibility_variant(case,name):
    if case=='K01':
        v=1 if name.startswith('snapshot1') else 2 if name.startswith('snapshot2') else 3
        value=snapshot(W,[record(20,'Shape')] if v==1 else [],v,['resource-manifest-v1'])
        return {'explicit_legacy_import':v<3,'wire_root':'DocumentSnapshot','wire_model':value},[
            eq('ready',True),eq('legacy_fields_preserved',True)],None
    if case in ['K02','K03']:
        triangle=record(20,'Shape');triangle['kind_version']=2;triangle['content']['shape']['shape_kind']=3
        objects_=[record(10),triangle];caps=['resource-manifest-v1','section-v1','shape2-triangle-v1'];v=3
        if name=='missing-section-capability':caps.remove('section-v1')
        if name=='duplicate-capability':caps.insert(1,'section-v1')
        if name=='swapped-capability':caps[0],caps[1]=caps[1],caps[0]
        if name=='unknown-capability':caps.append('zzz-unknown-v1')
        if name.startswith('Section-in-snapshot'):v=int(name[-1]);objects_=[record(10)]
        if name=='Section-version2':objects_[0]['kind_version']=2
        value=snapshot(W,objects_,v,caps)
        return {'wire_root':'DocumentSnapshot','wire_model':value,'runtime_capabilities':[
            'resource-manifest-v1','section-v1','shape2-triangle-v1','container-topology-v1']},[
            eq('ready',case=='K02'),*([eq('required_capabilities',caps)] if case=='K02' else [eq('editable_partial_document',False)])],None
    if case=='K04':
        before=[object_('G','Group')];after=before+[object_('S','Group')]
        f={'checkpoint':snapshot(W,before,caps=['resource-manifest-v1']),'journal':[topology(before,after)],
            'reader_capabilities':['resource-manifest-v1']+(['container-topology-v1'] if name.startswith('capable') else []),
            'journal_required_capabilities':['container-topology-v1']}
        return f,[eq('ready',name.startswith('capable')),eq('new_effect_count',int(name.startswith('capable'))),eq('committed_prefix_preserved',True)], [('journal','Operation',W.encode('Operation',f['journal'][0]))]
    if case=='K05':
        m=metadata();after=cp(m['after'])+[object_('G','Group')]
        next(r for r in after if r['id']==identity(6))['placement']=placement(5,key(6))
        f={'checkpoint':snapshot(W,m['before']),'journal':[m['forward'],topology(m['after'],sorted(after,key=lambda r:r['id']['value']),1002)],
            'History_owner':False,'local_reservation':False,'expected_records':sorted(after,key=lambda r:r['id']['value'])}
        return f,[eq('canonical.records',f['expected_records']),eq('ledger_count',2),eq('History_owner',False),eq('local_history_entries',0)],[
            ('op17','Operation',W.encode('Operation',f['journal'][0])),('op18','Operation',W.encode('Operation',f['journal'][1]))]
    if case=='K06':
        f=wrap();f['checkpoint_after_release']=snapshot(W,f['before'],caps=['resource-manifest-v1'])
        f['release']=topology(f['after'],f['before'],1002);f['retained_journal_required']=['container-topology-v1']
        f['retained_journal']=[topology(f['before'],f['before']+[object_('G','Group')],1003)]
        f['writer_support']=['resource-manifest-v1'] if name=='writer-support-downgraded' else ['container-topology-v1','resource-manifest-v1','section-v1']
        return f,[eq('snapshot_required_capabilities',['resource-manifest-v1']),eq('journal_still_requires_topology',True),
            eq('writer_admitted',name!='writer-support-downgraded')],None
    raise ValueError(case)

def public_variant(case,name):
    f={'ordinary_consumer':'tests/fc08/native/public_consumer.cpp','private_access_allowed':False,'target':target()}
    if case=='P01':
        concrete=metadata('title' if name=='rename' else 'boundary') if name in ['rename','resize'] else \
            wrap(nonempty=False) if name=='create' else nested() if name=='release' else \
            geometry_variant('G01','literal-affine-reparent')[0] if name=='reparent' else cross_parent()
        f.update(concrete)
        f['typed_action']=name;f['compare_paths']=['ordinary-public-consumer','Debug-UI-shared-typed-seam']
        if name in ['undo','redo']:f['setup']=['forward']+(['undo'] if name=='redo' else [])
        return f,[eq('container_typed_api',True),eq('identities_and_outcomes_match',True),
            eq('canonical.records',f['before'] if name=='undo' else f['after']),
            eq('history.size',1),eq('history.cursor',0 if name=='undo' else 1)]
    if case=='P02':
        f.update({'object_id':{'value':'10000000000000000020000000000001'},'title':'中文 e\u0301 Axiom',
            'native_physical':[100,120],'DPR':2,'web_CSS':[50,60],'camera':{'scale':2,'translation':[10,20]},
            'Section_translation':[5,5],'preedit':'未确认','confirmed':'中文 e\u0301 Axiom','variant':name,
            'metrics_before':{'DPR':1,'generation':1},'metrics_after':{'DPR':2,'generation':2},
            'pointer':{'source':'touch','id':9007199254740993,'generation':4},'terminal':'Cancel'})
        return f,[eq('object_id',f['object_id']),eq('view_logical',[50,60]),eq('world',[20,20]),eq('local',[15,15]),
            eq('preedit_operation_count',0),eq('confirmed_title',f['confirmed']),eq('one_coordinate_map',True)]
    if case=='P03':
        f.update({'records':[object_('S'),object_('G','Group'),object_('a','Shape','S'),object_('b','Shape','G')],
            'selected':{'one-Section-default':[6],'all-Sections-default':[6,1],'Group-default':[5],
                'leaf-default':[9],'Section-leaf-default':[6,10],'explicit-Group-Section-release':[5,6]}[name],
            'action':'explicit-release' if name=='explicit-Group-Section-release' else 'default-delete'})
        if name=='all-Sections-default':f['records'].append(object_('A'))
        f['selected']=[identity(n) for n in f['selected']]
        if name=='Section-leaf-default':return f,[eq('outcome','UnsupportedMixedRemoval'),unchanged('before','after')]
        removed={'one-Section-default':[6],'all-Sections-default':[1,6],'Group-default':[5,10],'leaf-default':[9],
                 'explicit-Group-Section-release':[5,6]}[name]
        retained=sorted(set([int(r['id']['value'],16) for r in f['records']])-set(removed))
        return f,[eq('removed_ids',[identity(n) for n in removed]),eq('retained_ids',[identity(n) for n in retained]),eq('canonical_commit_delta',1)]
    if case=='P04':
        f['exact_inherited_binding']=name;f['reuse_without_duplicate_independence_claim']=True
        return f,[eq('selected_regressions_passed',True),eq('exact_sources_bound',True)]
    raise ValueError(case)

def main():
    catalog=json.loads((SEED/'effective-case-catalog.json').read_text(encoding='utf-8'))
    entries=[]
    for c in catalog['cases']:
        case=c['id'][5:]
        for name in VARIANTS[case]:
            blobs=None;recipe=None
            if case[0]=='W':f,p,blobs=w_variant(case,name)
            elif case[0]=='T':f,p,blobs=topology_variant(case,name)
            elif case[0]=='H':f,p,blobs,recipe=history_variant(case,name)
            elif case[0]=='G':f,p=geometry_variant(case,name)
            elif case[0]=='S':f,p=selection_variant(case,name)
            elif case[0]=='V':f,p=projection_variant(case,name)
            elif case[0]=='C':f,p=coherence_variant(case,name)
            elif case[0]=='L':f,p=lifecycle_variant(case,name)
            elif case[0]=='K':f,p,blobs=compatibility_variant(case,name)
            elif case[0]=='P':f,p=public_variant(case,name)
            else:raise ValueError(case)
            entries.append(materialize(case,name,f,p,blobs,recipe))
            print(c['id']+'/'+name,flush=True)
    manifest={'format':'fc08-concrete-corpus-v2','effective_catalog_sha256':digest((SEED/'effective-case-catalog.json').read_bytes()),
        'source_design':'FC08-P20-v0.1+CR01','production_writer_used':False,'product_P32_authorized':False,'variants':entries,
        'qualification_controls_not_product_positives':['FC08-G05/quality-at'],
        'historical_partial_index':'.aegis/materialized/FC-08/wire-variant-index.partial.json'}
    put(DEST/'corpus-manifest.json',jsonbytes(manifest))
    put(DEST/'variant-index.json',jsonbytes({'logical_cases':[c['id'] for c in catalog['cases']],
        'variants':[{'case_id':x['case_id'],'variant_id':x['variant_id']} for x in entries]}))
    design=jsonbytes({'format':'fc08-executable-verification-v2',
        'corpus_manifest':(DEST/'corpus-manifest.json').relative_to(ROOT).as_posix(),'runner':'tools/fc08/run_verification.py',
        'oracle':'tests/fc08/reference/oracles.py','profiles':['windows-native','web-wasm','headless'],'product_P32_authorized':False})
    put(DEST/'executable-verification.json',design)
    # The navigational entrypoint is a current candidate, never a historical
    # control receipt; immutable per-attempt manifests above preserve identity.
    (ROOT/'.aegis/materialized/FC-08/executable-verification.json').write_bytes(design)

if __name__=='__main__':main()
