# FC-02 当前实现与收尾记录

日期：2026-10-22。仓库 `Mostorm-Labs/axiom`，分支
`codex/fc02-windows-ime-resume`。本次测试及用户使用的产品 revision：
`3d7afa351dfbd42f18c449bda35f861089b1fb4f`。

当前返回状态为 **READY_FOR_CONTROL_REVIEW**：已完成本轮实现、普通消费者接口补齐、
既定自动回归和证据整理。用户对该版 Windows/Web 的反馈为“这两个端的测试基本通过了”。
这是一条范围有限的人工反馈，不扩展为逐项 IME/触摸/DPI 验收，也不是独立审查、Gate 或 PHYS PASS。
本次收尾只新增说明和证据，没有修改产品代码、Authority、历史 Gate/PHYS、Golden 或 SDK locks。

这是当前状态入口。较早报告描述各自 revision 的实际结果，包括已经解决的历史阻塞；
原 FAIL 和原始日志均保留，不用本报告覆写。

## 已交付的使用链

- 独立 RichText 创建/编辑、每对象持久化宽度、History、guarded delta 与 Snapshot/replay。
- Windows IMM32、Web composition/beforeinput；真实确认才写入，预编辑只参与派生显示。
- 共享 TextLayout 的光标、选择、命中和候选窗锚点；光标闪烁由 Runtime 决定。
- 字体目录、单资源状态、任意合法 RGBA/字号和格式叶子，经 RuntimeFacade 写入及读回。
  Debug UI 提供黑/蓝、32/24/20/16 四档字号、字体、对齐、粗斜体、下划线、删除线、项目符号。
  混合选区效果使用“任一相交内容启用即勾选”的策略。
- 相同 Noto CJK 与 11 个导入字体家族供 Windows/Web 消费，包含摘要和许可证。
- Shape 内嵌 rich-text label：Text/Select 双击进入、再次命中定位、内部单击移动光标、外部单击结束。
  固定字号、形状内留白及内部布局、派生省略号、选中时的空提示/溢出加号、向局部负 Y 扩展及 Undo。
- 普通 C++ MinimalRuntimeFacade 与 Web JS/WASM 消费者均具备格式/字体/状态/回执入口。
  普通 Host 编译项不再继承 `runtime/semantic/src` 或 qualification 定义。

入口和操作规则见 [消费者使用说明](FC02_CONSUMER_GUIDE.md)；
逐项 Source → Owner → Consumer → Oracle 见
[覆盖表](fc02/evidence/closeout-20261022/coverage-map.json)。

## 本轮重新执行的验证

| 验证 | 结果和边界 |
| --- | --- |
| 既定 CTest 清单 | 234/234，0 skipped；包括真实 Semantic/History、FC02Text、Windows 入口及普通消费者 |
| Python schema/边界/字体 | 17/17，0 skipped；使用已解析的 AXIOM_PROTOC 和锁定 SDK 环境 |
| Chrome 普通消费者 | 5/5；字体状态、格式/回执、拒绝无副作用、预编辑偏移、选中 Shape 格式 |
| Chrome 既有文本/画板 | 7/7 + 13/13 |
| Chrome Shape 再入/DPR | 三种形状 × DPR 1/2，共 6/6 |
| 普通消费者编译依赖、Release oracle | 两份 compile_commands 核对；普通 Host 无私有 include/资格定义，Windows assert 入口启用 /UNDEBUG，普通消费者用始终执行的 REQUIRE |
| Windows/Web 真实目标构建 | 启动前在同一精确 revision 配置/构建成功；保留命令、日志和产物摘要；本次未更改这些源码 |
| Windows/Web 人工 | 用户报告基本通过；没有提供新的逐项专项矩阵 |

原生构建使用 VS 2026 / MSVC 19.51、Release、/MT、/WX；Web 使用已安装的 Emscripten 6.0.6，
实际 Clang 为 24.0.0git，与 SDK 元数据里的 LLVM 22.1.8 不同。本报告如实保留工具链事实，
不声称该元数据匹配或完整 ABI 资格。实际 JS/WASM 浏览器回归已执行。
两端继续消费锁定的发布 Semantic/Skia SDK，没有重编发布依赖。

