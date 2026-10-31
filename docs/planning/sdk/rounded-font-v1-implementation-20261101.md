# 圆角与中英文手绘字体增量实现

日期：2026-11-01。仓库 Mostorm-Labs/axiom，分支 `codex/sdk-shell-axis-bridge`，锚点 `af5b1778284211c3834957084dd0a03e48308d69`。

授权是本会话的圆角/连线折角、两套字体与四行 Demo 请求，契约见 [rounded-font-v1-design.md](rounded-font-v1-design.md)。保留此前未提交的 Sketch v1、batch2 和 Table 工作；修改前逐文件副本/哈希与补丁在 `out/rounded-font-work/baseline-source`、`baseline.json`、`baseline.patch`。这是 repo-local 增量实现与验证记录，不构成正式 Gate 判定。未执行提交、推送或发布；SDK 仍为未发布的 `0.1.0` 候选，cornerVersion/styleVersion/font metadataVersion 是独立的数据契约。

## 对外行为与 owner

- 矩形、三角形和 Connector 支持连续 `cornerRadius`，范围 0–16384、本地单位。过大半径依据相邻边长度限制；矩形上限为短边一半，三角形上限为内切圆半径。0 清除圆角属性，不清除已有手绘 seed。椭圆、Sticky、Table、Section、VectorPath 等不适用对象在半径适用列表中明确排除。
- 连接线复用同一个圆角折线构造器：先计算既有语义路由，只装饰内部折角，保留端点与末端方向。避障采用固定次数缩小圆角和控制点包络检查，必要时保留尖角，不改写语义端点和路由。形状的实际圆角轮廓用于附着与填充/文字裁剪；尺寸和控制柄仍使用规则几何。
- 共用 `appearance.patch/begin/updatePreview/commit/cancel` owner→OperationEngine→History/发布链路。拖动仅本地预览，提交一次原子操作；多选按字段聚合 mixed，种子保持各自身份。新建默认外观与对象外观分开。
- Web 公共 API 仍是 root facade 的 DTO/命令；新增半径值、radiusEligible/radiusExcluded 和能力读回。共享 Session 传递相同值，Windows caller wrapper 的现有 JSON 命令即可使用；没有新增内部类/指针或 DLL 导出。Windows caller 语法检查不代表 Windows ABI/设备验证。
- Demo 第一行绘制工具，第二行主要对象/工具属性和字体，第三行详细属性与对象操作，第四行 Undo/Redo/Fit 等画板工具。圆角有滑块和数值框。文字编辑开始后可选字体；未进入文本对象时显示提示。表格单元格也复用已存在的字体格式命令。

## 共享几何与数据格式

`runtime/scene/corner_geometry` 输出 renderer-neutral 路径，圆弧采用圆形三次 Bézier 近似；`rounded_sketch_geometry` 只在非零 CornerStyle 时装饰，缓存不依赖相机、颜色和每帧随机。沿既有 Scene/Skia 路径用于正式/预览绘制、导出、边界与命中。命中时将主体、派生笔触和箭头视为集合并，分别应用遮罩并计算交集，避免复杂中间 Boolean union 的失败；无圆角对象保留旧路径与原有 Skia 椭圆表示。

新增闭集属性 `CornerStyleV1`，field `0x103`，PropertyValue 追加 index8、protobuf tag9。同步注册、适用性、清除/默认、校验、规范化、指纹、codec、Operation 与当前 descriptor；不更改冻结的历史 descriptor。当前 descriptor SHA-256：`45c74a45dd45d60576fd1382e48bc75ad14a950c4745636b23de173c42bfaf2a`。

Snapshot 新增 bit512 `rounded-corners-v1`，bit1024 `font-fallback-v2`；当前 reader profile2047，旧 reader 常量保留。未知版本、不支持的 reader、无效字体图明确拒绝。

## 字体来源、名称与确定性回退

