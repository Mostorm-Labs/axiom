# Notion v0.3 与仓库基线差距对账

> 状态：Reviewed / Non-normative audit
> 对账日期：2026-08-23
> 作用：记录输入、差距和迁移建议；规范性结论以 ADR-0025 和后续 Contract 为准
> 隐私：只保存脱敏标题和结论，不保存私有页面 URL、页面 ID、评论或本机路径

## 本文用语

| 术语 | 含义 |
| --- | --- |
| Authority | 某份输入在自身文档体系中声明的状态；不自动等于仓库规范状态。 |
| Gate | G0～G9 的纵向能力与证据关卡；回答“这一纵切面是否已经可运行、可证明”。 |
| Stage / Work Package | POC、RF、R1～R5 的风险验证或交付工作包；回答“由哪批工作和历史证据完成”。 |
| Evidence guardrail | 已有 POC/RF 的量化阈值、失败记录和真实设备证据；合并路线不得降低。 |
| Reference path | 永久保留的简单正确性实现，用于定位优化实现的首次分歧。 |

## 1. 本次重新读取的范围

本轮从 `Axiom 整体架构基线 v0.3` 根页重新遍历其当前可见文档树，并复核：ADR/RFC、01 Product
Capability Traceability、02 Product Object Model、03 Interaction / Behavior Model、04 Semantic
Schema / Operation Model、05 Runtime Capability Architecture、06 Module Detailed Design、07 Runtime
Data Flow、08 Platform Contract、09 Engineering、10 Verification、G0～G9 Master Plan 与最新
Collaboration / Sync handoff register。读取范围是采集时可见正文和表格；不能替代不可变快照，
也不包含评论、历史修订、附件或外部链接正文。09 Engineering 当前没有子文档，这是来源自身
尚未提供工程落地内容的明确缺口，而不是仓库已经实现了该部分。

Notion 内部状态并不统一：根页含 Accepted/Current Direction/Superseded/Open；Schema Final
Gate 自称 Release Candidate Lock；Runtime Capability 是 Proposed Freeze；Platform/Public
Boundary 仍有 Draft/Skeleton/Open；Verification 是 Draft/Freeze Candidate；G0～G9 是 Design
Approved/Execution Plan；Collaboration handoff 仍是 Current Direction。因此仓库只吸收用户已
明确裁决和已对账的方向，具体字段/ABI/算法仍需仓库 Gate 复现。

## 2. 已经对齐的部分

- Axiom 是 C++20 Visual Document Runtime；Semantic Document 是唯一持久语义真相。
- `Document → SceneCompiler → RuntimeScene → View/Frame → FrameGraph/Tile → Ganesh` 的派生链。
- Axiom 拥有 Ink/brush/prediction/canonical semantics；Arc 只拥有输入接入和 transient
  presentation；Canonical 与 Preview target ownership 分离。
- Snapshot + Operation continuation + BlobRef 的恢复模型；Saved 与 Synced 分离。
- Storage/Sync 位于 Axiom 之外；Shared Data Runtime 只通过窄 bridge 接入。
- SpatialIndex、Damage、RenderGroup、Raster/Tile 和 Scheduler 的优化不能替代简单 oracle。

## 3. P0 差距与处理

| 主题 | Notion 当前方向 | 仓库旧基线 | 本次处理 |
| --- | --- | --- | --- |
| 产品 Shell | Web React；Windows/Android/iOS/iPadOS RN；Windows Overlay 特例 | Windows Tauri；Apple Tier B | ADR-0025 改为 RNW/RN；macOS deferred/Web |
| Canonical mutation | Operation-only；无全局 Transaction | 多处 `Document transaction` | 统一为 Operation + Atomic Operation Apply；batch/undo group 分层 |
| Page topology | 一 Product Page = 一 Document；Page collection 在上层 | 单 Page/`DocumentRoot → Page*` 待定 | Document 排除 Page kind；上层拥有 Page collection |
| Data owner | Notion 提出 TS-first Shared Data Runtime 与 AxiomDataBridge opaque bytes | 仅 ports，owner/语言曾保持候选 | 接受中性 Shared Data Runtime 的数据侧职责；语言、物理 owner、Bridge/codec/DB/wire 保持开放 |
| 平台组合 | Platform Host 是 composition role；物理 Host Runtime 另命名 | 两个层级偶有混用 | 固定角色/模块双层术语 |
| 交付主线 | Evidence-Gated Vertical Build G0～G9 | POC/RF/R1～R5 两层路线 | G0～G9 作执行顺序；旧工作包作映射和证据护栏 |

