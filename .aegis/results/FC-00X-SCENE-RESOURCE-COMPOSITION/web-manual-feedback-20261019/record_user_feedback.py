import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime, timezone

root = Path.cwd()
previous = root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-ui-clang-repair-20261019'
destination = previous.parent / 'web-manual-feedback-20261019'
destination.mkdir(exist_ok=True)
source = 'e42c907f827e33864d80a47ba7f9d5938ba14e3a'
prior_evidence = 'b3a46ecdb282f92eabfe6e42c4773d905cfca0c8'
subprocess.run(['git', 'merge-base', '--is-ancestor', source, 'HEAD'], check=True)
artifacts = json.loads((previous / 'repair-on-artifacts.json').read_text())
assert artifacts['source_revision'] == source and artifacts['source_dirty'] == 0
record = {
    'recorded_at': datetime.now(timezone.utc).isoformat(),
    'source_revision': source,
    'prior_evidence_ref': prior_evidence,
    'status': 'USER_REPORTED_WEB_VERIFICATION_PASS',
    'observer': 'human user',
    'statement': 'web端测试也通过',
    'context': 'Follow-up to the compact Web Debug UI and strict Clang repair; correct Minimal Web page was opened at this source revision.',
    'result': 'PASS',
    'result_scope': 'User-reported overall Web retest; no new defect reported after the width/compiler repair.',
    'individual_matrix_results': 'No item-by-item observations supplied; prior detailed unobserved items are not automatically promoted to PASS.',
    'artifacts': artifacts,
    'production_changes': [],
    'formal_gate_or_phys_status_changed': False,
    'next_narrow_fix': 'None identified by this feedback.'
}
(destination / 'result.json').write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
(destination / 'REPORT.md').write_text(f'''# Web 人工复验反馈

用户在本轮 Debug UI 宽度与 Clang 修复完成后确认：

> web端测试也通过

结果：**PASS（用户报告的 Web 整体复验）**。本次反馈没有提出新的缺陷，当前没有由此反馈产生的下一轮窄修复。

- 源码 revision：`{source}`
- 前一轮构建与自动回归证据：`{prior_evidence}`
- 对应入口：`/axiom_canvas_demo/`，Debug UI ON/full。
- 产物身份沿用前一轮已核对的构建清单，详见 result.json。

这是本机测试流程中的用户观察记录。未提供逐项输入/观察的矩阵项目不自动改写为 PASS；
不变更 Authority、历史 Gate、正式 PHYS 状态、Android 状态或此前的 FAIL/PENDING 证据。
本次只追加反馈材料，不修改产品代码，也无需重新构建。
''', encoding='utf-8')
shutil.copy2(previous / 'seal_evidence.py', destination / 'seal_evidence.py')
shutil.copy2(Path(__file__), destination / 'record_user_feedback.py')
print(f'Recorded user-reported Web PASS for {source}.')
