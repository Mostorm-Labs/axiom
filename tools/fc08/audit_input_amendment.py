"""Independent review-candidate audit. Does not import its generator or any DUT.

This is corpus/profile evidence, never PackageBindingPreflight or product RED.
Literal selection checks and rational geometry witnesses are independently
authored here. All expected files and old oracles must remain unchanged.
"""
import argparse
from collections import Counter
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess

BASE = '63ea152b2944d21120c968d5e11e166d1096b44f'
OLD = '.aegis/materialized/FC-08/attempts/v0.2-obs01-corpus-bound'
PACKAGE = '.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.2/package.json'
SEED = '.aegis/materialized/FC-08/sources/accepted-seed/verification'
PROFILES = {'windows-native', 'web-wasm', 'headless'}


def load(root, path):
    return json.loads((root / path).read_text(encoding='utf-8'))


def verify_ref(root, ref):
    path = (root / ref['path']).resolve()
    assert path.is_relative_to(root), ref['path']
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == ref['sha256'], ref['path']
    assert len(data) == ref['size'], ref['path']


def nested_refs(value):
    if isinstance(value, dict):
        if {'path','sha256','size'} <= value.keys():
            yield value
        else:
            for item in value.values():
                yield from nested_refs(item)
    elif isinstance(value, list):
        for item in value:
            yield from nested_refs(item)


def changed_paths(a, b, prefix=''):
    if type(a) is not type(b):
        return {prefix}
    if isinstance(a, dict):
        common = a.keys() & b.keys()
        return ({prefix + '/' + k for k in a.keys() ^ b.keys()}
                | set().union(*(changed_paths(a[k], b[k], prefix + '/' + k) for k in common)))
    if isinstance(a, list):
        assert len(a) == len(b), prefix
        return set().union(*(changed_paths(x, y, prefix + '/' + str(n)) for n, (x, y) in enumerate(zip(a, b))))
    return set() if a == b else {prefix}


def inside(point, polygon):
    x, y = map(Fraction, point)
    result = False
    for (ax, ay), (bx, by) in zip(polygon, polygon[1:] + polygon[:1]):
        if (ay > y) != (by > y) and x < Fraction(bx-ax) * (y-ay) / (by-ay) + ax:
            result = not result
    return result


def geometry_agrees(inp, identities):
    graph_ids = {identities[name] for name in inp['graph']}
    records = {r['id']['value']: r for r in inp['records']}
    assert graph_ids == set(records), 'Selection graph and canonical records name different objects'
    for name, node in inp['graph'].items():
        record = records[identities[name]]
        local = node.get('local', [1,0,0,1,node.get('tx',0),0])
        assert list(record['transform'].values()) == local, (name, 'transform')
        parent = record['placement'].get('parent_id', {}).get('value')
        assert parent == (identities[node['parent']] if node.get('parent') else None), (name, 'parent')
        if node['kind'] == 'Shape':
            primitive = inp['kind_geometry'].get(name, 'rectangle')
            shape = record['content']['shape']
            assert shape['shape_kind'] == {'rectangle':1,'ellipse':2}[primitive], (name, primitive)
            assert [shape['width'],shape['height']] == node['local_rect'][2:], (name, 'size')
        elif node['kind'] == 'Section':
            boundary = record['content']['section']['boundary']
            assert [boundary[k] for k in ['x','y','width','height']] == node['boundary']


def expected_selection(variant, inp, identities):
    case, name = variant.split('/')
    if case == 'FC08-S01':
        return [identities['S']]
    if case in ('FC08-S02', 'FC08-S04'):
        return [identities['a']]
    if case == 'FC08-S03':
        return [identities['G' if name == 'unopened-Group' else 'S']]
    if case == 'FC08-S05':
        if inp['click'] is None:
            assert inp['identity_target']['value'] == identities['G']
            return [identities['G']]
        if name == 'empty-Group-paint':
            return []
        return [identities['a' if name == 'child' else 'S']]
    assert case == 'FC08-S06'
    assert inp['input']['shift_at_down'] is True
    assert 'ShiftReleased' in inp['schedule']
    assert inp['graph']['b']['hidden'] and inp['graph']['C']['locked']
    return [identities['X']] + ([] if inp['schedule'][-1] == 'Cancel' else [identities['a']])


