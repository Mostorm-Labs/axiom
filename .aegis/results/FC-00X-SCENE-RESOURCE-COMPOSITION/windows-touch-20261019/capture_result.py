"""Compile local evidence without claiming a Gate or physical acceptance."""
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT / 'out/p32-touch'
BASE = 'f1d72f22ad20209c8ccdbdd7474d86c651434914'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

def capture(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(source.read_text(encoding='utf-8-sig'), encoding='utf-8')

launch = json.loads((OUT / 'launch.json').read_text(encoding='utf-8-sig'))
revision = launch['source_revision']
subprocess.run(['git', 'merge-base', '--is-ancestor', BASE, revision], cwd=ROOT, check=True)
assert launch['exe_sha256'] == launch['process_sha256']
assert launch['exe_path'] == launch['process_path']
assert hashlib.sha256(Path(launch['exe_path']).read_bytes()).hexdigest() == launch['exe_sha256']
startup = (OUT / 'native-input.log').read_text()
assert f'build_revision={revision} dirty=0' in startup
assert 'event=frame-opportunity accepted=1' in startup
stderr = (OUT / 'native-stderr.log').read_text()
assert 'present skia failed' not in stderr

for name in ['selected-tests.log', 'selected-tests.xml', 'selected-build.log',
             'final-verification.log', 'clang-checks.log',
             'toolchain-facts.log', 'native-input.log', 'native-stdout.log', 'native-stderr.log']:
    if (OUT / name).exists(): capture(OUT / name, HERE / 'logs' / name)
for name in ['launch.json', 'idle-observation.json', 'selected-commands.json',
             'selected-tests.json', 'clang-commands.json']:
    capture(OUT / name, HERE / 'facts' / name)
capture(OUT / 'build/apps/minimal_runtime_host/touch-fixture.log', HERE / 'logs/touch-fixture-green.log')

package_path = ROOT / '.aegis/packages/FC-00X-SCENE-RESOURCE-COMPOSITION/package.json'
package = json.loads(package_path.read_text())
refs = [package[k] for k in ['authority_lock_ref', 'verification_lock_ref',
        'execution_contract_ref', 'implementation_context_ref', 'evidence_contract_ref']]
refs += package['oracle_refs']
bindings = []
for ref in refs:
    digest = hashlib.sha256((package_path.parent / ref['path']).read_bytes()).hexdigest()
    bindings.append({'path': ref['path'], 'expected': ref['sha256'], 'actual': digest})
assert all(b['expected'] == b['actual'] for b in bindings)
write_json(HERE / 'facts/package-bindings.json', bindings)

changed = git('diff', '--name-only', BASE, revision).splitlines()
allowed = ('apps/minimal_runtime_host/', 'runtime/composition/', 'runtime/input/',
           'runtime/interaction/', 'runtime/render/',
           '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/windows-touch-20261019/')
assert all(path.startswith(allowed) for path in changed)
source_paths = [p for p in changed if not p.startswith('.aegis/')]
source_facts = {p: {'git_blob': git('rev-parse', revision + ':' + p),
                   'disk_sha256': hashlib.sha256((ROOT / p).read_bytes()).hexdigest()}
                for p in source_paths}
write_json(HERE / 'facts/source-files.json', source_facts)
write_json(HERE / 'facts/scope-audit.json', {'baseline': BASE, 'result_revision': revision,
           'source_paths': source_paths, 'unauthorized_paths': [],
           'protected_authority_golden_gate_phys_android_changes': []})
original = Path('C:/Users/NearHub/Documents/ChatGPT/axiom')
original_status = subprocess.check_output(['git', '-C', str(original), 'status', '--porcelain'], text=True)
write_json(HERE / 'facts/repository-reconciliation.json', {
    'repository_url': git('remote', 'get-url', 'origin'), 'baseline': BASE,
    'fetched_baseline_ref': 'refs/remotes/origin/codex/fc00x-scene-resource-composition-p32-f08-resume',
    'fetched_baseline_revision': git('rev-parse', 'refs/remotes/origin/codex/fc00x-scene-resource-composition-p32-f08-resume'),
    'worktree': str(ROOT), 'branch': git('branch', '--show-current'),
    'original_checkout': str(original),
    'original_revision': subprocess.check_output(['git', '-C', str(original), 'rev-parse', 'HEAD'], text=True).strip(),
    'original_status_at_capture': original_status,
    'commit_identity': 'Codex <codex@openai.com>, command-scoped; global configuration unchanged',
})

cases = ET.parse(OUT / 'selected-tests.xml').getroot().findall('.//testcase')
assert cases and not any(c.find('failure') is not None or c.find('skipped') is not None for c in cases)
rows = ['# 逐项自动测试结果', '',
        '由 exact-source selected-tests.xml 生成；输入序列和状态断言见测试源码，原始输出在 XML 中。',
        'Windows 触摸输入的 source/pointer/generation、样本、对象和 Operation 数另见 logs/touch-fixture-green.log。',
        '这些自动结果不替代人工视觉/触摸观察。', '',
        '| Test | expected | actual | result |', '|---|---|---|---|']
rows += [f"| {c.attrib['name']} | exit 0，断言启用 | exit 0 | PASS |" for c in cases]
(HERE / 'test-results.md').write_text('\n'.join(rows) + '\n', encoding='utf-8')

diagnostics = [
    ('Windows pointer 入口', 'expected=1 actual=0', 'touch-red.log'),
    ('混合 source batch', '接纳了混合 batch 的 Down', 'regression-red2.log'),
    ('旧 generation/native cancel', '旧取消结束了复用后的 pointer', 'regression-red2.log'),
    ('取消后的相机基线', '新手势 scale 跳变', 'regression2.log'),
    ('缺失设备压力', 'missing-pressure policy expected=.5 actual=0', 'pressure-red.log'),
    ('失败鼠标 Down/capture', 'GetCapture 仍是 owner', 'mouse-capture-red.log'),
    ('真实 192 DPI 启动', '首帧持续 invalid transform or clip', 'native-stderr-first-fail.log'),
    ('D3D12 fractional clip oracle', 'progressed=0 canonical=0', 'fractional-red.log'),
]
result = {
    'baseline_revision': BASE, 'result_revision': revision,
    'repository': 'github:Mostorm-Labs/axiom', 'execution_branch': git('branch', '--show-current'),
    'result_ref': f'https://github.com/Mostorm-Labs/axiom/commit/{revision}',
    'evidence_materialization': 'Containing evidence commit; exact ref returned in chat after push.',
    'status': 'WINDOWS_MSVC_AUTOMATION_VERIFIED_PHYSICAL_PENDING',
    'captured_at': datetime.now(timezone(timedelta(hours=8))).isoformat(),
    'launch': launch, 'source_files': source_paths,
    'runtime_assets': {'icudtl.dat': hashlib.sha256(
        Path(launch['exe_path']).with_name('icudtl.dat').read_bytes()).hexdigest()},
    'tests': {'junit': 'logs/selected-tests.xml', 'per_test_results': 'test-results.md',
              'commands': 'facts/selected-commands.json', 'physical_matrix': 'manual-matrix.md'},
    'clang': {'unfiltered': 'FAIL_PREEXISTING_OUT_OF_SCOPE',
              'location': 'runtime/foundation/include/canvas/foundation/stable_order_key.hpp:29',
              'raw_evidence': 'logs/clang-unfiltered-fail.log',
              'changed_translation_units_with_exact_unchanged_header_filter': 'PASS',
              'full_unfiltered_clang_pass_claimed': False},
    'diagnostic_history': [{'id': a, 'observed': b, 'observed_status': 'FAIL',
                           'regression_status': 'PASS', 'red_log': 'logs/' + c}
                          for a, b, c in diagnostics],
    'remaining_blockers': ['用户尚未逐项完成人工触摸/视觉矩阵，保持 PHYSICAL_PENDING。',
                           '原始 Clang /WX 被未改动的 foundation header 告警阻断；未扩大修复范围。'],
    'needs_next_windows_input_narrow_fix': '尚无自动回归失败；由人工矩阵决定。',
    'gate_or_phys_pass_claimed': False,
}
write_json(HERE / 'result.json', result)
report = f'''# Windows Minimal Runtime Host 本轮结果

实现和构建源码 revision：`{revision}`。基线：`{BASE}`。
后续证据提交只保存日志/报告；它不会更改这份源码或 exe。

Windows 触摸接入、完整 pointer key、逐指取消、promoted mouse 去重、mask-gated
history/pressure/contact/tool、失败 Down 清理和 resize/focus/capture 接线已实现。
Runtime 的取消相机收尾和压力能力传递已修复。真实启动另外复现并修复了
192 DPI 下 production clip 错用整数 fixture 校验的问题；headless 合同保留。
AutoIntent 的原有 250 ms 逻辑保持不变，没有增加 rotation 支持。

MSVC 19.51 `/W4 /WX` 构建通过；{len(cases)} 个选定测试逐项见
[test-results.md](test-results.md)，原始断言输出在 [JUnit](logs/selected-tests.xml)。
真实 D3D12 首帧和 CLI 空闲采样见 [运行日志](logs/native-input.log) 与
[idle observation](facts/idle-observation.json)。这不构成人工触摸或视觉 PASS。

exe：`{launch['exe_path']}`

SHA-256：`{launch['exe_sha256']}`；实际进程 PID：`{launch['pid']}`，进程路径和文件 hash 相符。
启动日志内嵌源码 revision，`dirty=0`。Debug UI 编译启用，F12 显示仍待人工观察。

设备为 HJS-OPS-ADLP-A08、Windows 11 Pro 22631、i5-12450H、Intel UHD Graphics
驱动 32.0.101.7082，HID 触摸屏 VID_08D3/PID_1000。
Win32 当前桌面报告 DISPLAY1、2560×1440、192 DPI；VideoController 另报告 3840×2160，
两种 OS 查询结果均保留，未将它们混写为同一实测分辨率。
详见 [machine](facts/machine.json) / [displays](facts/displays.json)。

原始 Clang `/WX` 仍为 FAIL：未改动的 stable_order_key.hpp:29 有 sign-compare 告警。
仅屏蔽该确切旧 header 的预包含诊断后，本轮改动的 9 个 translation units 通过
clang-cl 22.1.8 `/W4 /WX /Zs`。不声称完整 Clang 构建通过。

## 构建和复核命令

在仓库根执行；原始实际参数保存在 facts/selected-commands.json 和日志中。

```powershell
python tools/setup_build_environment.py --core --semantic --facts-output out/build-environment.json --status
python tools/setup_build_environment.py --semantic --offline --status
cmd /d /c 'call "C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\Common7\\Tools\\VsDevCmd.bat" -arch=x64 -host_arch=x64 && python .aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/windows-touch-20261019/configure.py && python .aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/windows-touch-20261019/run_checks.py'
```

配置保留 full Debug UI、Render、RF01、Skia headless；SDK facts 中的 host protoc/runtime
通过 consumer_cmake_arguments 消费。Skia 复核为 installed、0 下载字节；没有重编 SDK。
工具链精确版本见 [toolchain](logs/toolchain-facts.log)。

## 失败历史与回归

以下 FAIL 均保留为已观察事实；最终回归通过不覆盖历史失败。
部分早期诊断针对未提交的中间工作树，其输出不冒充精确提交的 Gate 证据。
最终构建和回归统一绑定上面的精确 result_revision。

| 诊断 | 观察到的 FAIL | 原始日志 | 最终回归 |
|---|---|---|---|
'''
report += '\n'.join(f'| {a} | {b} | [log](logs/{c}) | PASS |' for a, b, c in diagnostics)
report += '''

## changed_files

'''
report += '\n'.join(f'- `{p}`' for p in source_paths)
report += '''

- 本目录下新增诊断、命令、facts、逐项测试结果和人工矩阵。

## 未完成项

[人工矩阵](manual-matrix.md) 全部维持 PHYSICAL_PENDING，等待用户逐项 actual。
无本轮自动回归失败；是否需要下一轮 Windows 输入窄修复取决于这些观察。
基础层 Clang 告警需另行授权的窄修复。未修改 Authority、Golden、历史 Gate、PHYS
或 Android 状态；没有宣称独立 Control Review/Gate/真机验收 PASS。

Win32 采集依据为 Microsoft 的
[pointer history](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getpointertouchinfohistory)、
[pointer capture](https://learn.microsoft.com/en-us/windows/win32/inputmsg/wm-pointerdown) 和
[optional touch masks](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-pointer_touch_info)。
'''
(HERE / 'REPORT.md').write_text(report, encoding='utf-8')
print(json.dumps({'result_revision': revision, 'exe_sha256': launch['exe_sha256'],
                  'junit_cases': len(cases), 'evidence_directory': str(HERE)}, indent=2))
