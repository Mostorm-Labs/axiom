# G5 第四步：复杂编辑与有界规模验证

本切片为 `READY_FOR_CONTROL_REVIEW`，仅指工程工具和本轮有界观察可供审查。主矩阵为 `OBSERVED_WITH_BLOCKED_TIERS`，没有产品规模资格、性能 SLO、G5/P34 或物理设备 PASS。受控工作树和分支为 axiom-geometry-navigation/axiom、codex/geometry-navigation-integration；anchor 为6cce9d93f8433d568f5383da3e65dd2608ed7cbe。

## 实现与身份

执行包为 `docs/planning/G5_COMPLEX_EDITING_AND_SCALE.md`。语义修改仍由公开 SDK → Runtime editing → OperationEngine 完成；fixture 作者只生成经现行 SnapshotCodec 验证的外部恢复输入。没有新建 document、selection、history 或几何 owner，没有公开私有 Runtime/HEAP，也没有增加 consumer API、改变 Semantic/Skia lock、schema、旧 Gate、预算或 SDK 版本（仍为0.3.9）。4217 的冻结安装包 SHA 与上轮完全一致。

新增工具覆盖五类编辑、codec fixture/独立 inspector、Scene/index 和 point projection 组件、公开 SDK restore、冷/热导航、进程族资源限制及结果分层。生产代码仅增加默认关闭的 source-point/budget 诊断：navigation timing v5；普通 OFF 包不含这些计时字符串。每套 Web 安装包36个文件，没有私有头文件、测试 executable 或独立依赖 archive。

身份必须按用途区分：

- `4e22db5e`：诊断和主工具实现、全量 native 回归；其 Runtime/SDK 源码与后续版本一致。
- `a4b0a4590773f86ae00026724cf5ccd2f4ea13a3`：主矩阵49个阶段和 matching ON/OFF Web SDK。修复了组件接纳检查、旧 sampler 模块路由及过时的 timing v2 判定。
- `91da0bff`：额外的合法多笔画总点数 fixture；没有提高单笔上限。
- `de08a2b06f51994071ccd94eb66971371868279e`：补充观察的执行上下文；增加浏览器退出/崩溃/错误通道记录。仍使用冻结的 a4b0a459 SDK。脚本验证后续 Runtime/SDK/schema 等生产输入与该 SDK source 完全一致。

`execution-inputs.json`、各 raw observation 的 sourceBefore/sourceAfter、artifacts 和 SDK 自报 build info 分别绑定这些身份。不要把采样上下文 revision 当成 SDK 编译 revision。native 主回归与之后重跑的五项 fixture contract 也分别保留日志与输入 SHA。

## 完成的编辑闭环

7个实际 canonical object records、4070个存储 knots（输入为4096个 pointer events）。五种动作各跑 ON/OFF：长笔画 affine 变换、部分擦除、便签文本、表格文本、图片/resource 替换。

十次执行都观察到真实 generation/operation 推进和对应 presented generation；已提交内容导出像素改变；undo 回到 before、redo 回到 after；其后导航没有修改文档、选择或 operation，receipt 和 clean close 检查通过。ON/OFF 的五组 before/after 导出像素哈希一致。表格/文本允许一个动作产生多个 history 项，按实际像素回退次数重做；没有把整份资源 manifest digest 当成可逆历史身份。

原始 observation、canonical snapshot、native inspector census 均在 `matrix/edit-*`。`diagnostic-default-pixels.json` 有十组导出配对。主矩阵20次完成的 root SDK 执行，其冷/热返回相同 camera 后的最终 canvas 像素哈希一致，见 `navigation-return-pixels.json`。

## 规模结果与边界

主矩阵49阶段：36个 `MEASURED`、13个 `BLOCKED_EVIDENCE`。Scene/index、point projection、semantic fixture、root SDK 分开计数；一个成功的 native query 不代表 SDK 已加载或呈现。

| 公开 SDK 场景 | 实际对象 | 总存储 knots | 视口内点中心 | DPR | 结果 |
| --- | ---: | ---: | ---: | --- | --- |
| sparse/dense 1K shapes | 1000 | 0 | 0 | 1、2 | 完成 restore、transform、冷/热导航 |
| sparse/dense 10K shapes | 10000 | 0 | 0 | 2 | 完成相同检查 |
| 单笔1K/10K/100K points | 1 | 对应1000/10000/100000 | 1000/1600/1600 | 1K为1、2，其余2 | 完成 restore、冷/热导航 |
| 100K objects，两种布局 | 未加载 | — | — | 2 | codec preflight 拒绝，未进入 root SDK |
| 1M objects，两种布局 | 未加载 | — | — | 2 | native worker 触及 RSS 上限；fixture 不可用 |
| 单笔1M points | 未加载 | — | — | 2 | 不满足现行 compact 单笔上限262144，拒绝 |
| **另行冻结的1M文档总点数** | **10** | **1000000** | **16000** | **2** | **codec、ON/OFF SDK restore/render/navigation 完成** |

合法1M总点 fixture 是10条各100K的笔画，seed42、y 间隔30；snapshot 为18,004,411 bytes。它与无效的“单笔1M”是两个不同输入，不替换或美化主矩阵的拒绝记录。后者的组件接纳仍 fail closed。额外结果在 `document-total/`、`document-total-default/` 和 `document-total-comparison.json`：实际 source counter 为1M，ON/OFF 冷/热最终像素一致；默认 OFF 没有诊断记录，cache occupancy 为 unavailable 而非0。

