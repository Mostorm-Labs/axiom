import json
from pathlib import Path
import subprocess

out = Path('out/p32-web-ui')
records = json.loads((out / 'clang-red.json').read_text())
for record in records:
    command = [arg for arg in record['command'] if not arg.startswith('-Wno-error=')]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (out / (record['name'] + '-green.log')).write_text(result.stdout)
    assert result.returncode == 0, result.stdout
    record.update(command=command, exit_code=result.returncode,
                  expected='Strict compiler accepts fixed source without either local exception', result='PASS')
(out / 'clang-green.json').write_text(json.dumps(records, indent=2))
print('Both strict Clang probes PASS without local exceptions.')
