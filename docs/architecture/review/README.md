# Axiom 架构重审工作台

> 状态：Draft / Non-normative
> 建立日期：2026-08-21
> 输入：SRC-USER-ARCH-REVIEW-BRIEF-20260821
> 当前进度：步骤 0、步骤 1、步骤 2 已完成；步骤 3“需求基线”前两组已确认，第三组已形成
> Resource/保存/恢复/Offline/Sync/Collaboration 候选，等待分 A/B/C 三回合审核
> 现行规范：在新的决定通过评审前，仍以
> [项目总体框架](../../PROJECT_FRAMEWORK.md)、
> [系统架构](../SYSTEM_ARCHITECTURE.md)和
> [现有 ADR](../../adr/README.md)为准。

2026-08-23 重新读取 Notion v0.3 后，已形成 [Notion/仓库差距对账](NOTION_V03_REPOSITORY_GAP_AUDIT.md)
和 ADR-0025 方向性再基线，并新增 [G0～G9 总路线](../../planning/AXIOM_GATES_AND_STAGES.md)。
它们先作为当前架构工作台的对账结果；代码尚未按 G1/G2 生产目标迁移，Notion 中 Draft、Open
或 Freeze Candidate 的内容仍需对应 Gate 复现。

本次动态读取按 `SRC-NOTION-ARCH-V03-CAPTURE-20260823` 登记；用户明确裁决按
`SRC-USER-ARCH-REBASE-CONFIRMATION-20260823` 登记。`Complete` 只适用于来源目录中写明的
本次 capture 范围，不表示 Notion 修订历史、评论、附件、外链或不可见子页已审，也不会重开
2026-08-21 已完成的六组来源身份审核。

### 2026-08-23 对账说明

本工作台的 `02_CURRENT_STATE_AUDIT.md` 仍然是以 `main@74c28b1`、2026-08-21 为基线的
历史审计快照。下面的新增说明是增量对账，不回写或删除快照中的原始冲突、失败证据和成熟度
判断；引用 2026-08-21 的现状时，仍应以快照正文为准。

根据本轮重新遍历 Notion v0.3 以及用户明确确认的 P0 方向，以下事项已经得到**方向性收敛**：

- 产品 Shell 方向为 Web React/WASM、Windows RNW、Android/iOS/iPadOS React Native；macOS
  暂缓 native 产品化并复用 Web，Platform Host 作为组合根角色；
- `Operation` 是唯一 canonical mutation，内部原子边界称 `Atomic Operation Apply`；传输/回放
  批次不自动形成跨 Operation 事务；
- 一个 Product Page 对应一个独立 Document，Page 集合由上层产品层管理，Document 不含 Page
  ObjectKind，并保持无限画布语义；
- Arc 是产品级 transient preview 能力，backend 失败时必须回退到 Canonical-only rendering；
- Shared Data Runtime 的数据侧职责与 Axiom 的语义边界已明确为后续实现方向。

这些方向不等于所有下游工作已经完成：Shared Data Runtime 的实现语言、Bridge/物理 ABI、包
边界、线程与存储格式，Schema 对新增对象的完整闭环，以及真实设备、恢复、性能和协作证据仍
保持 `Pending`/`Open`。Notion 中标记为 Draft、Open 或 Freeze Candidate 的内容不会仅因被
重新读取而升级为仓库规范；具体迁移按 [ADR-0025](../../adr/0025-product-shell-page-operation-data-runtime.md)
和 [G0～G9 总路线](../../planning/AXIOM_GATES_AND_STAGES.md) 逐 Gate 复现。

这组文档用于重新梳理 Axiom 的架构决策链。它不是第二套架构基线，也不会因为一轮讨论
就让已有决定自动失效。我们会把仓库、POC/RF 证据、Notion 草案和历史对话都当作输入，
先说明问题和约束，再比较方案，最后才决定是否保留、澄清或替代现有结论。

## 本文用语

| 中文首选名 | 英文或代码名 | 本文含义 |
| --- | --- | --- |
| 架构重审 | Architecture Review | 对当前架构的来源、假设、方案和证据重新建立追溯关系。 |
| 再基线 | Rebaseline | 在保留历史的前提下形成新的权威基线，不等于从零推翻。 |
| 规范性文档 | Normative Document | 对实现和后续设计具有约束力的已批准文档。 |
| 来源 | Source | 仓库文档、代码、验证证据、外部草案或历史讨论。 |
| 决策主题 | Decision Topic | 需要独立说明问题、方案、结论和验证方法的架构问题。 |
| 退出条件 | Exit Criteria | 当前步骤完成并允许进入下一步所需的可检查条件。 |