100K objects 的 error7 是 `kLimitExceeded`；现行 `codec.cpp` 的 snapshot repeated-item 上限为65535。没有修改这项 authority。1M native component 保留了 query 子阶段与 linear oracle 结果，但更新未在2GiB envelope 内完成，整个组件仍 BLOCKED。1M object fixture 也在该 envelope 内被终止；不能从部分结果推出恢复/渲染资格。

所有 native workers 为180s/2GiB，browser owning-process-family 为180s/3GiB，结束后清理完整。RSS 是逐进程求和，可能重复计共享页面，不能当作唯一物理内存或 GPU residency。cache 是采样到的 Runtime budget gauge；points/visible-centres/candidate source points 各自保留定义，visible centres 忽略 erase coverage，不等于最终可见像素。

## 暴露出的下一轮重点

这轮证明了具体输入能完成语义/像素检查，同时暴露了明显等待，不能宣布大场景流畅：

- ON 单次观察中，DPR2 10K sparse 的 load+initial display 约1.20s，dense 约6.66s；dense 冷/热 receipt 等待约5.13/5.30s。dense 全部10K对象处于视口，sparse 约400个；总对象数不能独立解释负载。这里没有默认 OFF 密集矩阵或统计性 SLO 结论。
- 100K 单笔 cold route 的输入期 GL submission 最大空窗约1.71s，warm 约34ms。
- 合法1M总点文档，ON/OFF load+initial display 约2.70/2.50s；两版 cold/warm 的输入期 submission 最大空窗都约1.2s。ON 的 input dispatch 最大约2.1ms、navigation opportunity CPU 最大约2.7ms，而空窗明显更长，应先调查 coverage/prewarm/latest-work 到实际提交之间的等待。RAF 或 CPU 总量不能替代显示连续性。
- 1M总点 ON 的两次 settle exact attempt，reference encode/digest 合计约41–45ms，编码约20MB reference 数据。共享 source payload 并未消除这段编码/摘要成本；它值得后续单独优化。
- 100K/1M objects 需要先对齐大文档 checkpoint/restore 的契约和资源设计，不能通过提高 Render 预算或直接修改 codec 常数掩盖边界。

以上是 headless Chrome、真实 WebGL2/ANGLE、合成 DOM 输入的单次工程观察。cold/warm 指 Runtime 首次/重复路线，不是 OS/GPU shader cache 的严格冷启动。submission 是观察到的默认 framebuffer 写入，receipt 为当前公开 coarse post-input receipt，不是 camera-qualified/物理显示时间。大规模点数样本没有执行编辑；五类编辑闭环来自小混合文档。

## 失败、工具修正与保留历史

首次合法1M总点 SDK 尝试在初次 canonical preparation 的 retry 阶段出现 target closed，进程非零退出，峰值没有达到 supervisor 上限。原始记录在 `experiments/initial-document-total-error`。加入退出事件/Chrome stderr 后，同一 fixture 的 ON 重跑及 OFF 对照完成，1K control 也完成；未复现首次关闭，根因仍未确定。没有声称修复了浏览器崩溃，也没有删除失败或以成功重跑代替它。Chrome 日志中的已有 printErr/timing 走 console error channel；不等同于 Runtime fatal error，生命周期事件标明是否发生在正常 cleanup。

`experiments/pre-admission` 是4e22db5e开发期矩阵，因其单笔组件没有先验证 compact 接纳上限而被 supersede；其中1M几何查询不能证明有效笔画。RED/GREEN 和最终 a4b0a459 接纳记录是当前依据。旧 sampler 的新增模块404及 timing version 误判也有 HTTP/真实浏览器 RED/GREEN；开发期兼容检查的 SDK/dirty source context 只用于复现工具问题，不作为最终主采样。

## 回归与复核

- 全量 native CTest1007/1073：失败/未构建66项与第三步完全相同，见 `regression-comparison.json`。四个旧 semantic binaries 编译失败、G3 snapshot authority 等仍保留；没有绕过。
- 最终 fixture contract 五项通过，覆盖实际 codec roundtrip、线性 oracle、非法单笔拒绝及合法多笔总点数。进程成功/真实超时/真实 RSS cap 三项测试通过。
- Node141/152：同一11项 platform-harness dist 缺失，新增八项 metrics/HTTP tests 通过。
- Python3.12 205项，4失败、2 skip；四项在6cce基线复现。默认 Python3.9 的初次运行不用于最终结论。
- workspace9个包仍缺少 tsc；boundary checker118个 contract files 通过；ON/OFF install 和4217不变性检查通过。

`SHA256.json` 覆盖本目录除其自身外全部文件。snapshot/log 以 gzip 保留，metadata 的 snapshot SHA 指解压后的原始 bytes；artifact/source SHA 指各自明确的 revision。`collect_evidence.py` 从冻结 out 产物收集，拒绝生产源码偏离；先运行以下工具再收集，性能采样期间不要并行构建/测试。

```sh
python3.12 verification/runners/g5/complex_scale_matrix.py out/host/axiom_g5_scale_fixture out/g5-step4-on/js out/g5-step4-matrix --default-sdk out/g5-step4-off/js
node verification/runners/g5/complex_scale_report.mjs out/g5-step4-matrix/matrix.json out/g5-step4-matrix-summary.json
python3.12 verification/runners/g5/complex_scale_point_total.py out/host/axiom_g5_scale_fixture out/g5-step4-on/js out/g5-step4-point-total
```

复核应使用各结果绑定的 source/tool/SDK，而非随意重用更新后的 build。正常消费既有锁定 Semantic/Skia SDK，不重建第三方依赖。本地证据没有发布 SDK，也没有替代后续独立控制审查或大型复杂对象/文本密集场景的正式验收。
