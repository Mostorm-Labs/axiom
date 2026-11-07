# G5 统一渲染执行补充契约

设计提案，修订3，基线a2d99bfd。与[主蓝图](G5_UNIFIED_RENDER_EXECUTION_BLUEPRINT_2026_11_08_R3.md)、[任务清单](G5_UNIFIED_RENDER_EXECUTION_2026_11_08_R3.json)配套。以下补足迁移、时序和测量约束；不表示实现已完成。

修订3为设计增补。已派发UR0a仍执行修订2冻结包；不替换其来源锁、不新增本轮完成门槛、不启动UR0b或生产实现。新增观测建议在UR0a回收时核对，在后续实施包中冻结。

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

## R3-1：较新帧替代已取消作业的反馈

由现有Runtime/Interaction管理有界的sealed交接记录。A、B都已提交、A绘制作业被取消时，不要求一个不会再出现的A-completion。首版只接受完整exact canonical替换：

1. 准备帧B时，Runtime从已同步的权威Scene冻结该帧可替换的sealed记录集合，绑定Runtime frame/basis、document/view/lifetime、gesture generation、OperationId、CanonicalCommitStamp、Scene及surface/metrics身份。成功提交后再关联既有captureCanonicalWork产生的完整CanonicalWork；关联时不能重新扩大名单。commit ordinal与semantic generation不能混用。
2. B通过既有严格完成校验后，仅生成B自己的合格receipt。Runtime将名单内A记为`ReplacedByCurrentFrame(B)`，退休A反馈；不补造A完成、A的CanonicalVisible或durability。
3. Undo/删除使A效果已不存在时，当前完整投影可以替代A旧显示，原因记为`SupersededByCurrentState`。不能仅比较revision大小，也不能要求当前帧仍含A的mask。
4. completion前若出现活动或sealed C，只退休B冻结名单中的记录，不能全局clear把C清掉。反馈退休与GPU租约退休分别处理。
5. Scene恢复成功后重新生成证明；跨document/lifetime或surface/metrics换代使旧证明失效。不能以旧像素、LOD、未知completion或局部覆盖代替完整exact证明；区域替代需后续单独设计，不纳入首版。

复用CanvasEditingRuntime严格presented验证入口，不放宽RuntimeFrameCoordinator::retirePreview的精确身份匹配。名单只是交接元数据，不新增文档/History owner。隐藏、离屏、owner退出、context loss导致的显示需求取消与“已呈现替代”分别记录，不伪造可见事实；必要的失效/最终清理路径不得遗留永久overlay、waiter或租约。

UR2前冻结证明字段与oracle。必测：A作业取消仅B完成；A/B提交后Undo A；删除；Scene恢复后只出当前帧；context换代；B提交后C活动；新帧不覆盖A/不是exact；跨document或对象incarnation拒绝。已有A-completion测试不能替代此组。

## R3-2：GPU队列与完成观测

沿既有GpuResourceRetirement、SkiaGpuLifetime与GpuCompletionService扩展私有观测，保持owner-thread、非阻塞、非重入；优先复用完成callback。不占canonical completion槽，不新增公开GPU API/receipt，不使用同步finish/readback制造完成数据，不重建锁定Skia SDK。

- 分开记录CPU排队、recorded未submit、submitted未观测完成，以及已完成但等待有序资源退休。未使用ticket不算GPU提交；group/ticket不等于GPU指令数，按context分组。
- 记录purpose、frame/job/attempt、context/surface generation、单调submit与完成观测时间、未完成group数量、最老年龄、submit至完成观测延迟、poll间隔、abandon/loss/unknown。
- 完成观测延迟含GPU排队与poll调度，不能称纯GPU执行时间或物理显示延迟。能力缺失标unsupported/unknown，submit不能代替complete；abandon与physical loss不算成功完成。
- 停止输入后检查必要工作、staging/pending-release的收敛；有效驻留缓存、WASM堆及RSS不要求归零。资源释放仍需真实退休依据。
- 内存额度不能单独限制复用同一target的排队量。UR1增加私有队列准入与可解释Pending：优先service完成、限制预热/可替代未提交需求、保证必要canonical公平进展；禁止忙等、无限重试、丢terminal或撤销已接受Operation。

