import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[2]
evidence = root / 'out/fc02-device'
original = evidence / 'intake/axiom_minimal_runtime_host_windows-3d7afa35.original.exe'
sdk = Path(r'C:\Users\NearHub\Documents\ChatGPT\axiom\.deps\skia-sdk\windows-x64-d3d12\release')
cjk = root / 'assets/fonts/noto-sans-cjk-sc'
catalog = root / 'assets/fonts/imported'
name = 'axiom_minimal_runtime_host_windows-3d7afa35.exe'
expected_exe = 'bf917e023f35f0342e21d0dc5dab54d665bb079da12eba229b970bda8fdc53c8'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

assert sha(original) == expected_exe
manifest = json.loads((cjk / 'resource.json').read_text('utf-8'))
assert sha(cjk / manifest['file']) == manifest['sha256']
assert (cjk / manifest['file']).stat().st_size == manifest['bytes']
for font in json.loads((catalog / 'catalog.json').read_text('utf-8'))['fonts']:
    assert sha(catalog / font['file']) == font['sha256']
    assert (catalog / font['file']).stat().st_size == font['bytes']
    assert (catalog / font['license_file']).is_file()

stages = {}
for stage in ('icu-only', 'icu-cjk', 'complete'):
    directory = evidence / ('distribution-' + stage)
    directory.mkdir(parents=True, exist_ok=False)
    shutil.copy2(original, directory / name)
    shutil.copy2(sdk / 'runtime/windows/icudtl.dat', directory / 'icudtl.dat')
    if stage != 'icu-only':
        shutil.copy2(cjk / manifest['file'], directory / 'axiom-font.otf')
        shutil.copy2(cjk / 'LICENSE.txt', directory / 'axiom-font.LICENSE.txt')
        shutil.copy2(cjk / 'NOTICE.txt', directory / 'axiom-font.NOTICE.txt')
    if stage == 'complete':
        shutil.copytree(catalog, directory / 'fonts')
        shutil.copytree(sdk / 'licenses', directory / 'third-party-licenses')
    assert sha(directory / name) == expected_exe
    stages[stage] = str(directory)

(evidence / 'staged-directories.json').write_text(json.dumps(stages, indent=2) + '\n', encoding='utf-8')
print(json.dumps(stages, indent=2))
