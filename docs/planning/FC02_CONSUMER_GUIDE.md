# FC-02 普通消费者入口与 Host 边界

这是 `3d7afa351dfbd42f18c449bda35f861089b1fb4f` 的实现使用说明，
不新增 Authority、稳定 ABI 或 SDK。当前结果和验证限制见 [收尾记录](FC02_CLOSEOUT.md)。

## Windows 分发目录

Windows exe 不是独立交付物。复制或压缩发布时，保留既有
`cmake/AxiomHostTextFont.cmake` 和 `apps/minimal_runtime_host/CMakeLists.txt`
打包的配套资源及相对目录：

```text
axiom_minimal_runtime_host_windows[-revision].exe
axiom-font.otf
axiom-font.LICENSE.txt
axiom-font.NOTICE.txt
fonts/                 # 整个 imported catalog、字体子目录和许可证
icudtl.dat             # 锁定 Skia SDK 的 runtime/windows 数据
```

主字体和 catalog 由 exe 所在目录解析，不从另一台机器的源码绝对路径读取。
字体必须与该产品 revision 的 manifest/catalog 中大小和摘要一致；不能只复制
一个喜欢的字体、替换成系统字体，或将文件名与资源身份混用。`imgui.ini` 是本机
UI 状态，不是必需分发资源，也不提交到 Git。

2026-10-22 本机仅复制 exe 的失败及补齐资源后的对照见
[Windows 启动调查](FC02_WINDOWS_DEVICE_STARTUP_20261022.md)。该次修复保留原 exe
及其 hash，不需要重建产品或安装 VC 运行库；这不是其它启动故障的通用归因。

## 用户操作

独立文本：Text tool 单击空白创建，Text/Select 双击已有文字再次编辑；Ink 双击不进入编辑。
Shape label：Text/Select 双击矩形、椭圆、三角形内部，光标定位到最近文字；
内部单击定位，外部空白单击结束并保留已确认文字。Enter 结束，Ctrl+Enter 换段。
IME 正在使用候选键时先交给 IME。空新草稿结束不会创建 Object、Operation 或 Undo。

Windows 按 F12 打开 Debug UI，Control → Text format 使用字体与格式控件。
Web 使用实际 Minimal Host 的 Debug UI；普通网页消费者也可使用以下 JS 入口。
选中 Shape 可以直接设置整个 label；在编辑中选择部分文字时可只设置该选区。
切换格式叶子不会覆盖其它叶子；缺省字段表示不修改，`false` 表示明确关闭。

## C++ RuntimeFacade

公开入口头文件为 `canvas/runtime/runtime_facade.hpp` 和 `canvas/runtime/text_input.hpp`。
Minimal Host 提供的 `MinimalRuntimeFacade` 转发同一 Composition owner，
业务 UI 借用 `RuntimeFacade&`，无需访问具体 `CanvasRuntimeFacade` 或私有 Semantic 头文件。

| 用途 | 接口/数据 |
| --- | --- |
| 工具、Undo/Redo | `submitCanvasControl` / 既有产品控制入口；当前 target 从 `mountedCanvasTargets()` 读回 |
| 开始、完成、定位、宽度、格式 | `submitTextIntent(TextIntent)` |
| 原生输入事实 | `submitTextInput(TextInputEvent)`；UTF-16 range，实际 composition 身份及可选 `preeditSelection` |
| 会话与格式读回 | `readTextEditor()`：phase/token/projection/selection/format/formatObject/fonts |
| 布局几何 | `readTextGeometry(expectedToken)`：caret/selection/diagnostics，旧 token 返回 unavailable |
| 字体目录 | `readFontCatalog()`：完整资源身份、revision/hash、byteLength/status/diagnostic |
| 单字体状态 | `readFontResourceStatus(ObjectId)`；不在当前目录时返回 nullopt |
| 意图终态 | `textIntentReceipt(clientId, requestId)` |
| 输入终态及释放 | `textInputReceipt(token, sequence)`、`acknowledgeTextInput(token, through)` |

所有这些调用与读回在 Runtime owner 线程执行。平台回调先进入自己的采集队列，由 owner pump 顺序转发；
不要从外部线程直接调用或修改 Runtime 容器。返回 Queued 只代表入队，
必须等 owner pump 后读终态，Applied/Observed 与拒绝不能混用。

`TextIntent` 使用当前 `CanvasTargetKey`、唯一非零 client/request identity 和当前会话 token。
同一 identity 同一载荷可以重放；同 identity 不同载荷返回 Collision。
过期、失效、乱序、背压、Busy 和拒绝都需要按状态处理，不得静默改号重放为新操作。
每次输入按读回的 nextInputSequence/token 发送，终态确认后再释放回执。
不要截断 DocumentId/ObjectId，也不要跨目标复用 selection 或 geometry token。

