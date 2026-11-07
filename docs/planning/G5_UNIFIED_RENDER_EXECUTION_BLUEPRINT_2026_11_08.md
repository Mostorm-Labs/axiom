# G5 统一渲染执行蓝图

状态：设计提案，修订2。此文具体化工程执行规则，不替代 Semantic authority、公开 SDK 契约或 Gate 验收。UR0先固定测量协议并复现基线，再固定后续优化包的验收值；基线性能不足不阻止进入优化。
基线：a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e。
工作树：/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom。
分支：codex/geometry-navigation-integration。
设计监察：01a1b6f3-e69a-7fe2-8d9b-0222e8741f03。
实现：01a0dacd-61e3-7ac3-a2cb-ab7532f1b28c。

配套的[执行与验收补充契约](G5_UNIFIED_RENDER_EXECUTION_CONTRACTS_2026_11_08.md)规定逐入口迁移、合成oracle、提交/Scene/反馈状态、作业租约和指标定义。实施包须同时遵守，不能只依据下表的阶段名称执行。

## 目标与取舍

整合用户粘贴的侧聊方案和本会话四步优化路径。沿 Runtime → Scene → FramePlan → Skia → Provider 主链，将默认 round-handdrawn revision4 的长线复制、变换、连续擦除、立即再擦、导航、Undo/Redo 做成首个完整实现。

- 已修复的拖动缓存绕行进入回归保护。
- 几何、本体、mask复用成为统一执行契约。
- 擦除提交及再次输入作为优先的跨层纵向验收切片。
- 内存策略根据工作集、分层计费和峰值调整。
- 全路径观测先行，统一策略逐个接入现有消费者。

统一的是执行规则，不是所有任务的质量和完成语义。Preview、Navigation、Canonical保留各自身份、fidelity、优先级和receipt资格。像素缓存命中直接合成；精确查询不能消费导航LOD来决定semantic hit/no-op。图片、文字、表格使用各自合适的Skia路径，不转换成笔画coverage。

无GPU、资源压力和device loss各有显式结果。对支持的GPU v4交互，缓存miss不得隐式触发整帧同步CPU重算。保留Skia与已有owners，不增加平台笔刷算法或第二份文档/Scene。

## 基线与证据边界

[新证据](../../verification/evidence/performance/g5-live-dpr-drag/README.md)确认：
- live erase DPR/scratch错位有DPR1/2/4、透明度、mask/cancel回归。
- 合格GPU目标上的无永久mask v4平移副本复用静态栅格，仅移动源重建。
- 7200样本、1原笔画+8副本、DPR2、64/192MiB：拖动p95从1402.5ms降至23.6–24.0ms。
- 256/768MiB对照为24.2–25.0ms，此负载未显示扩大预算收益。
- 时间包含调用方RAF等待，不能解释成物理显示延迟或32条/多轮擦除验收。

尚有：transient canonical编码后清空、prepared geometry消费者不完整、永久mask合成准入差异、活动mask全轨迹重构、exact查询全轮廓准备、mask-only Scene重新物化、下一Down前置canonical等待、Direct统计漏项。

旧证据保留原结论。原生基线1020/1086与66项继承失败、4项继承编译错误必须保留精确名称，不能称全量通过。

## 所有权

| Owner | 状态与职责 | 边界 |
| --- | --- | --- |
| Interaction / Ink | 输入生命周期、BrushSession、活动编辑、精确目标请求 | 不选择底层GPU helper开关 |
| Semantic | ObjectRecord、OperationEngine、原子提交、历史关联 | 唯一canonical mutation owner |
| Scene | 派生本体、bounds、chunks、索引、mask投影 | 同generation incremental等价于full |
| Render | 执行策略、prepared geometry、coverage/mask、材质、合成、资源作业 | 私有C++能力，不新增文档/History |
| Provider / platform | context、surface generation、能力、present、完成通知 | 不解释Brush/Scene |
| SDK / Shell | 现有传输与生命周期、UI | 不暴露缓存/Skia/内部对象 |

Authority沿用AGENTS.md、manifest指向的Architecture v0.4与现行Semantic/SDK契约。改变公开admission/同步返回语义时须单独设计契约，不能作为性能修补隐式进入。

## 私有执行契约

~~~mermaid
flowchart TD
    A[Preview / 编辑反馈 / Navigation / Canonical] --> B[Render 请求与统一执行决策]
    B --> C[有效像素复用]
    B --> D[合格共享几何与局部组件]
    D --> E[GPU coverage]
    E --> F[历史 mask 与活动 mask]
    F --> G[材质与有序合成]
    C --> G
    G --> H[Provider submit 与 present]
    H --> I[现有 Runtime 校验完成身份]
    D --> J[仅精确表示可供 CPU exact query]
    J --> K[现有 Interaction / OperationEngine]
    B --> L[显式 Pending / 有界 CPU / 失败恢复]
~~~

名称是设计示意；实现须复用已有owners，不能只加一个facade而保留调用者隐式分支。

