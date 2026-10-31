"""Exercise an installed DLL from a fresh application outside the checkout."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import hashlib
import os
import re

def run(command, cwd, records):
    try:
        result = subprocess.run([str(v) for v in command], cwd=cwd, text=True, capture_output=True, encoding='utf-8', errors='replace',timeout=120)
    except subprocess.TimeoutExpired as e:
        records.append({'command':[str(v) for v in command], 'cwd':str(cwd),'exit_code':None,'outcome':'TIMEOUT','stdout':str(e.stdout),'stderr':str(e.stderr)})
        raise
    records.append({'command':[str(v) for v in command], 'cwd':str(cwd), 'exit_code':result.returncode, 'stdout':result.stdout, 'stderr':result.stderr})
    print(result.stdout, result.stderr, flush=True)
    if result.returncode:
        raise RuntimeError('external consumer command failed')

def verify(prefix, output, export=None):
    prefix = Path(prefix).resolve(strict=True)
    output = Path(output).resolve()
    records=[]
    try:
        libs={p.relative_to(prefix).as_posix() for p in prefix.rglob('*.lib')}
        if libs != {'lib/axiom.lib'} or list(prefix.rglob('*.a')):
            raise RuntimeError(f'consumer package contains internal archives: {libs}')
        with tempfile.TemporaryDirectory(prefix='axiom-dll-consumer-') as temp:
            root=Path(temp)
            shutil.copytree(Path(__file__).parent/'consumer', root/'example')
            configure=['cmake','-S',root/'example','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DAxiomSDK_DIR='+str(prefix/'lib/cmake/AxiomSDK')]
            run([*configure,'-B',root/'build','-DAXIOM_EXAMPLE_CRT=MD'],root,records)
            run(['cmake','--build',root/'build'],root,records)
            # Use an unrelated CWD: assets must resolve from the deployed DLL/exe.
            run([root/'build/axiom_example.exe','--test'],Path(tempfile.gettempdir()),records)
            identities=[json.loads(line.removeprefix('BUILD ')) for line in records[-1]['stdout'].splitlines() if line.startswith('BUILD ')]
            expected_version=json.loads((prefix/'share/axiom-sdk/version.json').read_text(encoding='utf-8'))['version']
            if len(identities)!=1 or identities[0]['sdkVersion']!=expected_version:
                raise RuntimeError('missing or mismatched actual DLL identity')
            expected=os.environ.get('AXIOM_EXPECTED_REVISION')
            if expected and (identities[0]['sourceRevision']!=expected or identities[0]['dirty']):
                raise RuntimeError('DLL source does not match the frozen clean release revision')
            run([root/'build/axiom_c_loader.exe'],Path(tempfile.gettempdir()),records)
            run([root/'build/axiom_bridge_tests.exe'],Path(tempfile.gettempdir()),records)
            run(['dumpbin','/dependents',prefix/'bin/axiom.dll'],root,records)
            imports=records[-1]['stdout'].lower()
            if any(name in imports for name in ('protobuf','abseil','skia.dll','vcruntime','msvcp','ucrtbase')):
                raise RuntimeError('DLL unexpectedly depends on a component/CRT DLL')
            run(['dumpbin','/exports',prefix/'bin/axiom.dll'],root,records)
            exported=re.findall(r'^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)\s*$',records[-1]['stdout'],re.MULTILINE)
            expected={'axiom_abi_version','axiom_create','axiom_call','axiom_window','axiom_close','axiom_abandon','axiom_buffer_size','axiom_buffer_copy','axiom_buffer_release','axiom_input_source','axiom_input_begin','axiom_input_submit','axiom_input_cancel'}
            expected.update({'axiom_subscribe','axiom_unsubscribe','axiom_dispatch_events'})
            if set(exported)!=expected:
                raise RuntimeError(f'wrong v1 export surface: {exported}')
            if export:
                # Deploy the demo with /MT, while retaining the independent
                # /MD qualification above. Users need no VC redistributable.
                run([*configure,'-B',root/'deploy','-DAXIOM_EXAMPLE_CRT=MT'],root,records)
                run(['cmake','--build',root/'deploy','--target','axiom_example'],root,records)
                run([root/'deploy/axiom_example.exe','--test'],Path(tempfile.gettempdir()),records)
                run(['dumpbin','/dependents',root/'deploy/axiom_example.exe'],root,records)
                if any(name in records[-1]['stdout'].lower() for name in ('vcruntime','msvcp','ucrtbase')):
                    raise RuntimeError('deployed example unexpectedly needs a VC runtime DLL')
                export=Path(export);export.mkdir(parents=True,exist_ok=False)
                for name in ('axiom_example.exe','axiom.dll','icudtl.dat'):
                    shutil.copy2(root/'deploy'/name,export/name)
                shutil.copytree(root/'deploy/share',export/'share')
            result={'status':'PASS','commands':records,'build_identity':identities[0],'dll_sha256':hashlib.sha256((prefix/'bin/axiom.dll').read_bytes()).hexdigest(),'exports':exported,'physical_ime_touch':'PHYSICAL_PENDING'}
    except Exception as e:
        result={'status':'FAIL','error':str(e),'commands':records}
        raise
    finally:
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps(result,indent=2),encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--prefix',required=True);p.add_argument('--output',required=True);p.add_argument('--export-dir')
    a=p.parse_args();verify(a.prefix,a.output,a.export_dir)