## 4. Notion 内部仍需裁决的冲突

Notion 的目录完整度明显提高，但其中仍保留不同时期的候选结论。下表以用户本轮确认和
ADR-0025 为当前仓库方向；不为了让来源看起来一致而静默改写 Notion：

| 来源内冲突 | 当前仓库处理 | 后续动作 |
| --- | --- | --- |
| Arc/Platform 部分页面把 Native Preview 写成 optional，或认为 Web V1 不需要 Arc | Tier A Arc Preview 是硬需求；任何 backend 失败都进入 Canonical-only rendering | G4 统一验证 required capability、fallback 与实际 `CanonicalVisible` 交接 |
| Data Runtime 页面采用 TS-first / Shared TypeScript Data Runtime | 只接受中性的 `Shared Data Runtime` 职责；语言和物理 owner 未冻结 | G7 RFC 比较 TS、Native 或组合方案及 Bridge/发布代价 |
| Module closure 建议把 Host 统一命名为 `Axiom Platform Runtime` / `axiom_platform` | 只冻结 `Platform Host` 组合根角色；`Host Runtime` 仍是候选物理模块 | 步骤 4/6 再决定包名、target 和公开 SDK，不让候选命名倒推 owner |
| 07 closure 使用“implementation-complete”一类完成措辞 | 只解释为该页面的数据流设计闭环，不表示代码、平台矩阵、协作或 Evidence 已完成 | 以仓库实现和 G0～G9 Gate Report 判断完成度 |
| 04/06 对部分对象和字段给出 Proposed Freeze，但 02 Object Model 仍是 Draft/Taxonomy Candidate | 只吸收已确认的产品范围与不变量，不冻结尚有分支的字段/枚举 | G0/G1 物化 IDL、descriptor、codec、golden 和兼容规则 |

## 5. 不能假称已经闭合的部分

最新 Schema/Traceability 页面已经给出 Connector、Sticky、Group、15 类左右的 Operation、
Unicode scalar RichText positions、whole-operation atomicity 和 hard limits 候选；但仓库尚无
产品 IDL、descriptor lock、codec、canonical projection/digest 或对应 golden。相关 Object
Model 页面仍是 Draft/Taxonomy Candidate；Connector endpoint/attachment/routing、Group
zero-visual container 与 own-transform 分支、Sticky 的 canonical RichText child、Image 的
intrinsic/display box/crop/fit 顺序、Frame/PDF 的资源与分页语义也未全部闭环。仓库必须在
G0/G1/G6 物化并复现这些规则。用户已确认“要实现”只决定产品范围，不等于 Notion 已冻结
ObjectKind/字段值或仓库代码已经完成。

07 Runtime Data Flow 已把五类数据（Canonical、Transient、Derived、Transport、Control）、
remote persist-first/apply-second、no-echo、Local Recovery Closure、Blob closure、generation
namespace、`PresentedProof` / `CanonicalVisible` 等关系写得很具体；08 Platform Contract 也列出
ABI/Bridge、Surface/GPU、Thread、Lifecycle、IME 和 ExternalSurface 矩阵。但这些仍是设计合同：
公共 ABI layout、Arc ABI、Data Bridge backpressure、surface metrics、shutdown/device-loss、IME
和 ExternalSurface fallback 仍含 OPEN，仓库尚无相应产品实现或跨端故障证据。

10 Verification 已提出 language-neutral corpus、跨语言 runner、Observation/Result 分离、
`OPEN → BLOCKED_OPEN`、platform scenario/protocol vectors、session epoch/late-event fence、fault
hooks、CI governance 和 evidence issue pack。仓库已有 POC harness、golden 与真实设备报告，但
统一的产品 conformance CLI、corpus、platform runner、first-divergence 和 Gate Report 尚未建立；
这些内容应进入 G0，不能把 Notion 中的计划表当作已经通过的测试结果。

