# 手绘外观 v1 第一批实现与验证

日期：2026-11-01。基线：`af5b1778284211c3834957084dd0a03e48308d69`，
分支：`codex/sdk-shell-axis-bridge`。本次为用户批准的增量实现，未执行提交、发布或正式 Gate 判定。
范围与授权见 [设计契约](sketch-v1-design.md) 和 [执行契约](sketch-v1-contract.json)。

## 实现范围与 owner

| 层 | 实现与责任 |
| --- | --- |
| Semantic / schema | 闭集追加 `SketchStyleV1`、`PatternFillV1`，字段 `0x102`，校验、规范化、指纹、Operation codec、Snapshot 能力要求与当前 descriptor 一起更新。历史冻结 descriptor、FC08 authority 保持原身份。 |
| Interaction | `canvas_appearance.cpp` 负责 seed、默认外观、适用范围与 mixed 聚合、原子 patch、外观编辑会话，复用 OperationEngine、History、操作发布。 |
| Scene | `sketch_geometry.cpp` 生成不可变、本地坐标的贝塞尔路径和排线；相机与对象变换不重新抽样。连接线先路由再装饰，障碍控制包络有界减弱，必要时精确线段回退。 |
| Render | 正式帧、创建/外观预览、导出共用 Scene / Skia；视觉 bounds 含曲线控制包络及描边，选框、控制柄、尺寸和吸附使用稳定规则几何。填充主体整体可命中。 |
| SDK | Web 根导出、TypeScript、私有共享 Session、Windows 调用方 wrapper 同步增加 appearance DTO/命令，无新增内部对象、路径或可变 owner 导出。 |
| Shell | Demo 第二行提供手绘开关、连续程度滑块、0/1/2 快选、填充、排线参数、描边、透明度与虚线控件。 |

矩形、椭圆、三角形、Connector/直线/箭头、Sticky 底板已接通。Sticky 文字仍由文本系统绘制。
Group 不递归更改子对象；不适用对象通过 excluded / fillExcluded 明确读回。
Table/1 保留；Table 网格、Section、复杂 VectorPath、字体和自由笔迹未加入本批风格实现。

## 固定的契约决策

- `styleVersion=1` 与 SDK `0.1.0` 分开。roughness 连续范围 0–3；0 保留 seed 和排线。
- 第一次启用由 Runtime 分配非零 64 位 seed；关闭再开、改颜色、改程度、Undo/Redo、序列化与远端应用保留 seed。显式 `reseed` 可撤销。JS 使用十进制字符串无损传递。
- 参考 Rough.js 4.6.6 的固定提交 `56a2762171b1294d643501e8d14f120db6b27bd7`，实现受限 C++ 算法，不宣称 Rough.js 路径逐字节兼容。SplitMix64、独立轮廓/填充随机域、程度映射随 v1 固定。
- 小对象噪声按尺寸收敛；实线双遍描边以 `1 - .25 * min(roughness, 1)` 校准线宽，虚线单遍。轮廓与排线参数独立。
- 排线最多 8192 段，输出命令上界 32768，连接线路由段上界 4096；密度超限按固定、原点对齐的整数 stride 确定性稀疏。缓存最多 64 项、131072 条命令，不含颜色、透明度或相机。
- Snapshot 增加 reader bit 128，当前完整能力 255；带 sketch / pattern 的文档分别要求对应 v1 capability。旧 127 reader 和未知版本明确拒绝。
- Sticky 过去的硬编码黄色转为 kind-specific 缺省填充。显式 None 保留，并要求版本化外观元数据，避免旧消费者清掉属性后又显示黄色。第一次实际外观修改会物化这份元数据；空 patch / 无变化不物化。
- 新对象默认值不进入对象 History；创建预览保留一份 seed，提交时随新对象落入 canonical。

