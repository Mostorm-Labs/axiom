# G5 Web 冷/热导航采样

已完成中断的长线、图片、表格、便签及选中状态采样。结果为 `MEASURED`；这是
Web 工程基线，不签发 G5/P34，也不建立其他平台或物理输入延迟结论。

执行日期：2026-11-04。合并基线：`21270d3388dd9160ea1262791b901e11875e198f`。
基线上仅增加可关闭的诊断和采样工具，没有改变绘制、缓存或调度算法。
实际执行的是 dirty source；完整源码 SHA256 见两套数据中的
`source-before.json.gz` / `source-after.json.gz`，不能只用 HEAD 代表构建身份。

## 采样边界

- Chrome `156.0.8078.80`，headless，真实 WebGL2，
  `ANGLE (Apple, ANGLE Metal Renderer: Apple M2, Unspecified Version)`。
- Release Web/WASM SDK `0.3.7`，根 SDK facade；800×500 logical，1600×1000 physical，DPR 2。
- 两条各 2048 个原始输入点的长线，`round-handdrawn@4`。
  混合场景分别增加 2048×1024 PNG、有文本的 3×3 表格、有文本的便签。
  只选中目标对象；未选中场景清空选择。
- 每组 3 个独立 Runtime；每个 Runtime 先 cold，再 warm。每段有 72 个
  RAF-paced DOM wheel 输入：放大、反向缩小、平移、反向平移，最后返回原相机。
- cold 是建好场景后的第一次导航，warm 是同 Runtime exact settle 后的相同导航。
  场景构建已经执行 exact render，因此这里不是包加载、图片解码、OS/driver 或全部
  几何缓存冷启动。图片没有叠加到表格/便签场景中；本轮是四种有界工作负载。
- timing ON、OFF 各 24 个 Runtime / 48 段导航，共 96 段，6912 个输入。
  正式计时期间没有并行构建；机器没有被独占。顺序为先 ON 后 OFF，不能把组间
  时间差解释为计时开关的因果开销或优化收益。
- 保留首帧、慢帧、全部三轮，没有剔除 warmup。像素 readback、SHA256 和截图均在
  计时窗口外。RAF 是实际回调到达间隔，不是名义 RAF 时间戳差。

## 默认构建结果

以下 p95 来自每组每状态 3×72 个 RAF 样本合并；receipt 等待是三轮中位数。
单位均为 ms。精确原始数值和尾部统计保存在 `analysis.json`。

| 场景 | 选中 | cold RAF p95 | warm RAF p95 | cold exact receipt | warm exact receipt |
| --- | --- | ---: | ---: | ---: | ---: |
| 长线 | 否 | 18.5 | 18.9 | 585.0 | 598.4 |
| 长线 | 是 | 19.0 | 18.3 | 599.6 | 599.1 |
| 图片＋长线 | 否 | 18.4 | 18.4 | 599.3 | 583.3 |
| 图片＋长线 | 是 | 18.0 | 17.4 | 584.5 | 599.5 |
| 表格＋长线 | 否 | 18.5 | 17.9 | 598.4 | 599.9 |
| 表格＋长线 | 是 | 17.9 | 17.5 | 583.4 | 583.4 |
| 便签＋长线 | 否 | 17.2 | 17.5 | 600.0 | 583.4 |
| 便签＋长线 | 是 | 18.0 | 17.7 | 583.3 | 583.4 |

默认构建的单次 RAF 最大间隔 39.1ms；输入期间默认 framebuffer GL 提交最大间隔
33.1ms。每段 72/72 输入之后都有默认 framebuffer draw call；这证明该探针观察到
提交连续性，不等于 72 个物理屏幕帧。无 PerformanceObserver `longtask` 记录。
不能把 39.1ms 丢掉，也不能由无 longtask 推导无卡顿。

停手到 exact receipt 的全部默认样本范围为 **582.7–600.6ms**。这一等待与输入期间
的 RAF 连续性分别统计；跨越 exact settle 的提交空档不混入输入期间的空档指标。
`clientWaitSync` 的 timeout 全为 0，CPU 调用最大约 0.1ms；没有 GPU timestamp，
不能由此推导 GPU 执行时间或 input-to-photon。

## 诊断揭示的工作量

以下为八组各自三轮的中位数范围；计数取完整导航阶段，包括停手后的少量导航机会。

| 指标 / 每段 | cold | warm |
| --- | ---: | ---: |
| source query | 46–48 | 1–3 |
| tile build | 46–48 | 16 |
| knot steps | 75,628–79,716 | 24,528 |
| prefix scan knots | 4,088 | 0 |
| navigation CPU 合计 ms | 73.1–87.3 | 31.0–49.0 |

热态确实复用了前缀和部分 source/tile，但仍构建 16 个 tile；不能称为完整缓存命中。
这里的 `fallback` 是 coverage resolver 使用次数，不是独立 fallback 帧数。
选中场景 overlay CPU 每段中位数为 cold 1.6–3.2ms、warm 2.3–4.4ms，已包含在
navigation CPU 中，不能再次相加。本轮没有证据显示选择框是主要耗时来源。

