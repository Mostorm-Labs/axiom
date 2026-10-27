# Selection 第一批实现回执

四个授权任务已实现并本地提交；独立审查发现的三项 Important 均通过真实 RED 复现后修复。本记录不构成产品 Gate／PHYS 通过。

## 精确版本

- Branch: `codex/selection-unified-batch1`
- Base: `2fee75f66a7f96f6aac834228ede372a9abda59a`，包含 Section PR #155；最终 fetch 的 origin/main 未前进。
- 实际构建／测试 subject: `cde786d6c06a0e37538075c83e5bd4548ea6b162`；双端内嵌 revision 一致，dirty=0。
- 本目录所属提交为最终证据 HEAD，是上述 subject 的纯证据后继，完整 hash 在最终回复给出。
- 未推送或合并 main。独立 worktree 与本机产物保留。

## 修改及架构所有权

全部 51 个 subject 变更文件、摘要、职责见 [source-files.json](source-files.json)。实施提交：942e91b3、c70fb579、f3f88204、c1300ce6；审查修复：cde786d6。

1. SelectionSession 的 revision-fenced Replace/Add；旧 toggle/cancel 保留。平台 sample 尾部默认修饰键，经 ingress 传递。
2. 叶层 RegionSelectionSession 保存 Down 锁存与有界真实点；矩形／自由套索、even-odd 与精确 Shape 几何，AABB 只作候选。
3. 现有 CanvasEditingRuntime 组装正式输入、当前一致 Scene、共同 root/scope 归约与 Up 一次发布；取消、相机／surface／generation／新选区 fences，选择不写文档／History。
4. 现有 Facade/Control/Debug UI 与实际 Minimal Windows/Web 接线；只读多根轮廓、候选预览，primary-only 编辑在 UI 和 Runtime 拒绝，多根 Fit 与安全删除保留。

没有另建 Runtime／History／canonical selection store 或宿主选择模型；没有改 schema／Operation17/18／Semantic 实现／SDK locks／全局 Aegis registries／历史预期。原包六份 Markdown 与 ZIP 字节一致。实际修改并验证 Minimal hosts，未仅修改旧 Playground。

## 执行结果及精确命令

| 执行 | 结果 | 退出码 |
|---|---|---|
| Native MSVC19.51.36246 x64 Release /MT /WX，应用及相关 targets | 构建通过，47 个注册 CTest 全通过，0 skipped | 0 |
| 其中真实 RuntimeComposition 输入 fixture | 21 项 GTest 全通过 | 0 |
| Emscripten6.0.6 + 实际 Chrome/WASM WebGL2 | 构建通过，19 项检查通过，含 DPR2／独立像素检查 | 0 |
| Python descriptor/semantic/host/font 合同 | 18 项通过，0 skipped | 0 |
| Runtime 边界检查 | 102 个 contract files 通过 | 0 |

完整实际 argv、配置／构建／测试退出码见 [commands.json](commands.json)。Native discovery 会拒绝零匹配或缺少 target；没有用全量旧 Playground 测试代替相关回归。断言测试在 Release 显式保留 assertions。

[Native JUnit](raw/reviewed-native-regression.xml)、[原始日志](raw/reviewed-native-regression-ctest.log)、[浏览器原始观察](raw/browser-006/results.json)、[CDP events](raw/browser-006/cdp-events.json) 与构建／SDK facts／Python 日志均保存。Native 包含已有单对象变换、Section drop/follow/detach、Text、Image/Crop、Connector、Brush retry、Pan/Zoom、实际 ImGui 控件回归。

Windows 是实际 HWND/WndProc 路径，输入事实为合成。Chrome 实际运行生产 WASM，Node 运输 CDP 与字节；画布鼠标事件由 CDP 投递，工具切换走实际 host control transport，没有宣称物理点击 Chrome Debug UI。测试 server/Chrome 已结束，未新开用户操作窗口。

## 独立审查及失败保留

