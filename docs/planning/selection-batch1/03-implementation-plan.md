# Selection Batch 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 Runtime 上交付矩形／自由套索公共选择链，首先以基础 Shape 证明真实输入、精确命中、追加、取消和零文档变更。

**Architecture:** SelectionSession 保持唯一正式选区所有者。新增小型 RegionSelectionSession 管理瞬态区域；Scene 查询、目标归约和提交由现有 CanvasEditingRuntime 组装，UI 只显示回读并发送意图。保留 interaction 叶层与 editor composition 层的依赖方向。

**Tech Stack:** 现有 C++20、CMake/CTest、Runtime Scene/Render、Debug UI、Windows 与 Web/WASM 宿主。无新增第三方依赖。

**Spec:** `01-confirmed-design.md`；事实依据为 `02-source-gap-audit.md`。观察提交 `2fee75f66a7f96f6aac834228ede372a9abda59a`。

## Global Constraints

相交即选中；Replace 默认、Shift-at-Down 为 Add；取消不清正式选区；Select↔Hand 保留选择；无新 canonical schema/Operation/History；不改 Section 语义或历史证据；不把未覆盖对象当 Empty；不实现整套批量编辑、Group、Snap 或 Alignment。

本计划中的新增 API/文件名是实施提议，不是仓库里已经存在或正式冻结的 ABI。可按仓库约定局部调整，但报告名称变化与对应测试，不改变行为。

## Review Focus

1. 手势开始后发生对象删除或独立选区修改：不恢复失效身份，不覆盖更新后的选区（任务 1、3）。
2. 相机、DPR、surface 或 pointer generation 变化：旧事件不提交；同样视觉区域在不同缩放下选择一致（任务 2、3）。
3. 已选手柄、Section boundary、Connector port 与文本/Crop 独占：点选路由不被区域手势抢走（任务 3、4）。
4. 凹多边形／自交套索、ellipse AABB 空角：不能以包围盒命中冒充真实相交（任务 2）。
5. 多选时既有单对象编辑按钮和手柄：不得静默只改 primary（任务 4）。

## 文件结构

### 已读取、计划修改

- `runtime/interaction/include/canvas/interaction/selection_session.hpp`
- `runtime/interaction/src/selection_session.cpp`
- `runtime/interaction/tests/selection_session_test.cpp`
- `runtime/interaction/include/canvas/interaction/canvas_editing_runtime.hpp`
- `runtime/interaction/src/canvas_editing_runtime.cpp`
- `runtime/interaction/CMakeLists.txt`
- `runtime/input/include/canvas/input/platform_input_contract.hpp`
- `runtime/input/include/canvas/input/pointer_sample.hpp`
- `runtime/input/src/platform_interaction_ingress.cpp`
- `runtime/foundation/include/canvas/runtime/canvas_control_types.hpp`
- `runtime/composition/src/canvas_runtime_facade.cpp`
- `runtime/render/include/canvas/render/editing_overlay.hpp`
- `debug_ui/src/panels/control.cpp`

### 拟新增，小范围职责分离

- `runtime/interaction/include/canvas/interaction/region_selection_session.hpp`
- `runtime/interaction/src/region_selection_session.cpp`：瞬态路径、有效性、Down 锁存、取消，不直接依赖 Scene/Render。
- `runtime/interaction/src/canvas_region_selection.cpp`：现有 CanvasEditingRuntime 的区域查询、归约、预览与正式提交；加入 canvas_runtime_editor。
- `runtime/interaction/tests/region_selection_session_test.cpp`
- `runtime/composition/tests/selection_input_integration_test.cpp`

### 路径已定位，按真实调用链最小修改

- `runtime/input/tests/platform_interaction_ingress_test.cpp`
- `runtime/composition/include/canvas/runtime/canvas_runtime_facade.hpp`
- `runtime/composition/CMakeLists.txt`
- `runtime/composition/tests/canvas_runtime_facade_test.cpp`
- `runtime/composition/tests/fc08_section_consumer_test.cpp`：优先回归，不改其预期来掩盖问题。
- `apps/ink_playground/platform/windows/main.cpp`
- `apps/ink_playground/platform/web/bridge.cpp`
- `apps/ink_playground/platform/web/index.html`
- `apps/ink_playground/platform/web/pointer_event_batch_adapter.ts`

Windows/Web 文件存在已由目录核实，函数体并未完整审计。执行时追踪实际在用的事件入口；TS adapter 不一定是页面实际使用入口，禁止只改未接线的文件。不得手改生成的 axiom_ink_playground_web.js／wasm 来代替源码修复。

Overlay renderer、Debug controls forwarding 和必要构建文件通过其现有调用者定位，记录实际路径再改。允许同一链上必需的薄转发修改，不以此扩大功能范围。

## Task 1：正式选区的 Replace/Add 与修饰键传递

