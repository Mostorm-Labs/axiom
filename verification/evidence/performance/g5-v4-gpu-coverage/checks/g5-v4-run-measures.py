import json,os,pathlib,subprocess,time,sys
sys.path.insert(0,os.getcwd())
from verification.runners.g5.bounded_worker import run
root=pathlib.Path.cwd();os.environ['G5_ERASE_POINTS']='121'
rows=[('cpu-on-dpr2','out/g5-masked-on/js',2),('gpu-on-dpr2','out/g5-v4-final-on/js',2),('cpu-off-dpr2','out/g5-masked-off/js',2),('gpu-off-dpr2','out/g5-v4-final-off/js',2),('gpu-on-dpr1','out/g5-v4-final-on/js',1),('gpu-on-dpr4','out/g5-v4-final-on/js',4)]
for name,sdk,dpr in rows:
 print(json.dumps({'stage':'cohort','name':name}),flush=True)
 r=run(['node','verification/runners/g5/duplicated_erase_navigation.mjs',sdk,'out/g5-v4-measure-'+name,str(dpr),'candidate'],root/('out/g5-v4-measure-'+name+'-worker'),180,3072*1024)
 print(json.dumps({'stage':'cohort-complete','name':name,**r}),flush=True)
print(json.dumps({'stage':'all-measures-complete'}),flush=True)
