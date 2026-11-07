import os,sys,json,pathlib
sys.path.insert(0,os.getcwd())
from verification.runners.g5.bounded_worker import run
os.environ['G5_ERASE_POINTS']='121';r=pathlib.Path.cwd()
for name,driver in [('unique-off','verification/runners/g5/duplicated_erase_navigation.mjs'),('green-off','verification/runners/g5/repeat_erase.mjs'),('contained-off','out/g5-repeat-contained.mjs')]:
 print(json.dumps({'stage':name}),flush=True)
 v=run(['node',driver,'out/g5-repeat-off/js','out/g5-repeat-'+name+'-data','2','candidate'],r/('out/g5-repeat-'+name),180,3072*1024)
 print(json.dumps({'stage':name+'-complete',**v}),flush=True)