具体高低水位、年龄/延迟界限和观测开销由UR0c按设备/负载冻结，本修订不给虚构数值。Axiom实际GPU target/leaf可证明显式路径与Raster回退；不能证明Skia内部没有软件coverage或CPU准备。缺独立证据时保留UNKNOWN，不把目标类型升级为“全GPU”结论。

## R3-3：临时编码与缓存命中前工作量

UR1明确消除完整transient reference列表中“先canonical编码/摘要、随后clear”的工作。普通无提交拖动Move中，属于该临时列表的discarded encoding bytes及digest工作应为零。真正canonical/Operation/Snapshot编码、导出、SDK事件及并行待完成canonical按purpose单列，不要求进程总encoded_bytes为零。

应建立完整transient source的无编码构建模式；不能直接借用带局部筛选语义的buildNavigation替代，也不能删必要身份校验。现有canonical计时根不覆盖所有transient调用，零计数不能证明没有编码。

UR0a登记缺口，UR0b补低成本计数：reference build/encode/digest时间与字节、lookup候选数/时间、pointer-fast-hit、完整验证/hash输入字节、深比较次数及元素、metadata/mask/source实际复制字节、geometry build/replay点数。标明inclusive/exclusive，不能为了计数再扫描内容。

现有ImmutableValue等值先比共享指针，仅不同allocation才深比较；不能笼统声称每次命中都复制整笔画。UR1依据这些计数处理有证据的重复工作：驻留且具备不变性证明的body，不应随副本乘帧重复全量扫描。首次准入、内容变化、碰撞及无可信证明时仍需完整验证。hash仅索引，相同ObjectId、长度、裸指针或旧digest都不是内容相等的充分条件。

## R3-4：共享本体后的分叉隔离

UR1/UR2 required tests同时校验共享收益与独立结果：

| 分叉 | 必须证明 |
| --- | --- |
| A/B共享body，仅A增加永久/活动mask | B权威记录、像素、exact hit及mask历史不变 |
| 仅A平移/旋转/缩放 | geometry可共享，A像素重新证明；B有效热像素不受污染 |
| A/B有不同mask历史，Undo/Redo A | A正确reset/rebuild，B不变，不能以revision递增推断prefix |
| 删除或驱逐A，B仍pin body | B绘制/查询有效、不重复计费；最后消费者退出且真实GPU退休后额度收敛 |
| 不同document/lifetime使用相同ObjectId | 不串mask/pixels/resume/feedback/receipt；保持当前Runtime/Renderer私有范围 |
| 不同allocation同内容与同ID内容变更 | 同内容可复用，knot/config变化使body分叉；mask同长度异值只重判/reset mask结果、保留body；旧surface禁止复用device pixels、CPU geometry按依赖保留 |

独立oracle像素、exact query与权威记录一起验证，不能仅看build-count。无需为测试引入全局跨document缓存；未来若做内容寻址共享，仍需完整内容/资源证明和独立对象租约。现有owner-thread mutable replay scratch约束继续有效。

## 修订3的实施包映射

UR0a原包和修订2来源保持字节不变。R3-2/R3-3仅作为当前observation gaps的补充阅读，不新增本轮阻断要求；未覆盖项在控制面回收时登记，不追溯判原包失败。UR0b负责新增观测，UR0c冻结队列与开销门槛。UR1承接队列准入、丢弃编码消除、命中前工作量和共享基础隔离；UR2承接新帧替代反馈及mask/history/lifetime分叉；UR5验收连续操作后的队列与反馈收敛。

修订清单见 [amendment.json](g5-unified-render/design-r3/amendment.json)。它是设计映射，不是新的执行授权；下一实施包须重新绑定此修订的来源hash，不能静默改写正在使用的锁。
