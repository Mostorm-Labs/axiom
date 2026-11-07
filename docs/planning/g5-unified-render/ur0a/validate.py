#!/usr/bin/env python3
"""Bounded UR0a static validator. No Runtime, browser or dependency installation.

Only the JSON Schema vocabulary used by metrics.schema.json is implemented.
Unknown keywords fail closed. This is not a general metaschema implementation.
"""
import argparse
import hashlib
import json
import math
import re
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
RESULTS = ROOT / '.aegis/results/G5-UR0a'
START = 'a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e'
PACKAGE_SHA = 'cbbeec9c87fcd67f67498011c910778060dd2d893fa821b094d09f02e708905f'
KEYWORDS = set('''$schema $id $defs $ref $comment title description type properties
required additionalProperties items minItems maxItems enum const pattern minLength
maxLength minimum maximum anyOf allOf if then'''.split())


class Invalid(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise Invalid(message)


def load(path):
    def reject_constant(value):
        raise Invalid('non-JSON constant: ' + value)
    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), parse_constant=reject_constant,
                      object_pairs_hook=object_pairs)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args, binary=False):
    out = subprocess.check_output(['git', *args], cwd=ROOT)
    return out if binary else out.decode().strip()


def validate_vocabulary(schema, root):
    require(isinstance(schema, dict), 'schema node must be object')
    require(set(schema) <= KEYWORDS, 'unsupported schema keywords: ' + str(set(schema) - KEYWORDS))
    if '$ref' in schema:
        require(schema['$ref'].startswith('#/$defs/'), 'only local $defs refs supported')
        require(schema['$ref'].split('/')[-1] in root['$defs'], 'unresolved schema ref')
    for key in ('properties', '$defs'):
        for value in schema.get(key, {}).values():
            validate_vocabulary(value, root)
    for key in ('items', 'if', 'then'):
        if key in schema:
            validate_vocabulary(schema[key], root)
    for key in ('anyOf', 'allOf'):
        for value in schema.get(key, []):
            validate_vocabulary(value, root)
    if 'required' in schema:
        require(len(schema['required']) == len(set(schema['required'])), 'duplicate required field')
    if schema.get('additionalProperties') is False:
        require(set(schema.get('required', [])) <= set(schema.get('properties', {})),
                'required field not declared')


def shape(value, schema, root, path='$'):
    if '$ref' in schema:
        ref = schema['$ref'].split('/')[-1]
        shape(value, root['$defs'][ref], root, path)
        if ref == 'u64':
            require(int(value) <= 2**64 - 1, path + ' u64 overflow')
    checks = {
        'object': lambda: isinstance(value, dict),
        'array': lambda: isinstance(value, list),
        'string': lambda: isinstance(value, str),
        'integer': lambda: type(value) is int,
        'number': lambda: type(value) in (int, float) and math.isfinite(value),
        'boolean': lambda: type(value) is bool,
        'null': lambda: value is None,
    }
    if 'type' in schema:
        require(schema['type'] in checks, 'unknown type')
        require(checks[schema['type']](), path + ' expected ' + schema['type'])
    if 'const' in schema:
        require(type(value) is type(schema['const']) and value == schema['const'], path + ' const mismatch')
    if 'enum' in schema:
        require(any(type(value) is type(v) and value == v for v in schema['enum']), path + ' enum mismatch')
    if isinstance(value, dict):
        require(set(schema.get('required', [])) <= set(value), path + ' required fields missing')
        if schema.get('additionalProperties') is False:
            require(set(value) <= set(schema.get('properties', {})), path + ' unknown fields')
        for key, subschema in schema.get('properties', {}).items():
            if key in value:
                shape(value[key], subschema, root, path + '.' + key)
    if isinstance(value, list):
        require(len(value) >= schema.get('minItems', 0), path + ' too few items')
        require(len(value) <= schema.get('maxItems', math.inf), path + ' too many items')
        if 'items' in schema:
            for i, item in enumerate(value):
                shape(item, schema['items'], root, f'{path}[{i}]')
    if isinstance(value, str):
        require(len(value) >= schema.get('minLength', 0), path + ' string too short')
        require(len(value) <= schema.get('maxLength', math.inf), path + ' string too long')
        if 'pattern' in schema:
            require(re.search(schema['pattern'], value) is not None, path + ' pattern mismatch')
    if type(value) in (int, float):
        require(math.isfinite(value), path + ' nonfinite number')
        require(value >= schema.get('minimum', -math.inf), path + ' below minimum')
        require(value <= schema.get('maximum', math.inf), path + ' above maximum')
    for subschema in schema.get('allOf', []):
        shape(value, subschema, root, path)
    if 'anyOf' in schema:
        errors = []
        for subschema in schema['anyOf']:
            try:
                shape(value, subschema, root, path)
                break
            except Invalid as error:
                errors.append(str(error))
        else:
            raise Invalid(path + ' no anyOf alternative: ' + '; '.join(errors))
    if 'if' in schema:
        try:
            shape(value, schema['if'], root, path)
        except Invalid:
            pass
        else:
            if 'then' in schema:
                shape(value, schema['then'], root, path)