fresh read-only reviewer 审查 base..c1300ce6，给出三项 Important、无 Critical/Minor；未独立复跑测试、未二次审查。[审查摘要](raw/review-summary.md) 是执行者转录摘要，原回复保留于会话；[处置](review-disposition.json) 绑定测试与修复。

- 外部将已选 A reparent 到 Group G 后，Shift Add 沿用 A：真实 RED；现在在同一当前 view 上归约保留 roots 和新 hits，再一次 revision-fenced 发布。
- 相机／独立选择／删除使区域失效后旧全场景 transient 残留：真实 RED；通过既有 canonical handoff 退场，provider visibility 断言覆盖这些路径。
- unlocked primary + locked non-primary 可删除锁定对象：真实 RED；现在删除与 UI 检查全体 roots，双顺序、Runtime/Facade 拒绝及 canonical/History 不变受测。

[三项原始 RED](raw/review-red.log)；[首轮修复仍失败](raw/review-green.log) 暴露 Facade 只在单对象 record 内投影锁状态，随后修复；[最终 GREEN](raw/review-green2.log)。更早的真实失败、API 缺失编译、Windows Shift、旧 Select transport、Web blur generation 与 affordance 像素失败保留于 raw/historical、browser-001..005。初次 Git TLS／缺参数／CDP button:none／错误 Section fixture 分别按环境或夹具问题记录，不冒充产品 RED；未改冻结预期。

## 覆盖及逐项取舍

- Rectangle/Ellipse/Triangle：Native 与实际 Chrome 端到端精确选择通过。
- Section／独立 RichText／Connector／复杂 vector/dab/erased strokes／完整 Group region provider 延后；Partial 保留正式选区。代价：这些对象尚不能由本批区域选择完整获取。
- Image/Sticky 仅 affine frame geometry 单测，完整资源／子文本 E2E 为 NOT_RUN；代价：仍可能有未观测集成问题。Group 新增外部 reparent 的窄测试，未声称完整容器覆盖。
- 物理 mouse/pen/touch/IME、其他平台、G5 性能／故障与完整302 runner 为 NOT_RUN；代价：无对应验收证据。完整多选变换／批量格式／排序／锁、新 Group/Ungroup、Snap/Alignment 未扩入本批。
- reviewer 未建立 receivePlatformPresented 第二处 handoff-clear 在新区域活动时所需 live-frame 条件，未作猜测性独立补丁；代价：相关帧交错仍待专门回归。
- 不由源码审查推导 Gate 或独立测试复跑通过；代价：后续正式／物理验收仍需证据。无 deferred minors。[完整 ledger](implementation-ledger.md) 保存每项 Ruling。

## 本机产物与人工复测

Windows exe: `C:\Users\Chado\.codex\worktrees\selection-unified-batch1\canvas\out\selection-batch1\native\apps\minimal_runtime_host\axiom_minimal_runtime_host_windows.exe`

SHA-256: `36ddfcf2cc1c4f23301f2acf9f02a61bea73261e90a5feb8e90ff5db25baaf6f`。请保留其同目录 `fonts/`、`icudtl.dat`，从构建输出目录运行。

Web directory: `C:\Users\Chado\.codex\worktrees\selection-unified-batch1\canvas\out\selection-batch1\web\apps\axiom_canvas_demo`；JS/WASM/host.js 摘要见 [build-artifacts.json](build-artifacts.json)。

人工路径：F12 → Control → 创建基础 Shape；Select 空白拖动、Shift-at-Down 追加；Rectangle select／Freeform lasso 从对象上开始；取消、Hand 往返。检查对象／Operation／Undo 不变，多根只读轮廓完整，缺 provider 显示不完整。未实际观察的真机项目保持 NOT_RUN。

Raw stdout／CTest discovery 原样保留 emitter 的尾随空格，完整 git diff --check 因这些原始字节返回2；排除 raw/** 后源码／回执 diff check 返回0。没有为通过 whitespace 检查改写原始日志。