构建命令、自动结果、测试清单、原始输出和身份均在
[证据入口](fc02/evidence/closeout-20261022/README.md)；
[产物身份](fc02/evidence/closeout-20261022/artifacts.json)绑定启动前校验和本轮读回。
Web 使用 `http://127.0.0.1:4175/index.html?rev=3d7afa35`，避免误用旧 4173/4174 服务。

## 人工结果与保留事项

本次用户的原文和对应产物记录在
[human-feedback.json](fc02/evidence/closeout-20261022/human-feedback.json)。
之前明确通过的 Windows 文本入口、格式/字体选择及 Web 三项再次编辑结果仍保留在原证据中。
历史 Windows 工具入口失败、光标问题和 Web 再次编辑失败均不改写；新增复测另存。

下列专项未被这条“基本通过”逐项确认，保持 PHYSICAL_PENDING：

- 真实 IME 的候选窗位置、候选键、result/cancel、合成选区与重复终态。
- 合成中切工具、Debug UI 格式焦点、Alt+Tab、Esc、受控关闭及失败恢复。
- 真实触摸/笔的文本创建、定位、选区，Debug UI 隔离和既有多指规则。
- 多显示器/DPI、缩放/恢复后文本与候选窗的位置。
- Chrome 原生焦点、页面隐藏/受控离开/强制销毁的实际输入法行为。

当前已知限制：字体不保证全部 Unicode/emoji；缺字有诊断，不能以系统字体替代。
编辑投影上限为 65536 UTF-16 units；这不是 canonical 文档大小限制。
Shape 确认文字与自动扩展沿既有两个 History 记录提交，Undo 按记录逐步恢复；
旋转 Shape 的扩展方向是局部负 Y。原始关闭基准版本没有触摸双击入口；本报告末尾的后续记录新增了 Windows Runtime 入口与独立复测状态。全字符字体回退和强制离页保存不在本次承诺内。

本轮范围内没有新复现且未修复的实现失败。上述限制和未观察项不代表完整跨平台等价性已获证明。
独立 Control Review 尚未执行；后续审查应从本报告所在精确提交解析证据，
逐项人工验证也应单独新增记录。FC-03、FC-01A、FC-04、Android 和延期 FC-01 事项未启动。

## Follow-up: Windows touch double-tap on Shape labels (2026-10-22)

A physical-device report found that after creating and selecting a Shape, touch
double-tap did not enter its embedded text editor. The Windows Host forwarded
native pointer contacts but did not recognize a touch double-tap, while
touch-promoted mouse messages are intentionally filtered. The existing
`WM_LBUTTONDBLCLK` text entry therefore only handled mouse input.

The existing Runtime selection input path now recognizes two valid finger taps
on the same selected Shape and enters the existing TextEditSession, using shared
text hit-testing to place the caret. It handles empty and populated Shape labels
without adding host-side targeting, editing state, or canonical mutations.
Dragging, a pause outside the tap interval, concurrent contacts, capture loss,
tool changes and source/lifecycle cancellation clear the candidate. Ink
double-tap remains an Ink gesture.

The WindowProc/Runtime fixture covers all three Shape presets, initial selection
and entry, populated-label reentry, no additional canonical commit on entry, and
negative drag/timeout/multi-contact/capture-loss cases. Windows `/W4 /WX`
build passed; the selected FC02 list passed 235/235, Python contracts 17/17,
strict Clang checks 9/9, and the Web Minimal target linked. These automated
pointer fixtures are not a touchscreen observation. Physical retest of this new
behavior remains `PHYSICAL_PENDING`; historical reports remain unchanged.
Current details and a resource-complete Windows bundle are in the
[touch-to-edit evidence](fc02/evidence/windows-touch-shape-dbltap-20261022/README.md).
