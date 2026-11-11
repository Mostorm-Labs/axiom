"""Offline reduction of immutable browser rows. Never launches a browser."""
import collections
import hashlib
import json
from pathlib import Path
import re

ROOT = Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom')
RESULT = ROOT/'.aegis/results/G5-crossframe-history-pressure-r1'
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
    row['snapshotRoundtrip']=json.loads(__import__('subprocess').check_output([str(ROOT/'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip'),str(path.parent/'final.axsnap'),str(path.parent/'redo.axsnap')],text=True))
    pages = [json.loads(line)['page']for line in (path.parent/'trace-pages.jsonl').read_text().splitlines() if line]
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
    selected=[r for r in row['rounds']if row['mode']!='growth' or r['round']in[0,5,9]]
    assert len(up_spans) == 2*len(selected)
    phases, work = [], []
    for index, up in enumerate(up_spans):
        gesture = selected[index//2]
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
        common = dict(case=path.parent.name, round=gesture['round']+1, gesture='main' if index%2==0 else 'successor',
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


cases=[reduce_row(RESULT/"local-web/G5CFH1-DIAGNOSTIC"/name/"raw-row.json")for name in ["candidate-growth-diagnostic"]]
write("current-phase-work.json",{"status":"COMPLETE_SELECTED_UP_ROOTS", "cases":cases, "selection":"longbothUp;growthrounds1/6/10bothUp", "no_duplicate_CDP_trace":True,"diagnostic_not_normal_performance":True})
print([(c["root_begin_count"],c["root_end_count"],c["loss"])for c in cases])
