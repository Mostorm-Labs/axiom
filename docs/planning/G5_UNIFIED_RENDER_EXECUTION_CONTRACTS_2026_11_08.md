# G5 统一渲染执行补充契约

设计提案，修订2，基线a2d99bfd。与[主蓝图](G5_UNIFIED_RENDER_EXECUTION_BLUEPRINT_2026_11_08.md)、[任务清单](G5_UNIFIED_RENDER_EXECUTION_2026_11_08.json)配套。以下补足迁移、时序和测量约束；不表示实现已完成。

## UR0 的启动与退出

UR0分三步：
1. UR0a在采样前固定事件/时钟/窗口/工作负载/预算/超时/失败规则及必需行。
2. UR0b仅补诊断与runner，复现基线；性能优化不混进基线构建。
3. UR0c审查证据完整性、归因和测量开销，固定UR1之后各包的性能门槛。

UR0可在性能未达标时完成：结果明确记录BASELINE_READY与PERFORMANCE_UNMET，不能要求先达到33ms才允许修复。测量漏项、身份不可信或必要正确性oracle不可靠才阻断对应结论。基线正确性缺陷可以可靠记录为RED并先修复，但不能为错误输出签发性能达标。

33ms方向保持；50/100ms等新建议值在优化前审查固定，调整须说明适用设备/负载与理由。旧150ms证据保持原口径，不回写为新标准。本文不授予默认预算或公开SDK契约变更。

## 逐入口迁移登记

UR0冻结routes登记，至少有：
route_id、public_entry、callsite_and_branch、owner、classification、target_package、
allowed_outcomes、job_decision_leaf_identity、positive_case、negative_case、evidence_status。

classification取MIGRATE、KEEP_ORACLE、KEEP_LEGACY、KEEP_NON_INK、DEFERRED。
证据状态取COVERED、MISSING、DEFERRED；保持分类与已验证状态分离。
下列是必查种子，不是完整发现结果：

| 种子 | 必查分支 | 首次检查位置 |
| --- | --- | --- |
| canonical | cooperative prepare和Direct；冷/热/准入拒绝 | skia_renderer、skia_render_cache |
| transient edit | 普通拖动、带永久mask变换、creation的ink二次绘制 | skia_renderer、skia_headless_backend |
| partial erase | 活动反馈、damage hit/miss、seal后替换 | skia_render_cache、canvas_partial_erase |
| stroke preview | stable prefix、mutable/predicted tail、大视口重构 | skia_renderer |
| navigation | prepared/未prepared、粗层/细层、冷区、压力恢复 | skia_navigation_tiles |
| resource recovery | 无GPU、target创建失败、context lost/rebind | coverage targets、surface provider |
| retained compatibility | CPU export/oracle、旧算法、非ink绘制 | 对应既有入口 |

每条MIGRATE既须由公开SDK/真实输入入口触达，也须有同一interaction/frame/job到实际leaf的证据。leaf执行前应有匹配decision；仅证明GPU执行不能证明已走统一入口。资格构建中缺失decision、未知route或generation不符要失败；正常构建返回既定安全错误/恢复结果，不把测试assert变成产品崩溃。

静态双向检查覆盖入口→leaf和Raster/GPU target创建点→调用者。新增callsite需要登记。KEEP路径保留原因和独立测试；DEFERRED不能计入“已接通”。UR5逐行闭合，不用聚合GPU比例代替。

迁移按消费者逐条切换，保留独立reference。对照/双跑只在资格构建执行，不能污染主要延迟结果。rollback恢复前一已验证策略与版本匹配缓存，不能保留不兼容resume状态。每包定义差异与回退提交。

## 合成、damage 和 oracle

统一策略必须明确以下执行资格：

| 配置 | 要求 |
| --- | --- |
| destination-independent SrcOver | isolation范围、opacity应用次数、premultiplied alpha、颜色格式/空间固定 |
| destination-clear / backdrop-dependent | 保留语义作用范围，不一律改为隔离后SrcOver；共享本体不授权改变erase对象范围 |
| 半透明自交与叠加 | 使用同一geometry/material定义，防止body/mask/合成层重复乘opacity |
| 移动中间z序对象 | 静态上下有序段均保留；不能把活动对象画到最顶层 |
| 局部更新 | 覆盖旧bounds和新bounds及AA/filter/mask guard；保持coverage grid，处理对象删除和新露出区域 |
| 无retained-contents的provider | 使用完整替换，不假设上一帧buffer内容仍有效 |

对外可见的更新必须属于一个一致的frame basis。多瓦片增量可以先在离屏准备；如果partial present获准，必须有明确区域有效性证明，不能把不同mask/Scene代的区域拼成假完整canonical。分配失败不能把半完成scratch发布为有效缓存。

CPU严格字节oracle与GPU现有geometry-AA容差分别保留。reference不能使用待测实现生成的prepared/cache pixels；AA带按几何定义，不根据差异图扩张。导航LOD只验证其质量与覆盖契约，不伪装精确像素。

