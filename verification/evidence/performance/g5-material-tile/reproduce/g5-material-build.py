import os,subprocess,shutil,json,sys
from pathlib import Path
P=sys.argv[1];root=Path.cwd();env={**os.environ,'EMSDK_PYTHON':'/opt/homebrew/bin/python3.12'}
def run(cmd,log):
 with open(log,'w') as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 print(log,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
for profile,build,cache,hard in [('on','out/web-sampling',32,96),('off','out/web-sampling-off',32,96)]:
 prefix='out/'+P+'-'+profile
 run(['cmake','-S','.','-B',build,'-DAXIOM_RENDER_CACHE_MIB='+str(cache),'-DAXIOM_RENDER_SHARED_MIB='+str(hard)],prefix+'-configure.log')
 run(['cmake','--build',build,'--target','axiom_js_runtime','-j2'],prefix+'-build.log')
 run(['cmake','--install',build,'--prefix',str(root/prefix)],prefix+'-install.log')
 shutil.copyfile(build+'/CMakeCache.txt',prefix+'-CMakeCache.txt')
run(['cmake','-S','verification/runners/g5/native','-B','out/web-prepared-probe','-DAXIOM_RENDER_CACHE_MIB=32','-DAXIOM_RENDER_SHARED_MIB=96'],'out/'+P+'-gpu-configure.log')
run(['cmake','--build','out/web-prepared-probe','--target','g5_render_cache_gpu','-j2'],'out/'+P+'-gpu-build.log')
rows=[]
for mode in ['material','detail','oversized','frontier','quality','affine','background','contour','replay','exact-replay']:
 dest='out/'+P+'-gpu-'+mode
 run(['python3.12','verification/runners/g5/bounded_worker.py','--prefix',dest+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/prepared_gpu_probe.mjs','out/web-prepared-probe',dest+'.json','--'+mode+'-only'],dest+'-driver.log')
 row=json.loads(Path(dest+'.json').read_text());rows.append({'mode':mode,**row});Path('out/'+P+'-gpu-results.json').write_text(json.dumps(rows,indent=2)+'\n');print('GPU',mode,row['status'],flush=True)
 if row['status']!='PASS':raise SystemExit(1)
