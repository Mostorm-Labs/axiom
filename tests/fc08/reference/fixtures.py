"""Independent concrete FC08 tables. This module never imports the DUT.

These constructors author inputs/preimages, not a second implementation used as
the product. Wire encoding comes exclusively from the isolated field table.
"""
from copy import deepcopy as cp
import json
from pathlib import Path
from wire import Writer, record, identity, placement, transform, section, structural, operation
from oracles import compose, inverse, point, bits, quality

ROOT=Path(__file__).resolve().parents[3]
SEED=ROOT/'.aegis/materialized/FC-08/sources/accepted-seed/verification'
IDS={'A':1,'B':2,'C':3,'D':4,'G':5,'S':6,'P':7,'Q':8,'a':9,'b':10,'T':11,'X':12}
I=[1,0,0,1,0,0]

def seed(name): return json.loads((SEED/'fixtures'/f'{name}.json').read_text(encoding='utf-8'))
def key(n): return f'{n:08x}'
def object_(name,kind='Section',parent=None,order=None,local=None):
    return record(IDS[name],kind,IDS[parent] if parent else None,order or key(IDS[name]),local or I)
def objects(graph):
    out=[]
    for name,n in graph.items():
        r=object_(name,n.get('kind','Section'),n.get('parent'),n.get('order'),n.get('local',[1,0,0,1,n.get('tx',0),0]))
        if 'boundary' in n: r['content']['section']=section(n['boundary'],n.get('title',''))
        if 'local_rect' in n:r['content']['shape'].update(width=n['local_rect'][2],height=n['local_rect'][3])
        if n.get('hidden'):r['properties'].setdefault('entries',[]).append({'field_id':1,'value':{'bool_value':False}})
        if n.get('locked'):r['properties'].setdefault('entries',[]).append({'field_id':2,'value':{'bool_value':True}})
        out.append(r)
    return sorted(out,key=lambda r:r['id']['value'])
def children(records):
    by={None:[]}
    for r in records:
        if r['kind_id'] in (8,9,10):by.setdefault(r['id']['value'],[])
        p=r['placement'].get('parent_id',{}).get('value')
        by.setdefault(p,[]).append({'object_id':r['id'],'order_key':r['placement']['order_key']})
    return [dict(**({'parent_id':{'value':p}} if p else {}),complete_members=sorted(v,key=lambda x:x['object_id']['value']))
            for p,v in sorted(by.items(),key=lambda x:x[0] or '')]
def topology(before,after,number=1001):
    """Author exact F/U from independently declared before/after tables."""
    b={r['id']['value']:r for r in before};a={r['id']['value']:r for r in after}
    added=sorted(a.keys()-b.keys());removed=sorted(b.keys()-a.keys())
    if added and removed:raise ValueError('one effect only')
    edits=[];order_edits=[]
    for k in sorted(a.keys()&b.keys()):
        if a[k]['placement']!=b[k]['placement'] or a[k]['transform']!=b[k]['transform']:
            if a[k]['placement'].get('parent_id')==b[k]['placement'].get('parent_id') and a[k]['transform']==b[k]['transform']:
                order_edits.append({'object_id':a[k]['id'],'order_key_after':a[k]['placement']['order_key']})
            else:edits.append({'object_id':a[k]['id'],'placement_after':a[k]['placement'],'transform_after':a[k]['transform']})
    effect={'introduce_wrappers':{'containers':[a[k] for k in added]}} if added else \
           {'release_wrappers':{'containers_before':[b[k] for k in removed]}} if removed else {'relocate_objects':{}}
    return operation({'edit_container_topology':{'guard':{'nodes':[structural(r) for r in sorted(before,key=lambda r:r['id']['value'])],
        'child_sets':children(before)},'relocations':edits,'order_only_edits':order_edits,**effect}},number)
def transition(before,after):
    return {'before':before,'forward':topology(before,after),'inverse':topology(after,before,1002),
        'redo':topology(before,after,1003),'after':after,'undo_after':before}
def wrap(kind='Section',nonempty=True):
    before=[object_('a','Shape')] if nonempty else []
    after=[object_('S',kind)]+([object_('a','Shape','S')] if nonempty else [])
    return transition(before,sorted(after,key=lambda r:r['id']['value']))
