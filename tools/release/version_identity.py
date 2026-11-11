"""One value-only SDK identity for builds and consumer packages."""
import argparse
import json
import os
from pathlib import Path
import re

SEMVER = r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'
MAX_BUILD = 9007199254740991  # Lossless JSON/JavaScript integer transport.


def identity(version, build_number):
    if not isinstance(version, str) or not re.fullmatch(SEMVER, version):
        raise ValueError('version must be numeric X.Y.Z SemVer')
    if type(build_number) is not int or not 0 <= build_number <= MAX_BUILD:
        raise ValueError('buildNumber must be a lossless nonnegative integer')
    return {'version': version, 'buildNumber': build_number,
            'displayVersion': f'{version}.{build_number}'}


def read_identity(repo, env=None):
    metadata = json.loads((Path(repo) / 'sdk/version.json').read_text())
    value = (os.environ if env is None else env).get('AXIOM_BUILD_NUMBER')
    if value is not None and not re.fullmatch(r'0|[1-9][0-9]*', value):
        raise ValueError('AXIOM_BUILD_NUMBER must be a decimal integer')
    return identity(metadata['version'], int(value) if value is not None else metadata['buildNumber'])


def ci_build_number(env):
    value = env.get('GITHUB_RUN_NUMBER', '')
    if not re.fullmatch(r'[1-9][0-9]*', value):
        raise ValueError('a positive CI run number is required')
    return identity('0.0.0', int(value))['buildNumber']


def require_fresh_build(env):
    if int(env.get('GITHUB_RUN_ATTEMPT', '1')) != 1:
        raise ValueError('Recompilation requires a new workflow run; retry publication using existing artifacts only')


def manifest_identity(value):
    return {'version': value['version'], 'buildNumber': value['buildNumber'],
            'sdk_version': value['displayVersion']}


def validate_manifest(value):
    result = identity(value.get('version'), value.get('buildNumber'))
    if value.get('sdk_version') != result['displayVersion']:
        raise ValueError('manifest SDK build identity mismatch')
    return result


def render_sdk_files(repo, output, build_number, *, api_only=False):
    repo, output = Path(repo), Path(output)
    source = read_identity(repo, {})
    value = identity(source['version'], build_number)
    files = [('sdk/api/axiom.mjs', 'api/axiom.mjs'),
             ('sdk/api/axiom.d.mts', 'api/axiom.d.mts'),
             ('sdk/api/internal/transport.d.mts', 'api/internal/transport.d.mts'),
             ('sdk/web/axiom.d.mts', 'axiom.d.mts')]
    if api_only:
        files = [(name, target) for name, target in files if name.startswith('sdk/api/')]
    for name, target in files:
        content = (repo / name).read_text().replace(source['displayVersion'], value['displayVersion'])
        if name.endswith('axiom.mjs') or name.endswith('axiom.d.mts'):
            content = re.sub(r'(SDK_BUILD_NUMBER\s*(?:=|:)\s*)[0-9]+',
                             lambda m: m[1] + str(build_number), content)
        if name.startswith('sdk/web/'):
            content = content.replace("'../api/", "'./api/")
        path = output / target
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')
    if api_only:
        package_path = output / 'api/package.json'
        if package_path.exists():
            package = json.loads(package_path.read_text())
            package['buildNumber'] = build_number
            package_path.write_text(json.dumps(package, indent=2) + '\n')
        return value
    package = json.loads((repo / 'sdk/web/package.json').read_text())
    package['buildNumber'] = build_number
    (output / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    metadata = json.loads((repo / 'sdk/version.json').read_text())
    metadata.update(value)
    (output / 'version.json').write_text(json.dumps(metadata, indent=2) + '\n')
    return value


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    p.add_argument('--output', type=Path)
    p.add_argument('--build-number', type=int)
    p.add_argument('--ci', action='store_true')
    p.add_argument('--require-fresh-build', action='store_true')
    args = p.parse_args()
    if args.require_fresh_build:
        require_fresh_build(os.environ)
    number = args.build_number
    if args.ci:
        number = ci_build_number(os.environ)
    if number is None:
        number = read_identity(args.repo)['buildNumber']
    value = (render_sdk_files(args.repo, args.output, number) if args.output else
             identity(read_identity(args.repo)['version'], number))
    print(json.dumps(value))
