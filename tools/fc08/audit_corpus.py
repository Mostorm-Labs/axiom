"""Independent authoring audit, not a DUT or a substitute apply coordinator.

Checks literal fixture pairs and guard tables against the accepted structural
rules. It operates only on JSON values; it has no Axiom/store/submit imports.
"""
from copy import deepcopy as cp
import hashlib,json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from fixtures import children
from oracles import valid_forest,exact
from variants import VARIANTS

def structural(r):return {'object_id':r['id'],'kind_id':r['kind_id'],'kind_version':r['kind_version'],
    'placement_before':r['placement'],'transform_before':r['transform']}
def inspect_pair(before,op):
    records={r['id']['value']:cp(r) for r in before}
    payload=op['payload']
    if 'edit_container_topology' in payload:
        p=payload['edit_container_topology'];g=p['guard']
        nodes={n['object_id']['value']:n for n in g.get('nodes',[])}
        for k,n in nodes.items():
            assert k in records and exact(n,structural(records[k])),'structural guard differs from before'
        child_sets={s.get('parent_id',{}).get('value'):s for s in children(before)}
        for s in g['child_sets']:
            assert exact(s,child_sets[s.get('parent_id',{}).get('value')]),'child_set is not the complete before set'
        effects=[k for k in ['introduce_wrappers','release_wrappers','relocate_objects'] if k in p]
        assert len(effects)==1,'one effect required'
        effect=effects[0];new=set();removed=set()
        if effect=='introduce_wrappers':
            for r in p[effect]['containers']:
                k=r['id']['value'];assert r['kind_id'] in [9,10] and k not in records,'new wrapper invalid'
                new.add(k);records[k]=cp(r)
        elif effect=='release_wrappers':
            for r in p[effect]['containers_before']:
                k=r['id']['value'];assert r['kind_id'] in [9,10] and exact(records[k],r),'release preimage invalid'
                removed.add(k)
        else:assert p.get('relocations'),'empty Relocate'
        writes=set()
        if removed:
            frontier={k for k,r in records.items() if k not in removed and r['placement'].get('parent_id',{}).get('value') in removed}
            assert {r['object_id']['value'] for r in p.get('relocations',[])}==frontier,'release frontier incomplete'
        for r in p.get('relocations',[]):
            k=r['object_id']['value'];assert k in nodes and k not in writes,'missing guard/overlapping relocation'
            if new:assert r['placement_after'].get('parent_id',{}).get('value') in new,'Introduce relocates outside new wrapper'
            records[k]['placement']=cp(r['placement_after']);records[k]['transform']=cp(r['transform_after']);writes.add(k)
        for r in p.get('order_only_edits',[]):
            k=r['object_id']['value'];assert k in nodes and k not in writes,'missing guard/overlapping order edit'
            records[k]['placement']['order_key']=cp(r['order_key_after']);writes.add(k)
        for k in removed:del records[k]
    elif 'set_section_content' in payload:
        p=payload['set_section_content'];basis={r['object_id']['value']:r for r in p['geometry_basis']}
        for item in p['items']:
            k=item['object_id']['value'];r=records[k]
            assert r['kind_id']==10 and r['kind_version']==item['expected_kind_version']
            assert exact(r['content']['section'],item['expected_content']),'metadata preimage differs'
            if item['expected_content']['boundary']!=item['replacement_content']['boundary']:
                assert k in basis,'boundary edit lacks own geometry_basis'
            r['content']['section']=cp(item['replacement_content'])
    else:return None
    graph={k:{'kind':{1:'Shape',4:'RichText',8:'Sticky',9:'Group',10:'Section'}[r['kind_id']],
        'parent':r['placement'].get('parent_id',{}).get('value'),'order':r['placement']['order_key']['value'],
        'local':list(r['transform'].values())} for k,r in records.items()}
    valid_forest(graph)
    return [records[k] for k in sorted(records)]

def main():
    design=json.loads((ROOT/'.aegis/materialized/FC-08/executable-verification.json').read_text())
    m=json.loads((ROOT/design['corpus_manifest']).read_text(encoding='utf-8'));errors=[];checks=[]
    wanted={f'FC08-{c}/{v}' for c,vs in VARIANTS.items() for v in vs}
    assert wanted=={r['variant_id'] for r in m['variants']},'discovery mismatch'
    for row in m['variants']:
        try:
            for ref in [row['input'],row['expected'],*[w for w in row['wire'] if 'path' in w]]:
                assert hashlib.sha256((ROOT/ref['path']).read_bytes()).hexdigest()==ref['sha256'],'hash mismatch'
            f=json.loads((ROOT/row['input']['path']).read_text(encoding='utf-8'));e=json.loads((ROOT/row['expected']['path']).read_text(encoding='utf-8'))
            assert e['predicates'],'empty oracle'
            negative=any(p.get('expected') in ['Rejected','HistoryConflict','UnsupportedPose','NumericRange'] or p.get('path')=='accepted' and p.get('expected') is False for p in e['predicates'])
            # Literal-table audit only: no temporal intervening command is
            # guessed/applied here and no result is substituted for the DUT.
            if 'before' in f and 'after' in f and 'forward' in f and not negative and not f.get('schedule'):
                calculated=inspect_pair(f['before'],f['forward'])
                if calculated is not None:assert exact(calculated,f['after']),'F differs from literal after'
                if 'inverse' in f and calculated is not None:
                    back=inspect_pair(f['after'],f['inverse'])
                    if back is not None:assert exact(back,f['before']),'U differs from literal before'
                checks.append(row['variant_id'])
            if row['case_id']=='FC08-W01':
                for k in ['source_index','base_descriptor','proposed_descriptor']:assert (ROOT/f[k]).is_file(),'missing source input '+f[k]
        except (AssertionError,KeyError,ValueError) as err:errors.append({'variant_id':row['variant_id'],'error':str(err)})
    result={'format':'fc08-independent-authoring-audit','errors':errors,'literal_pair_checks':checks,'product_PASS':False}
    destination=ROOT/'out/fc08/corpus-audit.json';destination.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
    return int(bool(errors))
if __name__=='__main__':sys.exit(main())
