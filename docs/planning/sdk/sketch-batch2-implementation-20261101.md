# 手绘 v1 扩展实现与验证

日期：2026-11-01。仓库 Mostorm-Labs/axiom，分支 `codex/sdk-shell-axis-bridge`，
基线 `af5b1778284211c3834957084dd0a03e48308d69`，结果为该基线上未提交的工作区。
本报告记录 VectorPath / Table / Section 增量；第一批 Shape / Connector / Sticky 的实现和
历史证据保留在 [第一批报告](sketch-v1-implementation-20261101.md)。
授权、适用性与终止条件见 [设计契约](sketch-batch2-design.md) 和
[执行契约](sketch-batch2-contract.json)。这不是正式 Gate 或发布判定。

## 实现与接口边界

| 范围 | 实现与限制 |
| --- | --- |
| VectorPath | 保留 move/line/quad/cubic/close 与 nonzero/evenodd；开放轮廓不补闭合线。排线通过原始 Bézier body 精确裁剪，孔洞不成为实心命中区域。新增有界 createVectorPath/readVectorPath 值 DTO；不增加节点编辑器。 |
| Table | 每条完整横/竖网格生成一次，避免相邻单元格共边重复。子序列由对象 seed、横/竖域和稳定行列 ID 决定。先底色、再网格、再原有文字、最后活动单元格辅助框。原 Table/1 编辑保留，未增加单元格排线。 |
| Section | 仅文档边界的 fill/stroke/sketch；标题、header/padding、成员判定与选框不继承笔触。缺省灰色一像素，显式 NoStroke 可隐藏文档边界。框选保留规范矩形并纳入外溢描边。 |
| 外观 owner | 延用 CanvasEditingRuntime appearance session → OperationEngine → History / 操作发布。多选按用户参数聚合 mixed，保留每个对象 seed；预览不改 canonical，提交整批一次 Operation。 |
| Scene / Render | 新生成器 sketch_extended_geometry.cpp 输出不可变路径，复用原 v1 线段随机函数和共享 Skia。路径与填充 clip 共同用于正式绘制、预览、导出、视觉边界与命中辅助。Shell 无业务绘制实现。 |
| SDK | Web 根模块/types、共享 Session、Windows caller wrapper 同步；没有新增 DLL 导出、内部对象或可变 owner。Demo 增加带孔路径/开放曲线模板入口，选中 Table 显示“网格笔触”及原网格控件。 |

通用 fill/stroke 对 Table 明确不适用，颜色和线宽唯一归属 `TableContent.grid` / 既有 table 接口。
新增 `strokeEligible/strokeExcluded`，与 fill 适用列表一起读回；全不适用 patch 拒绝，混合范围只修改适用目标。
Group 不绘制、不递归改子对象；Image / RichText / 自由笔迹仍排除。

SDK 候选仍为未发布的 `0.1.0`；styleVersion 仍为 `1`，两者独立。
旧 v1 算法与随机序列不改写，新对象使用独立固定随机域。每项最多 4096 个输入命令、
8192 段排线、32768 个输出命令，缓存最多 64 项 / 131072 命令；过密排线使用固定整数 stride。
Table 缓存不依赖文字、底色或网格 paint，几何/行列身份变化才重新抽取对应几何。

## 能力拒绝与绘制缺口

没有新 wire 类型，因此第二批不改 descriptor。追加对象适用注册
`schema/axiom/v1/registry/sketch_extended_v1.yaml`，不重写历史冻结 authority。
Snapshot 内部 reader mask 扩为 uint16；原 0..255 保留，新增 bit 256 和
`extended-sketch-v1`，当前完整 reader 为 511。旧 profile 255 拒绝新对象上的版本化外观，
disabled 元数据也不能被清掉后假装成功。Operation 使用已有闭集字段适用性与版本校验拒绝。

历史无元数据 VectorPath 保留原绘制输出和蓝色缺省读回；首次实际外观变化才物化元数据及必要填充。
新建公开 VectorPath 始终由 Runtime 分配 seed、物化可 disabled 的 v1 元数据，以能力约束修齐
NoFill、描边和透明度绘制；默认值 no-op 不产生历史记录或版本记录。
Section stroke 同样要求版本元数据，避免旧读取器静默丢弃该外观。

公开路径请求最多 4096 条命令，坐标有限且绝对值不超过 1e7，点必须是精确二元组。
未知 op / fill rule、错误子路径顺序、额外字段、非有限坐标与非法身份拒绝，不部分创建。
省略 ObjectId 时由 Runtime 统一分配；readback 为应用持有的 canonical 值副本。
编辑会话沿原完整 stamp / token / 权限 / 生命周期规则，不另建同步冲突协议。

Windows 原有 `appearance(Json)` 创建调色板接口保留。对象 patch / session 使用新增
`appearanceCommand(Json)` 和 `readAppearance()`，避免仅返回类型不同的重载。

## 本次实际验证

证据位于 `out/sketch-batch2/`。Chrome 156 使用仓库外临时消费者目录中的匹配
JS/types/WASM/data；运行包主资产与本机服务的散列逐一对照一致。