**Files:** selection_session.hpp/.cpp/test、platform_input_contract.hpp、pointer_sample.hpp、platform_interaction_ingress.cpp 及其测试。

**Interfaces:** 在 interaction 定义 `SelectionCombineMode { kReplace, kAdd }`；新增 `bool SelectionSession::applyAcquisition(std::span<const foundation::ObjectId> resolvedFrontToBack, SelectionCombineMode mode, std::uint64_t expectedRevision)`。输入必须是已解析的合法 roots；内部去重、拒绝陈旧 revision，Add 保留仍合法的 primary；Replace 按确定的 front-to-back 首项设 primary。旧 click/toggle/cancel 的现有调用意图不得全局重写。

在 PlatformPointerSample 和 PointerSample 尾部追加默认无修饰键的字段，并由 normalize 传递。至少定义 Shift 标志；若复用已有 modifiers 类型优先使用。不得从 Runtime 查询系统全局键盘状态。

- [ ] 添加测试 `add_existing_root_does_not_toggle`：S0=[A,B]、H=[B,C]，结果=[A,B,C]，primary=A。
- [ ] 添加 `replace_empty_clears_add_empty_preserves`、`stale_acquisition_does_not_overwrite` 与 `duplicate_roots_are_unique`。
- [ ] 添加 ingress 测试：Down 的 Shift 保留到 normalized sample；默认不传 modifiers 的旧调用仍兼容。
- [ ] 运行新增测试，记录真实失败原因；编译失败不宣称为产品行为 RED。
- [ ] 实现最小接口并通过测试；原有 toggle 与 cancel 测试继续通过。
- [ ] 单独提交选区发布与输入契约修改。

## Task 2：区域会话与独立几何例证

**Files:** 新 region_selection_session.hpp/.cpp/test、canvas_region_selection.cpp、interaction/CMakeLists.txt。

**Interfaces:** `RegionSelectionSession` 提供 `begin/update/finish/cancel`，消费完整 PointerSample；其 begin 上下文携带当前选区 revision、相机/surface/metrics/绑定标识与模式。只保存本次 S0 快照和真实采样，不拥有另一份正式选区。finish 返回有效／无效／取消以及最终 view-space 区域，不自行提交文档。

`CanvasEditingRuntime::regionSelectionPointer(const input::PointerSample&)` 负责把最终区域变换到世界空间，查询当前一致 Scene，精确命中、共享归约后调用任务 1 接口。查询结果须区分 Complete、Partial/Unsupported、Stale/Failed，不能把查询失败当无命中。新几何辅助函数保持局部，不新增全局框架。

- [ ] 添加会话测试：Move 不改变正式 selection；Up 自动闭合；取消清临时轮廓；predicted 点不进入最终区域；Modifier 在 Down 锁存。
- [ ] 以独立坐标例证加入几何测试：ellipse 的 local bounds=[0,0,100,100]，区域[0,0,5,5]必须不命中；三角形顶点(50,0),(100,100),(0,100)与同一小区域不命中。
- [ ] 凹套索顶点(0,0),(100,0),(100,20),(20,20),(20,100),(0,100)，位于[60,60,65,65]的矩形不得被包围盒误选。
- [ ] 添加相交、双向包含、旋转 Shape 空角、自交 even-odd、重复点、NaN、退化区域与 zoom/DPR 对照。阈值沿用现有已接受配置；缺少数值则集中记录工程取值及边界断言。
- [ ] 实现会话与 Shape 精确查询；优先复用现有几何源，不使用生产结果生成预期。
- [ ] 其他 provider 能安全复用就接入；不能接入则保留 coverage 和显式不完整状态，不让本批完成声明扩大为全对象完成。
- [ ] 注册 `axiom_region_selection_tests` / CTest `FC01ARegionSelection`；新测试必须保证 assert 在所用构建配置中有效。
- [ ] 测试通过后单独提交。

## Task 3：真实输入路由与工具生命周期

**Files:** canvas_editing_runtime.hpp/.cpp、canvas_region_selection.cpp、控制类型与 Facade、selection_input_integration_test.cpp、composition/CMakeLists.txt。

**Interfaces:** 在 runtime 控制层新增 `CanvasSelectionMode { kPoint, kRectangle, kFreeform }`，旧工具枚举值不重排。通过现有控制 request/snapshot 传递子模式；Runtime 方法使用 `setSelectionAcquisitionMode(CanvasSelectionMode)`，避免与既有 `setSelectionMode(bool)` 混淆。