- Excalifont：官方 WOFF2，源 SHA-256 `ee41ec4c06bfa0728665499de6f4b4019e7953119ab20b5aeb5917f1609c3b2a`。仅以 fontTools4.68.0 无损解包为 TTF，未改字形或名称；输出190652 bytes，SHA-256 `574ab04d4598e048edf2c4f579664ff9865a8db729fd0db01a774cc9e9b38247`。
- Xiaolai：上游 v3.126 原始 TTF，22220806 bytes，SHA-256 `e2f68daf0e72777a8cf58bc83de1b98634b251e537ddbfca24b0ae50d1802da2`。
- 原始名称、版权、OFL、来源/工具版本与哈希保存在 `assets/fonts/imported/{Excalifont,Xiaolai}` 和 `handwriting-sources.json`。字体解包工具只存在于本地 authoring 目录，不是产品构建依赖。
- 两者 OFL1.1 允许使用、修改、重命名和随软件分发；修改版仍需 OFL 和原始版权，不得单独售卖字体，不能宣称上游设计为 Axiom 原创。UI 别名与字体内部命名可分离。未来真正改名/改字形时，应一致修改字体 naming metadata 并保留来源和修改说明，尊重 RFN/商标。本次用户未指定新名称，未进行这些修改。
- 本次文件中未发现显式 RFN 声明；Excalifont 的嵌入 metadata 另有商标声明。依据：[OFL FAQ](https://openfontlicense.org/ofl-faq/)、[Excalifont 官方页](https://plus.excalidraw.com/excalifont)、[Xiaolai OFL](https://raw.githubusercontent.com/lxgw/kose-font/v3.126/OFL.txt)。
- 新增未发布字体资源 ID 为 `0xFC0310`/`0xFC0311`，排序在既有资源之后，保留原有 Noto 默认字体。Excalifont 的显式单跳 fallback 指向 Xiaolai。新 metadata schema2/layoutVersion2 由严格 parser 校验，目标必须是已接纳的 version1 字体，不允许自引用、悬空、循环或嵌套。
- 共用 SkParagraph 只使用清单内注册的精确字体，关闭系统 fallback。实际字形 run 记录 fallback resource ID，canonical 文本仍保存用户选择的 primary font；缓存包含主/回退资源身份、hash/revision/generation。Web 和 Native 不分别选择系统字体。

## 验证与失败记录

最终机器可读汇总与逐项证据：`out/rounded-font-work/summary.json`。真实 Chrome 使用外部消费者目录、匹配版本的 JS/types/WASM/resources；未使用私有业务接口。

- `native.log`：几何、语义/能力、字体 shaping、Runtime 预览/撤销/seed。`browser.json`：公共能力、实际指针/Unicode 输入、圆角适用/多选/锁步历史、取消、种子、圆角命中、路由、Snapshot、远端、Table 字体与原生图像对照。Demo 的 `<select>` 通过 change 事件（等价于 selectOption）选择，文字和滑块使用真实 CDP 输入；另外通过 CUA 手动选择与输入验证。
- `path-conformance.json`：554136 条圆角路径命令，Native/WASM 最大坐标偏差 `9.094947017729282e-13`，阈值 `1e-9`。235750 条旧 v1 路径的两侧哈希与前批完全一致，最大偏差 `1.8189894035458565e-12`。
- `first-batch-browser.json`、`extended-browser.json`、`table-browser.json`、`export-browser.json`、`background-browser.json`、`generic-browser.json` 覆盖既有功能。所有总数与结果由最终 JSON 汇总读取，不以手写数字替代证据。
- `package-descriptor.log`：24 项 descriptor/消费者包/发布工具检查；`types.log` 为严格 TS（包含新增能力与半径正/反例）；`api.log`、`boundaries.log`、`windows-caller.log` 分别验证 JS 错误契约、112 项模块边界、Windows caller 语法。没有重建已发布 Semantic/Skia 或改其 lock。
- `ctest.log` 与 `ctest-comparison.json` 比较既有22个历史失败，全量原始运行 189/212 通过，23项失败，其中22项与历史基线相同，唯一新增为旧字体数量断言12（实际14）。更新该断言后，字体目录与5项文字选择用例共6项定向通过；最后命中 cpp 修正后其余211项重跑，189项通过、22项历史失败，无未关闭的新增失败。完整 FC02Text 未重复跑第二遍，以原始全量和最终受影响用例证据分别呈现。完整 CTest 的二进制/输入保留在 native-full-run-inputs.json 对应构建中；文字测试运行期间没有替换其可执行文件或改变头文件布局/字体资产。最后的 cpp 命中短路修正和字体目录断言，已在完整运行结束后重建原生并按受影响用例复核，分别记录全量原始结果与定向解决记录。

真实 red→green / 诊断记录：

1. `corner-red.log`、`font-red.log` 保留契约测试的实现前失败。
2. `native-clear-radius-red.log`：撤销到“圆角属性不存在”时，Clear 白名单漏了0x103；已补齐并回归。
3. `browser-old-reader-red.json`：Snapshot 后置校验仍用了旧511 reader；改为当前2047，恢复/远端像素一致。
4. `browser-default-order-red.json`：新增字体 ID 排到 Noto 之前；在未发布阶段改为0310/0311，恢复原有默认顺序并增加检查。
5. `types-features-red.log`：能力读回实现有字段、声明遗漏；补齐 roundedCorners/fontFallback 声明并以严格类型正反例验证。
6. `font-catalog-expectation-red.log` 是旧目录断言的失败证据；新增 Excalifont/Xiaolai 后将显式预期从12款扩展到14款，同时验证原有 Noto 仍排第一。最终6项受影响文字用例通过。
7. `browser-before-explicit-selection.json`、`browser-native-menu-driver-red.json` 是驱动假设错误：绘制后需明确选中；headless 原生弹出菜单键盘选择未生效。修正驱动，未绕过 Runtime。
8. `first-batch-initial-region-failure.json` 保留框选失败；只对真正设置了圆角的形状换用派生主体轮廓，未设置圆角的椭圆继续使用原有 Skia conic 路径。
9. 更大种子诊断发现 seed11/roughness1 椭圆 union 失败；`ellipse-historical-baseline.log` 在修改前的完整 selection 源码上同样复现，证明原有 PathOps 边界。继续回归表明它也会阻断本批共用样式的框选验证，因此只修复相关命中组合：主体与最多两个派生笔触/箭头路径分别做精确交集，不先对复杂随机轮廓做 Boolean union；擦除遮罩分别作用于每部分，等价于集合并后的减法。任一主体/笔触的精确交集已经证明命中时立即返回；不会让另一个与该命中结论无关的复杂笔触交集失败推翻已证明的命中。未证明命中时仍保留 Invalid，且缺失文本几何仍在前置校验拒绝。没有 AABB 命中替代。96 个椭圆输入（程度0/1/3 × seed1–32）及圆角/非圆角完全擦除用例已纳入永久 Native 回归。`ellipse-historical-repro.cpp`、失败前后日志保留。
10. 旧完整 CTest 在修正候选输入/命中依赖时主动中止，保留 `ctest-before-*-interrupted.log`，不把未完成的 FC02Text 当作通过；最终以独立完整运行为准。

本地交付：`http://127.0.0.1:4192/example.html`；样例 `http://127.0.0.1:4192/matrix.html`，仅使用共享 SDK 加载不透明 Operation。服务和本批配套资产与旧4188/4189/4190/4191目录分开；旧用户页面不做替换。`demo-assets.json` 记录最终41个包文件哈希，许可证随包保存。完整字体使数据资源变大；本批未扩展为已有 root/internal 双份打包策略的重构。

## 重现入口

```sh
cmake --build out/object-identity -j8
ctest --test-dir out/object-identity --output-on-failure -j8
cmake --build out/object-identity-web/build --target axiom_js_runtime -j8
out/object-identity/runtime/composition/axiom_rounded_font_fixture out/rounded-font-work/fixture
node verification/sdk_js/verify_rounded_font.mjs out/object-identity-web/build/sdk/web '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' out/rounded-font-work/browser.json out/rounded-font-work/fixture
```

其余外部消费者脚本位于 `verification/sdk_js/verify_*.mjs`。schema 检查的 AXIOM_PROTOC 必须来自已解析的 CMakeCache/SDK Store；普通构建不下载或重编依赖栈。