以下仍保持 RFC/验证状态：公共 ABI 的最终 layout、线程拓扑、Surface/GPU 共享、Snapshot
codec/database/compaction、AXTP 与 Document Schema 关系、server ordering、retry/backoff、
Transform/RichText/Connector 并发语义、Undo 冲突、权限和 reject/quarantine。实现者不得为使
Gate 通过而临时选择 CRDT/OT、local-wins 或更新 golden。

## 6. 完成度比较

Notion G0～G9 约有 92 个细粒度任务，明显强于仓库旧路线中的纵向实现切片、永久 reference
oracle、Inspector/Demo、统一 Gate Report、Local Data/Sync fault path。仓库则在以下方面更
完整：六端 POC 可移植性证据；Ink/100K/RichText/Arc 的量化门禁和失败历史；Public C ABI、
Skia SDK supply chain 与 C++ style；随机多副本收敛；安全、迁移、SBOM、回滚、fuzz、soak
和性能回归。

结论不是选择“点更多”的一边，而是取并集：G0～G9 成为唯一 promotion 顺序；POC/RF/R
保留工作包身份、当前状态和全部 evidence guardrails。任何旧 POC 的 Accepted 只证明其
范围，不会自动使对应生产 Gate 通过；任何 Notion Execution Plan 也不会把仓库中实际失败的
POC-03 Windows 性能门禁改成 PASS。

## 7. 双向映射

| Gate | 主要仓库输入 | 迁移关系 |
| --- | --- | --- |
| G0 Verification Foundation | 现有 Verification、POC harness/evidence | R1 前置横切层；建立统一 CLI/corpus/report |
| G1 Semantic Kernel | POC-01 replay/digest、ADR-0014/0020 | R1→R2 产品语义底座；POC schema/ABI 不升级 |
| G2 RuntimeScene Foundation | POC-03、RF-01 | 建立 Full/Incremental 与 Linear/optimized oracle |
| G3 Basic Canonical Canvas | POC-01 renderer、POC-03 direct path | 生产 Canvas vertical slice，不重复 POC-01 |
| G4 Interaction + Ink | POC-02、POC-06/Arc、POC-03 integrated ink；POC-05 仅作 host/input 辅助证据 | 吸收选择/变换/吸附/三类擦除与 Arc fallback |
| G5 Large Canvas Optimization | POC-03、RF-02/03、R3 | 逐步接入 spatial/chunk/damage/cache/tile/budget |
| G6 Rich Editing + Lifecycle | POC-04、POC-05、R2/R3 | 产品化文本、复杂对象、Overlay 和平台 lifecycle |
| G7 Local Data Runtime | ADR-0013/0020、R2 | Shared Data Runtime、Snapshot/journal、LocalReady |
| G8 Sync + Recovery | R4 | Outbox/Inbox、gap/bootstrap、faults、conflict seam |
| G9 Integrated Product Gate | R5（G9/Internal Alpha） | 全产品集成与 Internal Alpha 证据；不是最终发布全部工作 |

反向看，POC-01 是 G0/G1/G3 的实验输入；POC-02 是 G4 输入；POC-03 是 G2/G3/G4/G5
输入；POC-04 是 G6 输入；POC-05 是 G4 的辅助边界证据和 G6 的主要输入；POC-06 是
G4/G5/G9 输入。RF-01 在 G2 闭合，其 contract 被 G5 复用；RF-02/03 对应 G5。R1 横跨
G0～G3，R2 主要覆盖 G1/G3/G4/G6/G7，R3 覆盖 G3/G4/G5/G6，R4 覆盖 G8，R5 拆成 G9
Internal Alpha 与其后的 Hardening/Release。

## 8. 迁移纪律

1. 先完成 AR-0 文档对账，再允许 G0 promotion。
2. 每个 Gate 同时提交实现、Reference/Mock、自动化证据、可运行产物、诊断和 Gate Report。
3. E1 Contract/Unit、E2 Reference/Mock、E3 Integration/Golden、E4 Physical/Demo 分开报告。
4. `FullSceneCompiler`、`LinearSpatialIndex`、`NonTiledReferenceRenderer`、
   `ReferenceObjectStore`、`InMemoryStore`、`FakeSyncServer`、`FakeSurface/FakeDevice` 长期保留。
5. Gate Report 绑定 commit、平台、工具链、schema/algorithm version 和 artifact hashes。
6. Open policy 导致 `BLOCKED`，不能通过降低断言、自动更新 golden 或静默添加产品语义绕过。