`appearance.begin()` 捕获完整观察 stamp、适用 roots 和旧值；updatePreview 仅修改本地瞬态展示。
commit 校验 token、目标、selection/generation/scene/permission/lifecycle，整批一次原子提交、一次 History；cancel 和无变化无操作。
不同对象 seed 不参与 roughness 等用户参数 mixed 判断。外部持久化和云确认继续使用原有 Data/Axis 边界。

## 验证证据

本机证据位于 `out/sketch-v1/`，未作为产品资源打包。所有浏览器用匹配的 JS/types/WASM/data，
在仓库外临时消费者目录启动真实 Chrome 156.0.8078.74；未使用深层业务对象或模拟 Runtime。

| 验证 | 结果与证据 |
| --- | --- |
| 原生新增语义/Runtime/生成器 | 19 个 GTest 通过：`sketch-native.log`。覆盖版本拒绝、零程度、种子、原子会话/History、远端条件检查、批次锁定、排线主体命中、稳定几何吸附、小椭圆提交等。 |
| Chrome 外部 SDK + 实际 Demo | 73 项通过：`browser.json`。含 preview 无同步操作、一次提交/一次撤销、无变化/取消、stale/foreign/malformed token、失效清预览、保存恢复、远端无 echo、多选保留 seed、Group/Table 排除、真实滑块拖动与 Escape、避障连接线粗线宽预览与提交一致。 |
| Native / WASM 路径 | 235750 条命令比较通过；最大坐标差 `1.8189894035458565e-12`，容差 `1e-9`：`path-conformance.json`。不把不同浮点平台的 JSON 哈希误称为完全相等。 |
| Native / Web Skia 图片 | 同一 opaque Operation 样例，800×550，1760000 个通道值；实测最大差和平均差均为 0：`browser.json`、`native-render.png`、`browser-render.png`。判定允许最大通道差 2 / 平均差 .02 的抗锯齿容差。 |
| 受影响 Web 回归 | Table 57 项、图片导出 33 项、背景 22 项通过，分别见 `table-browser.json`、`export-browser.json`、`background-browser.json`；通用外部消费者 `generic-browser.json` 通过。 |
| 类型、接口与发布边界 | JS 公共错误契约通过；TypeScript strict 通过；Runtime 边界 110 个契约通过；包装测试 17 项通过。npm dry-run 39 文件，无内部头文件或静态依赖档案，未发布。 |
| schema / descriptor | 7 项测试通过；当前 descriptor `8bec27dea24eceaf196e26c1809f893984d1e1c1f92acb4336d5595ba61bc5b0`。历史 descriptor 仍为 `8c1da1c19b754f9345472f96452e58505ebd18c0874b04480e378eb95a38654d`。 |

构建复用现有 `out/object-identity` 和 `out/object-identity-web/build`，消费锁定 Semantic SDK Store 与 Skia。
未重编 Protobuf/Abseil/Skia，未修改依赖 lock。

本次验证过程中实际捕获并修复：

1. 单段连接线的终端切线约束曾使笔触完全变直，现分成两段显示曲线，保持端点方向同时有笔触弯曲。
2. 排线空隙曾导致三角形主体点选不命中，现填充命中使用完整主体。
3. 视觉粗糙 bounds 曾反馈到吸附，让 120 宽的目标生成 121.2929 宽，现交互只使用规则几何。
4. 小椭圆从其他矩形边缘开始绘制时，既有中心吸附会把宽度拉到 0，Up 被语义校验拒绝。原生与 Chrome 均复现；现拒绝使正尺寸盒子塌缩/翻向的吸附候选，20×14 椭圆可提交。
5. 粗线宽的避障连接线预览曾沿用旧 clearance，提交后从 y=141 跳到 y=122；现复用既有增量路由预览 owner，更新瞬态路径、视觉边界和选框，预览/提交截图相同。程度/颜色/填充不触发避障重算，线宽/描边启用状态变化才失效这份缓存。
6. Sketch 绘制曾提前覆盖终止失败路由的诊断样式。新增实际导出像素测试先复现为 960 个蓝色像素且无诊断色；现回到既有诊断绘制器，保留 NoRoute/预算失败的颜色和虚线，Pending 仍正常预览。
7. Sticky None 的旧消费者静默兼容问题，以版本元数据和能力拒绝闭合；历史 headless Sticky fixture 的绘制原点约定保留，原 golden 校验通过。

