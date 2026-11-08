"""Sidecar RSS/stage records around the immutable process-family guard.

The exact process table is returned unchanged to the original guard. This never
changes its ownership, RSS sum, threshold, timeout or cleanup decisions.
"""
import importlib.util,json,math,os
from pathlib import Path
HERE=Path(__file__).resolve().parent
def run(command,prefix,timeout,rss_kib):
    spec=importlib.util.spec_from_file_location('frozen_bounded_worker',HERE.parent/'bounded_worker.py')
    guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)
    original=guard.processes;known={};samples=0;stage=None;truncated=False
    prefix=Path(prefix);prefix.parent.mkdir(parents=True,exist_ok=True)
    limit=math.ceil(timeout/.05)+64
    with prefix.with_suffix('.rss.jsonl').open('w') as stream:
        def observed():
            nonlocal samples,stage,truncated
            import time
            table=original()
            candidates={pid for pid,p in table.items() if p['parent']==os.getpid() and len(command)>1 and command[1] in p['command'] and not p['command'].startswith('ps ')}
            family=set(candidates)
            while True:
                more={pid for pid,p in table.items() if p['parent'] in family}
                if more.issubset(family):break
                family.update(more)
            for pid in family:
                if pid in table:
                    if pid in known or len(known)<512:known[pid]=table[pid]['command']
                    else:truncated=True
            rows=[{'pid':pid,'ppid':p['parent'],'rss_kib':p['rss'],'state':p['state'],'role':'gpu' if '--type=gpu-process' in p['command'] else 'renderer' if '--type=renderer' in p['command'] else 'node-worker' if pid in candidates else 'browser-or-utility'}
                  for pid,p in table.items() if pid in known and p['command']==known[pid] and not p['state'].startswith('Z')]
            stdout=prefix.with_suffix('.stdout.log')
            if stdout.exists():
                with stdout.open('rb') as f:f.seek(max(0,stdout.stat().st_size-8192));tail=f.read(8192)
                for line in tail.decode(errors='replace').splitlines():
                    try:value=json.loads(line)
                    except ValueError:continue
                    if isinstance(value,dict) and 'stage' in value:stage=value['stage']
            if samples<limit:
                # One small row is streamed per already-required ps observation;
                # no trace or per-process time series is retained in memory.
                stream.write(json.dumps({'sample':samples,'monotonic_ns':str(time.monotonic_ns()),'stage':stage,'owned_processes':rows[:64],
                    'summed_owned_rss_kib':sum(p['rss_kib'] for p in rows[:64]),'attribution_truncated':len(rows)>64})+'\n');stream.flush();samples+=1
            else:truncated=True
            return table
        guard.processes=observed
        result=guard.run(command,prefix,timeout,rss_kib)
    metadata={'samples':samples,'sample_limit':limit,'truncated':truncated,'rss_sum_definition':'unchanged original guard, conservative per-process sum; shared pages may repeat','phase_markers':'last complete stdout JSON stage at ps observation; not a physical GPU measurement'}
    prefix.with_suffix('.rss-metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    return result
