#!/usr/bin/env python3
"""Package a built Windows or Web SDK and its demo with their required assets."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def package_tree(stage, archive, required, revision, kind):
    stage, archive = Path(stage), Path(archive)
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('source revision must be a full Git SHA-1')
    if archive.exists() or (stage / 'manifest.json').exists():
        raise ValueError('refusing to overwrite a package or manifest')
    for item in required:
        path = stage / item
        if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(stage.resolve()):
            raise ValueError(f'missing or invalid required file: {item}')
    paths = sorted(stage.rglob('*'))
    if any(path.is_symlink() for path in paths):
        raise ValueError('package input contains a symlink')
    files = [path for path in paths if path.is_file()]
    manifest = {'format': 'axiom-sdk-preview-v1', 'kind': kind,
                'source_revision': revision,
                'files': {path.relative_to(stage).as_posix(): sha256(path) for path in files}}
    archive.parent.mkdir(parents=True, exist_ok=True)
    try:
        with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
            for path in files:
                z.write(path, path.relative_to(stage).as_posix())
            z.writestr('manifest.json', json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        with zipfile.ZipFile(archive) as z:
            if z.testzip() is not None:
                raise ValueError('package archive failed integrity verification')
    except Exception:
        archive.unlink(missing_ok=True)
        raise


def copy_file(source, destination):
    if not source.is_file() or source.is_symlink():
        raise ValueError(f'missing required source: {source}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def build_assets(platform, prefix, semantic, skia, demo, repo, output, revision):
    if platform not in ('windows', 'web'):
        raise ValueError('unknown platform')
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('invalid source revision')
    output.mkdir(parents=True, exist_ok=True)
    sdk_name = f'axiom-sdk-{platform}-preview.zip'
    demo_name = f'axiom-demo-{platform}-preview.zip'
    js_name = 'axiom-sdk-js-preview.zip'
    if (output / sdk_name).exists() or (output / demo_name).exists() or (platform == 'web' and (output / js_name).exists()):
        raise ValueError('refusing to overwrite an existing release asset')
    config = prefix / 'lib/cmake/AxiomRuntime/AxiomRuntimeConfig.cmake'
    if not config.is_file() or any(sha256(root / 'manifest.json') not in config.read_text(encoding='utf-8')
                                   for root in (semantic, skia)):
        raise ValueError('installed SDK does not bind these exact Semantic and Skia packages')
    with tempfile.TemporaryDirectory(prefix='axiom-preview-') as tmp:
        root = Path(tmp)
        sdk = root / 'sdk-package'
        sdk.mkdir()
        shutil.copytree(prefix, sdk / 'sdk', symlinks=True)
        shutil.copytree(semantic, sdk / 'dependencies/semantic', symlinks=True)
        shutil.copytree(skia, sdk / 'dependencies/skia', symlinks=True)
        # Include the exact third-party identities alongside the actual packages.
        copy_file(repo / 'semantic-sdk.lock.json', sdk / 'locks/semantic-sdk.lock.json')
        copy_file(repo / 'r1-full-skia-sdk.lock.json', sdk / 'locks/r1-full-skia-sdk.lock.json')
        copy_file(repo / 'deps.lock.json', sdk / 'locks/deps.lock.json')
        sample = 'sdk_cpp_windows' if platform == 'windows' else 'sdk_web_external'
        shutil.copytree(repo / 'verification' / sample / 'consumer', sdk / 'example', symlinks=True)
        copy_file(repo / 'verification' / sample / 'README.md', sdk / 'example/README.md')
        copy_file(repo / 'assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf', sdk / 'example/axiom-font.otf')
        copy_file(repo / 'assets/fonts/noto-sans-cjk-sc/LICENSE.txt', sdk / 'example/axiom-font.LICENSE.txt')
        copy_file(repo / 'assets/fonts/noto-sans-cjk-sc/NOTICE.txt', sdk / 'example/axiom-font.NOTICE.txt')
        shutil.copytree(repo / 'assets/fonts/imported', sdk / 'example/fonts', symlinks=True)
        if platform == 'web':
            shutil.copytree(repo / '.deps/nlohmann-json/single_include/nlohmann', sdk / 'example/json/nlohmann', symlinks=True)
            copy_file(repo / '.deps/nlohmann-json/LICENSE.MIT', sdk / 'example/json/LICENSE.MIT')
        instructions = (
            'From this extracted directory, use a Developer PowerShell for the exact MSVC x64 compiler '\
            'recorded in sdk/lib/cmake/AxiomRuntime/AxiomRuntimeConfig.cmake, then run:\n\n'
            'cmake -S example -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded '
            '-DAxiomRuntime_DIR="$PWD/sdk/lib/cmake/AxiomRuntime" '
            '-DCMAKE_PREFIX_PATH="$PWD/dependencies/semantic" '
            '-DCanvasSkia_DIR="$PWD/dependencies/skia/lib/cmake/CanvasSkia"\n'
            'cmake --build build\nctest --test-dir build --output-on-failure\n'
        ) if platform == 'windows' else (
            'Activate Emscripten 6.0.6, then from this extracted directory run:\n\n'
            'emcmake cmake -S example -B build -G Ninja -DCMAKE_BUILD_TYPE=Release '
            '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=NEVER '
            '-DAxiomRuntime_DIR="$PWD/sdk/lib/cmake/AxiomRuntime" '
            '-DProtobuf_DIR="$PWD/dependencies/semantic/lib/cmake/protobuf" '
            '-DCMAKE_PREFIX_PATH="$PWD/dependencies/semantic" '
            '-DCanvasSkia_DIR="$PWD/dependencies/skia/lib/cmake/CanvasSkia"\n'
            'cmake --build build\npython build/serve.py\n'
        )
        (sdk / 'README.md').write_text(
            'Axiom SDK preview. This is not a stable ABI or a production release.\n'
            'Dependencies and their licenses are included under dependencies/. '
            'Font licenses accompany the example. See example/README.md and release notes for limitations.\n\n'
            + instructions, encoding='utf-8')
        required = ['README.md', 'sdk/lib/cmake/AxiomRuntime/AxiomRuntimeConfig.cmake',
                    'dependencies/semantic/manifest.json', 'dependencies/skia/manifest.json',
                    'example/CMakeLists.txt', 'example/axiom-font.otf']
        package_tree(sdk, output / sdk_name, required, revision, platform + '-sdk')
        app = root / 'demo-package'
        app.mkdir()
        if platform == 'windows':
            for name in ['axiom_minimal_runtime_host_windows.exe', 'icudtl.dat', 'axiom-font.otf',
                         'axiom-font.LICENSE.txt', 'axiom-font.NOTICE.txt']:
                copy_file(demo / name, app / name)
            shutil.copytree(demo / 'fonts', app / 'fonts', symlinks=True)
            instructions = 'Run axiom_minimal_runtime_host_windows.exe. Keep all files and fonts together. Press F12 for Debug UI.\n'
            app_required = ['axiom_minimal_runtime_host_windows.exe', 'icudtl.dat', 'axiom-font.otf', 'fonts/catalog.json']
        else:
            app_required = ['index.html', 'external_web.js', 'external_web.wasm', 'external_web.data',
                            'pointer_input.mjs', 'text_input.mjs', 'control_api.mjs', 'control_api.d.mts', 'serve.py']
            for name in app_required:
                copy_file(demo / name, app / name)
            copy_file(repo / 'assets/fonts/noto-sans-cjk-sc/LICENSE.txt', app / 'licenses/noto-cjk-LICENSE.txt')
            copy_file(repo / 'assets/fonts/noto-sans-cjk-sc/NOTICE.txt', app / 'licenses/noto-cjk-NOTICE.txt')
            font_root = repo / 'assets/fonts/imported'
            for path in font_root.rglob('*'):
                if path.is_file() and ('license' in path.name.lower() or 'ofl' in path.name.lower() or 'notice' in path.name.lower()):
                    copy_file(path, app / 'licenses/imported' / path.relative_to(font_root))
            instructions = 'Run python serve.py and open its printed URL. The server sets the required .mjs MIME type.\n'
        (app / 'README.md').write_text('Axiom Demo preview.\n' + instructions, encoding='utf-8')
        package_tree(app, output / demo_name, ['README.md', *app_required], revision, platform + '-demo')
        if platform == 'web' and (prefix / 'js').is_dir():
            js = root / 'js-package'
            shutil.copytree(prefix / 'js', js, symlinks=True)
            package_tree(js, output / js_name,
                         ['axiom.mjs', 'axiom.d.mts', 'axiom.wasm',
                          'axiom.data', 'pointer_input.mjs', 'text_input.mjs', 'example.html',
                          'serve.py', 'package.json', 'README.md', 'licenses/nlohmann-json-LICENSE.MIT'], revision, 'js-sdk')
    assets = [output / sdk_name, output / demo_name]
    if platform == 'web' and (prefix / 'js').is_dir():
        assets.append(output / js_name)
    return assets


def verify_release(directory, revision, require_js=False):
    expected = {f'axiom-{kind}-{platform}-preview.zip'
                for kind in ('sdk', 'demo') for platform in ('windows', 'web')}
    if require_js:
        expected.add('axiom-sdk-js-preview.zip')
    paths = {p.name: p for p in Path(directory).glob('*.zip')}
    if set(paths) != expected:
        raise ValueError(f'expected exactly {sorted(expected)}, got {sorted(paths)}')
    for name, path in sorted(paths.items()):
        with zipfile.ZipFile(path) as z:
            names = z.namelist()
            if len(names) != len(set(names)) or any(n.startswith('/') or '..' in Path(n).parts or '\\' in n for n in names):
                raise ValueError(f'unsafe or duplicate archive entries: {name}')
            manifest = json.loads(z.read('manifest.json'))
            kind = name.removeprefix('axiom-').removesuffix('-preview.zip').split('-')
            if manifest.get('source_revision') != revision or manifest.get('kind') != '-'.join(reversed(kind)):
                raise ValueError(f'wrong source revision or package kind: {name}')
            files = manifest['files']
            if set(files) != set(names) - {'manifest.json'}:
                raise ValueError(f'archive file list does not match manifest: {name}')
            for member, expected_hash in files.items():
                digest = hashlib.sha256()
                with z.open(member) as stream:
                    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                        digest.update(chunk)
                if digest.hexdigest() != expected_hash:
                    raise ValueError(f'archive member hash mismatch: {name}/{member}')
    checksums = ''.join(f'{sha256(path)}  {name}\n' for name, path in sorted(paths.items()))
    (Path(directory) / 'SHA256SUMS').write_text(checksums, encoding='ascii')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--verify-release', type=Path)
    p.add_argument('--require-js', action='store_true')
    p.add_argument('--platform', choices=['windows', 'web'])
    for name in ('prefix', 'semantic', 'skia', 'demo', 'repo', 'output'):
        p.add_argument('--' + name, type=Path)
    p.add_argument('--revision', required=True)
    args = p.parse_args()
    if args.verify_release:
        verify_release(args.verify_release, args.revision, require_js=args.require_js)
        return
    if not all((args.platform, args.prefix, args.semantic, args.skia, args.demo, args.repo, args.output)):
        p.error('packaging requires --platform, --prefix, --semantic, --skia, --demo, --repo and --output')
    for asset in build_assets(args.platform, args.prefix, args.semantic, args.skia, args.demo,
                              args.repo, args.output, args.revision):
        print(f'{sha256(asset)}  {asset.name}')


if __name__ == '__main__':
    main()
