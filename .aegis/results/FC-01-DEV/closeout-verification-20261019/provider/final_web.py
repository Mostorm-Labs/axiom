import json
from pathlib import Path
import subprocess
import sys

variant = sys.argv[1]
assert variant in ('on', 'off')
out = Path('out/fc01-closeout')
commands = [
    ['cmake', '-S', '.', '-B', 'out/p32-web/build', '-DCMAKE_CXX_FLAGS=-fexceptions',
     '-DCMAKE_PROJECT_INCLUDE=', '-DAXIOM_BUILD_DEBUG_UI=' + ('ON' if variant == 'on' else 'OFF'),
     '-DAXIOM_DEBUG_UI_PROFILE=' + ('full' if variant == 'on' else 'off')],
    ['cmake', '--build', 'out/p32-web/build', '--target', 'axiom_canvas_demo_web', '-j', '4']]
rows = []
for name, command in zip(('configure', 'build'), commands):
    with (out / f'web-{variant}-{name}.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    rows.append({'command': command, 'exit_code': result.returncode})
    (out / f'web-{variant}-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(variant, name, result.returncode, flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
