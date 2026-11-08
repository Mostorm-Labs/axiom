"""Explicit consumer builds using already-resolved locked SDK facts only."""
import argparse,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments

def build(facts,output,cache,shared,qualified):
    env=json.loads(Path(facts).read_text())['environment'];deps=Path(env['AXIOM_DEPS_DIR'])
    arguments=['cmake','-S',str(ROOT),'-B',str(output),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_TOOLCHAIN_FILE='+str(deps/'emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'),
        '-DCANVAS_SKIA_SDK_ROOT='+str(deps/'skia-sdk/web-wasm-webgl2/release'),
        '-DAXIOM_DEPS_DIR='+str(deps),'-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_RENDER=ON',
        '-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON',
        '-DAXIOM_BUILD_JS_SDK=ON','-DAXIOM_BUILD_RUNTIME_QUALIFICATION=OFF','-DBUILD_TESTING=OFF',
        '-DCANVAS_RENDER_BUILD_TESTS=OFF','-DAXIOM_INSTALL_DEVELOPER_COMPONENTS=OFF',
        '-DAXIOM_ENABLE_FRAME_TIMING=OFF','-DAXIOM_G5_UR0_DIAGNOSTICS='+('ON' if qualified else 'OFF'),
        f'-DAXIOM_RENDER_CACHE_MIB={cache}',f'-DAXIOM_RENDER_SHARED_MIB={shared}',
        *consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))]
    if (cache,shared) not in [(32,96),(64,192)]:raise ValueError('unfrozen budget')
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    run_env={**os.environ,'EMSDK_PYTHON':sys.executable}
    (output/'ur0b-build-request.json').write_text(json.dumps({'command':arguments,'facts':str(Path(facts).resolve()),'python':sys.executable,'qualification_only':qualified},indent=2)+'\n')
    for name,command in [('configure',['unused']),('build',['cmake','--build',str(output),'--target','axiom_js_runtime','-j4'])]:
        if name=='configure':command=arguments
        with (output/(name+'.log')).open('w') as f:subprocess.run(command,env=run_env,check=True,stdout=f,stderr=subprocess.STDOUT)
    return output/'sdk/web'

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--facts',required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--normal',action='store_true');p.add_argument('--cache',type=int,default=64);p.add_argument('--shared',type=int,default=192);a=p.parse_args()
    print(build(a.facts,a.output,a.cache,a.shared,not a.normal))
