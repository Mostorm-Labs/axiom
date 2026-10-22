# FC-02：delta2、History 与 Runtime 文本入口首轮实现

2026-10-19。任务起点 `3ddc58d2fa9d21f107e9f208c3a5dbf954a34290`，
分支 `codex/fc02-runtime-boundary-design`；main 保持
`ea135122f8e3266cafa433d69903f7950a5e6300`。本报告所在提交绑定最终源码；
[执行范围](fc02/implementation-closure.json)没有改写 Authority 或历史 Gate。

## 已实现的链路

`RuntimeFacade → composition owner queue → InteractionRuntime / TextEditSession →
CanvasEditingRuntime / EditorHistory → Semantic OperationEngine → Scene / TextLayout`。
MinimalRuntimeFacade 只转发。测试中的 FakeIME 使用真实 RuntimeComposition、
Semantic store、OperationEngine 和 History；先前 17 项协议原型不作为生产实现证明。

- RichText kind4/version2 持久化对象独立 wrap_width；范围为 float 正常最小正值至最大有限值。
  新草稿默认 320，legacy 无 layout 的语义保持不变；显式布局步骤才迁移版本。
- EditRichText 仍为 OperationKind14、普通 envelope/payload version1。
  delta1 保留原六步骤；delta2 必须具备按完整 DocumentId/ObjectId 绑定的 32 字节状态 guard。
  文本步骤可组合，SetLayout / RestoreState 各自必须独占 delta，失败不产生部分写入。
- 精确 Undo 保留段落 ID、run 边界、样式与布局/version。RestoreState 只替换文本状态，
  不覆盖 placement、transform、properties。History 在 apply 前准备/预留记录；
  文本创建 Redo 使用 RestoreObjects，补偿使用新完整 OperationId。
- typed UTF-16 输入经 ownerPump 执行；预编辑不进入文档。issued token 分离 Runtime、文档、
  attachment、view、adapter、session、projection 和完整对象身份；resize 保留输入会话。
  处理重复、碰撞、顺序、过期、确认释放、背压、取消、迟到回调及强制关闭。
- 工具切换和显式 Finish 在合成期间发出稳定 finish effect，等待真实 Commit。
  失败保留可见的待恢复输入，原回执保持拒绝，显式 Retry 可以提交；退出回执读回 canonical
  Operation 身份。发布回调只能观察提交前编辑投影，不能误触发自身的远端冲突处理。
- 文本占有输入时仍让平台 ingress 维护 pointer lifecycle，但不再向 Ink/Shape/Pan 重复派发。
  锁定/隐藏目标和祖先不可写。空新草稿不创建对象；已有对象的文本删除不会删除对象。
- 首版编辑投影/单次输入上限为 65536 UTF-16 units，超限明确拒绝/Unsupported；
  这不是 canonical 文档的大小限制，也不截断文档。scalar 与 UTF-16 边界检查拒绝孤立 surrogate。
- TextLayout 使用每对象局部 LayoutContext 和对应 cache identity，独立宽度不会污染其它对象
  或 legacy context。字体仍来自绑定资源，没有系统字体替代。

## Snapshot3 差异已落实

沿用既有 accepted digest projection：semantic hash 使用 framed semantic projection，
checkpoint hash 保留 document_digest、只排除 checkpoint_digest。此前错误算法生成的
algorithm1 数据会被拒绝，不引入同名兼容算法或静默重签。

required capability IDs 严格排序/去重/精确匹配实际内容，可组合 Triangle 与 RichText2；
未知、缺失、冗余 capability 和非规范 frontier 拒绝。Snapshot1/2 不承载 RichText2。
Snapshot3 typed bootstrap 需要调用方独立给定 DocumentId / source revision / frontier /
checkpoint digest，并在发布对象和 manifest 前校验 profile、digest 值和绑定。
调用方必须从可信恢复边界提供 binding；复制未受信快照中的值不构成身份认证。
混合 Ink + Triangle + RichText1 + RichText2 + Font manifest 已验证 Reference/Indexed 恢复。