| 检查 | 结果 / 证据 |
| --- | --- |
| 原生语义、生成器、Runtime、Skia | 34 个 GTest 通过，含第一批 19 项与增量 15 项；native.log。覆盖版本/seed 拒绝、孔洞/开放曲线、稳定共享边、能力拒绝、Table 会话/History/paint owner、Section 内容与边界命中、公开创建及旧路径首次外观变化。 |
| 扩展 Chrome 外部消费者 | 72 项通过；browser.json。实际创建/读回，三种 winding/孔洞，排线空隙与开放轮廓，三类对象的 transient preview / 一次提交 / Undo / Redo / cancel / no-op，六对象多选 seed、适用性与原子性，快照/远端图片一致，foreign/stale/权限失效拒绝，非法请求原子拒绝及实际样例页。 |
| 第一批 Chrome 回归 | 73 项通过；first-batch-browser.json。含真实 Demo 滑块拖动、Escape、一次 Undo、连接线路由预览连续性；原 Table 排除断言按新增“网格外观适用、通用 paint 排除”更新，Group 排除不变。 |
| Native / WASM 路径 | 新增 226806 条命令完全一致，最大坐标差 0；第一批 235750 条命令两侧各自散列与入场证据相同，跨平台最大差仍为 1.8189894035458565e-12 ≤ 1e-9；path-conformance.json。 |
| Native / Web 图片 | 新对象 opaque Operation 样例 800×550，1760000 通道，最大/平均差 0；browser.json pixels 与 fixture/native-render.png、browser-native-web.png。允许最大通道差 2、平均 .02。第一批图片对照仍为 0 差异。 |
| 受影响消费者 | Table 57、导出 33、背景 22 项通过；table-browser.json、export-browser.json、background-browser.json。通用 SDK 52 项通过；generic-browser.json 的 result。 |
| 接口与包 | TS strict、JS 公共错误契约通过；Runtime 边界 111 个契约通过。descriptor / 发布包装共 24 个测试通过（无跳过），package-descriptor.log。npm dry-run 39 文件，无内部头文件或静态库，未发布。 |
| Windows wrapper 源码 | macOS clang 以 -fdeclspec 对完整 caller 头文件和新路径入口做 syntax-only 检查通过；windows-caller.log。此检查不构成 Windows 编译器、DLL 链接、ABI 或打包证明。 |
| 实际页面 | IAB 中实际创建带孔路径、修改程度、放置表格、检查参数排除、用 Section 包含表格；视觉样例自适应显示并实际导出 PNG，gallery-export.png。browser-gallery.png 为匹配 Chrome 样例页面截图。 |

完整原生 build 后执行 212 项 CTest：190 通过，22 项失败与第一批基线逐项相同，没有新增失败。
`FC02Text` 用时 239.74 秒并通过；`G47QualificationReplay`、相关 Table/Section/Skia 检查通过。
完整输出 `ctest.log`，名称及基线差集 `ctest-comparison.json`。保留的 22 项历史失败见
第一批报告和该 JSON，不扩大本轮做无关修复。

本轮 red 证据也保留：孔洞被 bbox 命中、Section 外溢边界框选漏选、旧路径缺省读回不一致、
未知生成器版本/零 seed 和新建禁用状态元数据缺口。各修复均有随后完整 green 运行。
中间样例构建曾因 TableGridStyle 初始化及缺少 cell layout 失败，修正 fixture 后通过；
Windows caller 检查曾因宿主 declspec/include 配置失败，配置修正后发现并修复上述真实重载问题，
分别保留 fixture-build-failure.log、windows-caller-initial-failure.log、windows-caller-overload-red.log。
初次 schema 运行因未带 AXIOM_PROTOC 跳过 1 项；补入现有 CMakeCache 中 resolver 发出的 Store 路径后，24 项全部通过。原跳过日志另存 package-descriptor-without-protoc.log。
不将这些失败尝试当作成功证据。

## 复验与试用

继续复用已配置的原生和 WASM 构建以及锁定 Semantic SDK Store / 预编译 Skia；不重编依赖栈。

```sh
cmake --build out/object-identity --target axiom_sketch_tests axiom_sketch_path_probe axiom_sketch_render_fixture -j 8
out/object-identity/runtime/composition/axiom_sketch_tests
cmake --build out/object-identity-web/build --target axiom_js_runtime axiom_sketch_path_probe -j 8
out/object-identity/runtime/composition/axiom_sketch_render_fixture out/sketch-batch2/fixture --extended
node verification/sdk_js/verify_sketch_extended.mjs out/object-identity-web/build/sdk/web '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' out/sketch-batch2/browser.json out/sketch-batch2/fixture
ctest --test-dir out/object-identity --output-on-failure -j 8
```

- 可编辑 Demo：<http://127.0.0.1:4191/example.html>。
- 增量视觉样例：<http://127.0.0.1:4191/matrix.html>。
- 第一批视觉矩阵：<http://127.0.0.1:4191/first-batch-matrix.html>。
- 匹配包目录 `out/sketch-batch2/demo`，停止服务后可在该目录运行 `python serve.py --port 4191`。
- 入场工作备份 baseline.patch / baseline-source，最终源与资产指纹 source-hashes.json / demo-assets.json，汇总 summary.json。

没有修改 Semantic / Skia lock、发布版本或用户原有 4188/4189 页面；没有提交、推送或发布。
Windows DLL ABI、物理设备与 IME 认证仍未验证。字体、自由笔迹、节点编辑器和单元格排线不在本轮范围。
