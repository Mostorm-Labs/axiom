import html
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path.cwd()
out = root / 'out/p32-web'
chrome = Path('C:/Program Files/Google/Chrome/Application/chrome.exe')
url = 'http://127.0.0.1:8765/ink_playground/?revision=' + subprocess.check_output(
    ['git', 'rev-parse', 'HEAD'], text=True).strip()
args = [str(chrome), '--headless=new', '--no-first-run', '--no-default-browser-check',
        f'--user-data-dir={out / "chrome-smoke-profile"}', '--disable-background-networking',
        '--enable-logging=stderr', '--dump-dom', '--timeout=40000',
        '--virtual-time-budget=6000', url]
print(subprocess.list2cmdline(args), flush=True)
(out / 'chrome-smoke-command.json').write_text(json.dumps(args, indent=2) + '\n')
with (out / 'chrome-smoke-dom.html').open('wb') as stdout, (out / 'chrome-smoke-stderr.log').open('wb') as stderr:
    result = subprocess.run(args, stdout=stdout, stderr=stderr, timeout=55)
dom = (out / 'chrome-smoke-dom.html').read_text(encoding='utf-8')
match = re.search(r'<pre id="hud">(.*?)</pre>', dom, re.S)
hud = html.unescape(match.group(1)) if match else ''
try:
    state = json.loads(hud)
except ValueError:
    state = None
passed = result.returncode == 0 and isinstance(state, dict) and state.get('surface') == 'skia-webgl2'
record = {'status': 'PASS' if passed else 'FAIL', 'exit_code': result.returncode,
          'url': url, 'hud': state if state is not None else hud,
          'physical_status': 'PHYSICAL_PENDING',
          'scope': 'Chrome headless CLI startup only; no human input or physical interaction inferred'}
(out / 'chrome-smoke-result.json').write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record, indent=2), flush=True)
sys.exit(0 if passed else 1)
