#!/usr/bin/env python3
"""Build, independently exercise, and package one locked Axiom preview target."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import zipfile
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
from tools.release.preview_package import build_assets, sha256
from tools.release.consumer_package import package as build_consumer_assets


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
        node_root = emsdk / 'node/24.19.0_64bit'
        node = node_root / 'node.exe'
        verifier = Path(__file__).resolve().parents[2] / 'verification/sdk_js/verify_browser.mjs'
        evidence=Path(env.get('AXIOM_VERIFICATION_OUTPUT_DIR',root/'evidence'))
        evidence.mkdir(parents=True,exist_ok=True)
        run([node, verifier, root, chrome,evidence/'zip-browser.json'], env)
        # Exercise npm's actual publish whitelist, rather than only the ZIP.
        # This remains a local pack operation; it does not publish the package.
        packed = root / 'npm-packed'
        packed.mkdir()
        command = [node, node_root / 'node_modules/npm/bin/npm-cli.js', 'pack',
                   '--ignore-scripts', '--json', '--pack-destination', packed]
        print('COMMAND', json.dumps([str(arg) for arg in command]), flush=True)
        result = subprocess.run([str(arg) for arg in command], cwd=root, env=env,
                                text=True, capture_output=True, check=False)
        print(result.stdout, flush=True)
        if result.stderr:
            print(result.stderr, file=sys.stderr, flush=True)
        result.check_returncode()
        entries = json.loads(result.stdout)
        if len(entries) != 1:
            raise RuntimeError('npm must produce exactly one SDK package')
        filename = entries[0]['filename']
        if Path(filename).name != filename:
            raise RuntimeError('invalid npm package filename')
        extracted = packed / 'extracted'
        with tarfile.open(packed / filename) as package:
            package.extractall(extracted, filter='data')
        consumer = extracted / 'package'
        required = ['axiom.mjs', 'internal/axiom.mjs', 'axiom.d.mts', 'axiom.wasm',
                    'axiom.data', 'pointer_input.mjs', 'text_input.mjs',
                    'example.html', 'serve.py']
        required += [path.relative_to(root).as_posix()
                     for path in (root / 'licenses').rglob('*') if path.is_file()]
        for name in required:
            if not (consumer / name).is_file() or sha256(root / name) != sha256(consumer / name):
                raise RuntimeError(f'npm SDK package omitted or changed {name}')
        application = packed / 'application'
        application.mkdir()
        (application / 'package.json').write_text(
            '{"name":"axiom-external-type-consumer","private":true,"type":"module"}\n',
            encoding='utf-8')
        install = [node, node_root / 'node_modules/npm/bin/npm-cli.js', 'install',
                   '--offline', '--ignore-scripts', '--no-audit', '--no-fund', packed / filename]
        print('COMMAND', json.dumps([str(arg) for arg in install]), flush=True)
        subprocess.run([str(arg) for arg in install], cwd=application, env=env, check=True)
        types = (verifier.parent / 'api_types_test.mts').read_text(encoding='utf-8')
        (application / 'consumer.mts').write_text(
            types.replace("'../../sdk/web/axiom.mjs'", "'@mostorm/axiom'"), encoding='utf-8')
        typecheck = [node, node_root / 'node_modules/npm/bin/npm-cli.js', 'exec',
                     '--yes', '--ignore-scripts', '--package', 'typescript@5.9.3', '--',
                     'tsc', '--noEmit', '--strict', '--target', 'es2022', '--module',
                     'nodenext', '--moduleResolution', 'nodenext', 'consumer.mts']
        print('COMMAND', json.dumps([str(arg) for arg in typecheck]), flush=True)
        subprocess.run([str(arg) for arg in typecheck], cwd=application, env=env, check=True)
        run([node, verifier, consumer, chrome,evidence/'npm-browser.json'], env)


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
    env['AXIOM_EXPECTED_REVISION'] = args.revision
    semantic = Path(facts['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT']).resolve(strict=True)
    protoc = Path(facts['environment']['AXIOM_PROTOC']).resolve(strict=True)
    skia = args.skia.resolve(strict=True)
    build = args.build.resolve()
    env['AXIOM_VERIFICATION_OUTPUT_DIR'] = str(build/'evidence')
    prefix = build / ('preview-install-' + args.revision[:12])
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
        configure += ['-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', '-DAXIOM_BUILD_WINDOWS_SDK=ON']
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
        targets = ['axiom_sdk', 'axiom_sdk_public_resources_tests',
                   'axiom_sdk_public_document_identity_tests', 'axiom_sdk_public_document_operations_tests',
                   'axiom_sdk_public_document_snapshot_tests', 'axiom_fc08_section_codec_tests',
                   'axiom_fc02_text_tests','axiom_fc03_image_facade_tests','axiom_fc05_connector_facade_tests',
                   'axiom_fc08_section_consumer_tests','axiom_selection_summary_tests','axiom_selection_input_integration_tests',
                   'axiom_minimal_windows_text_tool_entry_tests','axiom_minimal_windows_text_pipeline_tests',
                   'axiom_windows_public_adapter_tests','axiom_fc00x_runtime_composition_tests','axiom_fc00x_runtime_facade_tests',
                   'axiom_brush_batch_frame_retry_tests']
    else:
        targets = ['axiom_js_runtime']
    run(['cmake', '--build', build, '--target', *targets, '--parallel', '6'], env)
    if args.platform == 'windows':
        required = {'SDKPublicDocumentSnapshot','SDKPublicDocumentOperations','SDKPublicDocumentIdentity','SDKPublicResources',
                    'FC02Text','FC03ImageFacade','FC05ConnectorFacade','FC08SectionConsumer','SelectionBatch3Summary',
                    'FC01ASelectionInputIntegration','FC02WindowsTextToolEntry','FC02WindowsTextToolFocus',
                    'FC02WindowsTextToolTouchFocus','FC02WindowsTextToolAfterInk','FC02WindowsTextFormatControls',
                    'FC02WindowsTextIngress','SDKWindowsPublicAdapter','FC00XRuntimeComposition','FC00XRuntimeFacade',
                    'FC00XBrushBatchEvaluation','FC00XBrushPressureCapability','FC00XBrushBatchSemantics',
                    'FC00XCanonicalBusyPreviewProgress','FC00XPreviewBusyCanonicalProgress','FC00XBusyCanonicalPendingFeedback',
                    'FC00XFatalProviderFailure','FC00XCancelKeepsViewport','FC00XFitFullScene'}
        run(['ctest', '--test-dir', build, '-R','^('+'|'.join(sorted(required))+')$',
             '--no-tests=error', '--output-on-failure','--output-junit',build/'sdk-consumer-regressions.xml'], env)
        cases=ET.parse(build/'sdk-consumer-regressions.xml').getroot().findall('.//testcase')
        if {c.get('name') for c in cases}!=required or any(c.find(s) is not None for c in cases for s in ('failure','error','skipped')):
            raise RuntimeError('missing, skipped or failed required native consumer regression')
        run([build / 'runtime/semantic/tests/axiom_fc08_section_codec_tests.exe'], env)
        run(['cmake', '--install', build, '--component', 'AxiomSDK', '--prefix', prefix], env)
        demo = build / ('dll-demo-' + args.revision[:12])
        run([sys.executable, '-X', 'utf8', repo / 'verification/sdk_windows_dll/run_external.py', '--prefix', prefix,
             '--output', build / 'windows-external.json', '--export-dir', demo], env)
    else:
        run([emsdk / 'node/24.19.0_64bit/node.exe', repo / 'verification/sdk_js/api_test.mjs'], env)
        run([emsdk / 'node/24.19.0_64bit/node.exe', repo / 'verification/sdk_js/verify_browser.mjs', build / 'sdk/web', args.chrome,build/'js-browser.json'], env)
        run(['cmake', '--install', build, '--prefix', prefix], env)
        demo = prefix / 'js'
    assets = build_consumer_assets(args.platform, prefix, demo, repo, args.output, args.revision)
    if args.platform == 'web':
        verify_js_bundle(assets[0], env, emsdk, args.chrome)
    else:
        with tempfile.TemporaryDirectory(prefix='axiom-dll-zip-') as temp:
            with zipfile.ZipFile(assets[0]) as archive:
                archive.extractall(temp)
            run([sys.executable, '-X', 'utf8', repo / 'verification/sdk_windows_dll/run_external.py', '--prefix', temp,
                 '--output', build / 'windows-zip-external.json'], env)


if __name__ == '__main__':
    main()
