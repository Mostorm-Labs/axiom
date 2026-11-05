#!/usr/bin/env python3
"""Bound only this worker's process family; no killall or existing browser attach.
RSS is summed per process (shared pages may be counted more than once). It is a
conservative containment observation, not a unique physical-memory measurement.
"""
import argparse,json,os,signal,subprocess,time
from pathlib import Path

def processes():
 text=subprocess.check_output(['ps','-axo','pid=,ppid=,rss=,stat=,command='],text=True)
 result={}
 for line in text.splitlines():
  fields=line.strip().split(None,4)
  if len(fields)==5:result[int(fields[0])]={'parent':int(fields[1]),'rss':int(fields[2]),'state':fields[3],'command':fields[4]}
 return result

def run(command,prefix,timeout,rss_kib):
 if os.name!='posix':raise RuntimeError('bounded worker requires a supported process-family RSS observer')
 if not command or timeout<=0 or rss_kib<=0:raise ValueError('invalid bounds')
 prefix=Path(prefix);prefix.parent.mkdir(parents=True,exist_ok=True)
 started=time.monotonic();known={};peak=0;reason=None
 with prefix.with_suffix('.stdout.log').open('wb') as stdout,prefix.with_suffix('.stderr.log').open('wb') as stderr:
  p=subprocess.Popen(command,stdout=stdout,stderr=stderr,start_new_session=True)
  try:
   while True:
    table=processes();family={p.pid};more=True
    while more:
     new={pid for pid,v in table.items() if v['parent'] in family};more=not new.issubset(family);family.update(new)
    for pid in family:
     if pid in table:known[pid]=table[pid]['command']
    rss=sum(v['rss'] for pid,v in table.items() if pid in known and known[pid]==v['command'] and not v['state'].startswith('Z'))
    peak=max(peak,rss)
    if rss>rss_kib:reason='rss_limit';break
    if time.monotonic()-started>timeout:reason='timeout';break
    code=p.poll()
    if code is not None:reason='completed' if code==0 else 'nonzero_exit';break
    time.sleep(.05)
  except Exception:
   reason='observer_failure';raise
  finally:
   # Include children already observed before their parent exits/reparents.
   for sig in [signal.SIGTERM,signal.SIGKILL]:
    table=processes()
    alive=[pid for pid,cmd in known.items() if pid in table and table[pid]['command']==cmd and not table[pid]['state'].startswith('Z')]
    for pid in sorted(alive,reverse=True):
     try:os.kill(pid,sig)
     except ProcessLookupError:pass
    if not alive:break
    time.sleep(.15)
   try:p.wait(timeout=2)
   except subprocess.TimeoutExpired:p.kill();p.wait()
 table=processes();remaining=[pid for pid,cmd in known.items() if pid in table and table[pid]['command']==cmd and not table[pid]['state'].startswith('Z')]
 stages=[]
 for line in prefix.with_suffix('.stdout.log').read_text(errors='replace').splitlines():
  try:
   value=json.loads(line)
   if isinstance(value,dict) and 'stage' in value:stages.append(value)
  except (ValueError,TypeError):pass
 result={'command':command,'reason':reason,'returncode':p.returncode,'elapsed_s':time.monotonic()-started,'peak_process_family_rss_kib':peak,
  'rss_limit_kib':rss_kib,'timeout_s':timeout,'last_stage':stages[-1] if stages else None,'cleanup_complete':not remaining,'remaining_owned_pids':remaining}
 prefix.with_suffix('.worker.json').write_text(json.dumps(result,indent=2)+'\n');return result

if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--prefix',required=True,type=Path);a.add_argument('--timeout',type=float,default=180);a.add_argument('--rss-mib',type=int,default=2048);a.add_argument('command',nargs=argparse.REMAINDER);args=a.parse_args()
 cmd=args.command[1:] if args.command[:1]==['--'] else args.command
 r=run(cmd,args.prefix,args.timeout,args.rss_mib*1024);print(json.dumps(r));raise SystemExit(0 if r['reason']=='completed' and r['cleanup_complete'] else 2)