更完整的术语与中英文写法见[术语表](GLOSSARY.md)。

## 为什么现在重审

当前仓库已经有 24 份 Accepted ADR、完整的 POC 证据以及正在推进的 RF/R1 工程，说明
技术探索并非空白。问题在于，许多结论直接以 ADR、Contract 或实现规格出现，方案比较和
前置假设没有被单独保存。与此同时，外部讨论已经提出新的 Axiom/Arc、Platform Host、
Native React Native Shell 和 Shared TypeScript Data Runtime 边界，其中一部分与仓库现行
基线并不一致。

所以这次工作的目标不是批量移动文件，而是回答四个问题：

1. 我们到底要解决什么产品问题，哪些是 V1 必须能力；
2. 每个长期决定当时比较过哪些方案，为什么选择现在的方向；
3. 决定、接口成熟度、实现进度和验证结果分别处于什么状态；
4. 需求、决定、契约、测试和证据能否沿同一条链追溯。

## 架构真相如何形成

架构真相不只来自 ADR。新的追溯链固定为：

```text
来源与证据
  → 需求、约束与问题
  → 方案比较 / RFC
  → ADR
  → Architecture Spec / Contract
  → Validation Plan
  → Evidence
  → Implementation
```

这个顺序不是僵硬的瀑布。Draft Contract 可以帮助验证 RFC，POC 也可以推翻一个候选方案；
但没有经过明确评审的草案不能自动升级为 Accepted 决定。

## 总体步骤

每个步骤使用独立文档记录。步骤编号只表示本轮审查顺序，不占用 ADR 编号。我们只在
真正开始某一步时创建正文，避免先生成一批空文档。

| 步骤 | 独立文档 | 要回答的问题 | 主要产出 | 退出条件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 0 | 本文、[文档工作流](DOCUMENTATION_WORKFLOW.md)、[术语表](GLOSSARY.md) | 怎样讨论、批准和保存架构决定？ | 文档类型、状态、编号、隐私和评审规则 | 工作流和术语表分别获得用户明确确认 | Completed（2026-08-21） |
| 1 | [来源目录](SOURCE_CATALOG.md) | 当前有哪些可靠输入，可以怎样引用？ | 来源 ID、覆盖主题、访问/读取范围、证据角色和引用限制 | 来源全集可复算；每项五个维度齐全；缺口和影响明确；用户逐组确认；校验通过 | Completed（2026-08-21 historical snapshot）；增量来源审核至 2026-08-23 |
| 2 | `02_CURRENT_STATE_AUDIT.md` | 仓库、Notion 和讨论中已经有哪些决定、矛盾和缺口？ | 逐文件迁移表、ADR 对账、冲突清单 | 每个现有规范和 ADR 都有处理建议 | Completed（2026-08-21） |
| 3 | [需求基线](03_REQUIREMENTS_BASELINE.md) | V1 产品到底必须做什么？ | 带稳定 ID 的功能与非功能需求、Vibe 对齐、非目标 | 每个 P0/P1 需求有平台范围和验收方法 | In progress：第二组已确认；第三组候选待 A/B/C 审核 |
| 4 | `04_SYSTEM_CONTEXT_AND_BOUNDARIES.md` | Axiom、Arc、Shell、Host、Data Runtime 和外部系统分别拥有何种状态？ | 系统语境、所有权、生命周期和信任边界 | 顶层模块没有重叠的权威状态 | Not started |
| 5 | `05_DECISION_BACKLOG_AND_DAG.md` | 哪些决定必须先做，哪些可以继续实验？ | 决策清单、依赖 DAG、风险和验证方式 | 所有待决主题有 owner、前置条件和文档等级 | Not started |
| 6 | 每个主题一组独立文档 | 对单个架构主题，问题、替代方案和证据是什么？ | Problem、RFC、ADR、Spec/Contract、Validation | 逐主题通过明确评审 | Not started |
| 7 | `07_CROSS_CUTTING_REVIEW.md` | 各主题组合后是否仍满足全局不变量？ | 所有权、线程、恢复、资源、安全和错误传播审查 | 无未处理的 P0 跨主题冲突 | Not started |
| 8 | `08_VALIDATION_AND_TEST_BASELINE.md` | 如何证明设计成立、如何防止回归？ | 需求到测试/设备/阈值/证据的追踪矩阵 | 关键需求与 ADR 都有可执行验证 | Not started |
| 9 | `09_IMPLEMENTATION_ALIGNMENT.md` | 当前 POC、RF 和 R1 如何迁移到新基线？ | 实施包、依赖、迁移、回滚及 DoR/DoD | POC 接口没有被误升为稳定产品 ABI | Not started |
| 10 | `10_DOCUMENT_MIGRATION_AND_ACCEPTANCE.md` | 如何发布新基线而不留下两份真相？ | 迁移清单、归档/替代关系和发布检查 | 链接、状态、术语、追溯和门禁全部通过 | Not started |