每个draw job携带：
- purpose：stroke-preview / edit-feedback / navigation / canonical / export-oracle。
- document epoch、scene/content revision、object/body identity、适用的preview revision。
- fidelity：exact、preview mutable tail或显式navigation LOD。
- object/view/device矩阵、DPR、clip、coverage grid/material origin。
- provider/device/resource generation、实际能力、材质、合成和mask。
- priority、work allowance及resume token。

决策结果必须可区分：
- ReusePixels：证明有效的像素，跳过geometry/coverage。
- PreparedGpuDraw：合格prepared geometry生成GPU coverage。
- BoundedCpuDraw：明确用途、支持范围及原因的有界CPU路径。
- Pending：可恢复准备/资源退休/回收，携带进度。
- Unsupported / ResourceUnavailable / DeviceLost / Invalid / Stale：明确失败或恢复条件。

结果还包括实际target类型、fidelity、原因与completion资格。帧可以包含多种job路径，帧级GPU=true不能隐藏CPU分支。执行/提交成功不等于present。首次完整支持范围是Web Ganesh v4，其余算法/平台按能力表记录支持或缺失证据。

## 缓存键与失效规则

| 表示 | 完整身份与证明 | 复用规则 |
| --- | --- | --- |
| immutable prepared body | 完整内容及影响几何的算法/配置/资源版本；exact/LOD分离 | 复制/transform/mask不重编译；首版可保守使用完整command equality；hash仅索引 |
| 活动笔画prefix/tail | session/generation、配置、confirmed prefix、预测尾revision、检查点 | 只保留算法证明稳定的前缀；predicted不进入seal |
| body coverage/material | body身份、实际矩阵、DPR、AA/grid/material origin、target/resource generation、合成语义 | geometry hit不等于pixel hit；变换后重新证明像素或用原几何重栅格 |
| mask结果 | body tile、精确有序历史mask、活动mask身份/revision | prefix证明后追加suffix；Undo/删除/替换使用正确basis |
| 有序合成/导航瓦片 | 区域/层级、顺序、source/mask/clip/background/resource证明、质量 | 保留有效区域；coarse不覆盖有效细节，不冒充canonical |

本体变更使相关geometry失效；transform变更保留geometry、重判device pixels；mask-only保留body和geometry；viewport变更保留world geometry；context loss废弃GPU对象并按新generation重建。资源变更按实际依赖失效，不仅看ObjectId。Undo/Redo不能依revision单调增加推断prefix。

分层表达依赖关系，不要求再建三个名为L1/L2/L3的大缓存。所有保留表示必须有owner、计费、租约和淘汰规则。

## 调度与恢复

优先当前输入反馈及新露出区域，其次当前提交画面完成，再补清晰度与prewarm。公平性必须保证持续导航不永久饿死必要canonical与资源释放。过时prewarm取消，同一有效需求的已完成局部工作保留。

预算必须约束最小工作单元；长query/轮廓编译/PathOp前检查2ms不代表调用本身有界。不可分割耗时先量化，再采用等价的分块/检查点或独立线程准备。线程不是本阶段先决条件。

擦除：
1. Move复用body与局部索引，增量活动mask，局部GPU反馈。
2. Up精确目标/no-op → publication与History可失败准备 → OperationEngine apply → 不再执行可失败分配的Runtime内部History/publication finalization → Scene。
3. mask-only Scene保留已证明未变本体/chunks；失败走既有派生恢复。
4. 新手势必须读取最新已提交状态；input、commit、presentation分别记录。
5. 取消下一Down完整present前置条件前，建立有身份绑定的保留反馈与待替换帧，证明不会显示旧mask或查询旧Scene。
6. Brush ARC retirement仍等待匹配CanonicalVisible；partial replacement反馈需建立自身有身份的sealed/awaiting-replacement生命周期，不能假设已有ARC contour token。GPU完成、commit、durability不能互代。

若Up转为排队接受，先定义admission/commit、依赖手势、cancel/source-lost契约。不可变快照上的预备结果须由Runtime owner在发布前重新验证完整basis。CPU exact查询保留独立精确oracle，不使用GPU像素近似代替。

资源状态：
- miss复用geometry，局部准备coverage。
- 可恢复压力：回收可选资源/等待真实GPU退休，保留resume和合法反馈。
- 最低配置可满足且需求稳定：必须最终前进，故障注入验证。
- 配置永久不可满足：明确ResourceUnavailable，禁止无限Retry。
- device loss：取消旧GPU任务资格，按新generation恢复。
- 保留旧显示结果不产生新canonical receipt。

## 预算与观测

预算分开描述：optional residency目标、shared hard ceiling、entry/count上限、mandatory scratch、staging/replacement、pending GPU release。共享allocation只计一次；逻辑驱逐不能提前释放未完成GPU额度；不得无条件销毁仍有效的导航覆盖。桌面更大预算需要工作集证据，多view另有聚合额度。

底层观测覆盖Direct/cache/preview/navigation/canonical及资源分配：
- identity：run/interaction/job/frame、object/body、scene/surface/resource generation。
- route：purpose/fidelity、实际CPU/GPU target、hit/build/replay、拒绝/回退/等待原因。
- work：完整body编译次数/点数、局部组件、历史/新增mask、coverage pixels/tiles、copy/upload。
- time：input queue、Down/Move/Up、exact prepare/PathOps、Operation准备/apply、History、Scene、draw/submit、qualified completion、下一Down。
- memory：prepared/body/mask/composite/scratch/staging/pending-release、峰值、准入拒绝。
- scheduler：queue age、resume、取消/重做原因、progress/stale reject。

