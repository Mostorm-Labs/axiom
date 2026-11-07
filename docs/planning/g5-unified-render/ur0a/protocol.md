# UR0a 观测协议 v1

状态：UR0a 静态交付，待控制面审查。任务包 `.aegis/packages/G5-UR0a/package.json`，起点 `a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e`。本文件固定私有验证数据口径，不增加 SDK API，不表示埋点、runner 或迁移已经完成。矩阵所有新动态观测均为 MISSING。

## 1. 所有权与不可混用的状态

公开 Web root SDK / 原生适配器 → RuntimeComposition → Input/Interaction/Ink → OperationEngine → Scene → FramePlan → SkiaRenderer → Provider 仍是主链。Input、commit、Scene 发布、有效 submit、GPU 完成、平台 qualified completion、物理显示和外部 durability 分别记录。Runtime 持有手势、History 和反馈交接；Render 持有派生资源；Provider 回报完成。观测层不分配文档/Operation 身份，不修补拒绝语义。

`routes.json` 里的 MIGRATE 是后续待办，不是已迁移。UR0b 可以记录 **CURRENT_BRANCH** 的实际决定及 leaf；只有未来有真实统一入口时才可记录 **UNIFIED_POLICY**。给旧 helper 加诊断 ID 不能证明已经经过统一策略。partial erase 当前没有 Brush ARC contour token；使用诊断 interaction 身份与真实 commit/basis 关联，不能编造 ARC token。

## 2. 时钟

- 单次浏览器实例使用 `performance.now()` 单调域；保存 clock_id、timeOrigin 元数据、实测分辨率与量化方法。时间用相对 run origin 的十进制整数 ns 字符串表示，`round_half_up(ms × 10^6)`；单位为 ns 不代表 ns 精度。Date.now/Node wall clock 仅作日志定位，不参与延迟相减。
- 输入内容时间 `PointerEvent.timeStamp` / normalized timestampNs 与计划/实际投递时钟分别存放。合成夹具可固定内容时间，不能据此声称真正绘制了 30 秒。
- 现有 C++ steady_clock span 可保留本地 inclusive/exclusive duration。没有校准时，绝对 start_ns 不得与浏览器时钟相减。UR0b 跨域校准采用 16 次 browser-before/WASM-clock/browser-after 括号采样，选最窄往返区间求 offset，保留半区间误差；run 前后各一次。误差上界超过 1ms 或前后 offset 区间不相交时，跨域时间线标 MISSING，只报告本地域 duration；不把误差算成产品排队。
- C++ span 若以显式 browser monotonic tap 取时，也须记录方法与成本。嵌套树用 span_id/parent_id；inclusive 包含子项、exclusive 扣除直接子项。总 CPU 只求不重叠 exclusive 或选定根，不累加全部 inclusive。多线程将来单列线程，不把并行 CPU 和当作 wall time。

## 3. 身份与丢样

每个 run 绑定源码/runner/fixture/完整 SDK JS、WASM、资源清单与 BuildInfo、预算、浏览器/GPU、CSS 尺寸、DPR、实际 target/capability、缓存起点、其他 GPU 负载和时钟。artifact 前后 SHA 不同即 evidence invalid。

每个计划事件在投递前已有 run_id、event_id、batch_id、interaction_id、sample IDs、phase 和 planned_ns。诊断 interaction_id 只在 run 内关联，不是新增 SDK token；真实 pointer/source/generation 从 Runtime 观测，缺失写 null/MISSING，不能拿 native pointer ID 代替 generation。

内部链计划关联 document/view/lifetime、interaction/contact、operation/commit revision、committed/Scene/presented generation、frame、surface/metrics/resource generation、job_id/attempt_id/decision_id/leaf_id、body/mask/preview revision。获取不到的身份不填默认 0。每个 attempt 恰有一个终态；Pending 是 attempt 结束而 job 可继续。job 最终须 Completed/Cancelled/Stale/Failed，或在截止时记录右删失。query/resume/GPU 各自 pin 的 lease 与 owner/generation/真实退休关联，逻辑驱逐不等于释放显存。

关键身份、terminal、decision、leaf 丢样，未知 route 或身份不匹配会阻断该行覆盖结论；普通统计丢样报告 observed/expected/dropped，不能声明“全部无 CPU fallback”。COVERED 必须来自相应精确身份的动态链；本次静态检查不赋予 COVERED。

## 4. 分段与窗口