最小像素组：下层半透明对象拖动、永久+活动mask、透明/彩色背景、warm→miss→pressure、DPR切换、feedback→canonical。复用已有独立用例，补足缺失组合。

## 提交、Scene 与反馈交接

统一观测committedGeneration、sceneGeneration、presentedGeneration。依赖命中查询的新手势只有在Scene追上提交代或有证明的最新投影时才能开始。Scene恢复失败时按现有admission返回明确结果，不查询旧mask，不凭旧显示像素推断新状态可用。

UR2须为“已提交但Scene尚未同步”固定Down、导航、Undo/Redo的行为。保留显示、输入接收、语义完成是分别判断的状态。移除下一Down前置present之前，先证明Scene恢复不再依赖这次present才能触发。

局部擦除replacement反馈与Brush ARC contour不能假设使用同一现成handoff：
- 当前partial feedback依赖活动trace；Up清trace，partial begin不创建ARC contour。
- 目标设计须分别标识ActiveFeedback、SealedAwaitingReplacement、Retired。
- 现有Runtime/Interaction持有手势与sealed交接状态；Render只消费有完整身份的不可变需求并保留派生资源；Provider回报完成，不新增SDK token。
- sealed反馈绑定完整document/view/lifetime、gesture/commit、surface/metrics及Scene/mask basis。
- A-Up→B-Down→A-completion时，旧完成只能退休其覆盖的A，不能清掉B。
- no-op不产生新Operation；使用既有已提交basis证明后退休反馈。
- cancel/rejected Up按未提交终止处理；不能把已接受commit当作cancel撤回。
- 新feedback覆盖旧sealed反馈时需保持正确顺序与basis，不重复擦除/重复透明度合成。

提交线性化也有一项现存缺口：eraserFinish在OperationEngine apply之后调用history.record，后者仍可能分配失败。既有EditorHistory已有prepareRecord/commitPrepared，应优先复用这套契约：
- commit前预留publication custody及History所需的可失败准备；不提前修改History cursor。
- 失败释放预留，不改变canonical。
- apply成功后的Runtime内部必需History/publication finalization不再执行可失败分配；不把已提交结果报告为未提交。这不承诺外部observer回调完全不分配。
- UR2固定History finalization与operation observer顺序，审查observeApplied中的commit-record复制、selection更新及SDK无DataPort时编码；observer失败不反转已提交结果。
- Scene/GPU失败进入派生恢复，保持恰好一次Operation及可用Undo。
- 审查publication、History、semantic apply之间的全部return/throw路径，不引入第二套事务owner。
此项纳入UR2独立小包，故障注入区分prepare失败与commit后派生失败。

必测时序：A-Up→B-Down→A-completion；A-Up→立即导航；commit→未呈现→Undo→Redo→旧completion；Scene失败→立即再擦；no-op、cancel、source lost与pointer ID重用。

## 作业取消、租约与线程

最低作业状态：Queued、CpuRunning、Prepared、GpuSubmitted、Completed、Cancelled/Stale。
每个job有唯一身份和attempt编号，resume绑定精确内容/资源basis及消费进度。

| 事件 | 未提交GPU的派生工作 | 已提交GPU的工作 |
| --- | --- | --- |
| camera/preview更新 | 取消过时输出，保留仍有效body/区域/checkpoint | 结果不得伪装新revision；等待实际退休 |
| 新Operation或Undo/Redo | 依受影响content/mask失效，不撤销已接受Operation | 校验原身份，退休原job租约，不更新当前结果；仍有效共享body/checkpoint可保留 |
| resize/rebind/context loss | 废弃旧target资格，CPU body按依赖保留 | 旧代completion不得确认新代；按provider/driver生命周期退休 |
| source lost/cancel/close | terminal按现有输入owner处理；取消未提交派生需求 | 保留必要退休记录，不回调已销毁Runtime/provider |

任何派生job（包括旧canonical draw）均可因basis过时停止发布；这不合并或撤销已接受Operation，不替新revision签发receipt，仍有效不可变body/checkpoint可以继续复用。

latest-wins只用于可替代的preview/navigation需求；confirmed输入样本、Up/Cancel/source-lost和已接受Operation不能被渲染调度合并丢弃。活动手势中的Undo/Redo继承现行Interaction策略；如需改变，先定义并验证，不在renderer自行仲裁。

现有PreparedNavigationGeometry虽通过shared_ptr<const>访问，仍有owner-thread mutable replayOutput与非原子usage计费。当前阶段按owner线程使用；需要worker时先分开不可变source/checkpoint和每job/consumer独占scratch，明确cache发布、usage更新与最终析构线程。不能直接把现有对象并发交给exact worker。

Query、resume和submit各自pin所需lease；cache驱逐只撤销可发现性，不能使使用中的geometry悬空。worker交付不可变结果及完整basis，由Runtime owner验证后发布。GPU完成回调只持安全的quota/retirement状态，不捕获已销毁Runtime/provider。

budget owner更换、document/view close、context换代或额度调低时，旧inflight仍归原retirement记录并计入聚合额度。新manager不能把旧资源变成账外内存。额度调低不强释放在途资源；阻止新增、淘汰可选资源并在真实完成后收敛。测试反复pressure→恢复、close/reopen、context换代及闲置后的live lease/bytes收敛，不要求WASM/进程RSS必然回落。

