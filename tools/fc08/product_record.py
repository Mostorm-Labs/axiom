"""Record exact-revision bootstrap checks and three real SUT profile runs."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from product_runner import ROOT,PACKAGE,PACKAGE_REVISION,now,ref,save,load

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',required=True,type=Path);p.add_argument('--run',required=True)
    a=p.parse_args();root=a.repo.resolve();assert root==ROOT
    out=root/'.aegis/results/FC-08-P31-BOOTSTRAP/attempts/bootstrap-003'/a.run
    out.mkdir(parents=True,exist_ok=False)
    facts=load(root/'out/fc08-bootstrap-003/native-environment.json');env={**os.environ,**facts['environment'],'PYTHONUTF8':'1'}
    git='D:/Softwares/Git/cmd/git.exe';head=subprocess.check_output([git,'rev-parse','HEAD'],cwd=root,text=True).strip()
    invocations=[]
    def run(name,argv):
        started=now();result=subprocess.run(list(map(str,argv)),cwd=root,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=1800)
        stdout=out/(name+'.stdout.log');stderr=out/(name+'.stderr.log')
        stdout.write_text(result.stdout,encoding='utf-8',newline='\n');stderr.write_text(result.stderr,encoding='utf-8',newline='\n')
        invocations.append({'name':name,'argv':list(map(str,argv)),'cwd':str(root),'start_utc':started,'end_utc':now(),
                            'exit_code':result.returncode,'stdout':ref(stdout),'stderr':ref(stderr)})
        print(name+': exit '+str(result.returncode),flush=True)
        return result
    run('semantic-offline',[sys.executable,'tools/setup_build_environment.py','--semantic','--offline','--status'])
    run('protoc-version',[env['AXIOM_PROTOC'],'--version'])
    tests=run('verification-tool-tests',[sys.executable,'-m','unittest','discover','-s','tests/fc08','-p','*_test.py','-v'])
    count=re.search(r'Ran (\d+) tests',tests.stderr)
    for phase in ['package','evidence']:
        run(phase+'-preflight',[sys.executable,'tools/fc08/accepted_preflight.py','--repo','.',
                              '--package-revision',PACKAGE_REVISION,'--package-path',PACKAGE+'/package.json',
                              '--phase',phase,'--output',out/(phase+'-preflight.json')])
    run('design-preflight',[sys.executable,'tools/fc08/product_design.py','--repo','.','--output',out/'implementation-design-preflight.json'])
    old=root.parent.parent/'fc08-verification-materialization/canvas/out/fc08/v02-off'
    common=[sys.executable,'tools/fc08/run_product_verification.py','--design',PACKAGE+'/verification.lock.json','--case','all',
            '--native-build',old/'windows-native','--web-build',old/'web-wasm','--protoc',env['AXIOM_PROTOC'],
            '--node','C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6/node/24.19.0_64bit/node.exe',
            '--chrome','C:/Program Files/Google/Chrome/Application/chrome.exe']
    for profile in ['windows-native','web-wasm','headless']:
        run('product-'+profile,[*common,'--profile',profile,'--output',out/'profiles'/profile])
    prior=load(root/'.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01/profiles.json')
    save(out/'toolchain-and-reuse.json',{'current_semantic_environment':facts,
        'actual_baseline_consumer_compilers':prior['actual_consumer_compilers'],
        'historical_build_invocations':[b for b in prior['builds'] if b['tag']=='v02-off'],
        'reuse_basis':'Exact prior OFF native binaries, WASM and JS hashes verified before actual new calls; production and observer source byte-equivalence checked. No dependencies or product binary rebuilt.',
        'old_observations_reused_as_new_results':False})
    preservation=run('preservation',[git,'diff','0d13b63702facc1534e815643c3bc6085d9c6618','--name-only','--diff-filter=CDMRTUXB'])
    report={'format':'fc08-bootstrap-exact-invocations-v1','runner_revision':head,'package_revision':PACKAGE_REVISION,
            'qualification_subject_revision':'427580ae13b60a39e88959b282cbffc695c9aabb','invocations':invocations,
            'all_commands_exit_zero':all(i['exit_code']==0 for i in invocations),'verification_tool_tests':int(count[1]) if count else None,
            'verification_tool_tests_are_not_Axiom_tests':True,'old_tracked_files_unchanged':not preservation.stdout.strip(),
            'environment':{'python':sys.version,'python_executable':sys.executable,'SDK_environment':facts['environment']},
            'product_P32_authorized':False,'P34_PASS':False}
    save(out/'invocations.json',report)
    print(json.dumps({'runner_revision':head,'all_commands_exit_zero':report['all_commands_exit_zero'],'output':str(out)},indent=2))
    raise SystemExit(0 if report['all_commands_exit_zero'] else 2)

if __name__=='__main__':main()
