# AR-0 架构对账报告与晋级前置

> 状态：Pass / 用户架构评审已批准（2026-08-23）
>
> 规范性：本报告是 AR-0 的过程与追踪基线；它不宣称 G0～G9、R1～R5 或任何产品实现已经通过。
>
> 对账日期：2026-08-23

## 1. 目的与边界

AR-0（Architecture Reconciliation）只负责把 Notion 最新 G0～G9 执行计划、仓库现状、已接受
ADR、历史 POC/RF 证据和用户已经确认的架构不变量放到同一张可追溯账本中。它不实现 G0，也不把
Notion 页面中的 `Design Approved`、`Current Direction`、`Proposed Freeze` 或勾选计划当成
仓库的 Accepted、Pass 或可发布契约。

本轮只做：

- 复读每个 Gate 及其直接引用的任务页，并记录来源定位符；
- 为每个任务分配仓库 `GT-*` 追踪 ID、依赖、R1～R5 贡献和当前对账状态；
- 解决或显式登记平台、所有权、Operation-only、Page/Document、Arc/Erase 和路线层级冲突；
- 建立 Gate Report 与 `Requirement → Decision → Implementation → Validation → Evidence`
  的追踪入口。

本轮不做：代码骨架、产品 ABI 冻结、Snapshot/Sync 具体格式、协作算法、线程拓扑、数据库/云
拓扑、L2/L3 缓存格式、真实设备门禁或任何 G1～G9 实现。

## 2. 唯一执行与晋级路线

```text
AR-0
  → G0 → G1 → G2 → G3 → G4 → G5 → G6 → G7 → G8 → G9
  → R5-B Hardening / Release
```

这是唯一的任务依赖与 Gate promotion 顺序。R1～R5 是产品交付里程碑视图；POC-01～06 和
RF-01～03 是风险验证、参考实现和历史 Evidence。它们可以被某个 Gate 复用，但不能形成第二
条依赖图、解锁链或 Accepted 状态。

### 2.1 R 里程碑映射

| 里程碑 | 主要贡献 Gate | 完成条件 |
| --- | --- | --- |
| R1 Runtime Foundation | G0、G1、G2、G3 | 这些 Gate 的必需任务、跨任务集成测试和里程碑退出条件全部通过；不能由 POC 合并替代。 |
| R2 V1 Local Visual Document Runtime | G1、G3、G4、G6、G7 | 上述 Gate 的必需任务、V1 行为语料、集成测试和本阶段退出条件全部通过。 |
| R3 Production Rendering and Shells | G3、G4、G5、G6 | 产品渲染、性能、Arc、Tier A Shell 和生命周期证据全部通过。 |
| R4 Collaboration MVP | G8 | 离线、同步、Presence、故障恢复和收敛门禁全部通过。 |
| R5 Hardening / Release | G9、R5-B | G9 Internal Alpha 通过后，R5-B 的发布、兼容、安全、恢复和稳定性门禁全部通过。 |

R1～R5 的状态只在 [R 里程碑状态表](R_MILESTONE_STATUS.md) 中维护；Gate 通过不会自动把
对应 R 阶段标为 Accepted。

## 3. 来源与任务身份规则

### 3.1 Notion 任务定位

G0 页面使用稳定的 `IH-00～IH-15 / WP-G0-00～16`；G1～G9 页面目前只显示页面内的
`Task 1..N`，没有可提交到仓库的全局人类可读 Task ID。因此账本使用：

- `Notion Task ID`：`Gx/Task N` 或 G0 的 `IH/WP` 编号。这是脱敏的来源定位符，不是假称 Notion
  原生 ID，也不写入私有 URL 或 block UUID；
- `Gate Task ID`：仓库稳定 ID，例如 `GT-G4-07`；补充的跨平台/缺失来源任务使用
  `GT-G3-09+`，并在来源列明确 `Repository reconciliation task`；
- R5-B 目前没有 Notion 任务页，仓库只登记 `R5B-*` 派生工作包，状态为
  `Missing / Blocked`，直到形成并接受权威任务清单。

### 3.2 对账分类与状态

`Disposition` 与任务状态严格分开：

| Disposition | 含义 |
| --- | --- |
| `Reuse` | 现有实现或 Evidence 可以直接作为该任务的输入，但不等于产品任务通过。 |
| `Modify` | 现有成果可以复用部分内容，需要按 Gate Contract 修改。 |
| `Missing` | 仓库尚无可复用的产品实现、报告或契约。 |
| `Conflict` | 现有文档/来源与当前用户基线或权威路线冲突，必须先对账。 |
| `Blocked` | 缺少上游决定、权威任务或外部条件，当前不能正确实现。 |

