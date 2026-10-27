# 当前源码差距表

基线：`2fee75f66a7f96f6aac834228ede372a9abda59a`。来源链接集中于 `05-source-references.md`。

## 1. 基线变化

本次读取 main 已合入 Section PR #155，因此不应再从 `427580ae…` 或 bootstrap-only 分支创建本功能分支。提交描述明确保留 full-corpus／physical／Gate 未完成项；本报告不把合入等同全面验收。

## 2. 核实的事实与实施影响

| ID | 源码事实 | 实施动作 |
|---|---|---|
| SEL-01 | `SelectionSession::marquee` 接收已有 HitCandidate 列表，再调用 selectable 过滤，并替换 selected_；没有区域路径输入 | 不把它当成已完成的矩形／自由套索。增加真实区域查询与会话，上层提交已解析 roots |
| SEL-02 | `HitTestService::selectable` 只做非零、visible、!locked 与去重 | 可复用过滤；不能把这个测试当作几何相交证明 |
| SEL-03 | `SelectionSession::toggle` 会移除已选对象，`click` 替换为单个对象 | Shift 追加增加独立 Replace/Add 接口，保留旧 toggle 调用的原意 |
| SEL-04 | `SelectionSession::cancel()` 清空选区；旧测试明确断言这个行为 | 不全局改 cancel 的含义。新增 RegionSelectionSession 的取消，只清临时会话 |
| SEL-05 | `setSelectionMode(false)` 调用 selection_.cancel；`setPanTool(true)` 进入该路径 | 解耦关闭工具与清空选区，修复 Select↔Hand 保留选择 |
| SEL-06 | `selectionPointer` 的 Down 在非手柄、非拖动已选对象时立即调用 selectAtViewPoint；空白结果清选区 | 空白 Down 变 Pending，Up 判点击，超过阈值转矩形；不抢已有变换／Section boundary／连接点 |
| SEL-07 | `CanvasToolKind` 只有 Ink/Select/Eraser/Pan/Text；当前控制类型没有矩形／自由套索子模式 | 在 Select 家族下添加子模式与对应回读，不重排既有枚举值 |
| SEL-08 | PlatformPointerSample 和 PointerSample 都没有 modifiers；normalizer 逐字段复制也没有此信息 | Shift 必须经过 Host→PlatformPointerSample→normalize→PointerSample→Down 锁存，不能仅靠 UI 状态 |
| SEL-09 | 真实 ingress 经 acceptPlatformBatch→dispatchRuntime_.submit；selection 回调最终调用只带 key/phase/x/y 的入口 | 区域路径保留完整 PointerSample，不在进入会话前丢掉 modifiers、predicted、sequence |
| SEL-10 | selectAtViewPoint 与 toggleSelectionAtViewPoint 分别写了 Sticky child→parent 归约；resolveHit 负责 Group scope | 提取共享归约 helper，避免第三份区域选择规则；不修改 Section 为 Group |
| SEL-11 | Scene 有 query、hitTest、revision、semanticGeneration；derived_geometry 有 Triangle、Connector、SectionBoundary | 复用候选和几何来源；已查头文件并不能证明所有对象的精确区域相交已经存在 |
| SEL-12 | renderSelectionOverlay 与多个 selected property／transform 方法读取 primary；transformDrag_.begin 收到单对象 targets | 多选展示与编辑能力分离。本批禁止“多选界面、仅改 primary”的伪批量操作 |
| SEL-13 | EditingOverlayInput 是单 selectedObject，SelectionOutline 是四角单轮廓 | 增量加入 region path 与候选／多根只读轮廓，不把套索轮廓存成对象 |
| SEL-14 | 当前 SelectionSession 单元测试只有 click/toggle、候选过滤、删除清理/cancel 三类函数 | 补真实几何、输入路由、取消、工具切换及零 canonical/History 变化测试 |

## 3. 不能从本次读取推出的结论

不能说历史 Lasso 分支从未实现过；只能确认本次已读 main 的控制接口与 selectionPointer 路径尚未形成设计要求的公共区域链。执行前仅核对是否有可复用的未合入实现，不自动合并整个旧分支。

没有完整审计所有 Scene provider、Windows/Web 事件处理函数或整个 RuntimeComposition；平台目录已定位，具体事件到 bridge 的调用点需 Codex 在同一工作树追踪后修改。不是重新做产品设计。

本轮没有执行 CMake/CTest、原生输入或浏览器测试。所有建议测试状态均为 NOT_RUN。

## 4. 已确认的构建边界

`runtime/interaction/CMakeLists.txt` 将 `canvas_runtime_interaction`（会话/输入叶层）与 `canvas_runtime_editor`（Scene/Render composition）分开。RegionSelectionSession 放叶层；使用 Scene/Render 的区域查询适配放 editor 层，不给叶层反向增加 Scene/Render 链接。

已登记的回归：G4SelectionSession、G4TransformSession、G46TransformHandleDrag、G4EditorHistory、G4SessionConflict、G4SessionLifecycle、G4GestureArbiter、G4ViewportGesture。这里只确认注册存在，不宣称运行通过。

已定位 Section 消费测试：`runtime/composition/tests/fc08_section_consumer_test.cpp`；本轮未完整读取其断言，应在实施工作树按实际 CMake 注册运行。

## 5. 第一批最重要的防回归

既有单对象拖动／缩放／旋转、Section 边界调整与 drop/follow/detach、文本与 Crop 独占、Connector 连接点，以及已有 SnapResolver 调用不得因区域选择接入而被重写或移除。
