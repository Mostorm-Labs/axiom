import time,sys,json,gzip
from pathlib import Path
sys.path.insert(0,str(Path('verification/runners/g5').resolve()))
from bounded_worker import run
root=Path.cwd();out=root/'out/g5-followup-final'
while 'all-observations-complete' not in (root/'out/g5-followup-execute.log').read_text():time.sleep(2)
for name,side,restore in [('on-dpr4-repeat','on',False),('off-dpr4','off',False),('on-dpr4-restore','on',True)]:
 print(json.dumps({'stage':'high-dpr-start','case':name}),flush=True)
 cmd=['node','verification/runners/g5/post_edit_navigation.mjs',str(root/f'out/g5-followup-{side}/js'),str(out/name),'4']
 if restore:cmd=['env','G5_CONTEXT_RESTORE=1',*cmd]
 w=run(cmd,out/(name+'-worker'),180,3072*1024)
 p=out/name/'observation.json.gz';b=json.loads(gzip.decompress(p.read_bytes()))if p.exists()else{}
 print(json.dumps({'stage':'high-dpr-complete','case':name,'reason':w['reason'],'status':b.get('status'),'error':b.get('error')}),flush=True)
