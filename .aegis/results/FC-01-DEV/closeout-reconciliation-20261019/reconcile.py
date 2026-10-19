"""Read-only FC-01 closeout reconciliation; append a scoped audit, never amend old evidence."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime, timezone

root = Path.cwd()
package = root / '.aegis/packages/FC-01'
results = root / '.aegis/results/FC-01-DEV'
destination = results / 'closeout-reconciliation-20261019'
destination.mkdir(exist_ok=True)
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()
def save(name, value):
    (destination / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

head = git('rev-parse', 'HEAD')
source = 'e42c907f827e33864d80a47ba7f9d5938ba14e3a'
assert git('remote', 'get-url', 'origin').lower().removesuffix('.git') == 'https://github.com/mostorm-labs/axiom'
for ancestor in ['2942c63b375366ed977b834aa0a48fe2fb551c6e',
                 '62a99c439e39636088c55e43cf7d76963bfe20eb',
                 'c98110c015f975012f25db5efa66c9679da35630',
                 '9ef47bb773c526ef9df0799ee5bb5291b3593fec', source]:
    subprocess.run(['git', 'merge-base', '--is-ancestor', ancestor, head], check=True)
production_delta = git('diff', '--name-only', source, head, '--', 'runtime', 'apps', 'debug_ui',
                       'verification', 'schema', 'tools', 'cmake', 'CMakeLists.txt')
assert production_delta == ''
descriptor = read(package / 'package.json')
bindings = []
for key, ref in descriptor.items():
    if key.endswith('_ref') and isinstance(ref, dict) and 'sha256' in ref:
        path = package / ref['path']
        data = subprocess.check_output(['git', 'show', head + ':' + path.relative_to(root).as_posix()])
        actual = hashlib.sha256(data).hexdigest()
        assert actual == ref['sha256'], ref['path']
        bindings.append({'path': path.relative_to(root).as_posix(), 'sha256': actual, 'result': 'MATCH'})
assert len(bindings) == 5
contract = read(package / 'evidence-contract.json')
focus = {
 'E01': 'Complete frozen SH2-G01..25 mapping and independent wire/typed admission comparison; preserve V1 negatives.',
 'E02': 'Qualify typed/wire/restore/replay/remote, idempotency and Snapshot2 reader/writer capability cases.',
 'E03': 'Map the frozen Create terminal/source-loss schedules to current common Runtime; preserve historical RED chronology honestly.',
 'E04': 'Requalify independent far-axis Snap, DPR/camera math and preview-to-Up consistency on current source.',
 'E05': 'Requalify analytic Triangle/Line/Arrow geometry, Full/Incremental Scene and pinned headless raster cases; locked Skia is now available.',
 'E06': 'Qualify selected commands and complete conditional History/preimage/delete-closure conflict matrix; do not infer missing implementation from obsolete reports.',
 'E07': 'Map current RuntimeFacade owner queue, target fences, coherent full-ID readback and UI routing to the frozen matrix.',
 'E08': 'Bind current real Minimal Windows/Web builds and adapter observations; keep Android user deferral explicit and reconcile frozen applicability before full closure.',
 'E09': 'Qualify Canonical Applied, backend-qualified Presented and Axis durability/NoEcho as distinct facts on current owners.',
 'E10': 'Unify exact package/source/provider identities, inherited authorized deltas, protected scope and regression mapping; independent review decides final Gate.'
}
matrix = []
for family in contract['required_evidence_families']:
    path = results / family['path']
    old = read(path)
    extra_path = path.parent / (family['id'].lower() + '-result.json')
    extra = read(extra_path) if extra_path.exists() else None
    matrix.append({
        'id': family['id'], 'obligation': family['obligation_id'],
        'frozen_classification': family['classification'],
        'historical_record': path.relative_to(root).as_posix(),
        'historical_status': old.get('status'),
        'historical_subject': old.get('source_subject_revision') or old.get('baseline'),
        'historical_gap': old.get('residual_detection_gap'),
        'supplemental_historical_gap': extra.get('residual_detection_gap') if extra else None,
        'current_qualification': 'NOT_YET_QUALIFIED_BY_THIS_AUDIT',
        'current_code_failure_inferred': False,
        'closeout_focus': focus[family['id']]})
assert len(matrix) == 10
save('package-preflight.json', {'repository': 'Mostorm-Labs/axiom', 'head': head,
    'source_candidate': source, 'required_ancestry_verified': True,
    'production_delta_since_candidate': [], 'bindings': bindings})
save('obligation-reconciliation.json', matrix)
save('result.json', {
    'recorded_at': datetime.now(timezone.utc).isoformat(), 'status': 'CLOSEOUT_AUDIT_ONLY_NOT_GATE_CLOSURE',
    'task': 'FC-01 closeout', 'examined_head': head, 'source_candidate': source,
    'package_materialization_ref': 'c98110c015f975012f25db5efa66c9679da35630',
    'user_direction': '先把FC-01完全收尾；FC-01A考虑在FC-02/03之后并入FC-04实施，后续计划待FC-01收尾后再评估。',
    'planning_status': 'User direction recorded; no roadmap Authority or frozen package edited.',
    'inherited_current_evidence': [
        '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-ui-clang-repair-20261019/result.json@b3a46ecdb282f92eabfe6e42c4773d905cfca0c8',
        '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-manual-feedback-20261019/result.json@aef78929b79d4c479b5ba9ac37114dc6c601aebe'],
    'findings': [
        'Exact FC-01 package/control/task/result ancestry and all five frozen package bindings verified.',
        'Current application source equals e42c907f; subsequent commits through examined head contain evidence only.',
        'Old FC-01 evidence spans several historical subjects and cannot be silently relabelled as the current complete E01-E10 result.',
        'Some old implementation/environment gaps may already be resolved; requalification is required, not automatic code-failure classification.',
        'verification/corpus/fc01/v1 is not materialized in this checkout; the existing run_dev.py is a build/test driver, not a complete frozen independent scenario runner.',
        'Android remains deferred by explicit user instruction; the original B08 applicability and any full-closure disposition require explicit reconciliation.',
        'Historical first-mutation RED ordering cannot be retroactively manufactured; preserve recovery evidence and route its disposition to independent review.'
    ],
    'next_action': 'Requalify existing independent FC-01 scenarios on the current subject, filling only frozen scenario/runner gaps; classify implementation failures before narrow repair.',
    'production_changes': [], 'authority_changes': [], 'gate_changes': [], 'android_status_changed': False,
    'fc01a_work_started': False, 'official_gate_pass_claimed': False})
(destination / 'REPORT.md').write_text(f'''# FC-01 收尾核对（首轮）

当前优先级：先完整收尾 FC-01，再评估 FC-02/03 及后续计划。
用户提出 FC-01A 在 FC-02/03 之后并入 FC-04 实施；本轮仅记录方向，不提前开展套索设计/实现，
不改路线 Authority 或既有冻结合同。

核对 HEAD：`{head}`。候选产品源码：`{source}`；两者之间仅有证据提交。
FC-01 原任务、控制、P31 包和旧结果的 ancestry 均已验证；五个包内文件哈希均匹配。

Windows/Web 当前构建、自动回归、用户实测反馈已经存档。它们是 FC-01 收尾可复用的材料，
不能自动代替原合同 E01–E10 的完整独立场景。旧 E06/E07 的 NOT_IMPLEMENTED 等文字也不能
直接判定当前代码缺失：这些记录绑定旧源码，之后 Runtime/Host 已有显著改动。

| 项目 | 当前收尾重点 |
|---|---|
{chr(10).join('| ' + row['id'] + ' | ' + row['closeout_focus'] + ' |' for row in matrix)}

完整映射见 obligation-reconciliation.json。当前主源码下尚无 verification/corpus/fc01/v1；
run_dev.py 负责构建/测试，仍需确认并补齐冻结独立场景的执行与映射。已有机制充分覆盖的项目
可复用，避免把置信度补强变成新的阻塞条件。发现真实实现失败后，先复现 RED，再做窄修复。

平台范围：Windows/Web 使用当前真实 Minimal consumers；原 FC-01 平台义务与后续 consumer
迁移之间应保留明确映射。Android 保留用户延期状态，不能静默改为 PASS 或从冻结义务中删除。
历史首轮 RED 执行顺序也不补造，由独立审查明确处置。

本轮只完成现状核对，没有重新执行 FC-01 全套场景，也没有宣布 FC-01-DEV 或 PHYS PASS。
下一步按原 FC-01 接受标准收敛当前版本的证据与实现缺口，准备可独立审查的最终交付。
''', encoding='utf-8')
shutil.copy2(root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-manual-feedback-20261019/seal_evidence.py', destination / 'seal_evidence.py')
shutil.copy2(Path(__file__), destination / 'reconcile.py')
print('Verified exact FC-01 ancestry and 5 package bindings; reconciled 10 original obligations without changing historical records.')
