import json
from pathlib import Path
import subprocess
import sys

variant = sys.argv[1]
assert variant in ['on', 'off']
out = Path('out/p32-web-ui')
label = 'strict-on-final' if variant == 'on' else 'strict-off'
commands = [
    ['cmake', '-S', '.', '-B', 'out/p32-web/build', '-DCMAKE_CXX_FLAGS=-fexceptions', '-DCMAKE_PROJECT_INCLUDE=',
     '-DAXIOM_BUILD_DEBUG_UI=' + ('ON' if variant == 'on' else 'OFF'),
     '-DAXIOM_DEBUG_UI_PROFILE=' + ('full' if variant == 'on' else 'off')],
    ['cmake', '--build', 'out/p32-web/build', '--target', 'axiom_canvas_demo_web', '-j', '4']]
(out / f'{label}-commands.json').write_text(json.dumps(commands, indent=2))
for suffix, command in zip(['configure', 'build'], commands):
    with (out / f'{label}-{suffix}.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    print(f'{label}-{suffix}: exit={result.returncode}', flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
