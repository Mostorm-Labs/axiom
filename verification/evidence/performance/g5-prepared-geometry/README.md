# G5 prepared geometry / exact settle continuation

本轮按授权顺序完成：采样基线提交 → exact 收敛改进 → 同矩阵复验 → v4 局部
contour、vector-solid prepared path、共享几何预算。仓库 `Mostorm-Labs/axiom`，
工作树 `axiom-geometry-navigation/axiom`，分支 `codex/geometry-navigation-integration`。

前两项分别提交为 `8dcaa021`、`89a24b51`。本目录记录后续几何实现的 dirty-source
执行证据：构建内 source revision 为 `89a24b51`，完整 source tree SHA256 为
`9944f3e0ec5f94db525971bfc13a37792cd3c99cd944bf89c8f18b65afdcaea8`。
最终提交包含这些源文件和证据；不能仅用构建内 HEAD 代表实现身份。

这是 `MEASURED_WITH_BASELINE_FINDINGS` 工程结果，不签发整个 G5/P34 或真实设备 PASS。

## 实现边界

- exact preparation 的私有 checkpoint 上限从 64 提高到 4096，继续在 32-knot
  单元间检查既有 8ms 配额。保留 100ms navigation idle 门槛、显式小预算、取消、
  surface/resource/generation 校验及完整 canonical receipt 协议。没有增加 RAF
  内部重入循环，没有提前发布 partial frame。
- Render 私有 `PreparedGeometryCache` 按完整 typed command 校验内容。旧 Scene
  digest 字符串并不完整编码所有 compact 字段，因此没有把它当成唯一缓存键。
  该缓存不拥有 BrushSession、OperationEngine 或新的文档状态。
- v4 只运行现有 contour compiler，保留每个原始相邻 patch 对。一次构建 SkPath，
  按原顺序分组，以保守 bounds 和 AA margin 选择与 tile 相交的 primitive。
  dots/caps、marker 变换、taper 和 paired patch 边界保持不变；透视/不可逆矩阵、
  极端坐标/缩放关闭局部 culling，仍消费同一完整几何。cap bounds 按实际 float
  中心/半径向外舍入，避免极小圆点在大世界坐标下被空 bounds 错误剔除。
- vector-solid 从既有 canonical outline 构建一次完整 SkPath，由导航 tile 共享。
  绘制仍委托统一 Skia command helper，保持 fill rule、closed、paint、AA 和 erase
  处理；没有新增平台绘制算法。canonical Direct 路径保留为 oracle。
- prefix distances、prepared paths 和一份键值 payload 使用统一 ResourceBudgetManager
  的 lease。构建峰值先 admission，完成后按模型化 footprint 收缩；LRU 仅淘汰
  未被使用的共享对象。tile/job 引用继续持有 lease，跨 budget owner 不复用 lease。
  预算拒绝使用既有完整 evaluator。模型化字节不是进程 RSS 或 GPU driver 的全部
  隐式内存。
- 拒绝 prepared prefix 的回退现已正确跨 checkpoint 累加全笔画距离，保持 taper
  与材质 phase；新增 20K-knot 拒绝案例先失败、修复后通过。

未改变语义 schema/descriptor、Brush 算法、Skia/Semantic lock、SDK facade/ABI、
旧 Gate 记录或第三方 SDK。共享的是 prepared source geometry；Scene/reference
payload 的全部复制、稳定 chunk index、统一 Tile/Scheduler 大规模策略仍是后续工作。

## 像素与工作量

- 原生完整/局部 contour 对比覆盖 30 个 family/path/transform 组合，包括单点、
  反向、cap、taper、marker、仿射、反射、透视；完全逐像素相同。
- 固定长线局部语料实际绘制 **558 / 4922** 个 primitive（11.3%），其余部分没有
  进入 draw。没有通过修改轮廓或丢弃预测/确认语义换取工作量下降。
- 1,000,000 世界坐标、512 倍变换下的极小圆点用例先 RED、修复后逐像素一致。
  该数值修复后重建两套 SDK、重跑全量回归/GPU 及全部匹配矩阵；本目录不使用
  修复前的采样替代最终源码证据。
- vector fill-rule、closed、paint/AA 与未缓存 helper 完全相同；共享、淘汰、拒绝、
  恢复、在途引用和 budget owner 更换均有原生检查。
- 实际 Chrome/Ganesh probe 对 v4 圆笔/marker 在六个变换场景逐像素比较局部/完整
  contour，连同既有 cache、resumption、hybrid、mask、lifecycle 等 Direct corpus
  全部通过，退出码 0。`gpu-probe.json` 绑定 JS/WASM SHA。
- 新 metrics guard 先失败后通过，确保 delta 累加、resident bytes 取峰值而非累加。
  Node 指标/性能语料共 9/9；最新原生 cache/prepared/navigation 回归 3/3。

## 同一 Web 矩阵复验

Release，Chrome 156.0.8078.80，实际 Apple M2 Metal/ANGLE WebGL2，DPR2、800×500
logical。四类场景、选中/未选中、三轮、cold/warm、ON/OFF，共 96 段导航。
冷/热定义、72 个输入和两条各 2048 点的 `round-handdrawn@4` 长线与原基线一致。
构建与重负载回归结束后才采样，保留全部慢帧；像素读取在计时窗口外。

ON/OFF **72/72** 像素检查一致，相对最初未优化基线也 **72/72** 一致。
文档、选择、Operation、相机、无输入期 canonical receipt、最终 exact receipt、
关闭和错误检查均通过，source/artifact 运行前后 SHA 不变。

