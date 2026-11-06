import os,subprocess,json
from pathlib import Path
R=Path.cwd();env={**os.environ,'EMSDK_PYTHON':'/opt/homebrew/bin/python3.12'}
for profile in ['on','off']:
 B='out/web-sampling'+(''if profile=='on'else'-off');P=R/('out/g5-followup-'+profile)
 for step,cmd in [('configure',['cmake','-S','.','-B',B,'-DAXIOM_ENABLE_FRAME_TIMING='+profile.upper(),'-DAXIOM_RENDER_CACHE_MIB=32','-DAXIOM_RENDER_SHARED_MIB=96']),('build',['cmake','--build',B,'--target','axiom_js_runtime','-j2']),('install',['cmake','--install',B,'--prefix',str(P)])]:
  with open('out/g5-followup-'+profile+'-'+step+'.log','w')as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
  print(json.dumps({'profile':profile,'step':step,'exit':p.returncode}),flush=True)
  if p.returncode:raise SystemExit(p.returncode)
