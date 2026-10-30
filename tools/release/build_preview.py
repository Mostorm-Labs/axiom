#!/usr/bin/env python3
"""Build, independently exercise, and package one locked Axiom preview target."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
from tools.release.preview_package import build_assets


def run(args, env=None):
    print('COMMAND', json.dumps([str(arg) for arg in args]), flush=True)
    subprocess.run([str(arg) for arg in args], env=env, check=True)


def verify_bundle(platform, archive, env, emsdk=None, chrome=None):
    with tempfile.TemporaryDirectory(prefix='axiom-preview-consumer-') as temp:
        root = Path(temp).resolve()
        with zipfile.ZipFile(archive) as package:
            package.extractall(root)
        source, build = root / 'example', root / 'build'
        configure = ['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                     '-DAxiomRuntime_DIR=' + str(root / 'sdk/lib/cmake/AxiomRuntime'),
                     '-DCMAKE_PREFIX_PATH=' + str(root / 'dependencies/semantic'),
                     '-DProtobuf_DIR=' + str(root / 'dependencies/semantic/lib/cmake/protobuf'),
                     '-DCanvasSkia_DIR=' + str(root / 'dependencies/skia/lib/cmake/CanvasSkia')]
        if platform == 'windows':
            configure.append('-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')
        else:
            configure += ['-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=NEVER',
                          '-DCMAKE_TOOLCHAIN_FILE=' + str(emsdk / 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake')]
        run(configure, env)
        run(['cmake', '--build', build], env)
        if platform == 'windows':
            junit = root / 'results.xml'
            run(['ctest', '--test-dir', build, '-R', '^ExternalWindowsSdkWindow$',
                 '--no-tests=error', '--output-on-failure', '--output-junit', junit], env)
            cases = ET.parse(junit).getroot().findall('.//testcase')
            if len(cases) != 1 or any(cases[0].find(status) is not None for status in ('failure', 'error', 'skipped')):
                raise RuntimeError('packaged Windows SDK consumer did not run and pass')
        else:
            run([emsdk / 'node/24.19.0_64bit/node.exe',
                 Path(__file__).resolve().parents[2] / 'verification/sdk_web_external/verify_manual_browser.mjs',
                 build, chrome], env)
            run([emsdk / 'node/24.19.0_64bit/node.exe',
                 Path(__file__).resolve().parents[2] / 'verification/sdk_js/verify_browser.mjs',
                 root / 'sdk/js', chrome], env)


def verify_js_bundle(archive, env, emsdk, chrome):
    with tempfile.TemporaryDirectory(prefix='axiom-js-package-') as temp:
        root = Path(temp).resolve()
        with zipfile.ZipFile(archive) as package:
            package.extractall(root)
        run([emsdk / 'node/24.19.0_64bit/node.exe',
             Path(__file__).resolve().parents[2] / 'verification/sdk_js/verify_browser.mjs', root, chrome], env)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform', required=True, choices=['windows', 'web'])
    p.add_argument('--facts', required=True, type=Path)
    p.add_argument('--skia', required=True, type=Path)
    p.add_argument('--build', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--revision', required=True)
    p.add_argument('--emsdk', type=Path)
    p.add_argument('--chrome', type=Path)
    args = p.parse_args()
    repo = Path(__file__).resolve().parents[2]
    actual_revision = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip()
    if actual_revision != args.revision or dirty:
        raise RuntimeError('preview artifacts require the exact requested revision and a clean source checkout')
    facts = json.loads(args.facts.read_text(encoding='utf-8'))
    env = os.environ.copy()
    env.update(facts['environment'])
    semantic = Path(facts['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT']).resolve(strict=True)
    protoc = Path(facts['environment']['AXIOM_PROTOC']).resolve(strict=True)
    skia = args.skia.resolve(strict=True)
    build = args.build.resolve()
    prefix = build / 'preview-install'
    configure = ['cmake', '-S', repo, '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=' + ('ON' if args.platform == 'windows' else 'OFF'), '-DCANVAS_BUILD_POC01=OFF',
        '-DCANVAS_BUILD_SEMANTIC=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
        '-DCANVAS_BUILD_RENDER=ON', '-DCANVAS_BUILD_RF01=ON',
        '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON', '-DAXIOM_BUILD_DEBUG_UI=ON',
        '-DAXIOM_DEBUG_UI_PROFILE=full', '-DCANVAS_SKIA_SDK_VARIANT=release',
        '-DCANVAS_SKIA_SDK_ROOT=' + skia.as_posix(),
        '-DCanvasSkia_DIR=' + (skia / 'lib/cmake/CanvasSkia').as_posix(),
        *consumer_cmake_arguments(semantic, protoc)]
    if args.platform == 'windows':
        configure += ['-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded']
    else:
        if not args.emsdk or not args.chrome:
            p.error('Web requires --emsdk and --chrome')
        emsdk = args.emsdk.resolve(strict=True)
        env.update({'EMSDK': str(emsdk), 'EM_CONFIG': str(emsdk / '.emscripten'),
                    'EMSDK_PYTHON': sys.executable,
                    'EMSDK_NODE': str(emsdk / 'node/24.19.0_64bit/node.exe')})
        env['PATH'] = str(emsdk / 'upstream/emscripten') + os.pathsep + env['PATH']
        configure += ['-DCMAKE_TOOLCHAIN_FILE=' + (emsdk / 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake').as_posix(), '-DAXIOM_BUILD_JS_SDK=ON']
    run(configure, env)
    if args.platform == 'windows':
        config = build / 'AxiomRuntimeConfig.cmake'
        if not config.is_file() or 'requires Windows x64, MSVC 19.51.' not in config.read_text(encoding='utf-8'):
            raise RuntimeError('locked Windows SDK requires MSVC 19.51; refusing a mismatched toolchain')
        targets = ['axiom_minimal_runtime_host_windows', 'axiom_sdk_public_resources_tests',
                   'axiom_sdk_public_document_identity_tests', 'axiom_sdk_public_document_operations_tests',
                   'axiom_sdk_public_document_snapshot_tests', 'axiom_fc08_section_codec_tests']
    else:
        targets = ['axiom_web_runtime_adapter', 'axiom_canvas_demo_web', 'axiom_js_runtime']
    run(['cmake', '--build', build, '--target', *targets, '--parallel', '6'], env)
    if args.platform == 'windows':
        run(['ctest', '--test-dir', build, '-R',
             '^(SDKPublicDocumentSnapshot|SDKPublicDocumentOperations|SDKPublicDocumentIdentity|SDKPublicResources)$',
             '--no-tests=error', '--output-on-failure'], env)
        run([build / 'runtime/semantic/tests/axiom_fc08_section_codec_tests.exe'], env)
        run([sys.executable, repo / 'verification/sdk_cpp_windows/run_external.py', '--build', build,
             '--semantic-root', semantic, '--skia-root', skia, '--output', build / 'windows-external.json'], env)
        demo = build / 'apps/minimal_runtime_host'
    else:
        run([emsdk / 'node/24.19.0_64bit/node.exe', repo / 'verification/sdk_js/api_test.mjs'], env)
        run([emsdk / 'node/24.19.0_64bit/node.exe', repo / 'verification/sdk_js/verify_browser.mjs', build / 'sdk/web', args.chrome], env)
        demo = build / ('external-web-demo-' + args.revision[:12])
        run([sys.executable, repo / 'verification/sdk_web_external/run_external.py', '--build', build,
             '--semantic-root', semantic, '--skia-root', skia, '--emsdk', emsdk, '--chrome', args.chrome,
             '--output', build / 'web-external.json', '--export-dir', demo], env)
    run(['cmake', '--install', build, '--prefix', prefix], env)
    assets = build_assets(args.platform, prefix, semantic, skia, demo, repo, args.output, args.revision)
    verify_bundle(args.platform, assets[0], env, emsdk if args.platform == 'web' else None, args.chrome)
    if args.platform == 'web':
        if len(assets) != 3:
            raise RuntimeError('standalone JavaScript SDK asset is missing')
        verify_js_bundle(assets[2], env, emsdk, args.chrome)


if __name__ == '__main__':
    main()