测试驱动本身曾错误发送鼠标移动的 button/force，触发浏览器释放 pointer capture；
已经校正为实际按下左键的 CDP 输入序列，最终 73 项结果来自修正后的完整运行。
历史 red/build/browser 日志保留在证据目录，不将失败尝试写成成功。

## 全量回归与限制

完整配置为 212 个 CTest；全量结果及与 Table/1 基线的对照见 `final-ctest.log`、`ctest-comparison.json`。
失败路由诊断修复后，新增 19 项与相关 SDK/Skia/Connector/Sticky/Table/G47 聚焦回归 15/15 通过（`focused-ctest.log`），再执行完整 212 项；最终 Web 73 项也使用修复后的匹配资产重新运行。
此前该环境的 Table/1 基线有 46 个失败；本次仍有 22 个历史失败，未出现新增失败。
配置/构建覆盖差异不能据此认定其余历史失败都由本功能修复。

保留失败名称：`canvas_fc02_shape_label_scene`、`canvas_g2_a3_full_incremental_equivalence`、
`G3HeadlessCanvasDemo`、`FC00XRuntimeComposition`、`FC04.MSE-11`、`FC04.MSE-20`、
`FC04.MSE-23`、`FC04.MSE-24`、`FC04.MSE-25`、`FC04.MSE-26`、`FC01Creation-width-style`、
`G4InkPlaygroundComposition`、`G4InkPlaygroundSurface`、`G4InkPlaygroundRepeatedStrokes`、
`G4MultiPointerPlayground`、`G4ViewportGestureState`、`G46BQueryStructure`、`G46PlaygroundTransform`、
`G47TextHost`、`G46CQualificationBundle`、`G46CQualificationBehavior`、`G45BrushRuntimePipeline`。
具体旧断言、qualification 与 headless fixture 失败输出保留在日志，未扩大本批做无关修复。

一次中间全量曾出现第 23 个失败 `G47QualificationReplay`（SIGSEGV），聚焦复验同样复现。
崩溃栈落在 qualification 子类的资源 map 初始化/写入；该轮编译尚未结束时修改了内部 Runtime 头文件，导致部分对象文件使用旧类布局。
将依赖这份头文件的 59 个原生项目对象失效后重新编译，G47 聚焦复验通过。没有重编外部依赖、修改该 fixture 或掩盖其断言。
对象列表见 `native-header-rebuild.json`，此前 23 失败的全量及复验保留于 `ctest-before-header-rebuild.log`、`focused-before-header-rebuild.log`。
最终重新运行完整回归，避免把这份混合布局的构建作为交付证据。

本机原生、共享 Session 源码和浏览器检查不构成 Windows DLL ABI、Windows 打包、物理设备或 IME 认证。
这些平台结论保持未验证。本次不发布 SDK，也不覆盖已发布的 0.0.1。

## 本机试用

- 可编辑 Demo：<http://127.0.0.1:4190/example.html>。选择图形工具，第二行设置新建默认；选中对象后编辑该对象/多选范围。滑块支持连续程度，拖动一次 Undo 一步，Escape 取消。
- 视觉样例：<http://127.0.0.1:4190/matrix.html>。含 0/1/2/3 程度、尺寸、不同轮廓宽度、虚线、透明度、两种排线及箭头，支持 PNG 导出。
- 运行目录：`out/sketch-v1/demo`；匹配资产散列：`out/sketch-v1/demo-assets.json`，与外部 Chrome 通过的主要资产一致。未刷新用户原有 4188/4189 页面。
