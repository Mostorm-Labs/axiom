import hashlib
import json
from pathlib import Path
import subprocess

root = Path.cwd()
out = root / 'out/p32-web-ui'
package = root / '.aegis/packages/FC-00X-SCENE-RESOURCE-COMPOSITION'
descriptor = json.loads((package / 'package.json').read_text())
bindings = []
for ref in [value for key, value in descriptor.items() if key.endswith('_ref') and isinstance(value, dict) and 'sha256' in value] + descriptor['oracle_refs']:
    path = package / ref['path']
    data = subprocess.check_output(['git', 'show', 'HEAD:' + path.relative_to(root).as_posix()])
    actual = hashlib.sha256(data).hexdigest()
    assert actual == ref['sha256'], ref['path']
    bindings.append({'path': ref['path'], 'expected': ref['sha256'], 'actual': actual})
(out / 'package-bindings.json').write_text(json.dumps(bindings, indent=2))
probe = out / 'stable_order_probe.cpp'
probe.write_text('#include "canvas/foundation/stable_order_key.hpp"\nint main() { return canvas::foundation::StableOrderKey(7).value() == 7 ? 0 : 1; }\n')
compiler = str(root / '.deps/emsdk/upstream/emscripten/em++.exe')
base = [compiler, '-std=c++20', '-fexceptions', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-fsyntax-only',
        '-I' + str(root / 'runtime/foundation/include'), '-I' + str(root / 'runtime/semantic/include')]
commands = [base + [str(probe)],
            base + ['-Wno-error=sign-compare', str(root / 'runtime/semantic/src/apply_plan.cpp')]]
records = []
for name, command in zip(['stable-order', 'apply-plan'], commands):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (out / f'{name}-red.log').write_text(result.stdout)
    assert result.returncode != 0 and ('sign-compare' if name == 'stable-order' else 'dangling-else') in result.stdout
    records.append({'name': name, 'command': command, 'exit_code': result.returncode, 'expected': 'Compiler rejects original source at the known diagnostic', 'result': 'EXPECTED_RED'})
(out / 'clang-red.json').write_text(json.dumps(records, indent=2))
print(f'{len(bindings)} exact bindings verified; both original Clang diagnostics reproduced.')
