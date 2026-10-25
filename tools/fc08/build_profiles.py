"""Resolve recorded facts into opt-in verification-host builds, no SDK production."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--profile',choices=['windows-native','web-wasm'],required=True)
    args = parser.parse_args()
    web = args.profile == 'web-wasm'
    facts = json.loads((ROOT / ('out/fc08/web-environment.json' if web else 'out/fc08/native-environment.json')).read_text())
    env = {**os.environ,**facts['environment']}
    target = 'web-wasm-webgl2' if web else 'windows-x64-d3d12'
    sdk = ROOT / '.deps/skia-sdk' / target / 'release'
    build = ROOT / 'out/fc08' / args.profile
    evidence = ROOT / '.aegis/results/FC-08-VERIFY-MATERIALIZE/builds' / args.profile
    evidence.mkdir(parents=True,exist_ok=True)
    commands = []
    def run(argv,name):
        result = subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
        (evidence/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8',newline='\n')
        commands.append({'argv':list(map(str,argv)),'exit_code':result.returncode,'log':(evidence/(name+'.log')).relative_to(ROOT).as_posix()})
        (evidence/'invocations.json').write_text(json.dumps(commands,indent=2)+'\n')
        print(name,result.returncode,flush=True)
        if result.returncode: raise SystemExit(result.returncode)
    run([sys.executable,'tools/skia/fetch.py','--profile','tools/skia/profiles/r1-full-v1.json',
         '--lock','r1-full-skia-sdk.lock.json','--target',target,'--variant','release'],'skia-consumer')
    config = ['cmake','-S','.', '-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON',
        '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_BUILD_RENDER=ON',
        '-DCANVAS_RF01_BUILD_TESTS=OFF','-DCANVAS_RENDER_BUILD_TESTS=OFF','-DCANVAS_BUILD_INK_PLAYGROUND=OFF',
        '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DCANVAS_SKIA_SDK_VARIANT=release',
        f'-DCANVAS_SKIA_SDK_ROOT={sdk.as_posix()}','-DAXIOM_BUILD_FC08_VERIFICATION=ON',
        *consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))]
    if web:
        emsdk = Path(os.environ.get('EMSDK','C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6'))
        env['EMSDK'] = str(emsdk)
        env['EM_CONFIG'] = str(emsdk/'.emscripten')
        env['EMSDK_PYTHON'] = sys.executable
        config += [f'-DCMAKE_TOOLCHAIN_FILE={(emsdk/"upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake").as_posix()}']
    run(config,'configure')
    run(['cmake','--build',build,'--target','axiom_fc08_baseline_probe','--parallel','6'],'build-probe')
    if not web:
        run([build/'tests/fc08/axiom_fc08_baseline_probe.exe','--discover'],'discovery')


if __name__ == '__main__': main()
