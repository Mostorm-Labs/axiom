import importlib.util
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8')

root = Path.cwd()
out = root / 'out/p32-web'
results = []
for name in ('web_host_page_test.py', 'c0_web_navigation_contract_test.py',
             'web_imgui_debug_ui_contract_test.py'):
    path = root / 'apps/ink_playground/tests' / name
    spec = importlib.util.spec_from_file_location(name[:-3], path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    cases = [value for key, value in vars(module).items()
             if key.startswith('test_') and callable(value)]
    for case in cases:
        case()
    results.append({'file': str(path.relative_to(root)), 'cases': len(cases) or 1,
                    'status': 'PASS'})
    print(name, len(cases) or 1, 'PASS', flush=True)
(out / 'source-checks.json').write_text(json.dumps(results, indent=2) + '\n')

snapshot = out / 'windows-user-observation'
snapshot.mkdir(exist_ok=True)
for name in ('native-input.log', 'native-stderr.log', 'native-stdout.log', 'launch.json'):
    shutil.copy2(root / 'out/p32-touch' / name, snapshot / name)
text = (snapshot / 'native-input.log').read_text()
events = re.findall(r'^event=(\S+) accepted=(\d+) .*', text, re.M)
summary = {
    'user_statement': '经测试，各项功能正常，在本机开始web端功能的测试吧，使用chrome浏览器打开页面',
    'status': 'USER_REPORTED_NORMAL',
    'observation_scope': '用户汇总反馈；未提供逐项实际操作记录，不将日志推断或汇总反馈改写为独立逐项验收。',
    'source_revision': 'b68e8f9f2c1bca65277f832dd78193f4ae0e2c0f',
    'event_counts': {kind: sum(key == kind for key, _ in events)
                     for kind in sorted({key for key, _ in events})},
    'rejected_event_count': sum(accepted == '0' for _, accepted in events),
    'last_frame': re.findall(r'^event=frame-opportunity.*', text, re.M)[-1],
    'official_gate_or_phys_status_changed': False,
}
(snapshot / 'observation.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(summary, ensure_ascii=False, indent=2))
