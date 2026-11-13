"""Compile standalone verification against existing locked consumer archives."""
from pathlib import Path
import json,subprocess,shlex,sys
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-mixed-reference-qualification-r1';OUT.mkdir(exist_ok=True)
BUILD=ROOT/'out/G5-CPUW-checkpoint-20261113/build-native'
def build(source_name="mixed_reference_edges.cpp",binary_name="mixed-reference-edges"):
 commands=json.loads((BUILD/'compile_commands.json').read_text());prior=next(v for v in commands if v['file'].endswith('/cpu_postworker_replay_test.cpp'))
 compile=shlex.split(prior['command']);source=ROOT/'verification/runners/g5'/source_name;obj=OUT/(binary_name+'.o');binary=OUT/binary_name
 compile[compile.index('-o')+1]=str(obj);compile[compile.index('-c')+1]=str(source);compile += ['-I'+str(ROOT/'verification/runners/g5')]
 command=subprocess.check_output(['ninja','-C',str(BUILD),'-t','commands','axiom_cpu_postworker_replay_tests'],text=True).splitlines()[-1]
 raw=command.split(' && ')[1];link=shlex.split(raw);link=[str(obj)if x.endswith('cpu_postworker_replay_test.cpp.o')else x for x in link];link[link.index('-o')+1]=str(binary)
 (OUT/'standalone-build-command.json').write_text(json.dumps({'compile':compile,'link':link,'cwd':str(BUILD),'dependencies':'accepted existing nativeSDK/archives, no rebuild'},indent=2)+'\n')
 for name,cmd in [('compile',compile),('link',link)]:
  with(OUT/(name+'.log')).open('w')as log:r=subprocess.run(cmd,cwd=BUILD,stdout=log,stderr=log,timeout=60)
  if r.returncode:raise SystemExit(r.returncode)
 print(binary)
if __name__=='__main__':build(*sys.argv[1:])
