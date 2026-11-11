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
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.release.version_identity import read_identity, manifest_identity, render_sdk_files, identity


PLATFORMS = {
    'android-arm64-v8a': ('android', 'lib/libaxiom.so', 'lib/arm64-v8a/libaxiom.so'),
    'android-x86_64': ('android', 'lib/libaxiom.so', 'lib/x86_64/libaxiom.so'),
    'macos-arm64': ('macos', 'lib/axiom.framework', 'axiom.framework'),
    'ios-arm64': ('ios', 'lib/axiom.framework', 'axiom.framework'),
    'ios-simulator-arm64': ('ios-simulator', 'lib/axiom.framework', 'axiom.framework'),
    'windows-x64': ('windows', 'bin/axiom.dll', 'bin/axiom.dll'),
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
                'assets/fonts/catalog.json',
                'assets/fonts/Excalifont/Excalifont-Regular.ttf',
                'assets/fonts/Excalifont/OFL.txt',
                'assets/fonts/Excalifont/README.md', 'licenses/skia/Skia.txt',
                'licenses/semantic/Protobuf.txt', 'share/axiom-native/version.json']
    if target == 'windows-x64':
        required += ['lib/axiom.lib', 'assets/icudtl.dat']
    if any(not (root / name).exists() for name in required):
        raise ValueError('incomplete native install: ' + target)
    native_version = json.loads((root / 'share/axiom-native/version.json').read_text())
    expected_build = read_identity(Path(__file__).resolve().parents[2])['buildNumber']
    if native_version['version'] != version or native_version.get('buildNumber') != expected_build:
        raise ValueError('native SDK version mismatch: ' + target)
    provenance_path = root / 'native-manifest.json'
    if not provenance_path.is_file():
        raise ValueError('native build provenance is required: ' + target)
    provenance = json.loads(provenance_path.read_text())
    if (provenance.get('format') != 'axiom-native-development-v1' or
            provenance.get('runtime_name') != 'axiom' or
            provenance.get('source_revision') != revision or provenance.get('target') != target or
            provenance.get('sdk_version') != identity(version, expected_build)['displayVersion'] or
            provenance.get('buildNumber') != expected_build or provenance.get('dirty') is not False):
        raise ValueError('native build provenance mismatch: ' + target)
    actual = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in root.rglob('*') if p.is_file() and p != provenance_path}
    # GitHub artifact transport can materialize Apple framework symlinks as
    # regular files/directories. Keep the strict byte-for-byte install check
    # for ELF/DLL payloads, while Apple framework identity is validated below
    # through its required binary and package metadata.
    apple_framework = target in {'macos-arm64', 'ios-arm64', 'ios-simulator-arm64'}
    if not apple_framework and provenance.get('files') != actual:
        raise ValueError('native install bytes differ from build provenance: ' + target)
    for path in root.rglob('*'):
        name = path.relative_to(root).as_posix()
        if target == 'windows-x64' and path.suffix == '.dll' and name != 'bin/axiom.dll':
            raise ValueError('unexpected Windows runtime DLL: ' + name)
        if target.startswith('android-') and path.suffix == '.so' and name != binary:
            raise ValueError('unexpected Android runtime SO: ' + name)
        if path.suffix == '.a' or (path.suffix == '.lib' and name != 'lib/axiom.lib'):
            raise ValueError('internal archive in native install: ' + name)
        if name.startswith('include/') and name != 'include/axiom/native.h' and path.is_file():
            raise ValueError('private header in native install: ' + name)
        if path.is_symlink() and not path.resolve().is_relative_to(root):
            raise ValueError('external symlink in native install: ' + name)
    return root


PUBLISHED_FONT_FILES = {
    'fonts/catalog.json',
    'fonts/Excalifont/Excalifont-Regular.ttf',
    'fonts/Excalifont/OFL.txt',
    'fonts/Excalifont/README.md',
}


def _published_asset_files(root):
    root = Path(root)
    result = []
    for path in (root / 'assets').rglob('*'):
        if not path.is_file() or path.name == 'icudtl.dat':
            continue
        relative = path.relative_to(root / 'assets').as_posix()
        if relative.startswith('fonts/') and relative not in PUBLISHED_FONT_FILES:
            raise ValueError('unexpected published font asset: ' + relative)
        result.append(path)
    return result