def unique(rows, key):
    keys = [r[key] for r in rows]
    require(len(keys) == len(set(keys)), 'duplicate ' + key)
    return set(keys)


def ordered(row, fields, label):
    values = [int(row[k]) for k in fields if row[k] is not None]
    require(values == sorted(values), label + ' time order')


def semantics(record, route_ids):
    until = int(record['observed_until_ns'])
    iid = unique(record['interactions'], 'interaction_id')
    events, submit_ids, receipt_ids = {}, set(), set()
    for interaction in record['interactions']:
        name = interaction['interaction_id']
        unique(interaction['events'], 'event_id')
        for event in interaction['events']:
            require(event['event_id'] not in events, 'duplicate global event_id')
            events[event['event_id']] = (name, event)
            ordered(event, ['planned_ns', 'producer_send_ns', 'handler_enter_ns', 'handler_exit_ns'], 'event')
            if event['admission_end_ns'] is not None:
                require(event['handler_enter_ns'] is not None, 'admission without handler')
                ordered(event, ['handler_enter_ns', 'admission_end_ns', 'handler_exit_ns'], 'admission')
            if event['admission'] == 'REJECTED':
                require(record['run_outcome'] != 'COMPLETE', 'rejected input claimed successful run')
                require(event['reason'], 'rejected input missing reason')
                require(any(m['event_id'] == event['event_id'] and m['metric_id'] == 'FIRST_FEEDBACK'
                            and m['status'] == 'FAILED' for m in record['metrics']),
                        'rejected feedback disappeared from denominator')
        commit = interaction['commit']
        if commit['state'] == 'NO_OP':
            require(commit['operation_id'] is None and commit['commit_ns'] is None,
                    'no-op allocated new Operation/commit')
            require(commit['before_generation'] == commit['after_generation'], 'no-op changed generation')
        if commit['state'] == 'REJECTED':
            require(commit['operation_id'] is None and commit['commit_ns'] is None,
                    'uncommitted rejection contains commit')
        if commit['state'] == 'APPLIED':
            require(all(commit[k] is not None for k in ['operation_id', 'commit_revision',
                    'before_generation', 'after_generation', 'commit_ns']), 'applied commit missing identity')
            require(int(commit['after_generation']) > int(commit['before_generation']), 'apply did not advance')
            require(commit['return_class'] in ('COMMITTED', 'POST_COMMIT_ERROR'), 'commit reported uncommitted')
        scene = interaction['scene']
        ordered(scene, ['start_ns', 'published_ns'], 'Scene')
        if scene['state'] == 'PUBLISHED':
            require(scene['published_ns'] is not None and scene['scene_generation'] is not None,
                    'published Scene missing identity/time')
        if interaction['kind'] == 'navigation':
            require(not interaction['canonical_receipts'], 'navigation invented canonical receipt')
            require(commit['state'] == 'NOT_APPLICABLE', 'navigation invented semantic commit')
        for submit in interaction['submits']:
            require(submit['submit_id'] not in submit_ids, 'duplicate submit id')
            submit_ids.add(submit['submit_id'])
            require(int(submit['at_ns']) <= until, 'submit beyond observation')
            if submit['qualification'] == 'EFFECTIVE':
                require(submit['valid_region_proof'] != 'MISSING', 'effective submit without region proof')
                require(submit['submitted_revision'] == submit['requested_revision'], 'stale effective submit')
                require(submit['target_ids'], 'effective submit without target')
        for receipt in interaction['canonical_receipts'] + interaction['resource_completions']:
            require(receipt['completion_id'] not in receipt_ids, 'duplicate completion id')
            receipt_ids.add(receipt['completion_id'])
            require(int(receipt['at_ns']) <= until, 'completion beyond observation')
            if receipt in interaction['canonical_receipts']:
                require(receipt['kind'] == 'CANONICAL_QUALIFIED', 'resource event in canonical receipts')
            if receipt['kind'] == 'CANONICAL_QUALIFIED' and receipt['qualification'] == 'QUALIFIED':
                require(receipt['matches_current_basis'], 'stale completion qualified current basis')
                require(all(receipt['basis'][k] is not None for k in receipt['basis'] if k not in
                            ('preview_revision', 'body_id', 'mask_basis')), 'qualified receipt missing identity')
                matches = [s for s in interaction['submits'] if s['qualification'] == 'EFFECTIVE'
                           and s['basis'] == receipt['basis'] and int(s['at_ns']) <= int(receipt['at_ns'])]
                require(matches, 'qualified completion has no matching prior effective submit')
        spans = interaction['spans']; ids = unique(spans, 'span_id')
        for span in spans:
            require(int(span['exclusive_ns']) <= int(span['inclusive_ns']), 'exclusive exceeds inclusive')
            require(span['parent_id'] is None or span['parent_id'] in ids, 'unknown parent span')
            if span['clock_domain'] == 'wasm_local_duration':
                require(span['start_ns'] is None and span['end_ns'] is None,
                        'local-only duration exposed as browser timestamp')
            else:
                require(span['start_ns'] is not None and span['end_ns'] is not None, 'browser span no bounds')
                require(int(span['end_ns']) - int(span['start_ns']) == int(span['inclusive_ns']), 'span length')
    unique(record['jobs'], 'job_id')
    attempt_ids, decision_ids, leaf_ids = set(), set(), set()
    for job in record['jobs']:
        require(job['interaction_id'] in iid, 'job references unknown interaction')
        for attempt in job['attempts']:
            for key, seen in [('attempt_id', attempt_ids)]:
                require(attempt[key] not in seen, 'duplicate ' + key); seen.add(attempt[key])
            require(attempt['route_id'] in route_ids, 'unknown attempt route')
            ordered(attempt, ['start_ns', 'end_ns'], 'attempt')
            require(attempt['outcome'] == 'MISSING' or attempt['end_ns'] is not None,
                    'ended attempt without terminal time')
            dec = attempt['decision']
            require(dec['decision_id'] not in decision_ids, 'duplicate decision id')
            decision_ids.add(dec['decision_id'])
            for leaf in attempt['leaves']:
                require(leaf['leaf_id'] not in leaf_ids, 'duplicate leaf id'); leaf_ids.add(leaf['leaf_id'])
                require(leaf['route_id'] in route_ids, 'unknown leaf route')
                require(leaf['decision_id'] == dec['decision_id'], 'foreign decision-to-leaf link')
                if dec['outcome'] == 'PreparedGpuDraw' and leaf['work_kind'] == 'coverage':
                    require(leaf['actual_target'] == 'GPU', 'GPU request recorded as actual CPU coverage')
                if leaf['actual_target'] in ('GPU', 'CPU', 'RECORDING'):
                    require(leaf['target_id'], 'actual target missing id')
    if any(record['loss'][k] for k in record['loss']):
        require(record['evidence_status'] != 'COMPLETE', 'dropped/unknown evidence claimed COMPLETE')
    unique(record['metrics'], 'observation_id')
    for metric in record['metrics']:
        require(metric['interaction_id'] in iid, 'metric unknown interaction')
        if metric['event_id'] is not None:
            require(metric['event_id'] in events, 'metric unknown event')
            require(events[metric['event_id']][0] == metric['interaction_id'], 'metric foreign event')
        state = metric['status']
        if state == 'COMPLETE':
            require(int(metric['end_ns']) >= int(metric['start_ns']), 'negative duration')
            require(int(metric['end_ns']) - int(metric['start_ns']) == int(metric['value_ns']), 'duration mismatch')
            require(int(metric['end_ns']) <= until, 'metric beyond observation')
        elif state == 'RIGHT_CENSORED':
            require(until - int(metric['start_ns']) == int(metric['lower_bound_ns']), 'wrong censor lower bound')
        else:
            require(metric['value_ns'] is None and metric['end_ns'] is None
                    and metric['lower_bound_ns'] is None, 'missing/failure treated as timing')
            require(metric['reason'], 'noncomplete metric missing reason')
    unique(record['summaries'], 'metric_id')
    require({m['metric_id'] for m in record['metrics']} == {s['metric_id'] for s in record['summaries']},
            'metric/summary set mismatch')
    fields = {'COMPLETE': 'complete_count', 'FAILED': 'failed_count', 'RIGHT_CENSORED': 'censored_count',
              'MISSING': 'missing_count', 'NOT_APPLICABLE': 'not_applicable_count'}
    for stat in record['summaries']:
        group = [m for m in record['metrics'] if m['metric_id'] == stat['metric_id']]
        require(stat['planned_count'] == len(group), 'failure removed from denominator')
        counts = Counter(m['status'] for m in group)
        for state, field in fields.items():
            require(stat[field] == counts[state], 'status denominator mismatch: ' + field)
        values = sorted(int(m['value_ns']) for m in group if m['status'] == 'COMPLETE')
        for field, numerator in [('p50_ns', 50), ('p95_ns', 95), ('p99_ns', 99), ('max_ns', 100)]:
            expected = str(values[max(1, (numerator * len(values) + 99) // 100) - 1]) if values else None
            require(stat[field] == expected, 'wrong nearest-rank/empty quantile ' + field)
        tail = 'INCOMPLETE' if any(counts[k] for k in ('FAILED', 'RIGHT_CENSORED', 'MISSING')) else (
            'COMPLETE' if values else 'NO_SAMPLES')
        require(stat['tail_status'] == tail, 'tail hid missing/failure/censoring')


def check_schema():
    schema, cases = load(HERE/'metrics.schema.json'), load(HERE/'schema-examples.json')
    routes = {r['route_id'] for r in load(HERE/'routes.json')['routes']}
    validate_vocabulary(schema, schema)
    answers = []
    for expected, group in [(True, 'valid'), (False, 'invalid')]:
        for case in cases[group]:
            require(case['record']['record_origin'] == 'SCHEMA_EXAMPLE', 'example pretending measurement')
            error = None
            try:
                shape(case['record'], schema, schema)
                semantics(case['record'], routes)
            except Invalid as caught:
                error = str(caught)
            require((error is None) == expected, 'unexpected example result: ' + case['case_id'] + ' ' + str(error))
            answers.append({'case_id': case['case_id'], 'expected': 'ACCEPT' if expected else 'REJECT',
                            'result': 'PASS', 'rejection': error})
    # Quantile math independent of examples: p95 over 20 observations is the nineteenth.
    require([1 + (p * 20 + 99)//100 - 1 for p in (50,95,99)] == [10,19,20], 'nearest-rank self-check')
    return {'schema_vocabulary': 'bounded supported subset', 'accepted': len(cases['valid']),
            'rejected': len(cases['invalid']), 'cases': answers}


def check_routes():
    d = load(HERE/'routes.json'); refs = d['source_refs']; routes = d['routes']; allocations = d['allocations']
    require(d['starting_revision'] == START, 'route source revision')
    ids = unique(routes, 'route_id'); aids = unique(allocations, 'allocation_id'); source_cache = {}
    def verify_ref(ref):
        path = ref['path']
        if path not in source_cache:
            current = (ROOT/path).read_bytes(); original = git('show', START + ':' + path, binary=True)
            require(current == original, 'changed audited source ' + path)
            source_cache[path] = (current.decode().splitlines(), hashlib.sha256(current).hexdigest())
        lines, digest = source_cache[path]
        require(ref['revision'] == START and ref['file_sha256'] == digest, 'source hash/revision: ' + path)
        require(lines[ref['line']-1].strip() == ref['line_text'], 'source line mismatch: ' + path)
        if 'symbol_or_call' in ref:
            require(ref['symbol_or_call'] in lines[ref['line']-1], 'source symbol mismatch')
    for ref in refs.values():
        verify_ref(ref)
    for entry in d['public_entries'].values():
        require(all(ref in refs for ref in entry['refs']), 'unknown public path ref')
    for route in routes:
        require(route['public_entry'] in d['public_entries'], 'unknown public entry')
        require(route['classification'] in ('MIGRATE','KEEP_ORACLE','KEEP_LEGACY','KEEP_NON_INK','DEFERRED'),
                'classification')
        require(route['evidence_status'] in ('MISSING','DEFERRED'), 'static route fabricated COVERED')
        for key in ('owner','target_package','allowed_outcomes','job_decision_leaf_identity',
                    'positive_case','negative_case','reason','callsite_and_branch'):
            require(route.get(key), 'missing route ' + key)
        for hop in route['callsite_and_branch']:
            require(hop['ref'] in refs and hop['current_branch'], 'unknown/empty callsite branch')
        for aid in route['allocation_ids']:
            require(aid in aids, 'route unknown factory')
            alloc = next(a for a in allocations if a['allocation_id'] == aid)
            require(route['route_id'] in alloc['reverse_route_ids'], 'missing factory reverse edge')
    sites = set()
    for alloc in allocations:
        verify_ref(alloc['site']); sites.add((alloc['site']['path'], alloc['site']['line']))
        require(alloc['evidence_status'] == 'MISSING', 'static factory fabricated dynamic coverage')
        require(alloc['reverse_route_ids'] and alloc['positive_case'] and alloc['negative_case'], 'factory lacks callers/cases')
        for rid in alloc['reverse_route_ids']:
            require(rid in ids, 'unknown reverse route')
            require(alloc['allocation_id'] in next(r for r in routes if r['route_id'] == rid)['allocation_ids'],
                    'missing route forward allocation edge')
    pattern = re.compile(d['discovery']['factory_pattern']); found = set()
    for relative in d['discovery']['roots']:
        for path in sorted((ROOT/relative).rglob('*')):
            if not path.is_file() or 'tests' in path.parts or path.suffix not in ('.cpp','.cc','.cxx','.hpp','.h','.mm','.mjs','.js'):
                continue
            for line, text in enumerate(path.read_text().splitlines(), 1):
                if pattern.search(text):
                    found.add((path.relative_to(ROOT).as_posix(), line))
    require(found == sites, 'factory scan mismatch missing=' + str(sorted(found-sites)) + ' stale=' + str(sorted(sites-found)))
    require(len(found) == d['discovery']['factory_sites'], 'factory count')
    require(set(d['seed_branches']) <= {r['seed'] for r in routes}, 'seed branch omitted')
    require(d['dynamic_coverage'] == {'new_runs':0,'covered_routes':0,'status':'MISSING'}, 'dynamic fabricated')
    return {'routes':len(routes),'source_refs':len(refs),'factory_sites':len(sites),
            'source_files':len(source_cache),'new_dynamic_runs':0,'covered_routes':0,
            'scope':'explicit lexical source factories/caller families, not whole-program proof'}


def check_matrix():
    m = load(HERE/'matrix.json'); rows=m['rows']; unique(rows,'row_id')
    require(m['status']=='STATIC_PLAN_ONLY' and m['starting_revision']==START, 'matrix status/revision')
    require(len(rows)==10, 'minimal main matrix row count')
    cases={'legacy9_drag','legacy16_erase','immediate_reerase','immediate_navigation','immediate_masked_drag'}
    require({(r['case'],tuple(r['budget_mib'].values())) for r in rows} ==
            {(c,b) for c in cases for b in [(32,96),(64,192)]}, 'required anchor/endpoint/budget missing')
    route_ids={r['route_id'] for r in load(HERE/'routes.json')['routes']}
    for row in rows:
        require(row['copies']+row['original_strokes']==row['total_objects'], 'copies vs total')
        require(row['total_objects']==(9 if row['case']=='legacy9_drag' else 16), 'object count')
        require(row['dpr']==2 and row['repetitions']==3 and row['independent_reset_each_repetition'], 'row profile')
        require(row['measurements'] is None and row['evidence_status']=='MISSING', 'invented measurement')
        limits=row['limits_seconds']
        require(limits=={'setup':45,'measurement':120,'cleanup':15,'worker_total':180}, 'deadline changed')
        require(sum(limits[k] for k in ('setup','measurement','cleanup'))<=limits['worker_total'], 'inconsistent time limits')
        require(row['rss_process_family_limit_bytes']==3*1024**3, 'RSS bound')
        require(set(row['required_routes'])<=route_ids, 'matrix unknown route')
        if row['case'].startswith('immediate'):
            require(not row['settle_between_measured_gestures'] and
                    row['measurement_schedule']=='preplanned_independent_tasks', 'immediate row waits')
    for fixture in m['fixtures'].values():
        for ref in fixture.get('source_refs',[]):
            require(sha(ROOT/ref['path'])==ref['sha256'], 'legacy fixture source hash')
            require(git('show',START+':'+ref['path'],binary=True)==(ROOT/ref['path']).read_bytes(),'legacy fixture source revision')
        if 'path' in fixture:
            require(sha(ROOT/fixture['path'])==fixture['sha256'], 'literal fixture hash')
    fixture=load(ROOT/m['fixtures']['CONTINUOUS_30S']['path']); points=fixture['samples']
    require(len(points)==7200, '30s fixture count')
    # The literal fixture uses declared column indices, not guessed positions.
    columns=fixture['columns']; ti=columns.index('content_timestamp_ns'); pi=columns.index('phase')
    times=[int(p[ti]) for p in points]
    require(times==sorted(set(times)) and times[-1]-times[0]==30_000_000_000, 'not literal30s')
    require(points[0][pi]=='down' and points[-1][pi]=='up' and all(p[pi]=='move' for p in points[1:-1]), 'fixture lifecycle')
    legacy=m['fixtures']['LEGACY_28_796']
    require(int(legacy['last_content_timestamp_ns'])-int(legacy['first_content_timestamp_ns'])==int(legacy['content_span_ns'])==28_796_000_000, 'legacy relabeled30s')
    plans=load(ROOT/m['fixtures']['IMMEDIATE_ACTIONS']['path'])
    require(plans['status']=='FROZEN_INPUT_NOT_MEASUREMENT', 'actions falsely measured')
    gap=int(plans['gap_ns']); counts={}
    for branch, plan in plans['plans'].items():
        events=plan['events'];unique(events,'event_id');times=[int(e['planned_offset_ns']) for e in events]
        require(times==sorted(times) and str(times[-1])==plan['last_planned_offset_ns'], 'action plan order')
        for a,b in [('E0','E1')]+([('E1','E2')] if branch=='reerase' else []):
            au=next(int(e['planned_offset_ns']) for e in events if e['event_id']==a+'-up')
            bd=next(int(e['planned_offset_ns']) for e in events if e['event_id']==b+'-0')
            require(bd-au==gap,'immediate erase gap changed')
        if branch!='reerase':
            end=next(int(e['planned_offset_ns']) for e in events if e['event_id']=='E1-up')
            next_event='N-0' if branch=='navigation' else 'D-down'
            start=next(int(e['planned_offset_ns']) for e in events if e['event_id']==next_event)
            require(start-end==gap, 'immediate follow-up gap changed')
        counts[branch]=len(events)
    policy=m['batch_policy'];require(policy['main_runs']==sum(r['repetitions'] for r in rows)==30,'main runs')
    require(policy['main_max_seconds']==30*180==5400,'main batch bound')
    require(policy['primary_batch_max_seconds']==5400+6*2*90==6480,'primary total')
    require(policy['total_including_optional_diagnostic_max_seconds']==6480+1080==7560,'bounded diagnostic total')
    require(m['overhead_qualification']['order']==['AB','BA','BA','AB','AB','BA'],'paired order')
    return {'mandatory_rows':10,'independent_repetitions':3,'planned_runs':30,'executed_runs':0,
            'literal_samples':len(points),'literal_content_span_ns':'30000000000','action_event_counts':counts,
            'primary_batch_bound_seconds':6480,'with_optional_diagnostic_bound_seconds':7560}


def check_inputs_scope():
    pre=load(RESULTS/'preflight.json');package=ROOT/'.aegis/packages/G5-UR0a/package.json'
    require(sha(package)==PACKAGE_SHA==pre['package_sha256'],'package changed')
    p=load(package)
    require(git('branch','--show-current')=='codex/geometry-navigation-integration','wrong branch')
    require(git('remote','get-url','origin')=='git@github.com:Mostorm-Labs/axiom.git','wrong repo')
    git('merge-base','--is-ancestor',START,'HEAD')
    require(git('rev-parse','HEAD')==pre['starting_revision']==START,'unreconciled descendant delta')
    for path,digest in pre['initial_preserved_inputs'].items():
        require(sha(ROOT/path)==digest,'frozen input changed: '+path)
    for key in ('authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref'):
        ref=p[key];require(sha(package.parent/ref['path'])==ref['sha256'],'package reference changed')
    authority=load(package.parent/'authority.lock.json')
    for ref in authority['sources']:
        require(sha(ROOT/ref['path'])==ref['sha256'],'authority input changed')
    advisory=load(RESULTS/'advisory-inputs.json')
    for path,digest in advisory['preserved_read_only_inputs'].items():
        require(sha(ROOT/path)==digest,'advisory input changed: '+path)
    require(advisory['changes_active_package'] is False,'advisory rebound package')
    changed=git('diff','--name-only',START,'--').splitlines()
    require(not changed,'tracked/production diff not empty')
    untracked=git('ls-files','--others','--exclude-standard').splitlines()
    preserved=set(pre['initial_preserved_inputs'])|set(advisory['preserved_read_only_inputs'])
    allowed=('docs/planning/g5-unified-render/ur0a/','.aegis/results/G5-UR0a/')
    forbidden=[path for path in untracked if path not in preserved and not path.startswith(allowed)]
    require(not forbidden,'unreconciled untracked paths: '+str(forbidden))
    return {'package_sha256':PACKAGE_SHA,'starting_revision':START,'result_revision':git('rev-parse','HEAD'),
            'frozen_input_count':len(pre['initial_preserved_inputs']),
            'advisory_read_only_input_count':len(advisory['preserved_read_only_inputs']),
            'production_diff_empty':True,'tracked_changed_paths':[],
            'own_dirty_paths':[path for path in untracked if path.startswith(allowed)]}


def check_review_links():
    required=['protocol.md','metrics.schema.json','routes.json','matrix.json','review.md']
    for filename in required:
        require((HERE/filename).is_file(),'missing required artifact '+filename)
    for name in ('protocol.md','review.md'):
        text=(HERE/name).read_text()
        for link in re.findall(r'\]\(([^)]+)\)',text):
            if not re.match(r'\w+://',link):
                require((HERE/link.split('#')[0]).exists(),'broken relative link '+link)
    obligations=load(ROOT/'.aegis/packages/G5-UR0a/verification.lock.json')['obligations']
    text=(HERE/'review.md').read_text()
    require(all(o['id'] in text for o in obligations),'review lacks obligation mapping')
    return {'required_review_artifacts':required,'obligations': [o['id'] for o in obligations],
            'dynamic_evidence':'MISSING; static checks do not qualify Runtime/platform performance'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    report={'schema_version':'g5-ur0a-static-validation-v1','execution_kind':'STATIC_DATA_ONLY',
            'status':'PASS','checks':[],'new_runtime_or_browser_runs':0}
    for name,check in [('package_and_scope',check_inputs_scope),('metrics_schema_examples',check_schema),
                       ('source_routes_and_factories',check_routes),('matrix_and_literal_fixtures',check_matrix),
                       ('required_artifacts_links_obligations',check_review_links)]:
        try:
            details=check(); report['checks'].append({'id':name,'status':'PASS','details':details})
        except (Invalid,KeyError,IndexError,TypeError,OSError,subprocess.CalledProcessError) as error:
            report['status']='FAIL';report['checks'].append({'id':name,'status':'FAIL','error':str(error)})
    payload=json.dumps(report,ensure_ascii=False,indent=2)+'\n'
    if args.output:
        target=args.output.resolve()
        require(target.is_relative_to(RESULTS.resolve()),'validation output must stay in authorized results root')
        target.write_text(payload)
    print(payload,end='')
    return 0 if report['status']=='PASS' else 1


if __name__=='__main__':
    raise SystemExit(main())
