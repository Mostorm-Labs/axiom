# 大场景导航提交等待修复

本切片为 `READY_FOR_CONTROL_REVIEW`，仅指本轮工程修复。没有签发 G5/P34、正式规模、SLO 或物理设备 PASS。受控分支为 codex/geometry-navigation-integration，执行包为 `docs/planning/G5_LARGE_NAVIGATION_SUBMISSION.md`，anchor4f7842db。

## 修复了什么

100K v4笔画超过完整 prepared-path 的临时空间接纳范围后，导航每个 world tile 都从第0个 knot 重放。屏外历史同样消耗每次32 knots的有限步骤：基线1M文档的75次机会中只有1次 present，而多数机会 CPU 仅约0.1–0.3ms。热路线也会重复这段工作。

Render 现在为这类大源保存同一个 `ink::CompactContourCompiler` 的每256 knots检查点及实际发射几何的保守边界。绘制仍由原编译器重放，使用完整原始 history、total distance、caps/tapers/seed；没有新的曲线算法或持久化几何。局部恢复覆盖相邻 patch，完成时才发射 finish。tile 直接跳过已证明不相交的 primitive 区间，实际绘制继续受现有 steps、GPU primitive、query 和 time policy 限制。距离只保存全局标量，不为这些源另存全长数组，也不在每个 tile 结束时重新扫描全长。

检查点、字符串估计及有界重放 scratch 先接纳再分配；Render source allocation 继续共享计费。缺少完整反馈时的 coarse replay 可使用现有 visible-critical 硬限，optional prewarm 仍受软限约束；硬限拒绝保留原来正确的全量回退。上限数值、public SDK/ABI/version0.3.9、Semantic/Skia lock、schema、G3/旧 Gate 均没有修改。Scene/camera/OperationEngine/Skia/provider 的 owner 不变，导航不创建 canonical receipt。

## 实际效果

公开 root SDK，真实 headless Chrome/WebGL2，DPR2；before 为冻结 a4b0a459 SDK，after 为 ed8df6af SDK。1M总点由10条各100K的有效笔画构成，两版各两轮、每轮冷/热各一条路线。下表是输入期间观察到的默认 framebuffer 写入的最大间隔，单位ms，不能当作物理显示延迟。

| 默认 OFF 场景 | 修复前 | 修复后 |
| --- | --- | --- |
| 1M总点，冷，两轮 | 1205.6 / 1205.1 | 107.0 / 80.8 |
| 1M总点，热，两轮 | 1194.0 / 1195.7 | 18.4 / 18.4 |
| 100K单笔，冷 | 856.4 | 53.2 |
| 100K单笔，热 | 17.6 | 18.6 |
| 10K密集对象，冷 | 198.4 | 178.9 |
| 10K密集对象，热 | 17.3 | 18.0 |

1M的冷/热合并配对中位数：OFF1200.4→49.6ms，ON1199.8→50.3ms，分别降低约95.9%/95.8%，达到本任务至少50%的工程回归条件。冷/热分别列出，不能用合并中位数掩盖冷启动尾部。完整16次配对运行均 MEASURED，数据及所有尾部在 `before/`、`after/`、`comparison.json`。

native 机器时间无关 oracle：100K源、同样8个 owner steps，旧回退391次机会后才反馈，新路径第1次；GPU quota32保持。新旧完整导航反馈像素一致；它们与 Direct存在相同的86个channel采样差异，未把导航 tile 误当 canonical。随后实际 canonical frame 与 Direct 精确一致。这个测试的初始过严假设和旧路径对照日志均保留。

## 正确性与范围

- 所有配对路线最终 canvas像素与基线逐哈希一致；document/selection/operation、实际DPR/对象/点数、receipt、close、source/artifact stability检查通过。
- 五类小混合编辑各跑 ON/OFF：变换、部分擦除、便签文本、表格文本、图片替换。10次执行的 apply/present/undo/redo检查通过，before/after committed export像素既匹配基线也匹配ON/OFF。
- 新的 full-history oracle 覆盖大源的checkpoint边界、taper、marker、明确跨边界 reversal/corner caps、仿射/镜像/透视 fail-open、primitive数量、共享及owner/content invalidation。
- 预算压力测试拒绝optional soft admission、允许既有visible hard admission、拒绝hard overflow，并检查旧owner在途租约和最终零释放；静态的32MiB cache/8MiB entry、2ms cooperative opportunity、steps64、GPU128等生产配置没有放大。
- 真实GPU的 replay、quality、exact-replay、affine、background、contour六模式通过。全部结果带当前私有GPU二进制SHA；这不是整个历史GPU candidate套件或跨平台资格。

## 仍需改进的地方

冷启动的检查点构建仍是不可抢占的 source-query 工作，本轮默认版冷空窗最高107ms；cooperative2ms不等于硬deadline。未来可拆分这项构建，本轮没有靠提高帧预算隐藏它。

反馈变连续后，新露出的边缘仍可暂用粗层：1M ON冷路线低清面积峰值约7.1%、时间加权约2.1%，约96%区域继续取原清晰 retained源；极小区域最大放大可达8x。warm加权低清约0.16%，最大2x。GPU sharp-overlap/coverage oracles通过；不能把修复前“只有末尾1次present、没有有效观测时长”的低清0当成更好的清晰度，或宣称本轮消除了模糊。

实际占用增加：1M ON采样的 ResourceBudget peak约40.77→60.67MB，其中约4.48MB为紧凑prepared状态，其余主要是现在能完成/保留的导航tile。所有上限与拒绝规则保持，资源量不是免费降低；budget gauge不是整个进程或GPU driver residency。进程族RSS上限仍180s/3GiB且清理完整。

10K密集对象的exact receipt等待仍约4.5s，基本未变；1M canonical reference encode/digest成本也不在此修复中。100K/1M objects的snapshot接纳限制、native资源边界以及旧G3等失败没有被升级为通过。上轮首次1M浏览器target关闭仍未确定根因；本轮所有配对运行未复现，也没有声称修复该故障。

## 回归与输入身份

native全量1007/1073；相同66项失败/未构建，见 `regression-comparison.json`。四个旧semantic test编译失败仍在完整build log。新增核心9项及后补明确reversal/cap用例通过。Node141/152，同一11个platform-harness dist缺失；Python3.12 205项，同一4失败/2skip；workspace9个包缺tsc；runtime boundary118个contract files通过。完整名字和日志在 `script-regressions.json`、`logs/`，没有通过删测试掩盖失败。

source/SDK身份区分：before采样上下文d7903b6b、SDK a4b0a459；实现/native full/GPU/candidate SDK ed8df6af；after/混合编辑采样上下文9f312e92，仅多了native reversal测试，生产源码与candidate SDK一致。完整ref/SHA在 `execution-inputs.json`、raw的sourceBefore/After和SDK buildInfo中。ON/OFF各36文件，OFF无timing标记，没有增加私有consumer payload。锁定SDK依赖复用，未重新构建。

新手测Demo：<http://127.0.0.1:4218/example.html>，服务冻结的 `out/g5-large-off/js`。4217和旧安装包SHA完全不变；没有重载或替换用户现有文档。

`collect_evidence.py`收集冻结out产物，`compare.py`执行配对/像素/工程回归检查，`edit_regressions.py`编排原公开SDK编辑场景。`SHA256.json`覆盖本目录除它自身外全部文件。采样不能与构建/回归并行；root SDK必须使用对应JS/WASM/resources，物理输入、长期idle/eviction、Windows ABI及正式大型复杂对象资格仍需独立证据。