## 第 6 步的主题顺序

具体主题按依赖关系逐个推进，不一次性生成全部结论：

1. 项目身份、产品范围与命名：Axiom、Canvas 和 Arc 分别是什么；
2. 公共 SDK 边界、Product Shell、Platform Host 与 Binding；
3. Semantic Document、Object、Operation、Command、History 与 Snapshot；
4. Runtime 生命周期、调度、线程、内存、诊断和资源预算；
5. SceneCompiler、Scene、SpatialIndex、Rendering、Tile、Cache 与 Resources；
6. Input、Ink、Arc、Preview/Canonical 与 handoff；
7. RichText、IME 与 TextEditSession；
8. Surface、Web、React Native、WASM、JNI、ObjC++ 和平台集成；
9. Persistence、Recovery、Sync 与 Collaboration；
10. 安全、兼容迁移、可观测性、供应链和发布。

主题顺序可以因证据依赖调整，但任何调整都应在状态表中记录理由。

## 2026-08-22 新增材料怎样进入流程

用户补充了 Semantic Schema/IDL、数据流和稳定 API 三份设计材料。它们已作为步骤 1 关闭后的
增量来源登记；两份历史对话导出读取为 `Partial`，Notion 当前页读取为 `Complete`，但三者
都保持 Non-normative。`Spec`、`IDL`、`Stable API`、`authority` 或 `freeze candidate` 是来源
自己的表述，不代表仓库已经接受或发布对应接口。

| 材料涉及的主题 | 在本工作流中的处理位置 |
| --- | --- |
| 保存、恢复、离线、同步和协作的用户结果 | 当前步骤 3 第三组先拆成 `INT-DATA/COL` 和可验收需求 |
| 模块划分、状态 owner、生命周期和信任边界 | 步骤 4 建立 System Context，步骤 5 排决策依赖 |
| Semantic Schema、Object/Operation/Snapshot 和 canonicalization | 步骤 5/6 形成独立 Problem、RFC、ADR、Schema Spec；步骤 8 验证 replay、digest 和迁移 |
| Runtime C ABI、Binding、Public SDK 和版本策略 | 步骤 4～6 对账现行 C API；步骤 8 做兼容矩阵，步骤 9 对齐实现 |
| 端到端数据流、Shared Data Runtime 和云端存储 | 步骤 4、6、7 拆 owner/port/服务方案，步骤 8 做故障、安全和恢复验证 |

其中，云端存储不是一项可以从 Schema 图直接推出的决定。第三组只确认产品必须观察到的
durability、recovery、offline 和 sync 结果；服务拓扑、数据库、对象存储、journal/outbox、
compaction、权限、加密、灾备和成本容量分别进入后续 RFC。新增 Schema 草案中的 `Page`
对象是已由 ADR-0025 排除的历史候选；后续只设计上层 Product Page repository 与
PageId→DocumentId 映射，不重新把 Page 放回 Axiom Document。

## 每一步怎样与用户协作

每一步都分为四个小回合：

1. **取证**：读取该主题所需的数据源，列出事实、用户需求、历史决定、推断和开放问题；
2. **问题定界**：先请用户确认 Problem、Requirements、Constraints 和 Non-goals；
3. **方案评审**：比较可行方案和代价，必要时设计 POC/Benchmark；
4. **形成结论**：用户明确确认后，才更新 ADR、规范、Contract 和验证计划。

“继续下一份”只表示进入下一项工作，不自动表示上一份 ADR 已 Accepted。决定被接受时应有
明确回复或 PR 审批记录。

## 步骤 2 识别的历史对账主题及当前状态