默认构建各组 RAF p95 为 **17.5–20.0ms**；exact receipt 组中位数 **166.3–199.7ms**，
最初基线约 **583–600ms**。主要收敛收益来自前一提交的 checkpoint 调度修复；
本次局部几何没有改变 canonical receipt 的定义或 CPU/GPU 完成边界。

下表为每组三轮 navigation CPU 合计中位数，比较几何实现前后的匹配诊断构建。
它不是物理显示/GPU 执行时间，也不是所有场景都会获益的因果基准。单位 ms。

| 场景 | 选中 | cold 前 → 后 | warm 前 → 后 |
| --- | --- | ---: | ---: |
| 长线 | 否 | 80.1 → 70.8 | 41.5 → 42.9 |
| 长线 | 是 | 87.4 → 68.6 | 40.4 → 38.0 |
| 图片＋长线 | 否 | 76.7 → 74.8 | 41.7 → 40.9 |
| 图片＋长线 | 是 | 83.4 → 80.3 | 45.3 → 38.3 |
| 表格＋长线 | 否 | 73.9 → 74.4 | 36.4 → 35.2 |
| 表格＋长线 | 是 | 81.1 → 79.3 | 33.3 → 41.0 |
| 便签＋长线 | 否 | 74.0 → 75.1 | 36.8 → 38.9 |
| 便签＋长线 | 是 | 81.2 → 85.7 | 51.1 → 38.4 |

每个 Runtime cold 构建两份共享几何，warm 构建 0，拒绝 0；模型化 peak footprint
为 **217646 bytes**。五个 warm 组减少 CPU 工作，长线未选中、表格选中、便签
未选中 warm 及部分 cold 组仍有上升，不能宣称普遍加速。warm 仍构建 **16 个 tile**；局部几何没有解决全部
tile residency 问题。旧 `knots` 计数继续代表逻辑进度，不能把它当作缓存 v4 的实际
编译访问数；新增 primitive 计数才表示本次局部 draw 工作。

另以 `vector-solid-v1@1` 经根 SDK 独立执行：两种选择状态、cold/warm、ON/OFF，
8 段导航；**6/6** initial/cold-end/warm-end 像素 SHA 相同。cold 两份 prepared path、
warm 无重建、拒绝 0，且真实记录到 `vector_prepared` 使用次数。它是独立功能
证据，不用 round-handdrawn 的计时替代 vector 性能结论。

## 全仓回归边界

尝试完整构建并执行配置中的 **1066 项 CTest：998 通过，68 失败或未运行**。
五个测试目标存在未覆盖枚举或缺少 initializer 的编译失败；另有既有 Semantic
allocation/authority、G3 snapshot、legacy playground、text、qualification 等失败。
全部名称、状态与原始输出保存在 `regression-comparison.json`、`full-ctest.log.gz`，
编译失败目标在 `full-build.log.gz`。

没有直接把这些失败标为“历史问题”：使用 `git archive 89a24b51` 的精确源码，在
受控工作树的 ignored out 目录内，以相同 Debug flags 和锁定 SDK 构建并复跑这
68 项；**68 个名称全部复现，新增失败名称为 0**。五项编译失败也在该基线复现。
`baseline-comparison.json`、`baseline-build.log.gz`、`baseline-ctest.log.gz` 提供证据。
这是失败名称对照，不给未完成的其他能力补发 PASS；G3 authority 等边界未修改。

Runtime 边界、workspace、whitespace 校验通过。默认 Web consumer install 单独
检查 36 个文件，无 private archive/header、probe 或 qualification 文件；安装后
实际 SDK Demo 完成画线和 wheel navigation，见 `demo/`。Semantic/Skia 均使用
锁定预构建包；没有重建这些依赖。

## 复现与数据

```sh
# 匹配 Release SDK；先跑 ON，再单独跑 OFF，使用新的空目录。
node verification/runners/g5/navigation_sampling.mjs out/web-sampling/sdk/web out/run-on 3
AXIOM_EXPECT_TIMING=0 node verification/runners/g5/navigation_sampling.mjs out/web-sampling-off/sdk/web out/run-off 3
# vector 独立根 SDK 场景；OFF 时另设 AXIOM_EXPECT_TIMING=0。
AXIOM_BRUSH_PROFILE=vector-solid-v1 AXIOM_BRUSH_REVISION=1 node verification/runners/g5/navigation_sampling.mjs out/web-sampling/sdk/web out/vector-on 1 long-line
# 私有实际 GPU oracle，编译配置见保存的日志/原有 g5/native CMake。
node verification/runners/g5/prepared_gpu_probe.mjs out/web-prepared-probe out/gpu.json
```

本目录保留 plain summary 和 gzip 原始文件，SHA256 校验清单不包含自身。
可直接从压缩副本重算：

```sh
python3.12 verification/evidence/performance/g5-web-cold-warm/analyze.py verification/evidence/performance/g5-prepared-geometry/diagnostic verification/evidence/performance/g5-prepared-geometry/default out/prepared-recheck
python3.12 verification/evidence/performance/g5-prepared-geometry/analyze.py verification/evidence/performance/g5-exact-progress verification/evidence/performance/g5-prepared-geometry/diagnostic verification/evidence/performance/g5-prepared-geometry/default verification/evidence/performance/g5-prepared-geometry/vector-diagnostic verification/evidence/performance/g5-prepared-geometry/vector-default out/prepared-recheck
```

后续仍需稳定 chunk identity/index、Scene 发布复制成本、更多复杂对象、统一
Damage/Tile/Scheduler/ResourceBudget 及 100K/1M 矩阵。本轮不把这些标为已完成。
