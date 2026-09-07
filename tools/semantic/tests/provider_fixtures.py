"""Real tiny archives for provider/CLI tests, not executable SDKs."""
from pathlib import Path
import shutil

from tools.sdk.archive import canonical_bytes
from tools.semantic.aggregate import aggregate
from tools.semantic.contract import ROOT, read_json
from tools.semantic.tests.release_fixtures import release_fixture
from tools.update_semantic_lock import make_v2_lock


def provider_fixture(base: Path):
    assets, records = release_fixture(base / 'input')
    release = base / 'release'
    aggregate(assets, release)
    index = read_json(release / 'semantic-sdk-index.json')
    tag = 'semantic-sdk-v2-' + index['releaseSetId'][:16]
    lock = make_v2_lock(release / 'semantic-sdk-index.json', tag)
    repo = base / 'project one'
    repo.mkdir()
    for name in ('deps.lock.json', 'semantic-toolchain.lock.json'):
        shutil.copyfile(ROOT / name, repo / name)
    (repo / 'semantic-sdk.lock.json').write_bytes(canonical_bytes(lock) + b'\n')
    mirror = base / 'mirror'
    shutil.copytree(release, mirror / tag)
    return repo, mirror, lock, index