所有诊断阶段的 exact settle 都有 **29 次尝试：28 retry + 1 accepted**。
各组 exact attempt CPU 合计中位数 **54.3–80.6ms**，attempt 之间的累计时间间隔
中位数 **386.5–412.8ms**。后者是 attempt 计时以外的墙钟间隔，可能与 GPU 工作
重叠，不代表 GPU 空闲。Runtime 还保留既有 100ms navigation idle 门槛。
这支持优先调查逐 RAF 重试的收敛调度，尚不能单凭 CPU 时间确定完整 GPU 因果。

exact demand 在后续 28 次尝试中被复用；每段只构造/编码一次 source，编码量约
83–84KB、初始 preparation scan 4088 knots。不能把 29 次尝试误报为 29 次完整
Scene/reference 重建。CPU coverage 仍占主要已归因 CPU 工作；现有 exact demand
复用已生效。

## 完整性与复验

- 48 个 Runtime 均成功关闭；96 段检查均确认文档 generation、对象数、选择身份、
  last commit 不变，相机返回初值，输入循环中无 canonical receipt，随后收到 exact
  receipt。没有 Runtime error 或 page exception。
- ON/OFF 的 24 组配对，各比较 initial、cold-end、warm-end，**72/72 RGBA SHA256
  相同**，含 renderer、尺寸一致。截图另外建立同 fixture，initial SHA 与实测数据
  一致；四张截图均已目视检查。
- 两次运行前后 source tree SHA、全部 SDK 文件 SHA 均不变，且两套运行的 source
  SHA 一致。两套构建随后执行增量 build，均为 `ninja: no work to do`。
- 全部 exact 诊断满足 `sum(exclusive_ns) == attempt.inclusive_ns`；不把父子
  inclusive 时间重复相加。WASM 计时有约 0.1ms 量化，零值不是无工作。
- 新/既有 Node 指标测试 8/8；原生 navigation/cache/damage/reference/chunk 相关
  回归 6/6；计时嵌套/异常清理和 OFF 不求值检查通过；Runtime 边界及 workspace
  校验通过。这里没有声称运行完整 CTest 或完成 100K/1M 验收。

`build-facts.json`、`build-environment.json` 绑定实际 CMake 选项、SDK 全包哈希与
锁定 Skia/Semantic 身份。Semantic 使用离线 Store 中的已发布包；没有重建第三方。
`runtime-diagnostics.patch` 保存基线上仅有的三个 Runtime 诊断修改。
`SHA256.json` 校验本目录交付文件（不包含自身）。

## 复现

在当前工作树根目录，用匹配的完整 Release SDK，分别启用/关闭
`AXIOM_ENABLE_FRAME_TIMING`。正常 SDK 默认保持 OFF。构建必须沿用仓库
`tools/setup_build_environment.py` 和锁定依赖；实际配置见保存的 CMakeCache。

```sh
node verification/runners/g5/navigation_sampling.mjs out/web-sampling/sdk/web out/g5-web-cold-warm/diagnostic 3
AXIOM_EXPECT_TIMING=0 node verification/runners/g5/navigation_sampling.mjs out/web-sampling-off/sdk/web out/g5-web-cold-warm/default-control 3
python3.12 verification/evidence/performance/g5-web-cold-warm/analyze.py out/g5-web-cold-warm/diagnostic out/g5-web-cold-warm/default-control out/g5-web-cold-warm/analysis
```

使用空的新输出目录，避免混入旧证据。也可直接重算本目录的 gzip 原始数据：

```sh
python3.12 verification/evidence/performance/g5-web-cold-warm/analyze.py verification/evidence/performance/g5-web-cold-warm/diagnostic verification/evidence/performance/g5-web-cold-warm/default out/g5-web-cold-warm/recheck
```

每个 case 的 JSON 保存全部 RAF、GL/fence、state、receipt、逐次 timing 行；对应
console JSON 保留日志。summary 的 `MEASURED` 只说明采样及证据检查成功，未设置
产品性能门槛。`captures/` 是计时外的视觉 QA。

## 后续优先级

1. 先调查 exact coverage/preparation 在 29 次 RAF 机会间的调度：以维持输入预算、
   cancellation 和 receipt 语义为前提，减少停手后的收敛等待。
2. 再做 v4 局部 contour / prepared geometry 与 source 共享预算，重点比较冷态
   4,088 prefix knots、约 7.5–8 万 knot steps，以及热态仍有的 16 次 tile build。
   另开 `vector-solid` workload 验证 prepared path；本轮 `round-handdrawn@4`
   不直接证明 vector-solid 的性能。
3. 稳定 chunk identity/索引与 Scene 发布成本、更多复杂对象、统一资源预算及
   100K/1M 矩阵仍是后续工作。本轮 2–4 个 canonical 对象（便签包含独立文本
   对象），不外推规模性能。
