import html
import json
from pathlib import Path
import re
import shutil
import subprocess

out = Path('out/p32-web')
shutil.copy2(out / 'reference-entry-audit.html', out / 'build/apps/reference-entry-audit.html')
args = ['C:/Program Files/Google/Chrome/Application/chrome.exe', '--headless=new', '--no-first-run',
        '--no-default-browser-check', '--disable-background-networking',
        f'--user-data-dir={out.resolve() / "chrome-reference-audit"}',
        '--enable-logging=stderr', '--dump-dom', '--timeout=40000', '--virtual-time-budget=6000',
        'http://127.0.0.1:8765/reference-entry-audit.html']
with (out / 'reference-entry-audit-dom.html').open('wb') as stdout, (out / 'reference-entry-audit-stderr.log').open('wb') as stderr:
    subprocess.run(args, stdout=stdout, stderr=stderr, check=True, timeout=55)
dom = (out / 'reference-entry-audit-dom.html').read_text(encoding='utf-8')
match = re.search(r'<pre id="result">(.*?)</pre>', dom, re.S)
result = json.loads(html.unescape(match.group(1))) if match and match.group(1) != 'PENDING' else {'status': 'PENDING'}
(out / 'reference-entry-audit-result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2), flush=True)
