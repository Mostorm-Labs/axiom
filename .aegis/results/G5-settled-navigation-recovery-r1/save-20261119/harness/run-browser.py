from pathlib import Path
import json,hashlib,subprocess,time,math,sys,shutil,os,signal
W=Path(__file__).resolve().parents[2];O=W/'out/G5-settled-navigation-recovery-r1';E=W/'.aegis/results/G5-settled-navigation-recovery-r1';R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');name=sys.argv[1]
baseline=['SNR-D-B-ORDER','SNR-D-B-ISOLATED','SNR-D-B-REVERSE','SNR-D-B-PROGRESS'];ordinary=['SNR-W-B1','SNR-W-C1','SNR-W-C2','SNR-W-B2'];roles={**{n:300 for n in baseline+['SNR-D-C-FINAL']},**{n:360 for n in ordinary},'SNR-P-C':480}
assert name in roles;timeout=roles[name];ledger=E/'sampling-ledger.json';d=json.loads(ledger.read_text())if ledger.exists()else{'task':'G5-settled-navigation-recovery-r1','capSeconds':4200,'chargedSeconds':0,'attempts':[]}
assert not any(x['id']==name for x in d['attempts']);assert all(x['status']=='COMPLETED'for x in d['attempts'])
if name in baseline:assert [x['id']for x in d['attempts']if x['id']in baseline]==baseline[:baseline.index(name)]
else:assert all(n in[a['id']for a in d['attempts']]for n in baseline)
if name in ordinary:assert [x['id']for x in d['attempts']if x['id']in ordinary]==ordinary[:ordinary.index(name)]
if name=='SNR-P-C':assert all(n in[a['id']for a in d['attempts']]for n in ordinary)
assert d['chargedSeconds']+timeout<=4200 and shutil.disk_usage(W).free>=8192*1048576
freeze=json.loads((E/('freeze-'+name+'.json'if name.startswith('SNR-D')else'ordinary-freeze.json')).read_text())
for n,v in freeze['files'].items():
 p=Path(n);assert p.stat().st_size==v['bytes']and hashlib.sha256(p.read_bytes()).hexdigest()==v['sha256'],n
subprocess.run(['/opt/homebrew/bin/python3.12',str(O/'task-storage.py'),str(192*1048576),str(1445310000 if name=='SNR-P-C'else 64*1048576)],check=True,capture_output=True)
entry={'id':name,'status':'STARTING','startedUnix':time.time()};d['attempts'].append(entry);ledger.write_text(json.dumps(d,indent=2)+'\n')
cmd=['/opt/homebrew/bin/python3.12',str(W/'verification/runners/g5/bounded_worker.py'),'--prefix',str(O/'web'/name/'guard'),'--timeout',str(timeout),'--rss-mib','6144','--',str(R/'.deps/emsdk/node/24.19.0_64bit/bin/node'),str(O/'web.mjs'),str(O/(name+'-request.json')),str(O/'web'/name)]
log=O/'web'/name/'runner.log';log.parent.mkdir(parents=True,exist_ok=True)
with log.open('w')as output:
 process=subprocess.Popen(cmd,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
 while process.poll()is None:
  try:subprocess.run(['/opt/homebrew/bin/python3.12',str(O/'task-storage.py')],check=True,capture_output=True)
  except Exception:
   os.killpg(process.pid,signal.SIGTERM);process.wait();raise
  time.sleep(.5)
g=json.loads((O/'web'/name/'guard.worker.json').read_text());entry.update(status='COMPLETED'if process.returncode==0 else'FAILED',guard=g,chargedSeconds=math.ceil(g['elapsed_s']),endedUnix=time.time());d['chargedSeconds']=sum(x.get('chargedSeconds',0)for x in d['attempts']);ledger.write_text(json.dumps(d,indent=2)+'\n');print(name,'exit',process.returncode,'charged',entry['chargedSeconds']);raise SystemExit(process.returncode)