任务的四个状态字段统一使用：`Not Started`、`Analyzing`、`Ready`、`Implementing`、
`Validating`、`Pass`、`Fail`、`Blocked`。AR-0 通过前，G0～G9 的 Gate Task 全部保持
`Not Started`；来源重读与预分析归入 AR-0，不冒充 G0 已经启动。明确存在上游开放冲突的任务
另在状态/阻塞列标为 `Blocked`，不把“尚未轮到”误写成 Blocked。

## 4. 已确认的架构不变量

以下内容来自用户直接确认和已接受的 ADR-0025，是当前所有 Gate 的约束，不重新由实现者选择：

1. Web 使用 React/TypeScript + WASM/WebGL；Windows 使用 RNW + Native Canvas/Overlay Host；
   Android 使用 RN + Native CanvasView/JNI；iOS/iPadOS 使用 RN + Native Canvas/ObjC++/Metal；
   macOS native 暂缓，通过 Web 使用并保留 shared Core conformance。
2. `Operation` 是 Document 唯一 canonical mutation；单个 Operation 通过
   `decode → normalize → validate → idempotency → prepare apply plan → atomic apply → publish`
   原子发布。不存在全局 `Transaction → operations[]` canonical 外层。
3. 一个 Product Page 对应一个独立 Axiom Document；Page Collection 由上层产品层拥有；每个
   Document 是支持负坐标和连续 pan/zoom 的无限画布。
4. Arc/FastInk 是 Tier A 产品硬需求；Arc backend 失败必须自动回退 Canonical-only，不得丢失
   confirmed input、阻断 Operation commit、改变 Document digest 或污染 Canonical Renderer。
5. Connector、Group、Frame、Sticky、PDF、Lasso、Align/Distribute、Smart Snap 均进入实现
   范围。擦除面向用户只有“对象擦除”和“部分擦除”两种模式，内部按 whole-stroke、细矢量
   segment split、粗笔/Dab/texture Pixel/Dab mask 三条路径验收。

## 5. 当前冲突、处理与阻塞

