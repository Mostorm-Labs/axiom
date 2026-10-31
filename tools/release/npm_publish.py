"""Prepare an immutable, verified Web release asset for private npm publication.

No credentials are read here. The tag-only Actions publication step supplies its
GitHub Secret to npm; PR builds only validate/pack the consumer payload.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.release.consumer_package import JS_REQUIRED

PACKAGE_NAME = '@axiom/axiom'
REGISTRY = 'http://npm.nearhub.cc/'


def prepare_package(archive, output, revision):
    archive, output = Path(archive), Path(output)
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('source identity must be an exact commit')
    if output.exists() or output.is_symlink():
        raise ValueError('refusing to overwrite existing work')
    with zipfile.ZipFile(archive) as z:
        members = z.infolist()
        names = [member.filename for member in members]
        if len(names) != len(set(names)) or len(names) > 5000:
            raise ValueError('duplicate or excessive archive entries')
        if sum(member.file_size for member in members) > 1024**3:
            raise ValueError('oversized npm payload')
        for member in members:
            name = member.filename
            path = PurePosixPath(name)
            mode = member.external_attr >> 16
            if (not name or '\\' in name or ':' in name or path.is_absolute() or
                    '..' in path.parts or path.as_posix() != name or member.is_dir() or
                    stat.S_IFMT(mode) not in (0, stat.S_IFREG)):
                raise ValueError('unsafe archive entry')
            if any(part.lower() == '.npmrc' or part.lower().startswith('.env') for part in path.parts):
                raise ValueError('credentials/configuration in npm payload')
        manifest = json.loads(z.read('manifest.json'))
        if (manifest.get('format') != 'axiom-sdk-preview-v1' or
                manifest.get('kind') != 'js-sdk' or manifest.get('source_revision') != revision):
            raise ValueError('release identity mismatch')
        if set(manifest['files']) != set(names) - {'manifest.json'}:
            raise ValueError('manifest file set mismatch')
        if set(JS_REQUIRED) - set(names):
            raise ValueError('missing required Web SDK assets')
        for name, digest in manifest['files'].items():
            with z.open(name) as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            if actual != digest:
                raise ValueError('release member hash mismatch: ' + name)
        package = json.loads(z.read('package.json'))
        if package.get('name') != PACKAGE_NAME or package.get('private') is not False:
            raise ValueError('npm scope/name/private policy mismatch')
        if package.get('publishConfig') != {'registry': REGISTRY, 'tag': 'preview'}:
            raise ValueError('npm registry/tag policy mismatch')
        if package.get('scripts'):
            raise ValueError('lifecycle scripts are forbidden in this consumer payload')
        version = package.get('version', '')
        if (version != manifest.get('sdk_version') or not re.fullmatch(
                r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*', version)):
            raise ValueError('preview npm version mismatch')
        if 'manifest.json' not in package.get('files', []):
            raise ValueError('npm whitelist must retain release provenance')
        # No output mutation until all identity, hash and package-policy checks pass.
        output.mkdir(parents=True)
        for name in names:
            target = output.joinpath(*PurePosixPath(name).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(name) as source, target.open('xb') as destination:
                shutil.copyfileobj(source, destination)
    return {'name': PACKAGE_NAME, 'version': version, 'registry': REGISTRY,
            'tag': 'preview', 'source_revision': revision}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    print(json.dumps(prepare_package(args.archive, args.output, args.revision)))