def _copy_published_assets(source, destination, *, include_icudtl=False):
    source, destination = Path(source), Path(destination)
    for path in _published_asset_files(source):
        _copy_tree(path, destination / path.relative_to(source / 'assets'))
    if include_icudtl and (source / 'assets/icudtl.dat').is_file():
        _copy_tree(source / 'assets/icudtl.dat', destination / 'icudtl.dat')


def _stage_react_native(repo, installs, output, revision):
    repo, output = Path(repo), Path(output)
    if output.exists() or output.is_symlink() or not installs or not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('exact revision, native installs and a new output directory are required')
    if source_identity(repo) != (revision, False):
        raise ValueError('consumer sources must be clean at the exact native build revision')
    build = read_identity(repo)
    version = build['version']
    ios = set(installs) & {'ios-arm64', 'ios-simulator-arm64'}
    if ios and len(ios) != 2:
        raise ValueError('iOS delivery requires device and simulator installs')
    validated = {target: validate_install(root, target, version, revision) for target, root in installs.items()}
    asset_sets = [{p.relative_to(root / 'assets').as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in _published_asset_files(root)}
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
    package['buildNumber'] = build['buildNumber']
    render_sdk_files(repo, output, build['buildNumber'], api_only=True)
    package.update({'main': './src/index.ts', 'types': './src/index.ts',
                    'react-native': './src/index.ts',
                    'exports': {'.': {'types': './src/index.ts', 'react-native': './src/index.ts',
                                      'default': './src/index.ts'}},
                    'files': ['src/**', 'api/**', 'apple/**', 'android/**', 'windows/**',
                              'native/**', '*.podspec', 'react-native.config.cjs', 'README.md', 'example.tsx', 'manifest.json']})
    # Kept only for existing development fixtures. The publishable path is the
    # common package plus one platform package; this compatibility bundle must
    # never be published as a public consumer package.
    package['private'] = True
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
            shutil.copy2(root / 'lib/axiom.lib', directory / 'lib/axiom.lib')
            _copy_published_assets(root, directory / 'assets', include_icudtl=True)
        if not shared_assets:
            shared_assets = True
            _copy_published_assets(root, output / 'native/assets')
    if ios:
        apple = output / 'native/apple'
        apple.mkdir()
        subprocess.run(['xcodebuild', '-create-xcframework',
                        '-framework', str(output / 'native/ios/axiom.framework'),
                        '-framework', str(output / 'native/ios-simulator/axiom.framework'),
                        '-output', str(apple / 'axiom.xcframework')], check=True)
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
                          'CFBundleShortVersionString': version, 'CFBundleVersion': str(build['buildNumber'])}, file)
    files = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in output.rglob('*') if p.is_file()}
    manifest = {'format': 'axiom-react-native-development-v1', 'source_revision': revision,
                **manifest_identity(build), 'targets': sorted(installs),
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


def _copy_tree(source, destination):
    if source.is_dir():
        shutil.copytree(source, destination, dirs_exist_ok=True, symlinks=True)
    elif source.is_file():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def _normalize_apple_framework(framework):
    """Restore canonical framework symlinks after artifact transport."""
    framework = Path(framework)
    versions = framework / 'Versions'
    canonical = versions / 'A'
    if not canonical.is_dir():
        return
    current = versions / 'Current'
    if current.exists() or current.is_symlink():
        if current.is_dir() and not current.is_symlink():
            shutil.rmtree(current)
        else:
            current.unlink()
    for name in ('axiom', 'Headers', 'Resources'):
        root = framework / name
        if root.exists() or root.is_symlink():
            if root.is_dir() and not root.is_symlink():
                shutil.rmtree(root)
            else:
                root.unlink()
        root.symlink_to(Path('Versions') / 'A' / name,
                        target_is_directory=name != 'axiom')


def _package_files(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and p.name != 'manifest.json'}


def _resource_set(validated):
    return [{p.relative_to(root / 'assets').as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in _published_asset_files(root)}
            for root in validated.values()]


def _stage_react_native_common(repo, output, revision):
    """Stage the publishable shared TypeScript package without platform payloads."""
    repo, output = Path(repo), Path(output)
    if output.exists() or output.is_symlink() or not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('exact revision and a new output directory are required')
    if source_identity(repo) != (revision, False):
        raise ValueError('consumer sources must be clean at the exact revision')
    build = read_identity(repo)
    version = build['version']
    package = json.loads((repo / 'sdk/react-native/package.json').read_text())
    api_package = json.loads((repo / 'sdk/api/package.json').read_text())
    if package.get('version') != version or package.get('private') is not False:
        raise ValueError('shared React Native package metadata is not publishable')
    if api_package.get('version') != version:
        raise ValueError('shared API and React Native versions differ')
    output.mkdir(parents=True)
    _copy_tree(repo / 'sdk/react-native/src', output / 'src')
    _copy_tree(repo / 'sdk/api', output / 'api')
    for path in (output / 'src').rglob('*.ts'):
        path.write_text(path.read_text().replace("'../../api/", "'../api/"))
    shutil.copy2(repo / 'sdk/react-native/README.md', output / 'README.md')
    shutil.copy2(repo / 'sdk/react-native/example.tsx', output / 'example.tsx')
    package['buildNumber'] = build['buildNumber']
    render_sdk_files(repo, output, build['buildNumber'], api_only=True)
    package['files'] = ['src/**', 'api/**', 'README.md', 'example.tsx', 'manifest.json', 'package.json']
    package['private'] = False
    (output / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    manifest = {'format': 'axiom-react-native-common-v1', 'source_revision': revision,
                **manifest_identity(build), 'qualification': 'NOT_QUALIFIED',
                'files': _package_files(output)}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def stage_react_native_common(repo, output, revision):
    output = Path(output).resolve()
    if output.exists() or output.is_symlink():
        raise ValueError('refusing to overwrite consumer package')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.axiom-stage-', dir=output.parent) as temp:
        staged = Path(temp) / 'package'
        result = _stage_react_native_common(repo, staged, revision)
        staged.rename(output)
    return result


def _platform_config(platform):
    if platform == 'windows':
        return "module.exports={dependency:{platforms:{windows:{sourceDir:'./windows',projects:[{projectFile:'AxiomReactNative.vcxproj',directDependency:true}],cppHeaders:['winrt/MostormAxiom.h'],cppPackageProviders:['MostormAxiom::ReactPackageProvider']}}}};\n"
    if platform == 'android':
        return "module.exports={dependency:{platforms:{android:{sourceDir:'./android',packageImportPath:'import dev.mostorm.axiom.AxiomPackage;',packageInstance:'new AxiomPackage()'}}}};\n"
    return f"module.exports={{dependency:{{platforms:{{{platform}:{{podspecPath:'./AxiomReactNative.podspec'}}}}}}}};\n"


def _platform_podspec(platform):
    osx = "s.osx.vendored_frameworks = 'native/macos/axiom.framework'" if platform == 'macos' else ''
    ios = "s.ios.vendored_frameworks = 'native/apple/axiom.xcframework'" if platform == 'ios' else ''
    deployment = "s.platform = :ios, '17.0'" if platform == 'ios' else "s.platform = :osx, '15.0'"
    return f"require 'json'\npackage = JSON.parse(File.read(File.join(__dir__, 'package.json')))\nPod::Spec.new do |s|\n s.name = 'AxiomReactNative'\n s.version = package['version']\n s.summary = 'Axiom React Native {platform} adapter'\n s.source = {{ :git => 'https://github.com/Mostorm-Labs/axiom.git', :tag => \"v#{{s.version}}\" }}\n {deployment}\n s.source_files = 'apple/*.{{h,mm}}'\n s.private_header_files = 'apple/*.h'\n s.requires_arc = true\n {ios}\n {osx}\n s.resources = 'native/AxiomAssets.bundle'\n s.frameworks = 'Metal', 'QuartzCore'\n s.{'ios.frameworks' if platform == 'ios' else 'osx.frameworks'} = '{'UIKit' if platform == 'ios' else 'AppKit'}'\n s.pod_target_xcconfig = {{ 'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20', 'HEADER_SEARCH_PATHS' => '$(PODS_TARGET_SRCROOT)/native/include' }}\n install_modules_dependencies(s)\nend\n"


def _stage_react_native_platform(repo, platform, installs, output, revision):
    """Stage one publishable platform adapter plus its matching axiom runtime."""
    repo, output = Path(repo), Path(output)
    if platform not in {'windows', 'android', 'ios', 'macos'}:
        raise ValueError('unknown React Native platform: ' + platform)
    if output.exists() or output.is_symlink() or not installs or not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('exact revision, native installs and a new output directory are required')
    if source_identity(repo) != (revision, False):
        raise ValueError('consumer sources must be clean at the exact revision')
    build = read_identity(repo)
    version = build['version']
    targets = set(installs)
    expected = {
        'android': {'android-arm64-v8a', 'android-x86_64'},
        'windows': {'windows-x64'},
        'ios': {'ios-arm64', 'ios-simulator-arm64'}, 'macos': {'macos-arm64'},
    }[platform]
    if platform in {'windows', 'macos'} and targets != expected:
        raise ValueError(f'{platform} delivery requires exactly {sorted(expected)}')
    if platform == 'ios' and targets != expected:
        raise ValueError('iOS delivery requires device and simulator installs')
    if platform == 'android' and not targets <= expected:
        raise ValueError('Android delivery contains an unsupported ABI')
    validated = {target: validate_install(root, target, version, revision) for target, root in installs.items()}
    common_package = json.loads((repo / 'sdk/react-native/package.json').read_text())
    platform_package = json.loads((repo / f'sdk/react-native-{platform}/package.json').read_text())
    if common_package.get('version') != version or common_package.get('private') is not False:
        raise ValueError('shared React Native package metadata is not publishable')
    if (platform_package.get('version') != version or
            platform_package.get('dependencies', {}).get('@mostorm/axiom-react-native') != version or
            platform_package.get('private') is not False):
        raise ValueError('platform and shared React Native versions differ')
    resources = _resource_set(validated)
    if resources and any(value != resources[0] for value in resources):
        raise ValueError('native resource set differs across targets')
    output.mkdir(parents=True)
    source_dir = repo / 'sdk/react-native'
    if platform == 'windows':
        _copy_tree(source_dir / 'windows', output / 'windows')
        _copy_tree(source_dir / 'src/specs', output / 'src/specs')
        (output / 'react-native.config.cjs').write_text(_platform_config(platform))
        runtime = output / 'native/windows'
        runtime.mkdir(parents=True)
        _copy_tree(validated['windows-x64'] / 'bin/axiom.dll', runtime / 'bin/axiom.dll')
        _copy_tree(validated['windows-x64'] / 'lib/axiom.lib', runtime / 'lib/axiom.lib')
        _copy_tree(validated['windows-x64'] / 'include', runtime / 'include')
        _copy_published_assets(validated['windows-x64'], runtime / 'assets', include_icudtl=True)
        shutil.copytree(validated['windows-x64'] / 'licenses', output / 'native/licenses')
    elif platform == 'android':
        _copy_tree(source_dir / 'android', output / 'android')
        _copy_tree(source_dir / 'src/specs', output / 'src/specs')
        (output / 'react-native.config.cjs').write_text(_platform_config(platform))
        for target, root in validated.items():
            _, _, destination = PLATFORMS[target]
            _copy_tree(root / PLATFORMS[target][1], output / 'native/android' / destination)
            _copy_tree(root / 'include', output / 'native/android/include')
        _copy_published_assets(next(iter(validated.values())), output / 'native/assets')
        shutil.copytree(next(iter(validated.values())) / 'licenses', output / 'native/licenses')
    else:
        _copy_tree(source_dir / 'apple', output / 'apple')
        _copy_tree(source_dir / 'src/specs', output / 'src/specs')
        (output / 'react-native.config.cjs').write_text(_platform_config(platform))
        (output / 'AxiomReactNative.podspec').write_text(_platform_podspec(platform))
        if platform == 'ios':
            apple = output / 'native/apple'; apple.mkdir(parents=True)
            subprocess.run(['xcodebuild', '-create-xcframework',
                            '-framework', str(validated['ios-arm64'] / 'lib/axiom.framework'),
                            '-framework', str(validated['ios-simulator-arm64'] / 'lib/axiom.framework'),
                            '-output', str(apple / 'axiom.xcframework')], check=True)
            _copy_published_assets(validated['ios-arm64'], output / 'native/assets')
            _copy_tree(validated['ios-arm64'] / 'include', output / 'native/include')
            shutil.copytree(validated['ios-arm64'] / 'licenses', output / 'native/licenses')
        else:
            _copy_tree(validated['macos-arm64'] / 'lib/axiom.framework', output / 'native/macos/axiom.framework')
            _normalize_apple_framework(output / 'native/macos/axiom.framework')
            _copy_published_assets(validated['macos-arm64'], output / 'native/assets')
            _copy_tree(validated['macos-arm64'] / 'include', output / 'native/include')
            shutil.copytree(validated['macos-arm64'] / 'licenses', output / 'native/licenses')
        bundle = output / 'native/AxiomAssets.bundle'
        shutil.copytree(output / 'native/assets', bundle)
        with (bundle / 'Info.plist').open('wb') as file:
            plistlib.dump({'CFBundleIdentifier': 'dev.mostorm.axiom.assets', 'CFBundleName': 'AxiomAssets',
                           'CFBundlePackageType': 'BNDL', 'CFBundleShortVersionString': version, 'CFBundleVersion': str(build['buildNumber'])}, file)
    package = json.loads((repo / f'sdk/react-native-{platform}/package.json').read_text())
    for path in (output / 'android').rglob('AxiomNativeModule.kt'):
        text = path.read_text()
        text = re.sub(r'axiom-sdk-[0-9.]+', 'axiom-sdk-' + build['displayVersion'], text)
        path.write_text(text)
    package['version'] = version
    package['buildNumber'] = build['buildNumber']
    package['dependencies']['@mostorm/axiom-react-native'] = version
    package['files'] = [name for name in package.get('files', []) if name != 'manifest.json'] + ['manifest.json']
    (output / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    readme_platform = 'android' if platform.startswith('android-') else platform
    shutil.copy2(repo / f'sdk/react-native-{readme_platform}/README.md', output / 'README.md')
    manifest = {'format': 'axiom-react-native-platform-v1', 'platform': platform,
                'source_revision': revision, **manifest_identity(build), 'targets': sorted(targets),
                'runtime': 'axiom', 'adapter': 'consumer-built',
                'runtime_files': {
                    'windows': ['native/windows/bin/axiom.dll', 'native/windows/lib/axiom.lib'],
                    'android': sorted(str(p.relative_to(output)) for p in (output / 'native/android').rglob('libaxiom.so')),
                    'ios': ['native/apple/axiom.xcframework'],
                    'macos': ['native/macos/axiom.framework'],
                }[platform],
                'qualification': 'NOT_QUALIFIED', 'files': _package_files(output)}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def stage_react_native_platform(repo, platform, installs, output, revision):
    output = Path(output).resolve()
    if output.exists() or output.is_symlink():
        raise ValueError('refusing to overwrite consumer package')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.axiom-stage-', dir=output.parent) as temp:
        staged = Path(temp) / 'package'
        result = _stage_react_native_platform(repo, platform, installs, staged, revision)
        staged.rename(output)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--common', action='store_true', help='stage @mostorm/axiom-react-native')
    parser.add_argument('--platform', choices=('windows', 'android', 'ios', 'macos'))
    parser.add_argument('--install', action='append', metavar='TARGET=PATH')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    if bool(args.common) == bool(args.platform):
        parser.error('choose exactly one of --common or --platform')
    if args.common:
        if args.install:
            parser.error('--install is only valid with --platform')
        result = stage_react_native_common(args.repo, args.output, args.revision)
    else:
        if not args.install:
            parser.error('--platform requires at least one --install TARGET=PATH')
        try:
            installs = dict(v.split('=', 1) for v in args.install)
        except ValueError as error:
            parser.error(f'invalid --install: {error}')
        result = stage_react_native_platform(args.repo, args.platform, installs, args.output, args.revision)
    print(json.dumps({k: v for k, v in result.items() if k != 'files'}))