下列问题最初识别于 2026-08-21；其中一部分已经由 2026-08-23 增量对账和 ADR-0025 收敛，
保留在此是为了保存决策链，不表示旧方案仍然有效：

- 项目与模块正式名称仍在 `Canvas v2`、`Axiom` 和 `Arc` 之间混用；
- Windows Tauri 与 Native React Native Shell 的冲突已关闭：现行产品 Shell 是 RNW；
- Persistence/Collaboration owner 已收敛为中性 Shared Data Runtime 的数据侧职责；具体语言、
  物理 owner、Bridge、store 与 wire 仍待 RFC；
- 仓库稳定 C ABI 与外部“只发布一个 Axiom SDK、Host/Arc 内部化”的边界需要重新对账；
- Arc required、transient preview ownership 与 Canonical-only fallback 已关闭；具体 ABI、
  platform surface 和物理延迟证据仍待 G4/G6；
- POC-03/RF-01 的 Scene、SpatialIndex、Tile 和 SkSG 结论需要区分边界决定与具体实现；
- 当前 24 份 ADR 全部 Accepted，但接口、实现和验证成熟度并不都相同；
- 安全、信任边界、兼容迁移、可观测性和资源预算尚未形成完整决策链。

## 旧文档处理原则

重审完成前不批量移动或重写旧文档。每份旧文档在步骤 2 中只会被标记为：

- **保留（Keep）**：内容和决策链足够完整；
- **澄清（Clarify）**：结论仍成立，但需补范围或关系；
- **拆分（Split）**：把混合的 ADR、RFC、Contract、实施和验证内容分开；
- **替代（Supersede）**：新 ADR 明确替代旧决定，旧文件保留历史；
- **归档（Archive）**：不再规范实现，但保留来源和继任链接。

现有 ADR-0001～0024 不重编号。新决定从 ADR-0025 继续；改变 Accepted 决定时必须新增
`Supersedes` ADR，不能静默改写历史。POC-01 已 Accepted，不会重新创建同名 POC；如果新
决定影响其假设，只重验受影响的门禁。

## 工作台文件

- [文档工作流](DOCUMENTATION_WORKFLOW.md)
- [术语表](GLOSSARY.md)
- [来源目录](SOURCE_CATALOG.md)
- [步骤 2：当前状态审计](02_CURRENT_STATE_AUDIT.md)
- [步骤 3：需求基线](03_REQUIREMENTS_BASELINE.md)
- [Notion v0.3 与仓库差距对账](NOTION_V03_REPOSITORY_GAP_AUDIT.md)
- [AR-0 架构对账报告](../../planning/AR0_RECONCILIATION_REPORT.md)
- [唯一 G0～G9 晋级路线](../../planning/AXIOM_GATES_AND_STAGES.md)
- [Gate 任务追踪账本](../../planning/GATE_TASK_TRACKER.md)
- [R1～R5 里程碑状态表](../../planning/R_MILESTONE_STATUS.md)

步骤 2 已完成五组审核，现状、冲突和 Decision Backlog 输入见[当前状态审计](02_CURRENT_STATE_AUDIT.md)。
步骤 3 的需求接收、编号、状态和去重规则，以及 Canvas、Page/Viewport、对象、本地编辑、
Ink 与 RichText 的第二组需求方向已经确认；第三组已完成只读取证和候选拆分，接下来按
Resource/Blob、保存与恢复、Offline/Sync/Collaboration 三回合确认。需求文档不会直接替换现行 ADR，也不会把 178 个竞品
source rows 原样变成 178 个正式需求；`SRC-CHAT-07` 在正文可核验前保持来源级缺口。

2026-08-22，用户确认第二组需求方向：Connector、Group、Frame、Sticky、PDF，Lasso、
Align/Distribute 和 Smart Snap 进入实现范围；产品采用多 Page，但每个 Page 对应独立 Document，
且每个 Page/Document 是无限画布；Erase 提供对象擦除和部分擦除两种用户模式，部分擦除按
Brush/Stroke capability 分派到 Segment 或 Pixel/Dab；Arc Preview backend 为硬性实现能力，
Arc 失效时必须回退到 Canonical-only rendering。这些需求已由 `Candidate` 进入 `Framed`，
但不自动改写现行 Accepted 文档、稳定 ABI 或实现状态；对应冲突和后续 ADR 输入见
[步骤 3 需求基线](03_REQUIREMENTS_BASELINE.md)。