| metric_id | 起点 → 终点 / 分母 |
| --- | --- |
| INPUT_QUEUE | 原计划 planned_ns → SDK 实际输入 handler enter；包含迟发、事件循环与送达等待，不推断 OS 延迟 |
| PRODUCER_LATE | 原计划 → producer 实际调用 dispatch 的时间；与 INPUT_QUEUE 分开，不能相加重复计费 |
| DISPATCH_DOWN / MOVE / UP / CANCEL | handler enter → finally return/throw；返回 false/抛错也有样本 |
| NEXT_DOWN_ADMISSION | 下一 Down handler enter → Runtime admission 已知结果；不能用 handler return 冒充已取得 Scene 可用性 |
| NEXT_DOWN_TO_FEEDBACK | 下一 Down 原计划 / enter 分别 → 首个反映本次需求的有效 submit |
| UP_TO_COMMIT | Up handler enter → OperationEngine 已发布真实 commit；no-op 不产生该样本，另计 no-op 数 |
| COMMIT_TO_SCENE | commit → Scene 发布符合该 committed generation 的投影；失败/恢复分别记录 |
| SCENE_TO_SUBMIT | 可用 Scene → 反映精确 basis 的有效 canonical submit |
| UP_TO_CANONICAL / UP_RETURN_TO_CANONICAL / COMMIT_TO_CANONICAL | Up enter / handler exit / commit 分别 → 匹配身份的 qualified canonical completion |
| FIRST_FEEDBACK | 需求首次进入 Runtime → 首个匹配 revision/pose、basis 和目标代的有效 submit |
| EFFECTIVE_SUBMIT_GAP | 有未满足需求的窗口内，相邻有效 submit；包含窗口开端到首个有效 submit、末次到截止的右删失段 |
| RAF_INTERVAL | 相邻 RAF 回调；仅为事件循环代理，不代替 submit 或显示 |
| QUIESCENCE | 最后计划 terminal 后 → 必要 work 归零或明确终态；独立于 canonical completion |

需求窗口从一次被 admission 的反馈/视口需求开始，到该需求被有效 submit 满足、被后续可替换需求 supersede、被明确 cancel/reject 或截止。连续需求按同 view/purpose 合并 busy interval；空闲无需求时间剔除。重复旧 pose、空 clear、其他 context draw、过期 frame 均不能缩短 gap。pixel hit 直接合成可以是有效 submit，但必须证明内容、pose、fidelity 和区域有效性；仅一个 GL draw 调用不够。批次同时覆盖多个 sample 时，共享 submit 时间保留各自 event/sample 关联，不虚增 submit 数。

navigation **没有 canonical receipt**。其有效反馈 submit 用 view/camera revision、Scene/source/区域与 surface basis 资格；没有物理呈现传感器时标 `submit_proxy`。资源退休 fence 即使与 navigation 同时发生也不是 navigation canonical completion。持续导航后真正 canonical 恢复若发生，单列其 canonical interaction，不转贴为导航 receipt。

canonical qualified completion 要求 operation/commit、Scene/frame、document/view/lifetime、surface/metrics/resource basis 与该需求匹配；旧 completion 可退休原 job 的资源，不能确认新 frame 或清掉 B 的 feedback。A-Up→B-Down→A-completion 必须分别记 A/B。Brush ARC 与 partial replacement 分开；partial 的 sealed/awaiting-replacement 仍是 UR2 待实现状态，不按现有功能填写。

## 5. 失败、删失、分位数

- 固定 nearest-rank：排序 N 个已完成样本，q(p)=x[max(1,ceil(pN))]，1-based；p50/p95/p99/max，N=0 全部 null。每次独立 run 分别报告，不把 3 次合并成一个更好看的 p95。旧 runner 的 floor(pN) 统计保留原名和旧数值，不能回写或直接当作本协议分位数。
- 分母是计划事件/需求数，分别给 planned、arrived、admitted/rejected、completed、censored、missing。拒绝时 dispatch 可完整，但预期反馈指标为 failure，不能被零延迟或成功样本替代。Up 返回错误而 generation/Operation 已变化须标 POST_COMMIT_ERROR，不能声称“拒绝故未提交”。
- 截止仍未有效 submit/completion：保存截至时刻、start、右删失 lower_bound_ns；不补 0、NaN 或假完成。handler 被 worker 杀死：dispatch 截止下界也需记录。身份/关键日志缺失属于 MISSING，而非可解释的右删失。
- 完成样本的分位数必须带 `complete_samples_only`，成功率、删失数和失败率同列。若有失败/删失，overall tail 结论为 INCOMPLETE；可报告删失下界，不能挑选三次成功来覆盖失败。
- no-op、cancel/source-lost、stale GPU retirement 各单列；no-op 只能引用既有 commit basis，无新 Operation。正确性、evidence completeness、performance 是三个独立轴；可靠 RED 可以驱动优化，慢基线不阻止进入下一包。33ms 是已认可方向，50/100ms 等产品门槛仍待 UR0c；本协议不签发产品 PASS。

## 6. 内容时间、真实投递和“立即”

保留旧锚点：`drag_copies.mjs`、`cumulative_erase.mjs` 的 7200 样本由 4ms 内容步长生成，首尾实际跨度 **28.796s**，并非严格 30s；setup 每 16 点等 RAF，拖动每 Move 等 RAF，擦除逐轮等 quiescence。这些原行为和统计不得改名为实时/立即证据。