def headless_expected(row, inp, exp):
    """Separate enumeration of allowed observation requirements, not generator import."""
    c = row['case_id']
    if c in {'FC08-W01','FC08-W02','FC08-W03','FC08-W04','FC08-W05','FC08-W06','FC08-W07',
             'FC08-T01','FC08-T02','FC08-T03','FC08-T04','FC08-T05','FC08-T06',
             'FC08-G01','FC08-K01','FC08-K02','FC08-K03','FC08-K04','FC08-K05','FC08-K06'}:
        return True
    if c == 'FC08-H04':
        return len(exp['predicates']) == 1 and exp['predicates'][0].get('path') == 'independent_size_predicate'
    if c == 'FC08-G03':
        return 'parent' in inp and 'intent' not in inp and {p.get('path') for p in exp['predicates']} == {'world_bits','world_point'}
    if c == 'FC08-G05':
        return inp.get('role') == 'independent-quality-predicate-control'
    return c == 'FC08-P04' and inp['exact_inherited_binding'] == 'touched-codec'


def audit(root, package_path=PACKAGE):
    root = root.resolve()
    git = ['D:/Softwares/Git/cmd/git.exe', '-C', str(root)]
    package = load(root, package_path)
    assert package['accepted'] is False and package['product_P32_authorized'] is False
    for key in ['source_truth_decision','source_bindings','change_ledger','corpus','variant_index','matrix_schema','matrix','method_bindings']:
        verify_ref(root, package[key])
    matrix = load(root, package['matrix']['path'])
    import jsonschema
    jsonschema.Draft202012Validator(load(root, package['matrix_schema']['path'])).validate(matrix)
    rows = load(root, package['corpus']['path'])['variants']
    old = load(root, OLD + '/corpus-manifest.json')['variants']
    idx = load(root, OLD + '/variant-index.json')
    ids = [r['variant_id'] for r in rows]
    assert len(ids) == len(set(ids)) == 302
    assert ids == [r['variant_id'] for r in old] == [r['variant_id'] for r in idx['variants']]
    assert len({r['case_id'] for r in rows}) == 60
    assert [r['variant_id'] for r in matrix['variants']] == ids
    assert len({r['family_id'] for r in matrix['variants']}) == 10
    methods = load(root, package['method_bindings']['path'])
    assert [m['method_id'] for m in methods['bindings']] == ids
    sources = load(root, package['source_bindings']['path'])
    for source in sources:
        verify_ref(root, source)
    # Existing owner bindings must resolve to the actual frozen production subject.
    for owner in methods['owners'].values():
        data = subprocess.check_output(git + ['show', package['production_subject_revision'] + ':' + owner['path']])
        assert hashlib.sha256(data).hexdigest() == owner['sha256'], owner['path']
    identities = load(root, SEED + '/fixtures/identity-map.json')
    seed = load(root, SEED + '/fixtures/concave-region.json')
    catalogue = {c['id']:c for c in load(root, SEED + '/effective-case-catalog.json')['cases']}
    delta_ids = []
    selection_checks = []
    profiles = Counter()
    for row, previous, mrow, method in zip(rows, old, matrix['variants'], methods['bindings']):
        for ref in [row['input'], row['expected']] + list(nested_refs(row['wire'])):
            verify_ref(root, ref)
        assert row['expected'] == previous['expected'], row['variant_id']
        assert row['wire'] == previous['wire'], row['variant_id']
        assert mrow['case_id'] == row['case_id'] and mrow['family_id'] == catalogue[row['case_id']]['family']
        assert mrow['input_sha256'] == row['input']['sha256'] and mrow['expected_sha256'] == row['expected']['sha256']
        inp = load(root, row['input']['path'])
        exp = load(root, row['expected']['path'])
        assert method['accepted_case'] == catalogue[row['case_id']]
        assert method['input'] == row['input'] and method['expected'] == row['expected']
        assert method['frozen_predicates'] == exp['predicates']
        assert method['input_top_level_keys'] == sorted(inp)
        assert set(mrow['profiles']) == PROFILES
        for profile, cell in mrow['profiles'].items():
            applicable = profile != 'headless' or headless_expected(row, inp, exp)
            assert cell['applicability'] == ('APPLICABLE' if applicable else 'NOT_APPLICABLE')
            assert cell['result'] == ('NOT_RUN' if applicable else 'NOT_APPLICABLE')
            assert cell['method_id'] == row['variant_id']
            assert cell['reason'] and cell['execution_requirement']
            if profile == 'web-wasm' and cell['evidence_role'] == 'PRODUCT_OBSERVATION':
                assert 'Chrome' in cell['execution_requirement'] and 'WASM' in cell['execution_requirement']
            profiles[profile + ':' + cell['applicability']] += 1
        is_selection = row['case_id'] in {f'FC08-S{i:02}' for i in range(1,7)}
        if not is_selection:
            assert row['input'] == previous['input']
            continue
        try:
            geometry_agrees(inp, identities)
        except AssertionError as exc:
            raise AssertionError(f"{row['variant_id']}: {exc}") from exc
        expected = expected_selection(row['variant_id'], inp, identities)
        preds = {p.get('path'):p for p in exp['predicates']}
        assert [v['value'] for v in preds['selected']['expected']] == expected
        assert preds['primary']['expected'] == ({'value':expected[0]} if expected else None)
        assert preds['canonical_commit_delta']['expected'] == 0
        if row['input'] != previous['input']:
            delta_ids.append(row['variant_id'])
            old_inp = load(root, previous['input']['path'])
            differences = changed_paths(old_inp, inp)
            b_index = next((n for n,r in enumerate(inp['records']) if r['id']['value'] == identities['b']), None)
            if row['case_id'] == 'FC08-S05' and inp.get('setup'):
                assert inp['graph'] == {'G':{'kind':'Group','parent':None,'local':[1,0,0,1,0,0]}}
                assert inp['records'] == old_inp['records']
                assert differences == {'/graph/S','/graph/a','/graph/b','/graph/G'}
            elif row['case_id'] == 'FC08-S04':
                assert inp['region'] == seed['polygon_even_odd']
                for name, seed_key in [('a','child_A_rect'),('b','child_B_rect')]:
                    x,y,w,h = seed[seed_key]
                    assert inp['graph'][name]['local'] == [1,0,0,1,x,y]
                    assert inp['graph'][name]['local_rect'] == [0,0,w,h]
                assert inp['kind_geometry']['b'] == 'rectangle'
                allowed = {'/kind_geometry/b'}
                for name in ('a','b'):
                    allowed |= {f'/graph/{name}/local/{i}' for i in (4,5)} | {f'/graph/{name}/local_rect/{i}' for i in (2,3)}
                for n in (1,2):
                    allowed |= {f'/records/{n}/transform/{k}' for k in ('tx','ty')}
                    allowed |= {f'/records/{n}/content/shape/{k}' for k in ('width','height')}
                assert differences <= allowed
            else:
                assert differences == {f'/records/{b_index}/content/shape/shape_kind'}
        selection_checks.append(row['variant_id'])
    polygon = seed['polygon_even_odd']
    assert inside([10,10], polygon) and not inside([50,50], polygon)
    assert all(inside(p,polygon) for p in [[0,0],[100,0],[100,100],[0,100]])
    assert not inside([50,75], polygon)  # interior witness disproves full containment
    assert Fraction((45-41)**2,25) + Fraction((45-41)**2,25) == Fraction(32,25) > 1
    # AABB overlap [40,41]^2 exists, but no point in it reaches the ellipse.
    ledger = load(root, package['change_ledger']['path'])
    assert {l['variant_id'] for l in ledger} == set(delta_ids) and len(ledger) == len(delta_ids)
    for entry in ledger:
        verify_ref(root, entry['old_input']); verify_ref(root, entry['new_input'])
        assert {c['pointer'] for c in entry['changes']} == changed_paths(load(root, entry['old_input']['path']), load(root, entry['new_input']['path']))
    # All previously tracked bytes, not only selected evidence, are preserved.
    modified = subprocess.check_output(git + ['diff', BASE, '--name-only', '--diff-filter=CDMRTUXB'], text=True).splitlines()
    assert not modified, f'Previously tracked files changed: {modified}'
    return {'format':'fc08-input-amendment-audit-v1','status':'PASS_CANDIDATE_CONSISTENCY_ONLY',
            'authority_acceptance':False,'package_preflights_run':False,'SUT_invocations':[],
            'counts':{'logical_cases':60,'variants':302,'families':10,'profile_cells':906,
                      'corrected_inputs':len(delta_ids),'unchanged_expected':302,'audited_selection_variants':len(selection_checks)},
            'profile_counts':dict(profiles), 'corrected_variant_ids':delta_ids,
            'selection_expectations_checked':selection_checks,
            'independent_geometry_witnesses':{'S02_nearest_ellipse_distance_squared':'32/25 > 1',
                                              'S04_section_corners_inside':True,'S04_interior_notch_witness':[50,75],
                                              'S04_A_hit':True,'S04_B_hit':False},
            'independence':'No generator, production codec/planner/normalizer/Runtime imported; independent literal identities and rational geometry. Expected files not regenerated.',
            'preservation':'git diff against63ea reports zero changes to all previously tracked files; sources/expected/oracles also individually hash verified.',
            'limitations':['Candidate audit is not control acceptance or product verification.',
                           'No revised package preflight, product runner or product RED execution before acceptance.']}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--package', default=PACKAGE)
    args = parser.parse_args()
    try:
        report = audit(args.repo, args.package)
        code = 0
    except (AssertionError, ValueError, OSError) as exc:
        report = {'status':'FAIL_CANDIDATE_AUDIT','finding':str(exc),'SUT_invocations':[]}
        code = 2
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        stream.write(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    raise SystemExit(code)


if __name__ == '__main__':
    main()