def cross_parent():
    s=seed('cross-parent');g=cp(s['before']);after=cp(g)
    after['S']={'kind':'Section','parent':None,'order':'20',**s['explicit_final_wrapper']['S']}
    for k,v in s['final_children'].items():after[k].update(v)
    result=transition(objects(g),objects(after))
    for side in ('forward','inverse'):
        for r in result[side]['payload']['edit_container_topology'].get('introduce_wrappers',{}).get('containers',[]):
            if r['id']==identity(6):r['content']['section'].update(padding=0.,header_height=0.)
    # The identical record preimage is retained in the inverse release.
    for r in result['after']:
        if r['id']==identity(6):r['content']['section'].update(padding=0.,header_height=0.)
    result['inverse']=topology(result['after'],result['before'],1002)
    result['redo']=cp(result['forward']);result['redo']['operation_id']=identity(1003)
    result['world_origins']={'a':[12,3],'b':[104,55]}
    return result
def nested():
    s=seed('nested-release')
    b=objects(s['before']);a=objects(s['after'])
    return {**transition(b,a),'frontier':[identity(2),identity(4)],'world_x_before':s['world_x_before'],'world_x_after':s['world_x_after']}
def metadata(field='title'):
    b=object_('S');a=cp(b);s=a['content']['section']
    s[field]={'title':'新标题 e\u0301','padding':12.,'header_height':24.,'boundary':{'x':-20.,'y':-10.,'width':120.,'height':90.}}[field]
    def op(x,y,n):return operation({'set_section_content':{'items':[{'object_id':identity(6),'expected_kind_version':1,
        'expected_content':x['content']['section'],'replacement_content':y['content']['section']}],
        'geometry_basis':[structural(x)] if field=='boundary' else []}},n)
    return {'before':[b],'after':[a],'undo_after':[b],'forward':op(b,a,1001),'inverse':op(a,b,1002),'redo':op(b,a,1003)}
def numeric_sample(n):
    parent=[1,1,1,1+2.**-n,0,0];old=[1.1,.2,-.3,.9,17.1,-9.2]
    inv=inverse(parent);local=compose(inv,old);new=compose(parent,local)
    probes=[{'local':p,'before':point(old,p),'after':point(new,p)} for p in [[0,0],[20,0],[20,10],[0,10]]]
    return {'parent':parent,'old_world':old,'local_bits':[bits(x) for x in local],'new_world_bits':[bits(x) for x in new],
        'points':probes,'quality_ok':all(quality(a,b) for p in probes for a,b in zip(p['before'],p['after']))}

def richtext(n,lengths,parent=None):
    paragraphs=[]
    for k,size in enumerate(lengths):
        paragraphs.append({'paragraph_id':identity(100000+n*32+k),'style':{'alignment':1,'line_height':1.25,'spacing_before':0.,'spacing_after':0.},
            'runs':[{'text':'x'*size,'style':{'font_resource_id':identity(512),'font_size':16.,'weight':400,'italic':False,
               'underline':False,'color':{'r':0.,'g':0.,'b':0.,'a':1.}}}]})
    return record(n,'RichText',parent,key(n),content={'rich_text':{'document':{'paragraphs':paragraphs}}})

