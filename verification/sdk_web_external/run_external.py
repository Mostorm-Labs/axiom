#!/usr/bin/env python3
"""Configure a Web SDK consumer outside the repository and preserve raw results."""
import argparse, json, os, shutil, subprocess, sys, tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--build', required=True, type=Path)
p.add_argument('--semantic-root', required=True, type=Path)
p.add_argument('--skia-root', required=True, type=Path)
p.add_argument('--emsdk', required=True, type=Path)
p.add_argument('--chrome', required=True, type=Path)
p.add_argument('--output', required=True, type=Path)
p.add_argument('--export-dir', type=Path, help='new directory for the verified interactive browser assets')
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
for sample, demo in [('text_bridge.inl','text_bridge.inl'), ('text_input.mjs','text_input.js')]:
    if (Path(__file__).parent/'consumer'/sample).read_bytes() != (repo/'apps/axiom_canvas_demo/platform/web'/demo).read_bytes():
        raise RuntimeError('example text bridge diverged from the verified Web host: '+sample)
env = os.environ.copy()
env['EM_CONFIG'] = str(a.emsdk / '.emscripten')
env['EMSDK_PYTHON'] = sys.executable
env['EMSDK_NODE'] = str(a.emsdk / 'node/24.19.0_64bit/node.exe')
env['EMSDK'] = str(a.emsdk)
env['AXIOM_SDK_PYTHON'] = sys.executable
env['PATH'] = str(a.emsdk / 'upstream/emscripten') + os.pathsep + env['PATH']
result = {'status':'NOT_RUN','steps':[]}
with tempfile.TemporaryDirectory(prefix='axiom-web-sdk-') as tmp:
    root = Path(tmp).resolve()
    temp_root = Path(tempfile.gettempdir()).resolve()
    if not root.is_relative_to(temp_root) or not root.name.startswith('axiom-web-sdk-') or root.is_relative_to(repo):
        raise RuntimeError('consumer must be in its dedicated external temporary directory')
    source, install, build = root/'source', root/'install', root/'build'
    shutil.copytree(Path(__file__).parent/'consumer', source)
    # nlohmann/json is an application-side bridge dependency from the locked
    # lightweight core setup; it is not bundled into the Runtime SDK.
    shutil.copytree(repo/'.deps/nlohmann-json/single_include/nlohmann', source/'json/nlohmann')
    shutil.copyfile(repo/'assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf', source/'axiom-font.otf')
    shutil.copytree(repo/'assets/fonts/imported', source/'fonts')
    # Copy and validate the declared locked package outside every checkout.
    skia = root/'skia'
    shutil.copytree(a.skia_root.resolve(), skia)
    sys.path.insert(0,str(repo/'tools/skia'))
    from fetch import verify_installed
    lock=json.loads((repo/'r1-full-skia-sdk.lock.json').read_text(encoding='utf-8'))
    verify_installed(skia,lock,'web-wasm-webgl2',repo/'tools/skia/profiles/r1-full-v1.json','release')
    commands = [
      ['cmake','--install',str(a.build.resolve()),'--prefix',str(install)],
      ['cmake','-S',str(source),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
       '-DCMAKE_TOOLCHAIN_FILE='+str(a.emsdk/'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'),
       '-DAxiomRuntime_DIR='+str(install/'lib/cmake/AxiomRuntime'),
       '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=NEVER',
       '-DProtobuf_DIR='+str(a.semantic_root.resolve()/'lib/cmake/protobuf'),
       '-DCMAKE_PREFIX_PATH='+str(a.semantic_root.resolve()),
       '-DCanvasSkia_DIR='+str(skia/'lib/cmake/CanvasSkia')],
      ['cmake','--build',str(build)]]
    commands[1].append('-DAXIOM_NLOHMANN_INCLUDE_DIR='+str(source/'json'))
    for command in commands:
        run=subprocess.run(command,text=True,capture_output=True,env=env,check=False)
        result['steps'].append({'command':command,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
        if run.returncode:
            result['status']='FAIL'; break
    else:
        shutil.copyfile(source/'index.html',build/'index.html')
        shutil.copyfile(source/'pointer_input.mjs',build/'pointer_input.mjs')
        shutil.copyfile(source/'text_input.mjs',build/'text_input.mjs')
        shutil.copyfile(source/'serve.py',build/'serve.py')
        command=[str(a.emsdk/'node/24.19.0_64bit/node.exe'),str(Path(__file__).parent/'verify_browser.mjs'),
                 str(build),str(a.chrome.resolve())]
        try:
            run=subprocess.run(command,text=True,capture_output=True,env=env,check=False,timeout=180)
        except subprocess.TimeoutExpired as error:
            result['steps'].append({'command':command,'exit_code':None,'stderr':str(error),'timeout_seconds':180})
            result['status']='FAIL_BROWSER_TIMEOUT'
        else:
            result['steps'].append({'command':command,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
            result['status']='PASS_SCOPED' if run.returncode==0 else 'FAIL_BROWSER'
        if result['status']=='PASS_SCOPED':
            command=[str(a.emsdk/'node/24.19.0_64bit/node.exe'),str(Path(__file__).parent/'verify_manual_browser.mjs'),
                     str(build),str(a.chrome.resolve())]
            try:
                run=subprocess.run(command,text=True,capture_output=True,env=env,check=False,timeout=180)
            except subprocess.TimeoutExpired as error:
                result['steps'].append({'command':command,'exit_code':None,'stderr':str(error),'timeout_seconds':180})
                result['status']='FAIL_MANUAL_BROWSER_TIMEOUT'
            else:
                result['steps'].append({'command':command,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
                if run.returncode: result['status']='FAIL_MANUAL_BROWSER'
        if result['status']=='PASS_SCOPED':
            # Negative controls must fail for the declared package constraints.
            debug=[str(x) for x in commands[1]]
            debug[debug.index('-B')+1]=str(root/'debug-build')
            debug[debug.index('-DCMAKE_BUILD_TYPE=Release')]='-DCMAKE_BUILD_TYPE=Debug'
            run=subprocess.run(debug,text=True,capture_output=True,env=env,check=False)
            result['steps'].append({'command':debug,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr,
                                    'expected':'reject incompatible Debug configuration'})
            if run.returncode==0 or 'matching Emscripten compiler and Release' not in run.stderr:
                result['status']='FAIL_NEGATIVE_DEBUG'
            manifest=skia/'manifest.json'; original=manifest.read_bytes()
            try:
                manifest.write_bytes(original+b'\n')
                altered=[str(x) for x in commands[1]]
                altered[altered.index('-B')+1]=str(root/'mismatch-build')
                run=subprocess.run(altered,text=True,capture_output=True,env=env,check=False)
                result['steps'].append({'command':altered,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr,
                                        'expected':'reject altered Skia manifest'})
                if run.returncode==0 or 'exact locked Semantic and Skia SDKs' not in run.stderr:
                    result['status']='FAIL_NEGATIVE_LOCK'
            finally:
                manifest.write_bytes(original)
        if result['status']=='PASS_SCOPED' and a.export_dir:
            files=['index.html','external_web.js','external_web.wasm','external_web.data',
                   'pointer_input.mjs','text_input.mjs','serve.py']
            for name in files:
                if not (build/name).is_file(): raise RuntimeError('missing browser asset: '+name)
            destination=a.export_dir.resolve()
            destination.mkdir(parents=True,exist_ok=False)
            for name in files: shutil.copyfile(build/name,destination/name)
            result['export_dir']=str(destination)
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'status':result['status'],'output':str(a.output)}))
sys.exit(0 if result['status']=='PASS_SCOPED' else 1)
