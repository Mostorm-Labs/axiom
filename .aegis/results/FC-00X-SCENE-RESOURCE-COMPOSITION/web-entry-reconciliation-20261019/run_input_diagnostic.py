import html
import json
from pathlib import Path
import re
import shutil
import subprocess

out = Path('out/p32-web')
shutil.copy2(out / 'input-diagnostic.html', out / 'build/apps/input-diagnostic.html')
args = ['C:/Program Files/Google/Chrome/Application/chrome.exe', '--headless=new',
        '--no-first-run', '--no-default-browser-check', '--disable-background-networking',
        f'--user-data-dir={out.resolve() / "chrome-input-diagnostic"}',
        '--enable-logging=stderr', '--dump-dom', '--timeout=45000', '--virtual-time-budget=12000',
        'http://127.0.0.1:8765/input-diagnostic.html']
with (out / 'input-diagnostic-dom.html').open('wb') as stdout, (out / 'input-diagnostic-stderr.log').open('wb') as stderr:
    result = subprocess.run(args, stdout=stdout, stderr=stderr, timeout=55)
dom = (out / 'input-diagnostic-dom.html').read_text(encoding='utf-8')
match = re.search(r'<pre id="result">(.*?)</pre>', dom, re.S)
value = json.loads(html.unescape(match.group(1))) if match and match.group(1) != 'PENDING' else {'status': 'PENDING'}
(out / 'input-diagnostic-result.json').write_text(json.dumps(value, indent=2) + '\n')
for entry in value.get('results', []):
    observation = entry.get('observation', {})
    print(json.dumps({key: val for key, val in entry.items() if key != 'observation'}))
    print(json.dumps({key: observation.get(key) for key in ('runtime_identity', 'preview_surface', 'input')}))
print(value.get('error', value['status']), flush=True)