def budget_recipe(name):
    """Returns actual encoded bytes + the small reproducible authoring recipe.

    Large blobs need not be checked in: the digest is measured from these real
    bytes and runners regenerate the exact recipe (not a mocked byte meter).
    """
    w=Writer()
    if name.startswith('restore-'):
        target=33554431 if name=='restore-under-limit' else 33554433
        def sized(n,size):
            def enc(last):return w.encode('ObjectRecord',richtext(n,[1048576]*15+[last],1))
            last=size-len(enc(0))
            for _ in range(8):last+=size-len(enc(last))
            r=richtext(n,[1048576]*15+[last],1)
            if len(w.encode('ObjectRecord',r))!=size:raise ValueError('restore record size')
            return r
        connector=record(40,'Shape',key=key(40));connector['kind_id']=7
        connector['content']={'connector':{'start':{'attached':{'target_object_id':identity(2),'anchor':{'stable_port':{'port_id':2}}}},
            'end':{'attached':{'target_object_id':identity(12),'anchor':{'stable_port':{'port_id':4}}}},'routing':1}}
        first_size=16777000
        def closure(second_size):return [record(1,'Section'),record(2,'Shape',1,key(2)),sized(30,first_size),sized(31,second_size),connector]
        def encoded(size):return w.encode('Operation',operation({'restore_objects':{'objects':closure(size)}},1002))
        second=target-len(encoded(first_size))+first_size
        for _ in range(8):second+=target-len(encoded(second))
        data=encoded(second)
        if len(data)!=target or second>16777216:raise ValueError('restore complete size')
        f=w.encode('Operation',operation({'delete_objects':{'object_ids':[identity(1)]}}))
        return [('F','Operation',f),('U','Operation',data)],{'recipe':name,'first_RichText_bytes':first_size,'second_RichText_bytes':second,
            'actual_restore_bytes':len(data),'closure_ids':[1,2,30,31,40],'outside_endpoint_retained':12,
            'outside_object':record(12,'Shape',key=key(12)),
            'font_resource':512,'command_admitted':target<=33554432}
    if name.startswith('root-children-'):
        n=int(name.rsplit('-',1)[1]);before=[record(100+i,'Shape',key=key(i+1)) for i in range(n)]
        new=record(90000,'Section',key=key(n+1));after=before+[new]
        f=topology(before,after);u=topology(after,before,1002)
        return [('F','Operation',w.encode('Operation',f)),('U','Operation',w.encode('Operation',u))],{
            'recipe':'root-children','N':n,'forward_root_members':n,'inverse_root_members':n+1,'command_admitted':n+1<=65535}
    family,target=name.rsplit('-',1);target=int(target)
    if family=='key':
        obj=record(20,'Shape',key='11'*target)
        return [('input','ObjectRecord',w.encode('ObjectRecord',obj))],{'recipe':'order-key','length':target,'size_predicate':target<=32}
    if family=='string':
        obj=record(10,content={'section':section(title='x'*target)})
        return [('input','ObjectRecord',w.encode('ObjectRecord',obj))],{'recipe':'Section-title','length':target,'size_predicate':target<=1048576}
    if family=='keyed':
        models=[record(100+i,'Group',key=key(i+1)) for i in range(target)]
        op=operation({'edit_container_topology':{'guard':{'nodes':[],'child_sets':[{'complete_members':[]}]},
            'introduce_wrappers':{'containers':models}}})
        return [('input','Operation',w.encode('Operation',op))],{'recipe':'keyed-containers','N':target,'size_predicate':target<=65535}
    if family=='record':
        def encoded(last):return w.encode('ObjectRecord',richtext(30,[1048576]*15+[last]))
        base=len(encoded(0));last=target-base
        for _ in range(8):last+=target-len(encoded(last))
        data=encoded(last)
        if len(data)!=target or not 0<last<=1048576:raise ValueError('record recipe failed')
        return [('input','ObjectRecord',data)],{'recipe':'RichText-record','paragraph_lengths':[1048576]*15+[last],
            'actual_bytes':len(data),'size_predicate':target<=16777216,'font_resource':512}
    if family=='operation':
        def models(last):return [record(100+i,'Section',key=key(i+1),content={'section':section(title='x'*(1048576 if i<31 else last))}) for i in range(32)]
        def op(last):return operation({'edit_container_topology':{'guard':{'nodes':[],'child_sets':[{'complete_members':[]}]},
            'introduce_wrappers':{'containers':models(last)}}})
        def enc(last):return w.encode('Operation',op(last))
        last=target-len(enc(0))
        for _ in range(8):last+=target-len(enc(last))
        data=enc(last)
        if len(data)!=target or not 0<last<=1048576:raise ValueError('operation recipe failed')
        rs=models(last);u=topology(rs,[],1002);inverse_bytes=w.encode('Operation',u)
        return [('F','Operation',data),('U','Operation',inverse_bytes)],{'recipe':'32-Section-titles','full_title_count':31,'last_title_length':last,
            'actual_bytes':len(data),'inverse_bytes':len(inverse_bytes),'size_predicate':target<=33554432,
            'command_admitted':max(len(data),len(inverse_bytes))<=33554432}
    raise ValueError(name)
