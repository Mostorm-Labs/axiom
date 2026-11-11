"""Offline reduction of immutable browser rows. Never launches a browser."""
import collections
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
RESULT = Path(__file__).resolve().parent
def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''): h.update(block)
    return h.hexdigest()
def read(path): return json.loads(Path(path).read_text())
def write(name, value): (RESULT / name).write_text(json.dumps(value, indent=2) + '\n')
def ref(path):
    path = Path(path)
    return dict(path=str(path.relative_to(ROOT)), sha256=sha(path), bytes=path.stat().st_size)
header = (ROOT/'runtime/foundation/include/canvas/foundation/internal/g5_observation.hpp').read_text()
TAGS = {i+1: value.strip().split('=')[0].strip() for i,value in enumerate(
    re.search(r'enum class Tag : unsigned \{(.*?)\};', header).group(1).split(','))}
def reduce_row(path):
    row = read(path)
    pages = row.get('tracePages', [])
    records = [record for page in pages for record in page['records']]
    assert len({record['seq'] for record in records}) == len(records)
    ends = {record['values'][0]: record for record in records if record['kind'] == 3}
    spans = {}
    for record in records:
        values = record['values']; tag = TAGS.get(record['tag'], str(record['tag']))
        if record['kind'] == 2:
            end = ends.get(values[0])
            spans[values[0]] = dict(id=values[0], tag=tag, parent=values[2], job=record['job'], count=1,
                inclusive_ns=int(end['values'][1])-int(values[3]) if end else None,
                exclusive_ns=int(end['values'][2]) if end else None)
        elif record['kind'] == 10 and len(values) == 6 and values[0] == '9':
            spans[values[1]] = dict(id=values[1], tag=tag, parent=values[2], job=record['job'],
                count=int(values[3]), inclusive_ns=int(values[4]), exclusive_ns=int(values[5]))
    up_spans = [span for span in spans.values() if span['tag'] == 'eraser_up']
    assert len(up_spans) == 2*len(row['rounds'])
    phases, work = [], []
    for index, up in enumerate(up_spans):
        gesture = row['rounds'][index//2]
        event = gesture['up' if index%2 == 0 else 'nextUp']
        children = [span for span in spans.values() if span['parent'] == up['id']]
        assert all(span['inclusive_ns'] is not None for span in children)
        root_ns = up['inclusive_ns']
        child_ns = sum(span['inclusive_ns'] for span in children)
        residual = root_ns-child_ns if root_ns is not None else None
        if residual is not None:
            assert residual >= 0 and residual == up['exclusive_ns']
        totals = collections.defaultdict(lambda: dict(count=0, inclusive_ns=0, exclusive_ns=0, incomplete=0))
        for span in spans.values():
            if span['job'] != up['job']: continue
            total = totals[span['tag']]; total['count'] += span['count']
            if span['inclusive_ns'] is None: total['incomplete'] += 1
            else:
                total['inclusive_ns'] += span['inclusive_ns']; total['exclusive_ns'] += span['exclusive_ns']
        common = dict(case=path.parent.name, round=index//2+1, gesture='main' if index%2==0 else 'successor',
            job=up['job'], span=up['id'])
        phases.append(dict(**common, status='COMPLETE_ROOT' if root_ns is not None else 'CENSORED_MISSING_END',
            DOM_dispatch_ms=event['end']-event['start'], root_ns=root_ns, direct_children=children,
            child_inclusive_sum_ns=child_ns, residual_ns=residual, nested_totals=dict(totals),
            rule='Only direct-child inclusive times reconcile root. Nested totals overlap; never sum them as disjoint phases.'))
        counters=[record for record in records if record['job']==up['job'] and record['kind']==10]
        def values_for(tag, marker):
            return [list(map(int, record['values'])) for record in counters
                if TAGS.get(record['tag'])==tag and record['values'][0]==str(marker)]
        history=values_for('wa_history_lookup',1); masks=values_for('mask_history',4)
        body=values_for('exact_body',1); exact=values_for('exact_body',2)
        assert len(history)==len(body)==len(exact)
        work.append(dict(**common, status='COMPLETE_40_TARGET_COUNTERS' if len(history)==40 else 'CENSORED_PARTIAL_COUNTERS',
            targets_observed=len(history), targets_expected=40,
            local_capacity_bytes=sorted({v[1] for v in history}),
            local_usage_max_bytes=max((v[2] for v in history), default=None),
            history_plan_ready=sum(v[3] for v in history), history_plan_absent=sum(v[3]==0 for v in history),
            history_denied_delta=sum(v[5] for v in history), history_primitive_builds=sum(v[6] for v in history),
            query_union_operations=sum(v[7] for v in history), history_union_operations=sum(v[8] for v in history),
            equivalent_memo_cumulative=max((v[9] for v in history),default=None),
            body_uses=sum(v[6] for v in body), body_identities=sorted({str(v[5])for v in body}),
            body_fallback_reason_counts=dict(collections.Counter(str(v[7])for v in body)),
            body_full_validation_knots=sum(v[11] for v in body), body_full_compile_calls=sum(v[12] for v in body),
            body_full_compile_inputs=sum(v[13] for v in body), prepared_compile_calls=sum(v[8] for v in exact),
            prepared_compile_inputs=sum(v[9] for v in exact), exact_materialized_paths=sum(v[6] for v in exact),
            exact_replay_knots=sum(v[5] for v in exact),
            query_path_builds=sum(v[7] for v in masks), history_path_builds=sum(v[8] for v in masks),
            query_path_timed_ns=sum(v[11] for v in masks), history_path_timed_ns=sum(v[12] for v in masks),
            fallback_erasePath_note='Cached-path counters exclude uncached erasePath calls after get denies. wa_path_build inclusive/exclusive still includes those calls when trace records survive.'))
    loss={kind:max((page['loss'][kind]for page in pages),default=0)for kind in ['critical','statistical']}
    return dict(raw=ref(path), pages=len(pages), records=len(records), loss=loss,
        root_begin_count=len(up_spans), root_end_count=sum(span['inclusive_ns'] is not None for span in up_spans),
        semantic_status=row['status'], snapshot_roundtrip=row['snapshotRoundtrip'], phases=phases, work=work)

diagnostic=RESULT/'local-web/G5WA1-DIAGNOSTIC-UP-ONLY-R1'
cases=[reduce_row(diagnostic/case/'qualified-row.json')for case in ['long800','long1600','growth6']]
assert sum(case['root_end_count']for case in cases)==15
assert sum(case['root_begin_count']for case in cases)==16
write('phase-timing.json', dict(status='PARTIAL_15_COMPLETE_ROOTS_ONE_CENSORED', units='nanoseconds',
    primary_normal_performance=False, cases=[{key:value for key,value in case.items()if key!='work'}for case in cases],
    missing='long1600 successor eraser_up span340 End lost; do not impute root/residual or assume missing aggregate/counters are zero.',
    loss_explanation='eraser_up is nested Purpose::none and absent from criticalScope whitelist. Its End is statistical; input root and selected phase children remain critical. Therefore critical0 does not establish all Up roots complete.'))
write('work-counts.json',dict(status='COMPLETE_FOR_15_ROOTS_CENSORED_FOR_ONE',
    cases=[dict(raw=case['raw'],loss=case['loss'],work=case['work'])for case in cases],
    render_cardinality=dict(status='CORROBORATIVE_PRIOR_DIAGNOSTIC_ONLY',
        scope='Final Up-only trace intentionally does not observe post-input canonical frames. Original diagnostic long800 has loss0; coverage counters below measure that separate lane only.',
        rows=[])))
work_file=read(RESULT/'work-counts.json')
prior_path=RESULT/'local-web/G5WA1-DIAGNOSTIC/long800/qualified-row.json'
prior=read(prior_path); prior_records=[r for page in prior['tracePages']for r in page['records']]
coverage=[r for r in prior_records if r['kind']==10 and TAGS.get(r['tag'])=='coverage' and r['values'][0]=='4' and r['purpose']==2]
work_file['render_cardinality'].update(raw=ref(prior_path), canonical_status='MISSING_COUNTER_ROWS_NOT_ZERO',
    canonical_note='Canonical scope/receipt records exist but no coverage marker4 counter rows in that purpose. Do not infer tile cardinality0. No post-Up canonical cardinality claim.',
    rows=[dict(job=r['job'],gpu_coverage_tiles=int(r['values'][1]),cpu_coverage_tiles=int(r['values'][2]),
        preparation_steps=int(r['values'][5]),coverage_builds=int(r['values'][6]),coverage_hits=int(r['values'][7]),
        retained_bytes=int(r['values'][8]),coverage_bytes=int(r['values'][9]),scratch_peak_bytes=int(r['values'][10]))for r in coverage],
    separately_observed_transient_rows=[dict(job=r['job'],values=r['values'])for r in prior_records
        if r['kind']==10 and TAGS.get(r['tag'])=='coverage' and r['values'][0]=='4' and r['purpose']==3],
    prefix_observations=[r for r in prior_records if TAGS.get(r['tag'])=='wa_prefix' and r['kind']==10])
write('work-counts.json',work_file)

overhead=RESULT/'local-web/G5WA1-OVERHEAD'
off=read(overhead/'off800/qualified-row.json'); on=read(overhead/'on800/qualified-row.json')
def time(row,key):
    event=row['rounds'][0][key];return event['end']-event['start']
timings={key:dict(off_ms=time(off,key),on_ms=time(on,key),delta_ms=time(on,key)-time(off,key))for key in ['up','nextDown','nextMove','nextUp']}
for value in timings.values(): value['delta_percent']=100*value['delta_ms']/value['off_ms']
write('on-off-overhead.json',dict(status='ONE_PAIR_DESCRIPTIVE_ONLY', plan=ref(RESULT/'overhead-plan.json'),
    wasm_sha256='6d40ad7ca7eb7fd9b5b4975d90d27c9ab9b84555f960d3ecdf3bdcf27167f439',
    rows=[ref(overhead/k/'qualified-row.json')for k in ['off800','on800']],timings=timings,
    interpretation='One fresh OFF page then one fresh ON page; not randomized/repeated. No uncertainty bound, causal performance PASS or ordinary-time correction.',
    OFF_trace_records=sum(len(page['records'])for page in off['tracePages']),
    ON_trace_qualification=reduce_row(overhead/'on800/qualified-row.json'),
    same_task_successor_gaps_ms=[row['rounds'][0]['nextDown']['start']-row['rounds'][0]['up']['end']for row in [off,on]]))

witness=[]
for kind in ['sticky','table','image']:
    path=RESULT/'local-web/G5WA1-BASELINE-WITNESS'/kind/'qualified-row.json';row=read(path)
    witness.append(dict(kind=kind,raw=ref(path),status=row['status'],plan=row['plan'],
        before_up_positive_pixels=row['feedback']['value']['positive'],baseline_expected_pixels=0,
        actual_before_up=row['activePixels']['at']<row['dragUp']['start'],pose_delta=row['poseDelta'],
        feedback_ms_from_main_up=row['feedback']['firstCorrectAt']-row['mainUp']['start'],
        receipt=dict(generation=row['end']['generation'],presented=row['end']['lastCommit']['presentedGeneration']),
        snapshot_roundtrip=row['snapshotRoundtrip'],
        note='First correct ROI after analytic typed source; correctness-only GPU readback stalls included. This does not measure first physical feedback or prove improved drag latency.'))
write('baseline-witness-results.json',dict(status='QUALIFIED_THREE_MISSING_TYPED_WITNESSES',rows=witness,
    semantic_details=ref(RESULT/'snapshots-semantics.json'),old_failed_evidence_unchanged=True))
baseline=[]
for case in ['long1600','growth6']:
    path=RESULT/'local-web/G5WA1-BASELINE-TAIL'/case/'qualified-row.json';row=read(path)
    rounds=[]
    for gesture in row['rounds']:
        rounds.append(dict(round=gesture['round']+1,main_up_ms=gesture['up']['end']-gesture['up']['start'],
            successor_up_ms=gesture['nextUp']['end']-gesture['nextUp']['start'],
            successor_down_ms=gesture['nextDown']['end']-gesture['nextDown']['start'],
            main_to_next_down_gap_ms=gesture['nextDown']['start']-gesture['up']['end']))
    baseline.append(dict(case=case,raw=ref(path),rounds=rounds,
        ready_from_first_main_up_ms=row['readyAt']-row['rounds'][0]['up']['start'],
        snapshot_roundtrip=row['snapshotRoundtrip'],errors=row['errors']))
write('ordinary-baseline.json',dict(status='OBSERVED_NOT_OPTIMIZED',rows=baseline,
    distinction='New immediate full successor dispatch uses no16ms pauses/ROI between moves. Old~9.2s paced successor and this~3.65s successor are different pacing, not an optimization comparison. Growth normal and diagnostic vary; no paired relative PASS.'))
print(json.dumps({'phase_roots':15,'censored':1,'ordinary_main_up_ms':baseline[0]['rounds'][0]['main_up_ms'],
    'overhead_main_delta_ms':timings['up']['delta_ms'],'typed_witnesses':len(witness)}))
