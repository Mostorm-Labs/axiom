import json,pathlib,subprocess,sys
out=pathlib.Path(__file__).parent
for name,args in [('configure',json.loads((out/'configure-command.json').read_text())),('build',['cmake','--build',str(out/'build'),'--target','axiom_fc03_image_facade_tests','axiom_minimal_windows_image_debug_ui_tests','--parallel','4']),('ctest',['ctest','--test-dir',str(out/'build'),'-R','^(FC03ImageFacade|FC03WindowsImageDebugUi)$','--output-on-failure'])]:
 with (out/(name+'.log')).open('w') as f:
  r=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT)
 print(name,r.returncode,flush=True)
 if r.returncode: print((out/(name+'.log')).read_text()[-5000:]);sys.exit(r.returncode)
 print((out/(name+'.log')).read_text()[-1200:])
