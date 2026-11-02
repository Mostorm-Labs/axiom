#!/usr/bin/env python3
"""Build/install the private native Session binding using locked SDKs only.

This produces development artifacts. RN app/physical qualification is separate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
from tools.semantic.cmake_consumer import consumer_cmake_arguments

TARGETS = {
    'macos-arm64': ('macos-arm64', 'macos-arm64-metal'),
    'ios-arm64': ('ios-arm64', 'ios-arm64-metal'),
    'ios-simulator-arm64': ('ios-simulator-arm64', 'ios-simulator-arm64-metal'),
    'android-arm64-v8a': ('android-arm64-v8a', 'android-arm64-v8a-gles3'),
    'android-x86_64': ('android-x86_64', 'android-x86_64-gles3'),
    'windows-x64': ('windows-x64-msvc-static', 'windows-x64-d3d12'),
}


def run(command, env=None):
    print('COMMAND', json.dumps([str(v) for v in command]), flush=True)
    subprocess.run([str(v) for v in command], check=True, cwd=REPO, env=env)


def build(target, directory, output, ndk=None, offline=False):
    if output.exists():
        raise ValueError('refusing to overwrite native install: ' + str(output))
    semantic, skia_target = TARGETS[target]
    directory.mkdir(parents=True, exist_ok=True)
    facts_path = directory / 'environment.json'
    run([sys.executable, 'tools/setup_build_environment.py', *([] if offline else ['--core']), '--semantic',
         '--target', semantic, '--facts-output', facts_path, '--status', *(['--offline'] if offline else [])])
    run([sys.executable, 'tools/setup_build_environment.py', '--semantic', '--target', semantic,
         '--offline', '--status'])
    # Verifies installed SDK identities/content and reuses accepted packages.
    run([sys.executable, 'tools/skia/fetch.py', '--profile', 'tools/skia/profiles/r1-full-v1.json',
         '--lock', 'r1-full-skia-sdk.lock.json', '--target', skia_target, '--variant', 'release'])
    facts = json.loads(facts_path.read_text())
    env = {**os.environ, **facts['environment']}
    skia = (REPO / '.deps/skia-sdk' / skia_target / 'release').resolve(strict=True)
    configure = ['cmake', '-S', REPO, '-B', directory, '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF', '-DCANVAS_BUILD_POC01=OFF',
                 '-DCANVAS_BUILD_RENDER=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
                 '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON', '-DAXIOM_BUILD_NATIVE_SDK=ON',
                 '-DAXIOM_BUILD_RUNTIME_QUALIFICATION=OFF', '-DAXIOM_INSTALL_DEVELOPER_COMPONENTS=OFF',
                 '-DAXIOM_BUILD_WINDOWS_SDK=OFF', '-DAXIOM_BUILD_DEBUG_UI=OFF',
                 '-DCANVAS_SKIA_SDK_ROOT=' + str(skia), '-DCanvasSkia_DIR=' + str(skia / 'lib/cmake/CanvasSkia'),
                 *consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']), Path(env['AXIOM_PROTOC']))]
    if target.startswith('ios-'):
        configure += ['-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_ARCHITECTURES=arm64',
                      '-DCMAKE_OSX_DEPLOYMENT_TARGET=17.0',
                      '-DCMAKE_OSX_SYSROOT=' + ('iphonesimulator' if 'simulator' in target else 'iphoneos')]
    elif target == 'macos-arm64':
        configure += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0']
    elif target.startswith('android-'):
        ndk = Path(ndk or os.environ.get('ANDROID_NDK_HOME', ''))
        if not (ndk / 'build/cmake/android.toolchain.cmake').is_file():
            raise ValueError('Android target requires locked NDK 27.2.12479018 via --ndk')
        if '27.2.12479018' not in (ndk / 'source.properties').read_text():
            raise ValueError('NDK does not match the accepted SDK ABI')
        configure += ['-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
                      '-DANDROID_ABI=' + target.removeprefix('android-'), '-DANDROID_PLATFORM=android-26',
                      '-DANDROID_STL=c++_static']
    else:
        configure += ['-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded']
    run(configure, env)
    run(['cmake', '--build', directory, '--target', 'axiom_native', '--parallel', '6'], env)
    run(['cmake', '--install', directory, '--prefix', output, '--component', 'AxiomNative'], env)
    metadata = {'format': 'axiom-native-development-v1', 'target': target, 'runtime_name': 'axiom',
                'sdk_version': json.loads((REPO / 'sdk/version.json').read_text())['version'],
                'source_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip(),
                'dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=REPO, text=True).strip()),
                'qualification': 'NOT_QUALIFIED',
                'files': {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in output.rglob('*') if p.is_file()}}
    (output / 'native-manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    return metadata


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=TARGETS, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ndk', type=Path)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    metadata = build(args.target, args.build.resolve(), args.output.resolve(), args.ndk, args.offline)
    print(json.dumps({k: v for k, v in metadata.items() if k != 'files'}))
