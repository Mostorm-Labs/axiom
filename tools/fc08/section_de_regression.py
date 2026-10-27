"""Reuse the exact accepted C matrix; preserve the independent baseline failure."""
import argparse,json,os,subprocess,sys
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--phase',required=True);p.add_argument('--python-only',action='store_true');a=p.parse_args();assert Path(a.repo).resolve()==ROOT
out=ROOT/'.aegis/results/FC-08-SECTION-DE'/a.phase;out.mkdir(parents=True,exist_ok=False)
facts=json.loads((ROOT/'out/fc08-section-de/environment.json').read_text());env={**os.environ,**facts['environment'],'PYTHONUTF8':'1'};journal=[]
def run(name,argv):
    r=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
    for k,v in [('stdout',r.stdout),('stderr',r.stderr)]:(out/(name+'.'+k+'.log')).write_text(v,encoding='utf-8')
    journal.append({'name':name,'argv':list(map(str,argv)),'exit_code':r.returncode});print(name+': '+str(r.returncode),flush=True)
    (out/'invocations.json').write_text(json.dumps({'commands':journal,'environment':facts['environment']},indent=2)+'\n')
    return r.returncode
if a.python_only:
    codes=[run('Axiom-contracts',[sys.executable,'-m','unittest','verification.tests.test_current_semantic_descriptor','verification.tests.test_semantic_contract','-v']),run('runtime-boundaries',[sys.executable,'tools/check_runtime_boundaries.py','--root',ROOT])]
    raise SystemExit(int(any(codes)))
accepted=ROOT/'.aegis/results/FC-08-SECTION-C-H03-v0.1/qualified-001'
prior=json.loads((accepted/'invocations.json').read_text());argv=next(c['argv'] for c in prior['commands'] if '--target' in c['argv'])
targets=argv[argv.index('--target')+1:argv.index('--parallel')]
build=ROOT/'out/fc08-section-c/build'
code=run('build',['cmake','--build',build,'--target',*targets,'--parallel','6'])
if code:raise SystemExit(code)
code=run('ctest',['ctest','--test-dir',build,'--tests-from-file',accepted/'ctest-names.txt','--output-on-failure','--timeout','120','--output-junit',out/'ctest.xml'])
suite=ET.parse(out/'ctest.xml').getroot();failed=[c.attrib['name'] for c in suite.findall('testcase') if c.find('failure') is not None]
known=['StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds']
skipped=[c.attrib['name'] for c in suite.findall('testcase') if c.find('skipped') is not None]
result={'ctest_exit_code':code,'tests':len(suite.findall('testcase')),'failed':failed,'skipped':skipped,'independently_reproduced_baseline_failure':known,'new_failures':[f for f in failed if f not in known]}
(out/'classification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
raise SystemExit(int(bool(result['new_failures'] or skipped)))
