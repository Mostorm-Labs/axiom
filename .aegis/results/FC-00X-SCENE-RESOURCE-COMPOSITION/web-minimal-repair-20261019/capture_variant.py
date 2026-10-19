import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path.cwd()
out = root / 'out/p32-web'
variant = sys.argv[1]
app = out / 'build/apps/axiom_canvas_demo'
destination = out / 'build/apps' / ('axiom_canvas_demo_off' if variant == 'off' else 'axiom_canvas_demo')
destination.mkdir(exist_ok=True)
names = ['index.html', 'host.js', 'axiom_canvas_demo_web.js', 'axiom_canvas_demo_web.wasm']
if variant == 'off':
    for name in names:
        shutil.copy2(app / name, destination / name)
commands = subprocess.check_output(['ninja', '-C', str(out / 'build'), '-t', 'commands', 'axiom_canvas_demo_web'], text=True)
link = next(line for line in reversed(commands.splitlines()) if '-sMODULARIZE' in line)
bridge = next(line for line in commands.splitlines() if 'bridge.cpp.o' in line and ' -c ' in line)
query = subprocess.check_output(['ninja', '-C', str(out / 'build'), '-t', 'query', 'apps/axiom_canvas_demo/axiom_canvas_demo_web.js'], text=True)
(out / f'repair-{variant}-dependencies.txt').write_text(query)
(out / f'repair-{variant}-link.txt').write_text(link + '\n')
(out / f'repair-{variant}-bridge-compile.txt').write_text(bridge + '\n')
assert 'libaxiom_ink_playground_common' not in query
assert ('libcanvas_debug_ui' in query) == (variant == 'on')
assert '-Werror' in bridge
assert '-DAXIOM_BUILD_DIRTY=0' in bridge
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
assert revision in bridge
facts = {'variant': variant, 'source_revision': revision, 'source_dirty': 0,
         'debug_ui_linked': 'libcanvas_debug_ui' in query, 'playground_linked': False,
         'own_bridge_werror': True, 'files': []}
for name in names:
    path = destination / name
    data = path.read_bytes()
    facts['files'].append({'path': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
(out / f'repair-{variant}-artifacts.json').write_text(json.dumps(facts, indent=2) + '\n')
print(json.dumps({key: value for key, value in facts.items() if key != 'files'}, indent=2))