- [ ] 构造真实 fixture，通过已有 Operation 创建对象；测试送入 `acceptPlatformBatch` 或现有 RuntimeComposition 的正式 input 入口，不能直接调用 marquee 后宣布端到端完成。
- [ ] 先写 `blank_down_is_pending_up_clears`：普通空白 Down 不清 S0，无拖动 Up 清空；Shift 空白点击保留；超过阈值进入矩形。
- [ ] 写 `explicit_region_starts_over_object` 与 `point_mode_preserves_existing_handle_drag`；显式区域模式不启动对象变换，点选模式仍按既有手柄/连接点/Section 所有权执行。
- [ ] 写 `cancel_and_late_up_do_not_commit`、`switch_to_hand_preserves_selection`、`source_lost_cleans_only_owned_session`。
- [ ] 写删除/独立新选区/重绑定/camera/metrics/第二触点用例：旧手势不回滚较新选区，不恢复失效对象；协调器接管导航时先取消区域，保持相机连续。
- [ ] 改接 `selection(const PointerSample&, sessionId)`，保留完整 sample；新 Region 路径不得在 key/phase/x/y 转发前丢掉 Shift。
- [ ] 将关闭 Select 与显式 clearSelection 分开；保留 legacy cancel 清空语义，区域 cancel 不调用它。
- [ ] 在当前 Scene 一致版本上完成最终查询；Scene 变化重查，无法保持绑定或 scope 时取消。Selection revision 已被独立操作改变时拒绝旧结果。
- [ ] 加入零写入断言：selection 前后 semantic generation、对象状态、applied operation count、History/redo 内容一致。可利用现有测试访问机制，不新增生产级万能后门。
- [ ] 注册 `axiom_selection_input_integration_tests` / CTest `FC01ASelectionInputIntegration`，运行并提交。

## Task 4：可见操作、平台 Shift 与防误编辑

**Files:** Debug UI control、Facade、EditingOverlay 与现有 renderer；上述实际 Windows/Web 源码及必要薄 forwarding；现有 composition/facade tests。

- [ ] Debug UI 提供点选／矩形／自由套索和 Hand 入口；显示 Runtime 回读，而非 UI 自存第二份 tool 或 selection。
- [ ] 将 Windows/Web 真实事件的 Shift 信息传递到正式输入；批量 coalesced 样本不错误覆盖 Down 的锁存值。平台代码只负责传递，不执行对象选择。
- [ ] 实现区域轮廓、轻量候选高亮及多根只读轮廓；候选阶段无编辑控制柄，不能复用正式选区反复清空/重建。
- [ ] 纳入现有 transient frame 的重试与清除条件：区域活动时不能因 `!creationActive && !selectionTransformActive` 被错误清掉；cancel/detach 后无残留。
- [ ] 对多选下仍只消费 primary 的属性／变换入口禁用或返回 Unsupported；同时测试 Runtime 端拒绝，不能只禁 UI。不要重写已能安全消费完整选区的 Section 命令。
- [ ] 运行单对象变换、Section boundary 与已有 drop/follow/detach、Text/Crop、Connector port、Pan/Zoom 回归。已有 SnapResolver 保持不变，不新做 Snap。
- [ ] 使用 Windows 真正 pointer 路径和实际 Chrome/WASM 演示：框选→Shift追加→套索→取消→Hand往返。不能以Node mock代替浏览器证明；缺环境时清楚报告该平台 NOT_RUN，不阻塞独立核心测试。
- [ ] 提交结果与剩余对象 coverage；第一批完成不等于整个选择能力交付完成。

## 构建与验证命令

以下是待执行命令，不是本轮运行记录。`BUILD_DIR` 指仓库既有、已配置的 Debug 构建目录；先按仓库当前文档配置，不能猜测 toolchain/preset。

```sh
cmake --build "$BUILD_DIR" --config Debug --target axiom_selection_session_tests axiom_region_selection_tests axiom_selection_input_integration_tests
ctest --test-dir "$BUILD_DIR" -C Debug -N -R 'G4SelectionSession|FC01ARegionSelection|FC01ASelectionInputIntegration'
ctest --test-dir "$BUILD_DIR" -C Debug --output-on-failure -R '^(G4SelectionSession|FC01ARegionSelection|FC01ASelectionInputIntegration)$'
```

必须先确认三个 CTest 名称均实际注册。零测试命中不是通过。其他回归先 build 对应 target，再运行：

```sh
ctest --test-dir "$BUILD_DIR" -C Debug --output-on-failure -R '^(G4TransformSession|G46TransformHandleDrag|G4EditorHistory|G4SessionConflict|G4SessionLifecycle|G4GestureArbiter|G4ViewportGesture)$'
```

Section／facade／平台测试名称以工作树 CMake 为准，记录实际名称与命令，不虚构当前未核实的测试 target。每个任务均执行先失败→最小实现→通过→提交；不为此建立新的大型 runner。

## 收尾回执

报告 branch、base/head、文件与任务映射、真实构建/测试命令及退出码、原生与浏览器的执行范围、失败/未执行项、支持对象清单和后续对象清单。不要写产品 Gate PASS，不自动 merge main。