| ID | 冲突/缺口 | 当前处理 | 影响与下一步 |
| --- | --- | --- | --- |
| AR0-C01 | G2 Notion Task 7 要求完整 100K production scene gate；仓库旧文档曾把它缩写成 candidate baseline 并推到 G5A。 | 恢复多规模 baseline、oracle parity、局部更新、固定 profile candidate limit 和 Gate Report；它不是通用 Product SLO，G5A 仍负责 production spatial/performance。 | G2-07 绑定 algorithm 1、固定 seed/scene/viewport/DPR/600-frame trace 与 `<= 5,000` candidate；POC-03 Windows 历史性能 Fail 原样保留，不能伪造 G2 Pass。 |
| AR0-C02 | G3 子计划只列 Headless/Windows/Web，用户基线要求 Android、iOS、iPadOS Tier A。 | 在账本增加仓库对账任务 `GT-G3-09/10`，并把 Android/Apple host evidence 纳入 G3 的完成边界。 | 在平台任务和证据契约补齐前，G3 不能 Pass；macOS 只做 shared Core/Web-reuse conformance。 |
| AR0-C03 | G4 子计划对 marquee、resize/rotate、z-order、Lasso、Align/Distribute、Smart Snap、三类擦除和 Apple Ink 证据不完整。 | 扩展 `GT-G4-06/07/08/09` 的任务说明，不改变 Notion 原 Task locator。 | G4 必须验证 Arc unavailable/timeout/presentation/internal failure→Canonical-only，且 confirmed input、Operation、digest 不变；`CanonicalVisible` 只能来自真实 PresentedProof。 |
| AR0-C04 | G6 子计划缺少 Connector/Group/Frame/Sticky/PDF 和 iOS/iPadOS Text/IME/lifecycle 任务。 | 增加 `GT-G6-10/11` repository reconciliation tasks，标记 Conflict/Blocked。 | 未形成权威任务与对象契约前不能宣称 G6 完整。 |
| AR0-C05 | G7 页面采用 TS-first Data Runtime；用户只确认中性的 Shared Data Runtime 职责。 | `GT-G7-01` 标为 Conflict/Blocked；语言、物理 owner、Bridge、数据库和发布形态交给后续 RFC。 | 不得在 G7 之前实现者临时选择 TS facade 或包边界。 |
| AR0-C06 | G8 collision/merge、AXTP、retry 和 conflict policy 仍 Open。 | 相关任务保留 `ConflictPolicyRequired` 阻塞项；不得选 CRDT/OT/local-wins 冒充结论。 | G8 只能在相应 ADR/Contract 接受后验证；Open 必须生成 Gate `BLOCKED`。 |
| AR0-C07 | G9 子计划缺 Apple host/evidence，Master 仍有 Tauri/Apple portability 旧文字。 | 增加 `GT-G9-13` Apple Tier-A integrated host/evidence 对账任务；现行仓库基线以 RNW/RN 为准。 | G9 不得把 Apple 视为仅 portability；Windows Tauri 只作为历史来源。 |
| AR0-C08 | 一 Page 一 Document 已接受，但来源 Gate 表没有完整 Page Collection/Repository 纵切面。 | 增加 `GT-G7-12` 和 `GT-G9-14`，分别闭合 repository/custody contract 与多 Page 产品集成恢复。 | 不得在 Axiom Document 中补 synthetic root，也不得把 Page Collection 产品 ownership 下沉给 Data Runtime。 |
| AR0-C09 | POC-05 证明 RNW/native Canvas/Overlay 可行，但没有验证 Windows 本地屏幕批注产品行为。 | 增加 `GT-G9-15`，并把 G3/G4 的 host/input/Arc seam 作为前置证据。 | 不得用 POC-05 替代 transparent topmost、click-through、多屏/DPI、focus/pen capture、lifecycle 和 fallback 的物理验收。 |
| AR0-C10 | R5-B 在 Notion Master 下没有独立任务页。 | 只建立仓库派生的 R5B work-package placeholders，明确 `Missing / Blocked`。 | 需要后续权威任务清单或用户批准的仓库派生拆分，不能伪造 Notion Task ID。 |
| AR0-C11 | `STAGED_DELIVERY_PLAN`、`PROJECT_FRAMEWORK` 仍保留 POC→RF→R 的视觉 DAG。 | 降级为历史 evidence lineage，并把唯一 promotion 链接回本报告和总路线。 | 任何 POC/RF/R 文字不得表达第二条依赖、解锁或晋级路线。 |

## 6. AR-0 追踪链与 Gate Report 约束

每个任务必须在 [Gate Task Tracker](GATE_TASK_TRACKER.md) 中有一行，按以下链追踪：

```text
Requirement / Constraint
  → Accepted Decision / RFC / Contract
  → Gate Task implementation
  → Unit / Contract / Property / Replay / Golden / Integration / Performance / Physical validation
  → Evidence bundle
```

Gate Report 必须绑定 Gate、commit、平台/设备、工具链和依赖版本、corpus/schema 版本、测试
结果、artifact hash、原始 Observation 与规范化 Result、适用性、第一处分歧和阻塞项。计划、
fixture、workflow 或“Self-Review PASS”不能替代实际运行 Evidence。

## 7. AR-0 退出条件

- [x] 用户确认的唯一 `AR-0 → G0 → … → G9 → R5-B` 路线已写入仓库总路线。
- [x] R1～R5 被定义为 many-to-many 里程碑覆盖层，而非第二条晋级路线。
- [x] Notion G0～G9 任务已重新读取；G0 的 IH/WP 和 G1～G9 的 `Gx/Task N` 定位规则已记录。
- [x] 全部 G0～G9 任务已在账本中登记，并完成仓库/来源对账分类。
- [x] 现行规划文档不再产生第二条可晋级 DAG，且 R 阶段退出条件均显式引用 Gate PASS。
- [x] 所有已知平台、Page/Document、Operation-only、Arc/Erase、Data Runtime 和任务来源冲突
  都有明确处置、owner 和下一步。
- [x] Markdown、链接、Mermaid、私有来源泄漏和工作区范围检查通过；命令与结果已记录到
  [AR-0 文档验证记录](../quality/evidence/ar0/reconciliation-validation-20260823.md)。
- [x] AR-0 Evidence 已绑定目标 Git commit、实际命令/结果和 artifact hash，并经用户架构评审批准。

因此 AR-0 当前是 `Pass`。G0 仍为 `Not Started`，但已满足进入 `GT-G0-00` 仓库分支/版本/来源
reconciliation 的晋级条件；G0 不得因 AR-0 通过而自动标记为 Ready 或 Pass。