历史 descriptor.lock、Authority、Golden、SDK locks 未改。
只更新 current descriptor 与增量字段检查；current SHA-256：
`ed3774e59cd4f5ca4d18d07d0f764f9dad5c241cef40f96dad06f149efdc5561`。
历史 SHA-256 仍为
`8c1da1c19b754f9345472f96452e58505ebd18c0874b04480e378eb95a38654d`。

## 验证与失败保留

[证据目录](fc02/evidence/initial-runtime/summary.json)记录每项 expected / actual / result，
原始 RED 和最终 JUnit 一并保留。实现失败记录为 FAIL，后续修复结果单独记录。

| 验证 | 最终结果 |
| --- | --- |
| 初始缺口：未知 Snapshot capability、typed digest、accepted projection、文本 History 支持 | 4 项 RED，最终 GREEN |
| 补充缺口：发布重入、文本输入穿透、失败终结重复回调 | 3 项 RED，最终 GREEN |
| FC02Text：真实 Semantic / History / Facade，包括拒绝路径 | 20/20 PASS |
| 选定 CTest（包含 FC02Text、既有 codec/engine/Snapshot1/2/History、Brush frame retry、composition/facade、Windows touch/input/resize） | 223/223 PASS |
| 当前 schema 可复现、增量约束与历史冻结保留 | 9/9 PASS |
| 严格 Clang 22.1.8 /W4 /WX / -Wextra -Wpedantic -Werror | 23 个改动源文件 PASS；syntax-only，不冒称 Clang 链接/运行 |
| MSVC 19.51.36260 x64 /WX；Windows D3D12 Minimal Host | 构建 PASS |

中间回归曾有两项失败：错误目标返回码的实现回归已恢复；新增 codec 测试的 oracle 已按
既有 decode/validate 分层纠正。缺 guard、未知 delta version 在 Engine 写入前拒绝；
缺失 width presence、未知 step 在 codec 拒绝。旧三参数状态校验接口也保持原签名。
额外 RED fixture 将 lost 后恢复调用从 bind 改为既有 rebind，不能把该 fixture 误用当成产品缺陷。

## 构建和复现

在已有独立 worktree、VS 2026 x64 Developer 环境执行：

```powershell
python tools/setup_build_environment.py --core --semantic --facts-output out/fc02-build-environment.json --status
python tools/setup_build_environment.py --semantic --offline --status
# 使用输出的 AXIOM_PROTOC、AXIOM_SEMANTIC_RUNTIME_ROOT、CMAKE_PREFIX_PATH；不重编 SDK。
cmake --build out/fc01-closeout/build --parallel 6 --target <targets.txt 中的目标>
ctest --test-dir out/fc01-closeout/build --tests-from-file <ctest-names.txt 完整路径> --output-on-failure -j 4
$env:AXIOM_PROTOC = (Get-Content out/fc02-build-environment.json -Raw | ConvertFrom-Json).environment.AXIOM_PROTOC
python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract -v
```

精确目标、测试名和 Clang 编译命令在证据目录。复用现有构建 cache，保持 Debug UI=ON/full、
Render/RF01/Skia headless=ON、锁定 Skia SDK、MSVC 19.51 与 /MT。
构建路径为 `out/fc01-closeout/build/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe`。
提交前资格构建身份/hash 单独记录；提交后的 clean revision 产物在
`out/fc02-runtime/final-artifact.json` 记录并在交付时返回，不混淆两者。

## 后续边界

本轮是指定底层实现闭环，不宣布 FC-02 产品完成、独立 Control Review、Gate PASS 或 PHYS PASS。
Windows/Web 原生 IME adapter、Text 工具与单击/双击/键盘 UI、caret/selection/hit-test 几何、
候选窗口定位和受控页面离开接线尚待实现。Enter=Finish、Ctrl+Enter=段落的 typed intent 已存在，
本轮没有把 FakeIME 调用冒充真实键盘/IME 行为。真实输入和呈现仍为 PHYSICAL_PENDING。
Legacy Web/Playground 测试接线、Android 修复、FC01A/FC03/FC04 未推进。
