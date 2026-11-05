import os,json,subprocess
from pathlib import Path
options={'width':636,'height':923,'points':2048,'lines':5,'shapes':2,'scribble':True,'scribblePhase':.63,'zoom':1.59,'drawLast':True,'uiReads':True}
def sample(label,side,mode,dpr,idle):
 dest='out/g5-gpu-'+label+'-'+side
 sdk='out/g5-'+('gpu' if side=='after' else 'reuse')+'-final-'+mode+'/js'
 env={**os.environ,'AXIOM_NAV_SELECTED':'1','AXIOM_NAV_MODES':'wheel','AXIOM_NAV_PAN_ONLY':'0','AXIOM_NAV_QUIESCE':'1','AXIOM_NAV_DPR':str(dpr),'AXIOM_NAV_OPTIONS':json.dumps(options)}
 with open(dest+'-driver.log','w') as log:
  p=subprocess.run(['python3.12','verification/runners/g5/bounded_worker.py','--prefix',dest+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/navigation_lifecycle.mjs',sdk,dest,str(idle),'2'],env=env,stdout=log,stderr=subprocess.STDOUT)
 print(label,side,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
# Reverse-order60s pair counters fixed-order thermal/cache drift.
for side in ['after','before']:sample('recheck',side,'off',2,60000)
# Same corpus and flags,high-DPR corroboration. Not another release gate.
for dpr in [3,4]:
 for side in ['before','after']:sample('dpr'+str(dpr),side,'on',dpr,1000)