观测采用低成本聚合与有界事件缓冲，记录丢样。详细profile与主延迟采样分离；不能为统计在热路径序列化整条笔画或readback。实际target必须由分配结果确认。RAF、submit、GPU完成和物理显示延迟分别命名。

## 首个连续操作矩阵

固定30秒快慢交替、自交、固定pressure/width/seed的轨迹，初版7200样本：
- 总笔画数1/8/16/32；共享复制与独立笔画分别记录。旧1+8=9夹具保留。
- 纯笔画和固定shape/image/sticky/table混合场景，DPR1/2/4。
- 拖动/旋转缩放 → 擦除1/2/5/10轮 → 立即再擦/导航 → Undo/Redo → 闲置恢复。
- 有历史mask对象拖动必测，无mask修复不替代。
- 同源码默认32/96与诊断64/192MiB分别报告，更大档依据准入数据追加。
- normal/cold miss/pressure/GPU retirement delay/context loss分别验证，未跑格标MISSING。

用户认可的阶段目标：交互帧间隔p95≤33ms。
另建议最坏交互/submit空窗≤100ms，Up/下一Down同步dispatch p95≤50ms、max≤100ms。这些新数字待UR0冻结，不是已验收事实。canonical最终完成独立设deadline，不能只保持反馈而永不完成。

至少3次独立主采样，逐次报告p50/p95/p99/max、失败和缺失，不只挑最快。profile/readback不混入。记录总点数/可见点数/组件、覆盖面积、DPR、mask历史、工作集。自动Chrome结果不宣称物理input-to-photon。

结构性验收：
- 驻留且未变共享body不随副本×帧重复完整编译；重建须有驱逐/变更原因。
- mask-only不重建本体，活动mask只处理必要suffix。
- 支持GPU场景出现未声明CPU coverage使测试失败；pixel hit可跳过coverage。
- 压力/取消/恢复无丢terminal、重复commit、假receipt或无限Retry。
- 独立CPU exact oracle、GPU非AA像素/透明度/自交/接缝及已有AA容差保留。
- 绑定源码、runner、JS/WASM/resources/BuildInfo、配置、SHA256和原始数据。
- 回归比较精确失败名称；继承失败单列，新增失败不能混入。

## 实施包与审查

| 包 | 交付 | 现有owner/入口 | 放行依据 |
| --- | --- | --- | --- |
| UR0 | 全路径清单、Direct统计、阶段时间线、连续runner、固定门槛 | verification/runners/g5、Render叶子与Interaction计时 | 基线复现、观测开销、归因与未跑格明确 |
| UR1 | 私有统一决策/能力表、共享准备契约，先接canonical/edit反馈 | skia_renderer、skia_render_cache、skia_prepared_geometry | route/fidelity/fallback可测；现有收益不回退 |
| UR2 | 密集擦除纵向闭环：exact body/locality、增量mask、mask-only Scene、再输入衔接 | selection_region_geometry、canvas_partial_erase、canvas_editing_runtime、Scene；必要时Semantic内部复制 | erase→erase→navigation端点、语义与像素同时通过；子项独立提交 |
| UR3 | v4 stable prefix/tail、带mask仿射编辑、导航粗细层、canonical/Undo恢复 | 已有preview/navigation/cache owners | 支持矩阵接通，故障恢复与最终进展可证 |
| UR4 | 工作集驻留；按实测决定静态有序段/damage优化 | ResourceBudgetManager、cache、GPU retirement | 保持z-order/透明度；压力与多view总量明确 |
| UR5 | 连续矩阵、物理交互复测、回归与交付审查 | verification、匹配Web SDK demo | 分档报告达标/未达标，不冒充全平台/Gate PASS |

UR2拆为多个小包。UR0证据可以调整UR2/UR3先后，但必须记录归因、范围及验收映射。预算调优不替代未接通路径修复。

本会话持有设计、范围、验收口径和独立审查；实现会话写源码/测试、构建artifact、提供demo及原始证据。同一受控树只给一个生产源码写入窗口；设计会话读源码、维护设计/评审。重性能采样串行，记录其他GPU负载。

每包交接：base/head SHA、owner与允许路径、不变量、RED/基线、diff、测试及继承失败、原始数据、artifact hashes、rollback commit、未闭合项。实现返回READY_FOR_REVIEW或具体BLOCKED，不自行放宽门槛。

审查读取diff、原始证据与独立oracle，不仅采用实现者摘要。下一包从已审精确commit出发；发现契约缺口先在设计面修订。工程包审查不签发官方G5/P34 PASS。

本轮交付蓝图与拆包提案，实施消息尚未发出；收到继续实施指令后先固定文档版本，再从UR0具体包开始。UR0以测量可信、问题可复现及目标固定为退出条件，不要求基线先达到性能目标。
