"""Aggregate existing raw observations; never convert baseline failures to PASS."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
SOURCE = 'f3d9e67508b4b8f6e2ddbf756b51c543efeafa55'
BASE = 'af1aff801369052eb7b94716f6b4ab3e899eb3e9'
BRANCH = 'refs/heads/codex/fc08-section-scene-host'
PKG = '.aegis/packages/FC-08-SECTION-DE-v0.5/package.json'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode().strip()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def put(name, value):
    p = OUT / name
    encoded = json.dumps(value, indent=2, ensure_ascii=False) + '\n'
    if p.exists():
        assert p.read_text(encoding='utf-8') == encoded, p
        return
    p.write_text(encoded, encoding='utf-8')

def ref(path):
    p = Path(path)
    return {'path': p.relative_to(ROOT).as_posix(),
            'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}

def suite(phase):
    cases = ET.parse(OUT / phase / 'ctest.xml').getroot().findall('testcase')
    failed = [x.attrib['name'] for x in cases if x.find('failure') is not None]
    skipped = [x.attrib['name'] for x in cases if x.find('skipped') is not None]
    return {'tests': len(cases), 'passed': len(cases)-len(failed)-len(skipped),
            'failed': failed, 'skipped': skipped}

assert git('rev-parse', 'HEAD') == SOURCE
assert git('symbolic-ref', 'HEAD') == BRANCH
assert not git('diff', '--name-only')
native = suite('native-exact-008')
semantic = suite('semantic-exact-002')
baseline = suite('baseline-raster-001')
assert native['tests'] == 38 and native['failed'] == ['FC01TriangleConnectorRaster'] and not native['skipped']
assert semantic['tests'] == 690 and semantic['failed'] == ['StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds'] and not semantic['skipped']
assert baseline['failed'] == native['failed']
assert (OUT / 'baseline-raster-001/identity.stdout.log').read_text().strip() == BASE
assert not (OUT / 'baseline-raster-001/preservation.stdout.log').read_text().strip()
browser = read(OUT / 'browser-exact-004/observations.json')
assert len(browser['rows']) == 42 and all(x['ok'] for x in browser['rows'])
assert browser['final']['revision'] == SOURCE and browser['final']['dirty'] == 0 and not browser['final']['failure']
assert read(OUT / 'browser-exact-004/invocation.json')['exit_code'] == 0
for phase in ['native-exact-008', 'web-exact-012']:
    inv = read(OUT / phase / 'invocations.json')
    assert inv['source_head'] == SOURCE
    assert all(c['exit_code'] == 0 for c in inv['commands'] if c['name'] != 'ctest')
    for entry in read(OUT / phase / 'source-manifest.json'):
        assert hashlib.sha256((ROOT / entry['path']).read_bytes()).hexdigest() == entry['sha256'], entry['path']
assert all(c['exit_code'] == 0 for c in read(OUT / 'python-003/invocations.json')['commands'])
for phase in ['preflight-exact-007', 'audit-exact-006']:
    for name in ['PackageBindingPreflight.json', 'EvidenceContractPreflight.json']:
        assert read(OUT / phase / name)['status'] == 'PASS'

put('native-exact-008/classification.json', {
    **native, 'ctest_exit_code': 8, 'new_failures': [],
    'classification': 'BASELINE_VERIFICATION_DEBT; raw failed status retained',
    'independent_exact_baseline': ref(OUT / 'baseline-raster-001/invocations.json')})
put('baseline-raster-001/classification.json', {
    'source_revision': BASE, 'production_tree_clean': True,
    'same_assertion': 'skia_headless_backend_test.cpp:751; pixel(129,103) == RGBA(0,255,0,255)',
    'original_expected_and_test_source_unchanged': True,
    'test_source': ref(ROOT / 'runtime/render/tests/skia_headless_backend_test.cpp'),
    'outer_command': 'cmd.exe /c call VS18/Community/VC/Auxiliary/Build/vcvars64.bat && python out/fc08-section-de/baseline-raster.py',
    'scope': 'Read-only detached baseline worktree and separate build; no current-source replacement',
    'result': baseline, 'ctest_exit_code': 8})

owners = {
    'semantic_staged_apply': ['runtime/semantic/src/operation_engine.cpp'],
    'forward_inverse_and_history_admission': ['runtime/interaction/src/undoable_command.cpp', 'runtime/interaction/src/editor_history.cpp'],
    'section_planner_and_prepared_selection_consequence': ['runtime/interaction/src/canvas_section_commands.cpp', 'runtime/interaction/src/selection_session.cpp'],
    'scene_projection_and_recovery': ['runtime/scene/src/hierarchy_geometry.cpp', 'runtime/scene/src/scene.cpp', 'runtime/scene/src/runtime_scene_projection.cpp', 'runtime/scene/src/incremental_runtime_coordinator.cpp'],
    'public_command_queue_and_composition': ['runtime/foundation/include/canvas/runtime/section_input.hpp', 'runtime/control/src/canvas_control_service.cpp', 'runtime/composition/src/canvas_runtime_facade.cpp', 'runtime/composition/src/canvas_runtime_section.cpp'],
    'title_shaping': ['runtime/text/src/text_layout.cpp'],
    'host_consumers_only': ['debug_ui/src/panels/control.cpp', 'debug_ui/src/control_router.cpp', 'apps/axiom_canvas_demo/platform/web/section_bridge.inl'],
}
put('ImplementationDesignPreflight.final.json', {
    'source_revision': SOURCE, 'package': ref(ROOT / PKG),
    'findings': 'Existing owners retained; no unresolved authority or semantic decision for this bounded D/E slice',
    'owner_source_bindings': {k: [ref(ROOT / p) for p in v] for k, v in owners.items()},
    'commit_flow': 'Public bounded request -> owner Interaction -> actual C F/U admission and real History reservation -> staged Semantic commit -> prepared existing SelectionSession consequence -> existing Scene publication',
    'publication_failure': 'Canonical Committed + projection Pending, then coherent existing recovery; no command resubmission',
    'rejection': 'Precommit busy/stale/invalid/numeric/History rejection has no canonical/History/redo write',
    'pose': 'One ancestor composition; binary64 ordered PoseConstruction with contraction disabled, explicit quality rejection; no child movement for boundary resize',
    'preview': 'Existing transient lane and handoff, Section boundary content override only; descendants projected from one ancestor override',
    'evidence': [ref(OUT / p) for p in ['red-geometry/invocation.json', 'red-consumer/invocation.json', 'red-default-removal/invocation.json', 'red-group-consequence/ctest.xml', 'native-exact-008/ctest.xml', 'browser-exact-004/observations.json']],
    'not_claimed': ['Full302 qualification', 'allocation/fault qualification completion', 'independent Gate', 'physical input'],
})

protected = ['schema', 'tests/fc08/reference', '.aegis/materialized',
             'semantic-sdk.lock.json', 'r1-full-skia-sdk.lock.json',
             'runtime/semantic/src/qualification_checkpoint.hpp',
             'runtime/interaction/src/qualification_checkpoint.hpp']
assert not git('diff', '--name-only', BASE, SOURCE, '--', *protected)
assert git('diff', '--name-only', BASE, SOURCE, '--', 'runtime/semantic').splitlines() == ['runtime/semantic/src/connector_validation.cpp']
put('preservation.json', {
    'baseline': BASE, 'source_revision': SOURCE, 'protected_paths': protected,
    'protected_diff': [], 'historical_evidence_and_packages_preserved': True,
    'all_previous_packages_including_product_P31_unchanged': True,
    'global_registries_and_main_not_changed': True,
    'only_semantic_delta': 'Explicit kSection alongside kGroup in existing non-connectable return-false switch; exhaustive compiler coverage only',
    'new_semantic_behavior': False,
    'qualification_hooks': 'Native and Web production builds explicit OFF; unchanged source/default; inherited C qualification test build reused separately',
})
put('changed-files.json', git('diff', '--name-only', BASE, SOURCE).splitlines())
for name in ['environment.json', 'web-environment.json']:
    shutil.copyfile(ROOT / 'out/fc08-section-de' / name, OUT / ('emitted-' + name))

native_build = ROOT / 'out/fc08-section-c/native'
web_build = ROOT / 'out/fc08-section-de/web'
flags = {}
for name, build in [('windows', native_build), ('web', web_build)]:
    entries = read(build / 'compile_commands.json')
    info = next(e for e in entries if e['file'].replace('\\', '/').endswith('/runtime/control/src/build_info.cpp'))
    command = info.get('command', str(info.get('arguments', [])))
    assert SOURCE in command and 'AXIOM_BUILD_DIRTY=0' in command
    assert 'AXIOM_FC08_QUALIFICATION_HOOKS:BOOL=OFF' in (build / 'CMakeCache.txt').read_text()
    flags[name] = info
put('build-identity.json', flags)
exe = native_build / 'apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe'
assert exe.is_file()
resources = [p for p in exe.parent.rglob('*') if p.is_file() and
             (p.name in {'axiom-font.otf', 'icudtl.dat', 'LICENSE', 'NOTICE'} or 'fonts' in p.relative_to(exe.parent).parts)]
put('artifacts.json', {
    'source_revision': SOURCE, 'compiled_dirty': 0,
    'windows_exe': {'path': str(exe), 'sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'bytes': exe.stat().st_size},
    'required_runtime_resources': [{'path': p.relative_to(exe.parent).as_posix(), 'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(resources)],
    'transfer_rule': 'Keep the executable with its adjacent runtime font/ICU resources, not the executable alone',
    'windows_interactive_process': 'NOT_STARTED_THIS_SLICE; native real consumer/raster execution recorded; physical GUI pending',
    'web_artifact_hashes': read(OUT / 'browser-exact-004/invocation.json')['artifacts'],
    'web_profile': 'Actual Chrome/WASM, headless SwiftShader; not a native GPU/touch observation',
})

readme = '''# FC-08 Section D/E — bounded implementation result

实现提交：`f3d9e67508b4b8f6e2ddbf756b51c543efeafa55`。物化分支：`refs/heads/codex/fc08-section-scene-host`。
返回 CONTROL_REVIEW；本记录不宣布 FC-08、DEV、P34、PHYS 或 release 通过。

## 实现范围

- Scene 复用已有 Placement 森林，祖先变换只组合一次；Section 使用自身边界，子对象超出边界仍可见。支持负边界原点、Group 根级选择、现有作用域与一致发布/恢复。
- `RuntimeFacade::submitSectionIntent/readSection` 通过已有队列、Composition、Interaction、真实 F/U 与 EditorHistory 路径提供创建、包裹、释放、重挂父级、边界缩放、重命名和选择。快照有完整目标身份与选择 revision；回执区分 canonical 提交和 Scene 就绪。
- Windows/Web 共用 Debug UI 的 Control → Section。宿主只提交意图。Section 标题由既有 TextLayout 注册字体排版，晚到字体资源可刷新；没有新增 canonical RichText 子对象。
- 拖动预览与取消沿用既有路径；边界缩放不移动子坐标。默认删除一个 Section 释放子对象；混合破坏性删除在写入前拒绝。

## 验证

| 项目 | 原始结果 |
| --- | --- |
| PackageBindingPreflight / EvidenceContractPreflight，真实 worktree `--repo .` | PASS，见 preflight-exact-007 |
| Windows /WX Release 与实际 Web Minimal Host | 构建 exit 0，源码 exact revision，dirty 0 |
| Native scoped + impacted CTest | 38 项：37 PASS，1 既有失败，0 skipped，原始 CTest exit 8 |
| 继承的语义/History 矩阵 | 690 项：689 PASS，1 既有失败，0 skipped，原始 CTest exit 8 |
| Axiom Python contract tests | 10 PASS；运行时边界检查 101 个合同文件通过 |
| 实际 Chrome/WASM | 42 个断言通过；包括真实 glyph、世界坐标、Undo/Redo、重复/过期请求及继续渲染 |

两项失败均保留原始失败状态：

- `StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds`：继承 A/B/C 已独立复现的 Shape RichTextTarget 旧债。
- `FC01TriangleConnectorRaster`：精确旧基线 `af1aff80` 的隔离工作树同样在未修改测试的第 751 行、同一像素失败；见 baseline-raster-001。没有修改 expected 或旧测试。

RED（几何、普通消费者入口、默认删除、Group 选择后置结果）与修复后的 GREEN 均保留。早期构建、工具或桥接输入失败没有覆写。

## 复现与人工测试入口

在本 worktree、VS 2026 x64 环境下运行（phase 使用新目录名，已有证据不可覆盖）：

```powershell
python tools/fc08/section_de.py preflight --repo . --phase <new-preflight>
python tools/fc08/section_de_build.py --repo . --profile windows --phase <new-native>
python tools/fc08/section_de_build.py --repo . --profile web --phase <new-web>
python tools/fc08/section_de_regression.py --repo . --phase <new-semantic>
python tools/fc08/section_de_regression.py --repo . --phase <new-python> --python-only
node tools/fc08/section_de_browser.mjs --repo . --output .aegis/results/FC-08-SECTION-DE/<new-browser>
```

具体展开后的命令、环境、工具链、原始退出码见每个目录的 invocations.json；Chrome 为 invocation.json。消费已锁定 SDK，不重编依赖。

Windows：`out/fc08-section-c/native/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe`；保留同目录 fonts、ICU 等资源。完整路径/hash 见 artifacts.json。
Web：`out/fc08-section-de/web/apps/axiom_canvas_demo`，本机 loopback 静态服务器打开 index.html。

人工重点：F12 → Control → Section 创建；选中现有对象后 Wrap selection；移动整体再调整 Section 边界；Rename；Release 与 Undo/Redo；设置 move target 后 Move into target / Move to root。边界调整时子对象不能移动，释放后子对象不能丢失。

## 未完成范围和下一最小步

完整 302 变体验收 runner 和所有 E01–E10 资格证明尚未完成；本轮测试不是对它们的替代。实际鼠标/触摸、IME、DPI 与原生 GPU 人工验收仍 PHYSICAL_PENDING。故障/分配资格矩阵和 G5 性能结论未声明。
公共释放命令当前一次释放一个 Section；多 Section 同时删除明确 Unsupported。自动拖放重挂父级等 INT-01 高级交互继续归 FC04/FC10；PH01–04 继续归 FC13。
Debug UI 标题编辑有 4096 字节本地输入框限制，超长标题禁用该输入；公共入口支持有界完整 UTF-8，不静默截断数据。
下一最小切片是将已落地的真实命令/Scene 观察口接入剩余冻结用例，并完成 Windows/Web 真机回归；不新增另一套容器或 History。

精确 owner/source 绑定见 ImplementationDesignPreflight.final.json；所有原始证据摘要见 evidence-manifest.json。最终交付身份见 terminal-result.json。
'''
assert not (OUT / 'README.md').exists()
(OUT / 'README.md').write_text(readme, encoding='utf-8')
manifest = [ref(p) for p in sorted(OUT.rglob('*')) if p.is_file() and p.name not in {'evidence-manifest.json', 'terminal-result.json'}]
put('evidence-manifest.json', {'excluded_self_references': ['evidence-manifest.json', 'terminal-result.json'], 'files': manifest})
put('terminal-result.json', {
    'task': 'FC-08 Section bounded D/E', 'status': 'READY_FOR_CONTROL_REVIEW_WITH_BASELINE_VERIFICATION_DEBT',
    'repository': 'Mostorm-Labs/axiom', 'result_revision': SOURCE, 'source_revision': SOURCE,
    'materialized_ref': BRANCH, 'task_anchor': BASE,
    'package_ref': {**ref(ROOT / PKG), 'revision': 'fd179205395cac82425b23d8be62e538c04edd72'},
    'required_authorization_decision': 'NONE_PENDING_FOR_D_E; direct user instruction materialized, old P31/evidence unchanged',
    'implementation_design': ref(OUT / 'ImplementationDesignPreflight.final.json'),
    'evidence_manifest': ref(OUT / 'evidence-manifest.json'),
    'changed_files': ref(OUT / 'changed-files.json'), 'artifacts': ref(OUT / 'artifacts.json'),
    'results': {'native': {**native, 'ctest_exit_code': 8, 'new_failures': []},
                'semantic': {**semantic, 'ctest_exit_code': 8, 'new_failures': []},
                'python_contracts': {'tests': 10, 'passed': 10, 'skipped': 0},
                'runtime_boundaries': {'contract_files': 101, 'status': 'PASS'},
                'chrome_wasm': {'checks': 42, 'passed': 42, 'source': browser['final']['revision'], 'dirty': 0},
                'production_builds': {'windows': 0, 'web': 0}},
    'blocking_new_implementation_failures': [],
    'baseline_verification_debt': [semantic['failed'][0], native['failed'][0]],
    'physical': 'PHYSICAL_PENDING',
    'full302_runner': 'NOT_COMPLETE; NOT promoted from scoped tests',
    'not_claimed': ['DEV PASS', 'P34 PASS', 'PHYS PASS', 'Release readiness', 'hosted CI', 'G5 performance'],
    'next_smallest_slice': 'Complete real frozen-case observation bindings and physical Windows/Web regression without expanding product semantics',
})
print(json.dumps({'result_revision': SOURCE, 'native': native, 'semantic': semantic, 'browser_checks': len(browser['rows']), 'evidence_files': len(manifest)}, ensure_ascii=False))
