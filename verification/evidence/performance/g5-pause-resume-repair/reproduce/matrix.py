import os,json,gzip,subprocess
from pathlib import Path
D=Path('out/g5-reentry-final-runs');D.mkdir(exist_ok=False)
cases=[('old-stress','old','on','zoom',4,False),('bad-stress','bad','on','zoom',4,False),('fixed-stress','fixed','on','zoom',4,False),('fixed-stress-repeat','fixed','on','zoom',4,False),('bad-stress-repeat','bad','on','zoom',4,False),('old-user-zoom','old','on','zoom',1,True),('bad-user-zoom','bad','on','zoom',1,True),('fixed-user-zoom','fixed','on','zoom',1,True),('bad-user-pan','bad','on','pan',1,True),('fixed-user-pan','fixed','on','pan',1,True),('old-off-user-zoom','old','off','zoom',1,True),('bad-off-user-zoom','bad','off','zoom',1,True),('fixed-off-user-zoom','fixed','off','zoom',1,True),('fixed-off-user-pan','fixed','off','pan',1,True)]
rows=[]
for name,label,diag,kind,delta,user in cases:
 sdk={'old':'out/g5-budget-closed-'+diag+'/js','bad':'out/g5-retire-'+diag+'/js','fixed':'out/g5-reentry-final-'+diag+'/js'}[label]
 dest=D/name;guard=diag=='on'and(label=='fixed'or(label=='bad'and not user))
 env={**os.environ,'G5_PINCH_DELTA':str(delta),'G5_USER_CORPUS':str(int(user)),'G5_EXPECT_RESPONSIVE':str(int(guard))}
 cmd=['python3.12','verification/runners/g5/bounded_worker.py','--prefix',str(dest)+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/pause_resume.mjs',sdk,str(dest),kind]
 with open(str(dest)+'-driver.log','w')as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 result=json.loads(gzip.decompress((dest/'result.json.gz').read_bytes()))
 expected='FAILED_RESPONSE_GUARD'if guard and label=='bad'else'MEASURED'
 row={'id':name,'label':label,'diagnostics':diag,'kind':kind,'delta':delta,'userCorpus':user,'expectedStatus':expected,'status':result['status'],'exit':p.returncode,'build':result['setup']['build'],'firstContentMs':result['feedback']['firstContentResponseMs'],'firstAcceptedMs':result['runtime']['firstAcceptedSubmitMs'],'maxContentGapMs':result['feedback']['contentGapMs']['max'],'maxAcceptedGapMs':result['runtime']['acceptedGapMs']['max'],'lastReceiptMs':result['feedback']['lastInputToLastCoarseReceiptMs'],'imageBytes':result['fixtureImageBytes'],'pixels':result['raw']['pixels'],'sourceClean':not result['sourceBefore']['dirty'],'sourceStable':result['sourceStable'],'artifactStable':result['artifactStable']}
 rows.append(row);(D/'matrix.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,result['status'],round(row['firstContentMs'],2),row['build']['sourceRevision'][:8],flush=True)
 if result['status']!=expected or not(row['sourceClean']and row['sourceStable']and row['artifactStable']):raise SystemExit(1)