## 可测量定义与连续输入

UR0固定一个浏览器单调时钟域；WASM span保留本地duration或显式校准，不直接相减Date.now与performance.now。

| 指标 | 起止/分母 |
| --- | --- |
| 输入排队 | 计划发送时间→实际handler进入；记录原计划、实际到达、late/catch-up批次 |
| dispatch | handler进入→返回，按Down/Move/Up分别统计 |
| RAF间隔 | 相邻RAF回调；仅表示主线程调度 |
| 有效反馈提交间隔 | 有未满足输入/视口需求时，相邻反映对应revision/pose的实际submit；额外记录首次需求→首个有效submit |
| canonical恢复 | Up、dispatch结束、commit、qualified completion分别打点，按精确身份关联 |
| 下一Down | 计划下一Down→实际进入→完成admission→首个有效反馈，分段报告 |
| quiescence | 最后输入后必要work归零/明确终态；不能替代canonical完成 |

导航没有canonical completion receipt。以实际有效feedback submit观测，不制造新canonical fence/receipt。缺少实际屏幕呈现观测时标明submit proxy。被拒绝输入、timeout、无有效submit记录为失败或右删失下界，不能从尾延迟统计里消失。无需求的静止时间不计为提交空窗；空提交和旧pose重放也不能缩短指标。

30秒语义时间轴与真实wall-time投递分别定义：
- 几何夹具固定timestamps首尾跨度30秒、7200样本，保存corpus hash。
- setup可通过公开入口加速生成；它不代表30秒实时交互。
- 实时绘制用独立计划的60/120/240Hz输入及coalesced批次，记录实际延误；不能每次等RAF再发下一个点，让慢系统自动降低负载。
- 过载时不修改原计划时间，补送/批处理按既有输入契约记录；不静默丢confirmed样本。
- “立即”定义为预定Up后的下一任务机会/固定≤1个刷新周期的计划输入，不能先等settle、receipt或quiescence；settled对照另跑。

现有cumulative_erase逐轮等待quiescence，结束后再测导航；旧drag runner每轮Move后等RAF。保留这些旧基线，同时新增立即衔接和固定节奏夹具，不能重命名旧结果冒充新矩阵。

## 有界矩阵与观测资格

UR0不运行所有维度的全笛卡尔积：
- 锚点：原9对象拖动、16对象重复擦除DPR2，分别保留32/96与64/192。
- 新关键端点：16对象、2次擦除后立即再擦/导航、带mask拖动；独立分支从相同fixture起步。
- 每包smoke：覆盖它更改的route/branch、正确性和拒绝恢复，不重新跑无关全矩阵。
- 主性能cohort：UR0列明必须三次的行；UR5覆盖1/8/16/32及DPR1/2/4的冻结组合。
- 故障正确性组：scene失败、资源拒绝、退休延迟、乱序completion等单列，不混入正常p95。

所有run有setup与measurement各自deadline、内存上限、清理及失败快照。可复用现有180s/3GiB worker限制，但32条独立30秒实时生成不可能塞进180s总限制；应分离准备、保存匹配fixture或预先批准更长的有界setup配置，不能把harness超时误判产品性能。批次总预算、必测行、停止条件在运行前记录。一次硬超时/内存保护可先停止该档诊断，保留失败；不能为得到三次成功而无限重跑。

观测资格：
- 每attempt恰有一个结束结果；job可有多个Pending attempt，但最终必须有完成/取消/失败或尚未完成的明确记录。
- root决策数、leaf执行数、cache-hit绕过数、实际target分配及release可对账；未知leaf/route阻断覆盖结论。
- 嵌套span注明inclusive/exclusive，禁止累加重复计算总CPU。
- 对同一构建/配置做低成本观测开/关配对；冻结允许开销与噪声处理方式。详细profile仅作归因。
- 关键identity/terminal/decision丢样使该行证据不完整；普通统计丢样必须报告覆盖率，不能声称全路径无fallback。
- 设备、浏览器/GPU、窗口CSS尺寸、DPR、缓存状态、其他GPU负载与artifact绑定；主性能cohort运行中不读回或截图。

## 拆包补充

UR0交付routes登记、metric schema、矩阵行清单、状态/失败记录及baseline review；性能未达标可以作为下一包的明确RED。

UR1先完成执行决策/route身份/lease-owner约束，仍在既有owner线程运行；不要把多线程作为统一策略的必要前提。

UR2拆为：提交前History预备 → 精确geometry局部复用 → 增量mask与mask-only Scene → 独立Scene恢复/反馈交接 → 立即再输入端点。根据UR0归因可调整性能子项顺序，时序安全前置不能省略。

实施前每包固定设计版本、base SHA、允许文件、测试及rollback。设计版本在独立文档提交中固定，再开启源码写入窗口；实现不得在跑结果后单方修改门槛。升级统一方案不删除历史authority/evidence或自动扩大首期算法/平台范围。
