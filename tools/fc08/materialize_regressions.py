"""Pin the applicable retained regression subset before executing it."""
import hashlib,json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'.aegis/materialized/FC-08/attempts/v0.2-obs01-regressions'
def ref(path):
    path=path if isinstance(path,Path) else ROOT/path
    b=path.read_bytes();return {'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(b).hexdigest()}
def main():
    base=ROOT/'verification/corpus/semantic/v1/wire/bg'
    prefixes=['BG-001-','BG-002-','BG-004-','BG-007-','BG-008-','BG-010-','BG-N04-','BG-N06-','BG-N07-','BG-N08-']
    roots={'BG-N04':'Transform2D','BG-N06':'Id128','BG-N07':'Id128','BG-N08':'OrderKey'}
    fixtures=[]
    for folder in sorted(base.iterdir()):
        if not any(folder.name.startswith(s) for s in prefixes):continue
        case=json.loads((folder/'case.json').read_text(encoding='utf-8'));positive=case['expected']['outcome']=='ACCEPTED'
        root=json.loads((folder/'authoring/input.projection.json').read_text(encoding='utf-8'))['rootType'] if positive else roots[case['id']]
        artifact=folder/('expected/canonical.pb' if positive else case['input']['artifact'])
        fixtures.append({'id':case['id'],'root':root,'input':ref(artifact),'case':ref(folder/'case.json'),
            'accepted':positive,'canonical_hex':artifact.read_bytes().hex() if positive else None,
            'category':None if positive else case['expected']['category']})
    sources=[{'id':'inherited-selection','source':ref('runtime/interaction/tests/selection_session_test.cpp'),
        'target':'axiom_fc08_selection_regression','predicates':['click/toggle deterministic primary','hidden/locked filter','deletion prune and existing local cancel'],
        'FC08_new_selection_policy_claimed':False},
       {'id':'inherited-connector','source':ref('runtime/composition/tests/fc05_connector_facade_test.cpp'),
        'target':'axiom_fc08_connector_regression','predicates':['public typed create/routing','real History Undo/Redo','generation/canonical invalidation','close'],
        'test_portability_delta':'Only split one mixed size_t/uint64_t auto declaration in build-local copy; original source unchanged'},
       {'id':'inherited-control','source':ref('runtime/composition/tests/canvas_runtime_facade_test.cpp'),
        'target':'axiom_fc08_control_regression','predicates':['real queue/receipt ordering','close writer lane','stale target','full128 readback']}]
    output={'format':'fc08-exact-inherited-subset-v2','goldens':fixtures,'sources':sources,
        'mapped_cases':['FC08-W07','FC08-K01','FC08-P04'],
        'FC01A_B01_B10':'Retained historical family, not reopened or claimed passed. Only the exact applicable legacy source predicates above are executed; Section-specific geometry remains a future capability obligation.',
        'mock_History_test_excluded':'runtime/interaction/tests/editor_history_test.cpp uses Submit fake; use public real-owner connector and qualification transitions instead',
        'reuse_rule':'One raw run may support each mapped assertion on the same profile; no duplicate independence claim',
        'product_P32_authorized':False}
    DEST.mkdir(parents=True,exist_ok=True)
    (DEST/'inherited-regression-plan.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
    print('Exact immutable regression subset pinned')
if __name__=='__main__':main()
