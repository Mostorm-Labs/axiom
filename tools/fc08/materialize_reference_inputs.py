"""Concrete independent raw probes, arithmetic tables and immutable controls."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from materialize_sources import MAT, digest, immutable, save_json
from wire import Writer, record, snapshot, introduce_empty, content_edit, operation, section, field
from oracles import compose, inverse, point, bits, quality, c01, c02, worlds, release_frontier


def main():
    writer = Writer()
    corpus = ROOT/'tests/fc08/corpus'
    specs = [('section-record','ObjectRecord',record(10)),
             ('section-snapshot3','DocumentSnapshot',snapshot(writer,[record(10)])),
             ('op17-content','Operation',content_edit()),
             ('op18-introduce','Operation',introduce_empty()),
             ('legacy-insert','Operation',operation({'insert_objects':{'objects':[record(20,'Shape')]}})),
             ('legacy-snapshot1','DocumentSnapshot',snapshot(writer,[record(20,'Shape')],1)),
             ('legacy-snapshot2','DocumentSnapshot',snapshot(writer,[],2)),
             ('legacy-snapshot3','DocumentSnapshot',snapshot(writer,[],3,['resource-manifest-v1']))]
    probes = []
    for name,root,value in specs:
        data = writer.encode(root,value)
        path = corpus/'wire'/f'{name}.pb'
        immutable(path,data)
        save_json(path.with_suffix('.input.json'),{'message_type':root,'value':value})
        probes.append({'id':name,'root':root,'path':path.relative_to(ROOT).as_posix(),'sha256':digest(data),
                       'new_feature':name.startswith(('section-','op17-','op18-')),
                       'expected_new_reader':'ACCEPT','production_writer_used':False})
    save_json(MAT/'r01-inputs.json',probes)
    seed = MAT/'sources/accepted-seed/verification'
    samples = json.loads((seed/'fixtures/section-content-wire.json').read_text())
    for sample in samples['cases']:
        immutable(corpus/'wire'/f"{sample['id']}.pb",bytes.fromhex(sample['wire_hex']))
    assert writer.encode('SectionContent',section()).hex() == samples['cases'][0]['wire_hex']
    numeric = []
    for index,(parent,old) in enumerate([
        ([1.1,0.2,-0.3,0.9,17.1,-9.2],[0.7,0.1,0.2,1.3,0.3,-0.7]),
        ([0.3,0.7,-1.1,0.9,100.1,-50.3],[1.2,-0.4,0.6,0.8,-3.3,9.9])],1):
        inv = inverse(parent)
        local = compose(inv,old)
        new = compose(parent,local)
        points = [[0,0],[20,0],[20,10],[0,10]]
        checks = [{'point':p,'before':point(old,p),'after':point(new,p),
                   'quality_ok':all(quality(a,b) for a,b in zip(point(old,p),point(new,p)))} for p in points]
        assert all(c['quality_ok'] for c in checks)
        numeric.append({'variant_id':f'FC08-G06/non-dyadic-{index}','parent':parent,'old_world':old,
                        'inverse_bits':[bits(x) for x in inv],'local_after_bits':[bits(x) for x in local],
                        'new_world_bits':[bits(x) for x in new],'checks':checks,
                        'undo_expected_bits':[bits(x) for x in old],'repeat_F_U':16,
                        'arithmetic':'exact rational -> RN ties-even binary64 after each elementary op; no FMA'})
    save_json(corpus/'numeric/non-dyadic.json',numeric)
    controls = json.loads((seed/'temporal-controls-CR01.json').read_text())
    good_c02 = deepcopy(controls['C02']['bad_trace'])
    good_c02[2]['selected'] = ['T']
    result = {'classification':'R02 oracle sensitivity only; not observed product traces',
              'C01_bad':c01(controls['C01']['bad_trace']), 'C01_good':c01(controls['C01']['good_trace']),
              'C02_bad':c02(controls['C02']['bad_trace']),'C02_good':c02(good_c02),
              'C02_good_trace':good_c02}
    assert result['C01_bad']['first_failure']==1 and result['C02_bad']['first_failure']==2
    assert result['C01_good'] is None and result['C02_good'] is None
    save_json(MAT/'oracle-controls.json',result)
    # Literal before/after tables are copied as data, evaluated by a separate graph walk.
    for name in ['cross-parent','nested-release','affine-reparent','concave-region']:
        raw = (seed/f'fixtures/{name}.json').read_bytes()
        immutable(corpus/f'graphs/{name}.json',raw)
    nested = json.loads((seed/'fixtures/nested-release.json').read_text())
    assert release_frontier(nested['before'],nested['remove']) == nested['frontier']
    for stage in ('before','after'):
        calculated = {k:point(t,[0,0])[0] for k,t in worlds(nested[stage]).items()}
        assert calculated == nested[f'world_x_{stage}']
    print('Raw future/legacy probe inputs, six seed vectors, non-dyadic bits, graph literals and R02 controls materialized')


if __name__ == '__main__': main()
