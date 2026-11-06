#!/usr/bin/env python3
"""P20 counterbalanced pilot;two blocks never qualify Runtime nonregression."""
import argparse,json,os,subprocess
from pathlib import Path

def plan():
    return ([{'case':case,'kind':'AA','block':b,'order':list('AAAA')}for case in ['pan','zoom']for b in range(2)]+
            [{'case':case,'kind':'AB','block':b,'order':list('ABBA')}for case in ['pan','zoom']for b in range(2)])
def main():
    p=argparse.ArgumentParser();p.add_argument('--baseline',required=True);p.add_argument('--candidate',required=True);p.add_argument('--output',required=True);a=p.parse_args()
    dest=Path(a.output);dest.mkdir(parents=True,exist_ok=False)
    spec=plan();(dest/'plan.json').write_text(json.dumps(spec,indent=2)+'\n');rows=[]
    options={'width':636,'height':923,'points':2048,'lines':5,'shapes':2,'scribble':True,'scribblePhase':.63,'zoom':1.59,'drawLast':True,'uiReads':True}
    for block in spec:
      for position,label in enumerate(block['order']):
        sdk=a.baseline if label=='A'else a.candidate;name=f"{block['kind']}-{block['case']}-{block['block']}-{position}-{label}";run=dest/name
        env={**os.environ,'AXIOM_NAV_MODES':'wheel','AXIOM_NAV_SELECTED':'0','AXIOM_NAV_PAN_ONLY':'1'if block['case']=='pan'else'0','AXIOM_NAV_QUIESCE':'1','AXIOM_NAV_DPR':'2','AXIOM_NAV_OPTIONS':json.dumps(options)}
        cmd=['python3.12','verification/runners/g5/bounded_worker.py','--prefix',str(run)+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/navigation_lifecycle.mjs',str(Path(sdk).resolve()),str(run),'1000','2']
        with open(str(run)+'-driver.log','w')as f:r=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
        summary=json.loads((run/'summary.json').read_text())if(run/'summary.json').exists()else{}
        assert summary['schema']=='g5-web-navigation-lifecycle-v2'
        assert all(p['summary']['runtimeSubmitV2']['status']=='OBSERVED'for p in summary['results'][0]['phases'])
        row={**block,'position':position,'label':label,'id':name,'path':str(run),'exit':r.returncode,'status':summary.get('status','BLOCKED_EVIDENCE')};rows.append(row)
        (dest/'matrix.json').write_text(json.dumps({'schema':'g5-feedback-timing-v2-pilot','scope':'Method qualification/diagnostic pilot only;not Runtime nonregression acceptance','results':rows},indent=2)+'\n')
        print(name,row['status'],flush=True)
        if r.returncode or row['status']!='MEASURED':raise SystemExit(1)
if __name__=='__main__':main()