新 fixture `fixtures/continuous-30s-v1.json` 是字面输入 authority：7200 点，首尾内容跨度恰好 30,000,000,000ns，整数 1/1000 逻辑坐标，固定压力输入/availability、phase 与 seed。定义在 matrix 中；字节 hash 唯一。它只固定输入，不重新实现 BrushEngine。

新实时投递由独立预排的 60/120/240Hz 任务边界产生，不等待 RAF、receipt、settle 或 quiescence。按内容时间窗口分组 Move（60Hz 最多 4、120Hz 最多 2、240Hz 最多 1 个；边界 Down/Up 单独 dispatch，最后不足一组照送），末个已分配 sample 的内容时间是批次计划发送点。名义频率与边界短批次均保留，绝不为凑固定组数丢 confirmed。Move 的 getCoalescedEvents 路径在当前 collector 中可用；其后仍逐 fact 同步提交，不假装 C++ 一次接收整批。

setup 可经公开 SDK 加速建原笔画和复制；记录 mode=accelerated_setup，不计为实时绘制性能。实时校准另列。32 条独立实时笔画需 ≥960s 内容投递，不能塞入 180s worker；这类未来 UR5 行 DEFERRED，须独立有界准备或另获 setup 额度。

“立即”采用 **提前排定** 的 B 输入：B-Down/第一 wheel 的 planned = A-Up planned + 8,333,333ns（假设刷新周期 16,666,667ns，必须记录实测刷新周期；计划间隔超过一个实测周期则该“立即”条件不合格）。A1-Up→A2-Down 同样如此。B 不等待 A 返回结果、完成 receipt 或安静条件；单线程阻塞令实际 B 迟到时保留原 planned，排队照计。后台 producer 不得动态滑动计划来隐藏阻塞。

迟到 producer 按原顺序送到期批次，最多 8 个批次后让出一个 task；不丢 confirmed，不合并 Down/Up/Cancel/source-lost，不依 RAF 降低负载。catch-up 批次逐条记 original planned/actual/late/backlog。预测尾只用于 preview，可按既有策略替换，并且不能进入 seal/Operation。

三条新分支从相同合格初始 16 对象 fixture 启动，先做 E0→立即 E1，再在 E1-Up 后立即 E2 / navigation / masked-drag；仅分支间可 reset。无首次目标/Scene stale/Down reject 也保留结果。settled 对照另有 row_id，不能在立即序列中暗等。metadata 或截图、export、readback 均在 timed window 之外，不以清晰读回耗时污染交互延迟。

## 7. 观测开销资格（不是产品性能门槛）

UR0b 准备同一 diagnostic-capable binary 的资格私有 toggle：OFF 为必要外层计划/handler wrapper，ON 在同一 wrapper 上增加内部有界 ring/计数；不新增公开 SDK 调试 API。二者都关 profiler/readback/热路径字符串序列化。编译 ON/OFF 的两个不同 artifact 不能冒充同二进制开关配对。每模式保留 artifact/fixture/budget、丢样和最终文档检查。

冻结 6 个独立配对，3 个 AB、3 个 BA，顺序 `[AB,BA,BA,AB,AB,BA]`；每对 reset 相同 fixture，串行，无其他 build/性能采样。共同可比量为外层 dispatch p95、测量 wall duration、终态/文档一致性；只有 ON 才具备的内部阶段不能与 OFF 的“0”比较。最小 wrapper 自身成本仍是未测共同基底，不能宣称零开销。

每对算 ON−OFF，配对 median 增量：dispatch p95 不超过 max(1ms, OFF median×5%)；测量 wall 不超过 max(20ms, OFF median×5%)。这些是采集器可用性界限，不能成为产品延迟放宽值。OFF MAD 同时大于 1ms 和 median×10% 时标 NOISY；任何关键事件丢失均不合格。一次诊断重测必须有具体噪声原因、单列旧结果，不能无限重试直到通过；超限或无法判定时 UR0c 不采用该套主要延迟结论。

## 8. 有界执行与可复核性

matrix 冻结每行 setup/measurement/cleanup deadline、总 worker 180s/3GiB 进程族 RSS、重复数和整批预算。RSS 是 harness 保护而非 Renderer/GPU 预算，不要求 WASM/RSS 在释放后必然回落。触及保护即保留原始数据/部分终态、清理并停止该档；不借提高预算或换机器偷偷补成功。

每 attempt 结束一次；归档 source/package/config/runner/完整 artifact hashes、原计划与实际轨迹、拒绝/删失、job/decision/leaf/target/lease 对账、日志丢样、截图/像素 oracle 结果（独立于计时）。COVERED 资格需独立 CPU exact / 几何 AA oracle；reference 不得读 candidate cache。现有独立能力与缺口见 review.md。UR0a 仅运行静态校验与纯数据 schema 样例，停止于 READY_FOR_CONTROL_REVIEW，不启动 UR0b。
