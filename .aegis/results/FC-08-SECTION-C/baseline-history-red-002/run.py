import subprocess,json,hashlib,sys,os
from pathlib import Path
ROOT=Path.cwd();out=ROOT/'.aegis/results/FC-08-SECTION-C/baseline-history-red-002';subject=ROOT/'out/fc08-section-c/baseline-history-source';subject.mkdir(parents=True,exist_ok=False)
G='D:/Softwares/Git/cmd/git.exe';revision='c4fde4d4b1843673a71cce0546921f4494b16635'
files=subprocess.check_output([G,'ls-tree','-r','--name-only',revision,'runtime/semantic/include','runtime/interaction/include','runtime/foundation/include','runtime/interaction/src/editor_history.cpp','runtime/semantic/src/semantic_types.cpp']).decode().splitlines();manifest=[]
for name in files:
 data=subprocess.check_output([G,'show',revision+':'+name]);path=subject/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);manifest.append({'path':name,'sha256':hashlib.sha256(data).hexdigest()})
argv=['cl.exe','/nologo','/std:c++20','/O2','/DNDEBUG','/MT','/W4','/WX','/permissive-','/EHsc',*[f'/I{subject/p}' for p in ['runtime/semantic/include','runtime/interaction/include','runtime/foundation/include']],str(out/'test.cpp'),str(subject/'runtime/interaction/src/editor_history.cpp'),str(subject/'runtime/semantic/src/semantic_types.cpp'),f'/Fo{subject}'+os.sep,f'/Fe{subject}/baseline_history_red.exe']
commands=[]
def run(name,args):
 r=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');commands.append({'argv':args,'exit_code':r.returncode});(out/(name+'.stdout.log')).write_text(r.stdout,encoding='utf-8');(out/(name+'.stderr.log')).write_text(r.stderr,encoding='utf-8');return r.returncode
code=run('build',argv)
if code==0:code=run('observe',[str(subject/'baseline_history_red.exe')])
(out/'invocations.json').write_text(json.dumps({'subject_revision':revision,'actual_SUT':'Baseline EditorHistory::prepareRecord and baseline semantic operation-kind dispatch; not whole FC08 product RED-01','baseline_source_manifest':manifest,'commands':commands,'exit_code':code,'timing':'additional immutable-baseline observation after implementation; original red-001 was capability-presence probe only'},indent=2)+'\n')
print('baseline result',code);sys.exit(0 if code==1 and len(commands)==2 else 1)
