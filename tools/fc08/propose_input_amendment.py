"""Materialize a review candidate; never accept it or execute a product SUT.

All old files are read-only. New outputs use exclusive creation or byte-identical
resume (never overwrite). Profile choices
are explicit method bindings, not an implicit runner skip policy.
"""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess

BASE = '63ea152b2944d21120c968d5e11e166d1096b44f'
PRODUCT_PACKAGE = 'f67b9db29090449799bd3e05ef7f1d269dab6d31'
SUBJECT = '427580ae13b60a39e88959b282cbffc695c9aabb'
OLD = '.aegis/materialized/FC-08/attempts/v0.2-obs01-corpus-bound'
NEW = '.aegis/materialized/FC-08/attempts/v0.3-control-remediation-02'
PKG = '.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.2'
SEED = '.aegis/materialized/FC-08/sources/accepted-seed/verification'
PROFILES = ('windows-native', 'web-wasm', 'headless')


def read(root, path):
    return json.loads((root / path).read_text(encoding='utf-8'))


def binding(root, path):
    data = (root / path).read_bytes()
    return {'path': path, 'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)}


def write(root, path, value):
    target = root / path
    target.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    if target.exists():
        if target.read_bytes() != payload:
            raise ValueError(f'Refusing to overwrite different existing artifact: {path}')
        return binding(root, path)
    with target.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(payload.decode('utf-8'))
    return binding(root, path)


def diff(old, new, pointer=''):
    if type(old) is not type(new):
        return [{'pointer': pointer, 'old': old, 'new': new}]
    if isinstance(old, dict):
        return ([d for k in old.keys() & new.keys() for d in diff(old[k], new[k], pointer + '/' + k)]
                + [{'pointer': pointer + '/' + k, 'old': old[k], 'operation': 'remove'} for k in sorted(old.keys() - new.keys())]
                + [{'pointer': pointer + '/' + k, 'new': new[k], 'operation': 'add'} for k in sorted(new.keys() - old.keys())])
    if isinstance(old, list):
        if len(old) != len(new):
            raise ValueError('This amendment may not add/remove input elements')
        return [d for i, (a, b) in enumerate(zip(old, new))
                for d in diff(a, b, pointer + '/' + str(i))]
    return [] if old == new else [{'pointer': pointer, 'old': old, 'new': new}]


# Closed, case-by-case method book. The boolean refers ONLY to the semantic-only
# headless profile. Native and Web still need their actual production owners.
METHODS = {
    'W01': (True, 'host-descriptor', 'Compile accepted IDL; compare independent tag table and inherited descriptor delta once (VR-C04).', 'canonical_engine'),
    'W02': (True, 'semantic-codec', 'Observe normal wire ingress/domain checks or the explicitly frozen model-to-writer direction.', 'canonical_engine'),
    'W03': (True, 'semantic-codec', 'Decode presence/duplicate fields and observe normal semantic ingress, without local History.', 'canonical_engine'),
    'W04': (True, 'semantic-codec', 'Observe model canonical writer and inherited malformed-wire ingress separately.', 'canonical_engine'),
    'W05': (True, 'semantic-codec', 'Feed bounded mutation payload to normal atomic semantic apply; compare canonical pre/post vector.', 'atomic_apply'),
    'W06': (True, 'semantic-codec', 'Apply Section property operations; observe unchanged descendant bags and accepted masks.', 'canonical_engine'),
    'W07': (True, 'inherited-codec', 'Run pinned inherited codec/snapshot/operation carriers, retaining exact bytes and state assertions.', 'snapshot_ingress'),
    'T01': (True, 'semantic-topology', 'Apply literal forward/inverse/redo through normal semantic ingress and inspect sorted records and commit deltas.', 'atomic_apply'),
    'T02': (True, 'semantic-topology', 'Apply explicit cross-parent placements/guards and inspect literal poststate plus common world geometry.', 'atomic_apply'),
    'T03': (True, 'semantic-topology', 'Apply nonadjacent release and its explicit inverse; inspect exact frontier and canonical local matrices.', 'atomic_apply'),
    'T04': (True, 'semantic-topology', 'Validate final parent graph through semantic ingress, including hierarchy rejection with zero mutation.', 'atomic_apply'),
    'T05': (True, 'semantic-topology', 'Pause semantic prepare, apply frozen intervening operations, then validate guarded atomic apply.', 'canonical_engine'),
    'T06': (True, 'semantic-topology', 'Apply explicit order-only/relocation payloads and inverse records; no EditorHistory cursor is asserted.', 'atomic_apply'),
    'T07': (False, 'command-checkpoints', 'Observe full canonical+History+cursor/redo vector at real command precommit reservation checkpoints.', 'existing_command_seam'),
    'H01': (False, 'command-history', 'Drive real forward/Undo/Redo with fresh OperationIds and actual entry/cursor timeline.', 'real_History'),
    'H02': (False, 'command-history', 'Drive real Undo conflicts and retained leaf edits through the saved-entry owner.', 'real_History'),
    'H03': (False, 'command-budget', 'Export real F and U and observe local History admission; raw codec size alone is not this variant.', 'existing_command_seam'),
    'H04': (False, 'command-budget', 'Full-operation recipes require both size and real command admission; legal sublimit-only carriers have a separate explicit binding.', 'existing_command_seam'),
    'H05': (False, 'command-reservation', 'Inject actual History/outcome reservation failure and observe unchanged redo/cursor plus release-once.', 'real_History'),
    'H06': (False, 'command-prepared-plan', 'Pause real prepared command ticket; resume with original F/guards and appropriately rebuilt S1/U/reservation.', 'existing_command_seam'),
    'H07': (False, 'command-budget', 'Construct actual delete/restore closure and require inverse budget before local command commit.', 'existing_command_seam'),
    'H08': (False, 'command-history', 'Observe idempotency/NoChange through real command and History owner, not semantic apply alone.', 'real_History'),
    'G01': (True, 'semantic-common-geometry', 'Replay literal affine reparent and inverse; observe canonical matrices and common derived world origin without EditorHistory.', 'canonical_engine'),
    'G02': (False, 'boundary-gesture', 'Drive accepted boundary handle intent and observe Section-only write plus unchanged descendants.', 'existing_command_seam'),
    'G03': (False, 'pose-command', 'Real PreserveWorld intent needs conservative structural bounds; literal common composition has a separate binding.', 'existing_command_seam'),
    'G04': (False, 'selection-pose-command', 'Drive selected-root Move/Rotate/Scale with actual root normalization and descendant common geometry.', 'existing_command_seam'),
    'G05': (False, 'pose-command', 'Drive actual PoseConstruction command rejection/success; do not impose its quality limit on raw codec/replay.', 'existing_command_seam'),
    'G06': (False, 'pose-command-history', 'Construct actual F and retain exact preimage for real Undo/repeated F/U; math-only agreement is partial coverage.', 'real_History'),
    'S01': (False, 'runtime-selection', 'Drive lasso and existing scope owner; Section own-boundary containment suppresses descendants.', 'existing_selection'),
    'S02': (False, 'runtime-selection', 'Drive partial lasso through real exact-geometry hit path; AABB candidates alone must not select ellipse.', 'existing_selection'),
    'S03': (False, 'runtime-selection', 'Drive nested containment and unopened/entered Group scope, retaining inherited root projection.', 'existing_selection'),
    'S04': (False, 'runtime-selection', 'Drive authored concave polygon against literal rectangles; corners-only containment is insufficient.', 'existing_selection'),
    'S05': (False, 'runtime-selection', 'Drive paint click priorities and supported object-list identity route, without fabricated empty-Group paint.', 'existing_selection'),
    'S06': (False, 'runtime-selection', 'Drive real Down/Move/Shift release/Up or Cancel schedule and inspect eligibility, union, primary and restored selection.', 'existing_selection'),
    'V01': (False, 'scene-projection', 'Observe published Scene draw/query with offscreen own boundary and visible descendant.', 'Scene_publication'),
    'V02': (False, 'scene-render', 'Observe actual draw records and deterministic interior pixel for own opacity/fill without inherited child opacity.', 'Scene_publication'),
    'V03': (False, 'scene-projection', 'Observe old/new geometry, damage, aggregates, external Connector and query basis after actual publication.', 'Scene_publication'),
    'V04': (False, 'scene-recovery', 'Commit once, fail real Scene prepare phases and drain finite recovery to newer coherent generation.', 'Scene_publication'),
    'V05': (False, 'scene-resources', 'Delay actual title layout/resource results, change binding and observe projection-only refresh.', 'Scene_publication'),
    'C01': (False, 'frame-preview', 'Observe every real preview/publication handoff and finite positive recovery; synthetic bad traces only test oracle sensitivity.', 'frame_presentation'),
    'C02': (False, 'frame-preview', 'Observe common move vs boundary-only preview on descendant render/query basis.', 'preview_handoff'),
    'C03': (False, 'frame-preview', 'Delay g+1 publication and recover g+2; observe actual preview retirement at covering handoff.', 'preview_handoff'),
    'C04': (False, 'selection-callback', 'Change current selection after commit, then deliver real old receipt/Scene callbacks and inspect selection/scope.', 'preview_handoff'),
    'C05': (False, 'selection-callback', 'Detach/remount initiating view and deliver callback with old target/epoch; observe new view independently.', 'preview_handoff'),
    'L01': (False, 'owner-lifecycle', 'Drive admitted pointer terminal/Cancel/capture-loss schedules on the mounted owner with canonical+History observations.', 'owner_pump'),
    'L02': (False, 'owner-identities', 'Submit exact 128-bit IDs and stale target/pointer/selection identities at real owner admission.', 'owner_admission'),
    'L03': (False, 'owner-receipts', 'Drive scoped request retry/collision/eviction through real request ledger and observe at-most-once effects.', 'control_queue'),
    'L04': (False, 'owner-bounds', 'Fill real bounded queue/outcome slots; observe protected terminal delivery and zero mutation on denied reservation.', 'control_queue'),
    'L05': (False, 'owner-outcomes', 'Observe actual canonical and projection receipt axes plus retained old API meanings.', 'owner_admission'),
    'L06': (False, 'owner-close', 'Submit owned values from non-owner ingress and drive close/detach during queue/staging at real shared write lane.', 'owner_pump'),
    'K01': (True, 'snapshot-bootstrap', 'Decode actual inherited snapshots through explicit legacy import/bootstrap, comparing frozen fields and bytes.', 'bootstrap_owner'),
    'K02': (True, 'snapshot-capabilities', 'Write/read actual snapshot3 with derived Section/Triangle/resource capabilities and canonical digest.', 'snapshot_ingress'),
    'K03': (True, 'snapshot-bootstrap', 'Feed malformed capability/tuple snapshots to actual bootstrap; inspect rejection before partial Ready.', 'bootstrap_owner'),
    'K04': (True, 'journal-replay', 'Read checkpoint then replay journal through actual capability-limited ingress, retaining committed prefix.', 'replay_without_History'),
    'K05': (True, 'journal-replay', 'Replay real accepted17/18 without any EditorHistory owner or local reservation, inspecting ledger/state.', 'replay_without_History'),
    'K06': (True, 'snapshot-journal-writer', 'Use explicit semantic release and observe derived checkpoint capabilities, separate journal and writer admission.', 'replay_without_History'),
    'P01': (False, 'public-consumer', 'Compile ordinary public consumer and Debug UI paths; require independent outcomes and real History through typed owner seam.', 'public_facade'),
    'P02': (False, 'actual-host-bridge', 'Run actual native and Chrome/WASM bridge adapters with frozen coordinate/identity/IME facts; mocks or Node alone cannot cover.', 'public_facade'),
    'P03': (False, 'public-removal', 'Drive real public removal classifier/command for default Delete and explicit mixed Release atomically.', 'public_facade'),
    'P04': (False, 'inherited-owner-regression', 'Run exact inherited regressions on declared real owners; only semantic codec carriers admit semantic-only headless.', 'public_facade'),
}


def choose_method(row, inp, exp):
    case = row['case_id'].removeprefix('FC08-')
    headless, seam, rationale, owner = METHODS[case]
    role = 'PRODUCT_OBSERVATION'
    if case == 'W01':
        role = 'SHARED_HOST_TOOL_PROOF'
    elif case == 'H04' and {p.get('path') for p in exp['predicates']} == {'independent_size_predicate'}:
        if 'recipe' not in inp:
            raise ValueError('Sublimit binding requires a concrete raw-carrier recipe')
        headless, seam, owner = True, 'semantic-size-only', 'canonical_engine'
        rationale = 'Frozen full variant asserts only independently metered raw-carrier size. No command_admitted predicate; E03/VR-C02 separates this from F/U and History admission.'
    elif case == 'G03' and {'parent', 'child', 'point', 'expected_point'} <= inp.keys() and 'intent' not in inp:
        headless, seam = True, 'common-geometry-literal'
        rationale = 'Frozen variant is literal parent/child composition and transformed point only, with no command/bounds/History assertions. Observe actual common Scene geometry helper.'
    elif case == 'G05' and inp.get('role') == 'independent-quality-predicate-control':
        headless, seam, role = True, 'independent-oracle-control', 'SHARED_ORACLE_CONTROL_ONLY'
        rationale = 'Manifest explicitly lists this variant as a qualification control, not a product positive. Evaluate frozen quality predicate once; no SUT or product success credit.'
    elif case == 'P04' and inp.get('exact_inherited_binding') == 'touched-codec':
        headless, seam, owner = True, 'inherited-codec', 'snapshot_ingress'
        rationale = 'Exact inherited binding is codec-only. Execute pinned codec carriers in semantic-only headless; History/control/selection variants retain their actual owners.'
    return headless, seam, rationale, owner, role


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    args = parser.parse_args()
    root = args.repo.resolve()
    git = ['D:/Softwares/Git/cmd/git.exe', '-C', str(root)]
    for revision in (BASE, PRODUCT_PACKAGE, SUBJECT, '0d3b10bba1ac4a31d669dd4a45f94975ec24f3c3'):
        subprocess.run(git + ['merge-base', '--is-ancestor', revision, 'HEAD'], check=True)
    manifest = read(root, OLD + '/corpus-manifest.json')
    catalog = read(root, SEED + '/effective-case-catalog.json')
    obligations = read(root, SEED + '/reviewed-obligations.json')
    by_case = {c['id']: c for c in catalog['cases']}
    assert set(METHODS) == {c.removeprefix('FC08-') for c in by_case}
    seed = read(root, SEED + '/fixtures/concave-region.json')
    ledger = []
    for row in manifest['variants']:
        original = read(root, row['input']['path'])
        if row['case_id'] not in {f'FC08-S{i:02}' for i in range(1, 7)}:
            continue
        changed = deepcopy(original)
        if row['case_id'] == 'FC08-S05' and original.get('setup'):
            decision_id = 'TRUTH-S05-EMPTY-GRAPH'
            assert len(original['records']) == 1 and original['records'][0]['kind_id'] == 9
            changed['graph'] = {'G': {'kind': 'Group', 'parent': None, 'local': [1,0,0,1,0,0]}}
        elif row['case_id'] == 'FC08-S04':
            decision_id = 'TRUTH-S04-SEED-RECTANGLES'
            for name, key, identity in [('a', 'child_A_rect', 9), ('b', 'child_B_rect', 10)]:
                x, y, width, height = seed[key]
                changed['graph'][name]['local'][4:] = [x, y]
                changed['graph'][name]['local_rect'] = [0, 0, width, height]
                changed['kind_geometry'][name] = 'rectangle'
                record = next(r for r in changed['records'] if r['id']['value'] == f'{identity:032x}')
                record['transform'].update(tx=x, ty=y)
                record['content']['shape'].update(shape_kind=1, width=width, height=height)
        else:
            decision_id = 'TRUTH-SHARED-ELLIPSE'
            record = next(r for r in changed['records'] if r['id']['value'] == f'{10:032x}')
            assert original['kind_geometry']['b'] == 'ellipse'
            assert record['content']['shape']['shape_kind'] == 1
            record['content']['shape']['shape_kind'] = 2
        old_input = deepcopy(row['input'])
        new_input = write(root, f"{NEW}/corpus/{row['variant_id']}/input.json", changed)
        ledger.append({'variant_id': row['variant_id'], 'decision_id': decision_id,
                       'old_input': old_input, 'new_input': new_input,
                       'expected_unchanged': deepcopy(row['expected']), 'changes': sorted(diff(original, changed), key=lambda x:x['pointer'])})
        row['input'] = new_input
    manifest.update(format='fc08-concrete-corpus-v3-review-candidate', status='PROPOSED_PENDING_ACCEPTANCE',
                    parent_corpus=binding(root, OLD + '/corpus-manifest.json'),
                    amendment_package=PKG + '/package.json')
    corpus_ref = write(root, NEW + '/corpus-manifest.json', manifest)
    ledger_ref = write(root, NEW + '/input-change-ledger.json', ledger)
    index_ref = binding(root, OLD + '/variant-index.json')
    source_paths = [SEED + '/effective-case-catalog.json', SEED + '/reviewed-obligations.json',
                    SEED + '/execution.md', SEED + '/fixtures/concave-region.json',
                    'schema/axiom/v1/registry/shape_kind_registry_v1.yaml',
                    'tools/fc08/materialize_corpus.py', 'tests/fc08/reference/fixtures.py',
                    'tests/fc08/reference/wire.py', 'tests/fc08/reference/oracles.py',
                    'tests/fc08/reference/variants.py',
                    '.aegis/materialized/FC-08/sources/accepted-exports/P20.md',
                    '.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-002/terminal-result.json']
    sources = []
    for path in source_paths:
        ref = binding(root, path)
        ref['history'] = subprocess.check_output(git + ['log', '-4', '--format=%H %s', '--', path], text=True).splitlines()
        sources.append(ref)
    sources_ref = write(root, NEW + '/source-bindings.json', sources)
    decision_ref = write(root, NEW + '/source-of-truth-decision.json', {
        'format': 'fc08-source-truth-proposal-v1', 'status': 'PROPOSED_PENDING_ACCEPTANCE',
        'accepted': False, 'owner': 'P20 Verification / control review',
        'sources': sources_ref, 'change_ledger': ledger_ref,
        'decisions': [
            {'id': 'TRUTH-SHARED-ELLIPSE', 'disposition': 'PROPOSE_VERSIONED_INPUT_CORRECTION',
             'rule': 'For S01/S02/S03/nonempty S05/S06, preserve authored kind_geometry.b=ellipse and correct canonical record shape_kind to2.',
             'basis': 'selection_variant authors the shared ellipse explicitly; S02 comment and exact AABB-vs-curve obligation distinguish it from rectangle. objects(graph) loses the specialization by using generic record Shape default1. Other shared cases do not constrain a different primitive.',
             'independent_numeric_witness': {'ellipse_bbox': [40,40,10,10], 'lasso_max': [41,41], 'nearest_normalized_squared_distance': '32/25 > 1'},
             'ambiguity_policy': 'This source-backed choice is a proposal, not inferred authority acceptance.'},
            {'id': 'TRUTH-S04-SEED-RECTANGLES', 'disposition': 'PROPOSE_VERSIONED_INPUT_CORRECTION',
             'rule': 'Use concave-region.json original rectangle bounds: a[9,9,2,2], b[49,49,2,2], graph and canonical records; both shape_kind1.',
             'basis': 'Accepted catalog explicitly names that literal seed; generator imports polygon but substitutes shared shapes/positions. Literal seed is the case-specific authored truth proposed here, ahead of generic generator defaults.',
             'independent_numeric_witness': 'A center(10,10) is inside; B rectangle[49,49,2,2] is strictly inside excluded notch x40..60,y40..110. Section corners are inside but notch crosses interior.'},
            {'id': 'TRUTH-S05-EMPTY-GRAPH', 'disposition': 'PROPOSE_VERSIONED_INPUT_CORRECTION',
             'rule': 'For the two empty-Group variants, replace stale graph S/a/b with the authored empty G matching the unchanged canonical records.',
             'basis': 'Catalog requires empty Group. Generator reassigns local graph to G but f.graph retains the earlier dictionary. Its canonical records already contain only G. Correct fixture graph metadata; leave setup and expected selection unchanged.'}
        ],
        'expected_policy': 'All 302 expected bindings and bytes unchanged. No expected file recomputed, no old oracle edited, no production SUT consulted.',
        'unused_metadata_policy': 'Leave unused kind_geometry entries in the two empty-Group variants unchanged; absent graph nodes never acquire paint geometry.',
        'gate': 'Requires explicit control acceptance of this truth proposal and the complete matrix before revised P31 preflights or product RED.'})
    owners = read(root, '.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-002/implementation-design-preflight.json')['source_bindings']
    common_geometry = 'runtime/scene/src/derived_geometry.cpp'
    owners['common_geometry'] = dict(binding(root, common_geometry), source_revision=SUBJECT)
    methodbook = []
    matrix = []
    for row in manifest['variants']:
        inp = read(root, row['input']['path'])
        exp = read(root, row['expected']['path'])
        headless, seam, reason, owner, role = choose_method(row, inp, exp)
        if seam in ('semantic-common-geometry', 'common-geometry-literal'):
            owner = 'common_geometry'
        method_id = row['variant_id']
        methodbook.append({'method_id': method_id, 'case_id': row['case_id'],
                           'accepted_case': by_case[row['case_id']],
                           'family_method': next(f['execution_method'] for f in obligations['families'] if f['id'] == by_case[row['case_id']]['family']),
                           'input': row['input'], 'expected': row['expected'],
                           'input_top_level_keys': sorted(inp), 'frozen_predicates': exp['predicates'],
                           'method': seam, 'rationale': reason, 'owner_binding': owner,
                           'headless_applicable': headless, 'evidence_role': role})
        profiles = {}
        for profile in PROFILES:
            applicable = profile != 'headless' or headless
            execution = ('one exact shared host IDL/descriptor run; VR-C04 reuse, not browser behavior' if role == 'SHARED_HOST_TOOL_PROOF' else
                         'one exact independent host oracle sensitivity control; no product credit' if role == 'SHARED_ORACLE_CONTROL_ONLY' else
                         'actual Chrome browser loading compiled WASM SUT; Node/mock not sufficient' if profile == 'web-wasm' else
                         'actual Windows native compiled SUT' if profile == 'windows-native' else
                         'actual semantic/common geometry SUT without UI, EditorHistory or local command reservation')
            profiles[profile] = {
                'applicability': 'APPLICABLE' if applicable else 'NOT_APPLICABLE',
                'result': 'NOT_RUN' if applicable else 'NOT_APPLICABLE',
                'environment': 'NOT_ASSESSED' if applicable else 'NOT_REQUIRED',
                'reason': reason + (' Semantic-only headless lacks this full observation owner; partial math/raw coverage cannot count as the variant.' if not applicable else ''),
                'execution_requirement': execution if applicable else 'No invocation for this profile; Windows/Web obligations remain mandatory.',
                'method_id': method_id, 'evidence_role': role}
        matrix.append({'variant_id': row['variant_id'], 'case_id': row['case_id'],
                       'family_id': by_case[row['case_id']]['family'],
                       'input_sha256': row['input']['sha256'], 'expected_sha256': row['expected']['sha256'],
                       'profiles': profiles})
    book_ref = write(root, NEW + '/profile-method-bindings.json', {
        'format': 'fc08-profile-method-bindings-v1', 'status': 'PROPOSED_PENDING_ACCEPTANCE',
        'catalog': binding(root, SEED + '/effective-case-catalog.json'),
        'accepted_method_source': binding(root, SEED + '/execution.md'),
        'owners': owners, 'bindings': methodbook,
        'not_a_runner': True, 'seam_policy': 'Existing owner source locations identify real observation boundaries, not claims that future Section behavior/API/runner already exists.'})
    cell_schema = {'type': 'object', 'additionalProperties': False,
                   'required': ['applicability','result','environment','reason','execution_requirement','method_id','evidence_role'],
                   'properties': {
                       'applicability': {'enum': ['APPLICABLE','NOT_APPLICABLE']},
                       'result': {'enum': ['NOT_RUN','NOT_APPLICABLE','BLOCKED_ENVIRONMENT','PASS','FAIL']},
                       'environment': {'enum': ['NOT_ASSESSED','NOT_REQUIRED','AVAILABLE','MISSING']},
                       **{k: {'type': 'string','minLength': 1} for k in ['reason','execution_requirement','method_id','evidence_role']}},
                   'allOf': [
                       {'if': {'properties': {'applicability': {'const': 'NOT_APPLICABLE'}}},
                        'then': {'properties': {'result': {'const': 'NOT_APPLICABLE'}, 'environment': {'const':'NOT_REQUIRED'}}},
                        'else': {'properties': {'result': {'not': {'const':'NOT_APPLICABLE'}}, 'environment': {'not': {'const':'NOT_REQUIRED'}}}}},
                       {'if': {'properties': {'result': {'const':'BLOCKED_ENVIRONMENT'}}},
                        'then': {'properties': {'applicability': {'const':'APPLICABLE'}, 'environment': {'const':'MISSING'}}}}
                   ]}
    schema_ref = write(root, NEW + '/profile-applicability.schema.json', {
        '$schema': 'https://json-schema.org/draft/2020-12/schema', '$id': 'urn:axiom:fc08:profile-applicability:1',
        'type': 'object', 'additionalProperties': False,
        'required': ['format','status','variants','method_bindings'],
        'properties': {
            'format': {'const': 'fc08-profile-applicability-v1'},
            'status': {'const': 'PROPOSED_PENDING_ACCEPTANCE'},
            'method_bindings': {'type':'object'},
            'variants': {'type': 'array','minItems':302,'maxItems':302,'items': {
                'type':'object','additionalProperties':False,
                'required':['variant_id','case_id','family_id','input_sha256','expected_sha256','profiles'],
                'properties': {**{k:{'type':'string','minLength':1} for k in ['variant_id','case_id','family_id','input_sha256','expected_sha256']},
                               'profiles': {'type':'object','additionalProperties':False,'required':list(PROFILES),
                                            'properties':{p:cell_schema for p in PROFILES}}}}}}
    })
    matrix_ref = write(root, NEW + '/profile-applicability.json', {
        'format': 'fc08-profile-applicability-v1', 'status': 'PROPOSED_PENDING_ACCEPTANCE',
        'method_bindings': book_ref, 'variants': matrix})
    amendment = {
        'format': 'fc08-verification-input-amendment-v1', 'package_id': 'FC-08-P20-P31-INPUT-AMENDMENT-v0.2',
        'status': 'PROPOSED_PENDING_ACCEPTANCE', 'accepted': False,
        'authorized_scope': 'P20/P31 input-truth and profile-binding remediation only; no product P32 or registry amendment.',
        'parent_product_package': dict(binding(root, '.aegis/packages/FC-08-PRODUCT-P31-v0.1/package.json'), revision=PRODUCT_PACKAGE),
        'bootstrap_receipt_revision': BASE, 'production_subject_revision': SUBJECT,
        'supersedes_unaccepted_candidate': binding(root, '.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.1/package.json'),
        'materialized_ref': 'refs/heads/codex/fc08-p20-p31-closure',
        'source_truth_decision': decision_ref, 'source_bindings': sources_ref, 'change_ledger': ledger_ref,
        'corpus': corpus_ref, 'variant_index': index_ref,
        'matrix_schema': dict(schema_ref, schema_version='1'), 'matrix': dict(matrix_ref, matrix_version='1'),
        'method_bindings': book_ref,
        'counts': {'logical_cases':60,'unique_variants':302,'obligation_families':10,'profile_cells':906,'corrected_inputs':len(ledger)},
        'unchanged_expected_count':302, 'product_P32_authorized':False,
        'preservation_policy': 'Old materialization, all expected files, oracle implementations, accepted source catalog and historical results remain byte-identical.',
        'acceptance_required': ['Explicit P20 truth decision acceptance','Explicit acceptance of exact matrix/schema/method bindings'],
        'after_acceptance_sequence': ['corpus consistency and independence audits','PackageBindingPreflight at exact revised package revision','EvidenceContractPreflight at same revision','ImplementationDesignPreflight','actual product runner and tests at exact revision','FC08-RED-01 against unmodified production subject','independent P31 control review'],
        'preflight_policy': 'This proposal does not silently rebind old f67 package locks. Revised exact package and successful preflights remain required after acceptance.'}
    ref = write(root, PKG + '/package.json', amendment)
    print(json.dumps({'status':'PROPOSED_PENDING_ACCEPTANCE', 'amendment':ref, 'counts':amendment['counts']}, indent=2))


if __name__ == '__main__':
    main()
