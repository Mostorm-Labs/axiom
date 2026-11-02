"""Stage an isolated React Native consumer package from native SDK installs.

Staging is development delivery, never evidence of platform qualification.
No dependency archives or private Runtime headers belong in this package.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import plistlib
import shutil
import subprocess
import tempfile


PLATFORMS = {
    'android-arm64-v8a': ('android', 'lib/libaxiom_native.so', 'lib/arm64-v8a/libaxiom_native.so'),
    'android-x86_64': ('android', 'lib/libaxiom_native.so', 'lib/x86_64/libaxiom_native.so'),
    'macos-arm64': ('macos', 'lib/AxiomNative.framework', 'AxiomNative.framework'),
    'ios-arm64': ('ios', 'lib/AxiomNative.framework', 'AxiomNative.framework'),
    'ios-simulator-arm64': ('ios-simulator', 'lib/AxiomNative.framework', 'AxiomNative.framework'),
    'windows-x64': ('windows', 'bin/axiom_native.dll', 'bin/axiom_native.dll'),
}


def source_identity(repo):
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=repo, text=True).strip())
    return revision, dirty


def validate_install(root, target, version, revision):
    if target not in PLATFORMS:
        raise ValueError('unknown native target: ' + target)
    root = Path(root).resolve(strict=True)
    _, binary, _ = PLATFORMS[target]
    required = [binary, 'include/axiom/native.h', 'assets/axiom-font.otf',
                'assets/fonts/catalog.json', 'licenses/skia/Skia.txt',
                'licenses/semantic/Protobuf.txt', 'share/axiom-native/version.json']
    if target == 'windows-x64':
        required += ['lib/axiom_native.lib', 'assets/icudtl.dat']
    if any(not (root / name).exists() for name in required):
        raise ValueError('incomplete native install: ' + target)
    if json.loads((root / 'share/axiom-native/version.json').read_text())['version'] != version:
        raise ValueError('native SDK version mismatch: ' + target)
    provenance_path = root / 'native-manifest.json'
    if not provenance_path.is_file():
        raise ValueError('native build provenance is required: ' + target)
    provenance = json.loads(provenance_path.read_text())
    if (provenance.get('source_revision') != revision or provenance.get('target') != target or
            provenance.get('sdk_version') != version or provenance.get('dirty') is not False):
        raise ValueError('native build provenance mismatch: ' + target)
    actual = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in root.rglob('*') if p.is_file() and p != provenance_path}
    if provenance.get('files') != actual:
        raise ValueError('native install bytes differ from build provenance: ' + target)
    for path in root.rglob('*'):
        name = path.relative_to(root).as_posix()
        if path.suffix == '.a' or (path.suffix == '.lib' and name != 'lib/axiom_native.lib'):
            raise ValueError('internal archive in native install: ' + name)
        if name.startswith('include/') and name != 'include/axiom/native.h' and path.is_file():
            raise ValueError('private header in native install: ' + name)
        if path.is_symlink() and not path.resolve().is_relative_to(root):
            raise ValueError('external symlink in native install: ' + name)
    return root


def _stage_react_native(repo, installs, output, revision):
    repo, output = Path(repo), Path(output)
    if output.exists() or output.is_symlink() or not installs or not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('exact revision, native installs and a new output directory are required')
    if source_identity(repo) != (revision, False):
        raise ValueError('consumer sources must be clean at the exact native build revision')
    version = json.loads((repo / 'sdk/version.json').read_text())['version']
    ios = set(installs) & {'ios-arm64', 'ios-simulator-arm64'}
    if ios and len(ios) != 2:
        raise ValueError('iOS delivery requires device and simulator installs')
    validated = {target: validate_install(root, target, version, revision) for target, root in installs.items()}
    asset_sets = [{p.relative_to(root / 'assets').as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in (root / 'assets').rglob('*') if p.is_file() and p.name != 'icudtl.dat'}
                  for root in validated.values()]
    if any(value != asset_sets[0] for value in asset_sets):
        raise ValueError('native resource set differs across targets')
    package = json.loads((repo / 'sdk/react-native/package.json').read_text())
    if (package['version'] != version or
            json.loads((repo / 'sdk/api/package.json').read_text())['version'] != version):
        raise ValueError('RN/API/native versions differ')
    # All admission checks precede output mutation.
    shutil.copytree(repo / 'sdk/react-native', output,
                    ignore=shutil.ignore_patterns('node_modules', 'build', '.gradle', '.cxx', '__tests__'))
    shutil.copytree(repo / 'sdk/api', output / 'api')
    for path in (output / 'src').rglob('*.ts'):
        path.write_text(path.read_text().replace("'../../api/", "'../api/"))
    package.update({'main': './src/index.ts', 'types': './src/index.ts',
                    'react-native': './src/index.ts',
                    'exports': {'.': {'types': './src/index.ts', 'react-native': './src/index.ts',
                                      'default': './src/index.ts'}},
                    'files': ['src/**', 'api/**', 'apple/**', 'android/**', 'windows/**',
                              'native/**', '*.podspec', 'react-native.config.cjs', 'README.md', 'example.tsx', 'manifest.json']})
    (output / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    shared_assets = False
    for target, root in validated.items():
        platform, source, destination = PLATFORMS[target]
        directory = output / 'native' / platform
        binary = directory / destination
        binary.parent.mkdir(parents=True, exist_ok=True)
        if (root / source).is_dir():
            shutil.copytree(root / source, binary, symlinks=True)
        else:
            shutil.copy2(root / source, binary)
        shutil.copytree(root / 'include', directory / 'include', dirs_exist_ok=True)
        shutil.copytree(root / 'licenses', output / 'native/licenses' / target)
        if target == 'windows-x64':
            (directory / 'lib').mkdir(exist_ok=True)
            shutil.copy2(root / 'lib/axiom_native.lib', directory / 'lib/axiom_native.lib')
            shutil.copytree(root / 'assets', directory / 'assets')
        if not shared_assets:
            shared_assets = True
            shutil.copytree(root / 'assets', output / 'native/assets', ignore=shutil.ignore_patterns('icudtl.dat'))
    if ios:
        apple = output / 'native/apple'
        apple.mkdir()
        subprocess.run(['xcodebuild', '-create-xcframework',
                        '-framework', str(output / 'native/ios/AxiomNative.framework'),
                        '-framework', str(output / 'native/ios-simulator/AxiomNative.framework'),
                        '-output', str(apple / 'AxiomNative.xcframework')], check=True)
        shutil.rmtree(output / 'native/ios')
        shutil.rmtree(output / 'native/ios-simulator')
    if ios or 'macos-arm64' in installs:
        # CocoaPods expands loose resource globs into flat Copy Resources steps.
        # Ship a complete bundle so fonts with equal basenames retain directories.
        bundle = output / 'native/AxiomAssets.bundle'
        shutil.copytree(output / 'native/assets', bundle)
        with (bundle / 'Info.plist').open('wb') as file:
            plistlib.dump({'CFBundleIdentifier': 'dev.mostorm.axiom.assets',
                          'CFBundleName': 'AxiomAssets', 'CFBundlePackageType': 'BNDL',
                          'CFBundleVersion': version}, file)
    files = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in output.rglob('*') if p.is_file()}
    manifest = {'format': 'axiom-react-native-development-v1', 'source_revision': revision,
                'sdk_version': version, 'targets': sorted(installs),
                'qualification': 'NOT_QUALIFIED', 'files': files}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def stage_react_native(repo, installs, output, revision):
    output = Path(output).resolve()
    if output.exists() or output.is_symlink():
        raise ValueError('refusing to overwrite consumer package')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Keep incomplete framework assemblies out of the consumer destination.
    with tempfile.TemporaryDirectory(prefix='.axiom-stage-', dir=output.parent) as temp:
        staged = Path(temp) / 'package'
        result = _stage_react_native(repo, installs, staged, revision)
        staged.rename(output)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--install', action='append', required=True, metavar='TARGET=PATH')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    result = stage_react_native(args.repo, dict(v.split('=', 1) for v in args.install), args.output, args.revision)
    print(json.dumps({k: v for k, v in result.items() if k != 'files'}))
