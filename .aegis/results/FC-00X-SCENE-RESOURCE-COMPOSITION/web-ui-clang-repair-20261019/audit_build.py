import json
from pathlib import Path
import subprocess

root = Path.cwd()
out = root / 'out/p32-web-ui'
entries = json.loads((root / 'out/p32-web/build/compile_commands.json').read_text())
records = []
for entry in entries:
    command = entry['command']
    assert 'unchanged-foundation-diagnostic.hpp' not in command
    assert '-Wno-error=dangling-else' not in command
    assert '-Wno-error=sign-compare' not in command
    if entry['file'].endswith(('runtime/semantic/src/apply_plan.cpp', 'apps/axiom_canvas_demo/platform/web/bridge.cpp')):
        assert '-Werror' in command
        records.append(entry)
assert len(records) == 2
cache = (root / 'out/p32-web/build/CMakeCache.txt').read_text()
assert 'CMAKE_PROJECT_INCLUDE:UNINITIALIZED=\n' in cache or 'CMAKE_PROJECT_INCLUDE:FILEPATH=\n' in cache or 'CMAKE_PROJECT_INCLUDE:STRING=\n' in cache
assert 'CMAKE_CXX_FLAGS:STRING=-fexceptions\n' in cache
(out / 'strict-compile-audit.json').write_text(json.dumps({'entries_checked': len(entries),
    'local_exceptions_present': False, 'strict_commands': records, 'result': 'PASS'}, indent=2))
print(f'{len(entries)} compilation commands checked; both old local exceptions absent.')