`TextIntentKind::kFormat` 的 `TextFormatPatch` 支持：
color（4 个合法 RGBA float）、fontSize、font（完整资源 ObjectId）、bold、italic、underline、
strikethrough、bulleted、alignment（1=Left、2=Center、3=Right）。
新字体必须先经既有 manifest/resource 准入和字节校验，再由相同 TextLayout 使用；
字体选择意图不等于字体文件上传入口。不得用字体显示名称替代资源身份。

普通编辑格式意图保持 `object` 为零以使用当前选区/后续输入样式；
明确修改选中 Shape 的整个 label 时，使用读回的非零 `formatObject`。
非空选区的效果与项目符号采用任一相交字符/段落启用即读为 true；
关闭会统一清除该选区叶子。颜色、字号、字体和对齐按起点读回。
当前没有提供可依赖的逐叶子混合态报告，不应从 `mixedFormat` 推断完整选区样式。
合成期间先请求实际 native completion，完成前不得把格式修改当作已应用。

## Web JS/WASM 普通消费者

`axiomMinimalHost.textInput` 暴露 `snapshot()`、`intent(kind, options)`、
`fontCatalog()`、`fontStatus(id)`、`receipt(clientId, requestId)`、
`complete()`、`finish()`、`error()`。JSON 中 uint64 用十进制字符串，
完整 ObjectId 用 32 位小写十六进制字节串；不要转换为 JS Number。
projection/preedit 是 UTF-16 code unit 数组，selection 是 `[start, length]`。

在已进入文本编辑且无活动 composition 的情况下，普通消费者可设置当前范围：

```javascript
const editor = axiomMinimalHost.textInput;
const clientId = '42002';       // 消费者自己的非零身份
const requestId = '1';         // 该 client 下递增；重放时保留相同载荷
await editor.intent(15, {      // TextIntentKind::kFormat
  clientId, requestId,
  format: {color: [0, 0, 1, 1], bold: true, fontSize: 24}
});
const terminal = editor.receipt(clientId, requestId);
const observed = editor.snapshot();
// 按 terminal.status 处理终态，再显示 observed.format；不要以 Promise resolve 代替成功。
```

需修改整个选中 Shape 时，为同一意图显式加入 `object: editor.snapshot().formatObject`。
调用方先检查非零目标并使用新的 requestId。单字体查询使用 `fontCatalog().entries` 的 id。
宽度入口是 `intent(5, {width: value})`；零等非法值必须原样交给 Runtime 拒绝，不能变成默认 320。
这些数字是当前 source API 的枚举映射，未来消费者更新须与源码同步，不构成冻结 ABI。

DOM textarea 只是原生输入载体。宿主转发实际 composition/beforeinput、实际可观察的预编辑选择，
不能伪造确认、绘制另一份文档文本、测量行宽或自行计算 canonical delta/Undo。
`complete()`/`finish()` 可能等待真实 IME 结果并失败；超时不等于成功确认。

## 保持 Host 精简

| Owner | 保留职责 |
| --- | --- |
| Semantic/History | canonical 文本、版本、验证、Operation、回放与补偿 |
| Interaction/Runtime | 工具/会话/目标、文本范围、格式意图、终结与准入、命中/退出、Shape 扩展规则 |
| TextLayout/Scene/Render | 单一 shaping/layout、真实字体、caret/selection、派生省略号/提示和绘制 |
| Composition | 资源绑定、owner pump、生命周期、帧身份/需求及真实反馈协调 |
| Windows/Web Host | 原生输入事实、物理焦点/IME、坐标单次转换、平台资源/GPU 执行和真实反馈 |
| Debug UI/产品 UI | 提交 Facade intent、读回终态与状态，不持有另一套编辑状态 |

Windows 主窗口焦点与 IMM32 候选定位、Web carrier 焦点/DOM composition 和可见时的时钟机会不同，
属于平台适配。文字布局、格式语义、确认/取消、History 和帧资格必须保持共同 owner。
Web asset rev 应传播至 host/editor/glue/WASM/data，不能仅给 HTML 加版本参数。

对照回归：`FC02MinimalTextConsumer`、`FC02Text`、`FC02WindowsTextFormatControls`，
以及 `apps/axiom_canvas_demo/tests/fc02_text_consumer.mjs`。
完整覆盖和人工限制见 [本轮证据](fc02/evidence/closeout-20261022/README.md)。
