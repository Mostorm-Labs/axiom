# Axiom Gate Task Tracker

> 状态：AR-0 / Pass
>
> 规范性：Task-tracking baseline。它记录任务身份、依赖和状态，不把计划或历史 Evidence
> 升级为实现完成。
>
> 最近对账：2026-08-23；来源为已脱敏的 Notion G0～G9 最新执行页、用户确认基线和仓库
> 当前状态。私有 URL、page ID 和 block UUID 不写入仓库。

## 1. 使用规则

- 唯一晋级顺序是 `AR-0 → G0 → G1 → G2 → G3 → G4 → G5 → G6 → G7 → G8 → G9 → R5-B`。
- G0 的 `WP/IH` 是来源原生编号；G1～G9 的 `Gx/Task N` 只是页面内来源定位符，不是假称
  Notion 原生全局 Task ID。
- `Repository reconciliation task` 表示用户已确认的产品范围没有出现在最新来源任务表中；
  这些行不会伪装成 Notion 任务。
- Gate Task ID 只提供稳定身份，不编码拓扑顺序；任务必须按 `Dependencies` 和对应 Gate Report
  执行，因此较小编号可以依赖同 Gate 中较大编号的范围补充任务。
- `Dependencies` 只允许写完整 Task ID、完整 Task ID 的逗号列表、闭区间
  `GT-Gx-NN..GT-Gx-MM`，或上游 `Gx Pass` / `AR-0 Pass`；不得使用省略前缀的 `01,02`、
  裸 `G1..G3` 或自然语言。Skia SDK、Accepted Contract、journal 等非任务前置条件记录在
  ADR/RFC/Contract 或 Blocker 中，不混入可解析依赖图。
- `Disposition` 与状态分开。Reuse/Modify 只说明可以利用现有成果，不表示任务 Pass。
- 设计、实现、验证和最终状态都只使用：`Not Started`、`Analyzing`、`Ready`、
  `Implementing`、`Validating`、`Pass`、`Fail`、`Blocked`。
- 单任务 Pass 不等于 Gate Pass；Gate Pass 也不等于 R 里程碑 Accepted。

## 2. Requirement 与 Decision 简写

下列 `REQ-*` 组名只是对已存在 Framed Requirements 的紧凑引用：

| 简写 | 对应需求 |
| --- | --- |
| `REQ-CAN` | `REQ-FUNC-CAN-001`、`REQ-EDIT-VIEW-001..002` |
| `REQ-OBJ` | `REQ-FUNC-OBJ-001..007` |
| `REQ-EDIT` | `REQ-EDIT-SEL-001..002`、`REQ-EDIT-XFORM-001..002`、`REQ-EDIT-SNAP-001`、`REQ-EDIT-ORDER-001`、`REQ-EDIT-HIST-001` |
| `REQ-INK` | `REQ-INK-001..007` |
| `REQ-TEXT` | `REQ-TEXT-001..004` |

`REQ-GAP-VER/SCENE/PLAT/DATA/COL/NFR` 是**需求缺口标记**，不是已接受 Requirement。
它们表示步骤 3/8 尚未为验证基础、Scene/Render、平台、数据、协作或非功能目标建立完整
Requirement ID；在正式需求建立后必须替换，不能拿缺口标记通过 Gate。
`AR0-REQ-*` 只表示用户对本轮路线/追踪流程的直接约束。

Decision/Contract 简写：

| 简写 | 主要约束 |
| --- | --- |
| `D-AR0` | 用户唯一晋级路线、文档工作流、ADR-0025、AR-0 对账报告 |
| `D-G0` | 验证策略、文档工作流、Gate Report 纪律；具体 schema 在 G0 冻结 |
| `D-G1` | ADR-0001/0003/0014/0016/0019/0020/0025；产品 IDL/codec 仍待 G1 |
| `D-G2` | ADR-0003/0012/0016/0019/0021、RF-01 baseline contract |
| `D-G3` | ADR-0005/0009/0010/0012/0017/0021/0022/0025 |
| `D-G4` | ADR-0004/0011/0012/0014/0016/0017/0018/0024/0025 |
| `D-G5` | ADR-0007/0010/0012/0017/0019/0021 |
| `D-G6` | ADR-0006/0010/0012/0013/0023/0025 |
| `D-G7` | ADR-0013/0020/0022/0025；Data Runtime physical form RFC pending |
| `D-G8` | ADR-0013/0014/0020/0025；sync/conflict/AXTP ADR/RFC pending |
| `D-G9` | G0～G8 accepted contracts and reports；ADR-0025 platform matrix |
| `D-R5B` | G9 accepted baseline；compatibility/security/release task authority pending |

## 3. Gate 总状态

| Gate | 状态 | 任务覆盖 | 晋级剩余条件 |
| --- | --- | --- | --- |
| AR-0 | Pass | 6 tasks; validation evidence is commit-bound and architecture review is approved | Dependency for `GT-G0-00` is satisfied |
| G0 | Validating | `GT-G0-00..GT-G0-09` Pass; later G0 tasks remain Not Started | Complete `GT-G0-10..GT-G0-16`, produce reproducible G0 Evidence, then perform `GT-G0-17` review |
| G1 | Not Started | 8 tasks | G0 Pass |
| G2 | Not Started | 7 tasks | G1 Pass |
| G3 | Not Started | 10 tasks including 2 repository scope supplements | G2 Pass |
| G4 | Not Started | 9 tasks | G3 Pass |
| G5 | Not Started | 12 tasks | G4 Pass |
| G6 | Not Started | 11 tasks including 2 repository scope supplements | G5 Pass |
| G7 | Blocked | Execution has not started; 12 tasks including 1 Page repository supplement, with GT-G7-01 blocked by an Open RFC | G6 Pass and resolve physical Data Runtime form |
| G8 | Blocked | Execution has not started; 12 tasks, with GT-G8-06/09 blocked by Open policy | G7 Pass and accepted sync/conflict/AXTP decisions |
| G9 | Not Started | 15 tasks including Apple, Page Collection and Windows screen-annotation supplements | G8 Pass |
| R5-B | Blocked | 8 repository placeholders; no authoritative Notion task page | G9 Pass and accepted R5-B task authority |

本次动态 capture 中定位并登记为 Notion 来源的 locator rows 共 106 个：G0 的 18 个追踪单元和
G1～G9 的 88 个页面任务。它们受动态来源无不可变 revision 的限制；账本只能证明本次 capture
的枚举结果，不能独立证明 Notion 永久全集。仓库另加
8 个范围补充任务，因此 G0～G9 共 114 个 Gate Task；R5-B 另有 8 个明确标成非 Notion
来源的阻塞占位任务。

## 4. 任务矩阵

Evidence 列记录任务通过时必须生成的目标路径。标为 `planned` 表示当前没有 Evidence；
现有 POC/RF 资产只在 Disposition 对账时作为输入，不能写成产品 Gate 结果。


### AR-0

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| User route baseline | `AR0-01` | Freeze the sole promotion route and R milestone semantics | R1–R5 | AR0-REQ-ROUTE | D-AR0 | — | Modify | Pass | Pass | Pass | [总路线](AXIOM_GATES_AND_STAGES.md)；[对账报告](AR0_RECONCILIATION_REPORT.md) | — | Pass |
| Notion G0–G9 latest plans | `AR0-02` | Re-read and enumerate every source task | R1–R5 | AR0-REQ-SOURCE | D-AR0 | AR0-01 | Modify | Pass | Pass | Pass | [来源目录](../architecture/review/SOURCE_CATALOG.md)；[Notion/仓库差距对账](../architecture/review/NOTION_V03_REPOSITORY_GAP_AUDIT.md) | — | Pass |
| Repository reconciliation | `AR0-03` | Create stable Gate task identities and dependency ledger | R1–R5 | AR0-REQ-TRACE | D-AR0 | AR0-01, AR0-02 | Missing | Pass | Pass | Pass | [本任务账本](GATE_TASK_TRACKER.md) | — | Pass |
| Repository reconciliation | `AR0-04` | Create Gate-to-R milestone and end-to-end trace views | R1–R5 | AR0-REQ-TRACE | D-AR0 | AR0-03 | Missing | Pass | Pass | Pass | [R 里程碑状态表](R_MILESTONE_STATUS.md)；[本任务账本](GATE_TASK_TRACKER.md) | — | Pass |
| Repository reconciliation | `AR0-05` | Remove competing promotion routes and close baseline conflicts | R1–R5 | AR0-REQ-ROUTE | D-AR0 | AR0-01, AR0-02 | Modify | Pass | Pass | Pass | [总路线](AXIOM_GATES_AND_STAGES.md)；[历史工作包视图](STAGED_DELIVERY_PLAN.md) | — | Pass |
| Repository validation | `AR0-06` | Validate Markdown, links, fences, privacy and diff scope | R1–R5 | AR0-REQ-DOC | D-AR0 | AR0-03, AR0-04, AR0-05 | Modify | Pass | Pass | Pass | [AR-0 文档验证记录](../quality/evidence/ar0/reconciliation-validation-20260823.md) | — | Pass |


### G0

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WP-G0-00 / IH-00 | `GT-G0-00` | Repository and branch reconciliation | R1 | REQ-GAP-VER | D-G0 | AR-0 Pass | Modify | Pass | Pass | Pass | [仓库与分支基线对账记录](../quality/evidence/g0/gt-g0-00-repository-reconciliation-20260823.md) | — | Pass |
| WP-G0-01 / IH-01 | `GT-G0-01` | Schema and verification workspace skeleton | R1 | REQ-GAP-VER | D-G0 | GT-G0-00 | Modify | Pass | Pass | Pass | [验证工作区骨架记录](../quality/evidence/g0/gt-g0-01-verification-workspace-skeleton-20260823.md)；[IH-01/02 依赖纠错记录](../quality/evidence/g0/gt-g0-02-protocol-package-blocked-20260823.md) | — | Pass |
| WP-G0-02 / IH-02 | `GT-G0-02` | Protocol package and envelope codec | R1 | REQ-GAP-VER | D-G0 | GT-G0-01 | Modify | Pass | Pass | Pass | [历史 preflight 阻塞记录](../quality/evidence/g0/gt-g0-02-protocol-package-blocked-20260823.md)；[协议包与 Envelope Codec Evidence](../quality/evidence/g0/gt-g0-02-protocol-package-20260823.md) | implementation commit `d8e4fbe`; commit-bound hashes recorded in Evidence | Pass |
| WP-G0-03 / IH-03 | `GT-G0-03` | Runner core A: handshake, session, action, completion | R1 | REQ-GAP-VER | D-G0 | GT-G0-02 | Modify | Pass | Pass | Pass | [Runner Core A Evidence](../quality/evidence/g0/gt-g0-03-runner-core-a-20260823.md) | implementation commit `78e4d08`; commit-bound hashes recorded in Evidence | Pass |
| WP-G0-04 / IH-04 | `GT-G0-04` | Runner core B: event, source, fault, fence, finalization | R1 | REQ-GAP-VER | D-G0 | GT-G0-03 | Modify | Pass | Pass | Pass | [Runner Core B Evidence](../quality/evidence/g0/gt-g0-04-runner-core-b-20260824.md) | implementation commit `eeb02b1`; commit-bound hashes recorded in Evidence | Pass |
| WP-G0-05 / IH-05 | `GT-G0-05` | Scripted adapter and transport | R1 | REQ-GAP-VER | D-G0 | GT-G0-03 | Modify | Pass | Pass | Pass | [Scripted Adapter 与 Transport Evidence](../quality/evidence/g0/gt-g0-05-scripted-adapter-transport-20260824.md) | implementation commit `512cc7b`; commit-bound hashes recorded in Evidence | Pass |
| WP-G0-06 / IH-06 | `GT-G0-06` | Materialize 56 protocol vectors | R1 | REQ-GAP-VER | D-G0 | GT-G0-04, GT-G0-05 | Modify | Pass | Pass | Pass | [Protocol Seed 56 vectors Evidence](../quality/evidence/g0/gt-g0-06-protocol-vectors-20260824.md) | implementation commit `156cbea`; 112/112 results and 7/7 mutation guards are commit-bound | Pass |
| WP-G0-07 / IH-07 | `GT-G0-07` | Shared CLI and protocol CI gate | R1 | REQ-GAP-VER | D-G0 | GT-G0-06 | Modify | Pass | Pass | Pass | [Shared CLI 与 Protocol CI Evidence](../quality/evidence/g0/gt-g0-07-shared-cli-protocol-ci-20260824.md)；[hosted CI run](https://github.com/Mostorm-Labs/axiom/actions/runs/32686530146) | implementation `2292c16`; commit-bound hashes and hosted CI artifact recorded | Pass |
| WP-G0-08 / IH-08 | `GT-G0-08` | Verification native hooks and common host | R1 | REQ-GAP-VER, REQ-GAP-PLAT | D-G0 | GT-G0-00, GT-G0-07 | Modify | Pass | Pass | Pass | [Native Hooks / Common Host Evidence](../quality/evidence/g0/gt-g0-08-native-hooks-common-host-20260824.md)；`verification/evidence/g0/gt-g0-08/` | implementation commit `1d38785`; product normalized loss ingress remains a downstream platform integration gap, not an IH-08 blocker | Pass |
| WP-G0-09 / IH-09 | `GT-G0-09` | Materialize 28 platform scenarios | R1 | REQ-GAP-VER, REQ-GAP-PLAT | D-G0 | GT-G0-01 | Missing | Pass | Pass | Pass | [Platform Scenario Corpus Evidence](../quality/evidence/g0/gt-g0-09-platform-scenarios-20260824.md)；`verification/evidence/g0/gt-g0-09/` | E3 adapter execution belongs to `GT-G0-10..13`; not a blocker for corpus authoring | Pass |
| WP-G0-10 / IH-10 | `GT-G0-10` | Web reference adapter | R1 | REQ-GAP-VER, REQ-GAP-PLAT | D-G0 | GT-G0-08, GT-G0-09 | Modify | Pass | Pass | Pass | [Web Reference Adapter Evidence](../quality/evidence/g0/gt-g0-10-web-reference-adapter-20260824.md)；`verification/evidence/g0/gt-g0-10/` | — | Pass |
| WP-G0-11 / IH-11 | `GT-G0-11` | Windows native adapter | R1 | REQ-GAP-VER, REQ-GAP-PLAT | D-G0 | GT-G0-08, GT-G0-09 | Modify | Pass | Pass | Validating | [Windows Native Adapter Evidence](../quality/evidence/g0/gt-g0-11-windows-native-adapter-20260824.md)；`verification/evidence/g0/gt-g0-11/` | Windows runner required for Win32/D3D12 native execution and 28-case physical Evidence | Validating |
| WP-G0-12 / IH-12 | `GT-G0-12` | Android instrumentation adapter | R1 | REQ-GAP-VER, REQ-GAP-PLAT | D-G0 | GT-G0-08, GT-G0-09 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G0/<commit>/GT-G0-12/ (planned)` | — | Not Started |
| WP-G0-13 / IH-13 | `GT-G0-13` | Apple XCTest-style adapter for iPhone and iPadOS | R1 | REQ-GAP-VER, REQ-GAP-PLAT | D-G0 | GT-G0-08, GT-G0-09 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G0/<commit>/GT-G0-13/ (planned)` | macOS core conformance ownership must be added to G0 platform scenarios or assigned by the G0 Gate Report | Not Started |
| WP-G0-14 / IH-14 | `GT-G0-14` | PR CI dependency graph | R1 | REQ-GAP-VER | D-G0 | GT-G0-07, GT-G0-10..GT-G0-13 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G0/<commit>/GT-G0-14/ (planned)` | — | Not Started |
| WP-G0-15 / IH-15 | `GT-G0-15` | Nightly and release wiring | R1 | REQ-GAP-VER | D-G0 | GT-G0-10..GT-G0-14 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G0/<commit>/GT-G0-15/ (planned)` | — | Not Started |
| WP-G0-16 | `GT-G0-16` | Gate Report schema and G0 aggregator | R1 | REQ-GAP-VER | D-G0 | GT-G0-07, GT-G0-10..GT-G0-15 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G0/<commit>/GT-G0-16/ (planned)` | — | Not Started |
| G0/Task 4 | `GT-G0-17` | G0 Gate review | R1 | REQ-GAP-VER | D-G0 | GT-G0-00..GT-G0-16 | Blocked | Not Started | Not Started | Not Started | `verification/evidence/gates/G0/<commit>/GT-G0-17/ (planned)` | All G0 tasks and reproducible evidence are prerequisites | Not Started |


### G1

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G1/Task 1 | `GT-G1-01` | Create semantic target and canonical type boundary | R1,R2 | REQ-OBJ, REQ-INK, REQ-TEXT | D-G1 | G0 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-01/ (planned)` | — | Not Started |
| G1/Task 2 | `GT-G1-02` | Implement codec boundary and strict decode result | R1,R2 | REQ-OBJ, REQ-INK, REQ-TEXT | D-G1 | GT-G1-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-02/ (planned)` | — | Not Started |
| G1/Task 3 | `GT-G1-03` | Implement ReferenceObjectStore and IndexedObjectStore | R1,R2 | REQ-OBJ, REQ-EDIT | D-G1 | GT-G1-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-03/ (planned)` | — | Not Started |
| G1/Task 4 | `GT-G1-04` | Implement normalize, validation, idempotency and ApplyPlan | R1,R2 | REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | D-G1 | GT-G1-02, GT-G1-03 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-04/ (planned)` | Notion direct plan omits Normalize and Idempotency; use confirmed operation-only baseline | Not Started |
| G1/Task 5 | `GT-G1-05` | Implement SemanticDocument atomic apply and ChangeSet | R1,R2 | REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | D-G1 | GT-G1-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-05/ (planned)` | — | Not Started |
| G1/Task 6 | `GT-G1-06` | Snapshot, canonical projection, digest and replay | R1,R2 | REQ-GAP-DATA, REQ-EDIT-HIST-001 | D-G1 | GT-G1-05 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-06/ (planned)` | — | Not Started |
| G1/Task 7 | `GT-G1-07` | Semantic Replay Inspector CLI | R1,R2 | REQ-GAP-VER | D-G1 | GT-G1-06 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-07/ (planned)` | — | Not Started |
| G1/Task 8 | `GT-G1-08` | G1 Gate evidence | R1,R2 | REQ-GAP-VER | D-G1 | GT-G1-01..GT-G1-07 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-08/ (planned)` | — | Not Started |


### G2

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G2/Task 1 | `GT-G2-01` | Replace experimental semantic binding with G1 adapter | R1 | REQ-CAN, REQ-OBJ | D-G2 | G1 Pass | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-01/ (planned)` | — | Not Started |
| G2/Task 2 | `GT-G2-02` | Freeze RuntimeScene projection and visual-bounds contract | R1 | REQ-CAN, REQ-GAP-SCENE | D-G2 | GT-G2-01 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-02/ (planned)` | — | Not Started |
| G2/Task 3 | `GT-G2-03` | Make FullSceneCompiler a permanent explicit oracle | R1 | REQ-GAP-SCENE, REQ-GAP-VER | D-G2 | GT-G2-01, GT-G2-02 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-03/ (planned)` | — | Not Started |
| G2/Task 4 | `GT-G2-04` | Add LinearSpatialIndex oracle | R1 | REQ-EDIT-VIEW-001, REQ-GAP-SCENE | D-G2 | GT-G2-02 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-04/ (planned)` | — | Not Started |
| G2/Task 5 | `GT-G2-05` | Enforce localized incremental updates | R1 | REQ-CAN, REQ-GAP-SCENE | D-G2 | GT-G2-01..GT-G2-04 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-05/ (planned)` | — | Not Started |
| G2/Task 6 | `GT-G2-06` | Build Scene Inspector | R1 | REQ-GAP-VER, REQ-GAP-SCENE | D-G2 | GT-G2-03..GT-G2-05 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-06/ (planned)` | — | Not Started |
| G2/Task 7 | `GT-G2-07` | 100K production scene gate: multi-scale baseline, oracle parity, localized update, fixed-profile candidate limit and Gate Report | R1 | REQ-GAP-NFR, REQ-GAP-SCENE | D-G2 | GT-G2-01..GT-G2-06 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G2/<commit>/GT-G2-07/ (planned)` | Fixed profile is algorithm 1, seed 0x43414e5641533033, 100K nodes, 1K columns, cell 32, 600-frame 1920×1080/DPR1 trace; candidate count <= 5,000 is profile-bound, not a generic Product SLO | Not Started |


### G3

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G3/Task 1 | `GT-G3-01` | Create render target and renderer-neutral frame input | R1,R3 | REQ-CAN, REQ-GAP-PLAT | D-G3 | G2 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-01/ (planned)` | — | Not Started |
| G3/Task 2 | `GT-G3-02` | Implement NonTiledReferenceRenderer | R1,R3 | REQ-OBJ, REQ-GAP-SCENE | D-G3 | GT-G3-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-02/ (planned)` | — | Not Started |
| G3/Task 3 | `GT-G3-03` | Implement Skia Ganesh backend adapter | R1,R3 | REQ-OBJ, REQ-GAP-PLAT | D-G3 | GT-G3-02 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-03/ (planned)` | Requires the verified, locked Skia SDK; no source fallback | Not Started |
| G3/Task 4 | `GT-G3-04` | Add camera and view interaction model | R1,R2,R3 | REQ-CAN, REQ-EDIT-SEL-001 | D-G3 | GT-G3-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-04/ (planned)` | — | Not Started |
| G3/Task 5 | `GT-G3-05` | Headless demo host and HUD stats | R1,R3 | REQ-GAP-VER, REQ-GAP-NFR | D-G3 | GT-G3-02..GT-G3-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-05/ (planned)` | — | Not Started |
| G3/Task 6 | `GT-G3-06` | Windows RNW native Canvas/Overlay host demo and screen-annotation surface seam | R1,R3 | REQ-PLAT-WIN-ANNOTATION-001, REQ-GAP-PLAT | D-G3 | GT-G3-03..GT-G3-05 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-06/ (planned)` | Notion source says native/Tauri-era Windows; current contract requires RNW. This task only closes the special-host surface/control seam and cannot claim the screen-annotation product gate | Not Started |
| G3/Task 7 | `GT-G3-07` | Web WASM/WebGL2 demo | R1,R3 | REQ-GAP-PLAT | D-G3 | GT-G3-03..GT-G3-05 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-07/ (planned)` | — | Not Started |
| G3/Task 8 | `GT-G3-08` | Cross-platform golden and Gate Report | R1,R2,R3 | REQ-GAP-VER, REQ-GAP-PLAT | D-G3 | GT-G3-05..GT-G3-07, GT-G3-09, GT-G3-10 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-08/ (planned)` | Notion evidence scope omits Tier-A mobile hosts; each iOS, iPadOS and macOS result must be separate, and every platform needs an accepted visual tolerance or an auditable not_applicable decision | Not Started |
| Repository reconciliation task | `GT-G3-09` | Android RN Native CanvasView/JNI canonical host | R1,R3 | REQ-GAP-PLAT | D-G3 | GT-G3-03..GT-G3-05 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-09/ (planned)` | — | Not Started |
| Repository reconciliation task | `GT-G3-10` | iOS/iPadOS RN ObjC++/Metal canonical hosts and macOS core conformance | R1,R3 | REQ-GAP-PLAT | D-G3 | GT-G3-03..GT-G3-05 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G3/<commit>/GT-G3-10/ (planned)` | Evidence must report iOS, iPadOS and macOS independently; a combined pass is invalid | Not Started |


### G4

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G4/Task 1 | `GT-G4-01` | Productionize PointerSampleBatch and InputRouter | R2,R3 | REQ-INK-001, REQ-INK-003 | D-G4 | G3 Pass | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-01/ (planned)` | — | Not Started |
| G4/Task 2 | `GT-G4-02` | Productionize StrokeSession and BrushEngine | R2,R3 | REQ-INK-001, REQ-INK-003 | D-G4 | GT-G4-01 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-02/ (planned)` | — | Not Started |
| G4/Task 3 | `GT-G4-03` | Define PointerTrace corpus and deterministic runner | R2,R3 | REQ-INK-001..003, REQ-GAP-VER | D-G4 | GT-G4-01, GT-G4-02 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-03/ (planned)` | — | Not Started |
| G4/Task 4 | `GT-G4-04` | Production PreviewModel and required Arc bridge with fallback | R2,R3 | REQ-INK-002, REQ-INK-007 | D-G4 | GT-G4-02 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-04/ (planned)` | Requires accepted Arc protocol/ABI contract before implementation | Not Started |
| G4/Task 5 | `GT-G4-05` | Canonical AddStroke commit and CanonicalVisible acknowledgement | R2,R3 | REQ-INK-001, REQ-INK-007 | D-G4 | GT-G4-02, GT-G4-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-05/ (planned)` | CanonicalVisible requires real PresentedProof; GPU submit, flush or render-return is insufficient | Not Started |
| G4/Task 6 | `GT-G4-06` | Click/tap, marquee, Lasso, move/resize/rotate, z-order, Align/Distribute and Smart Snap | R2,R3 | REQ-EDIT-SEL-001..002, REQ-EDIT-XFORM-001..002, REQ-EDIT-SNAP-001, REQ-EDIT-ORDER-001 | D-G4 | G3 Pass | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-06/ (planned)` | Source task title is narrower than confirmed product scope | Not Started |
| G4/Task 7 | `GT-G4-07` | Object erase, segment split and Pixel/Dab mask with versioned brush dispatch | R2,R3 | REQ-INK-004..006 | D-G4 | G3 Pass | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-07/ (planned)` | Source task omits the complete three-path erase contract; Gate Evidence must cover dispatch-version replay compatibility and unknown-version rejection | Not Started |
| G4/Task 8 | `GT-G4-08` | Tier-A Ink Playground 0.1 | R2,R3 | REQ-INK-001..007, REQ-PLAT-WIN-ANNOTATION-001, REQ-GAP-PLAT | D-G4 | GT-G4-01..GT-G4-07 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-08/ (planned)` | Source entrypoints omit iOS/iPadOS and Windows must be RNW; the Windows profile must exercise native screen-host pen capture without RN JS hot-path events | Not Started |
| G4/Task 9 | `GT-G4-09` | G4 deterministic, physical-device and fallback evidence | R2,R3 | REQ-INK-001..007, REQ-PLAT-WIN-ANNOTATION-001, REQ-GAP-VER | D-G4 | GT-G4-01..GT-G4-08 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G4/<commit>/GT-G4-09/ (planned)` | Requires Windows, Web, Android, iPhone and iPadOS evidence plus unavailable/timeout/presentation/internal-error Arc faults; screen-host pen capture, confirmed input, Operation commit, digest and zero per-sample/per-frame RN JS events are mandatory | Not Started |


### G5

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G5/Task 1 | `GT-G5-01` | Spatial benchmark and production candidate comparison | R3 | REQ-GAP-SCENE, REQ-GAP-NFR | D-G5 | G4 Pass | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-01/ (planned)` | Must retain the G2/G3 accepted oracle contracts | Not Started |
| G5/Task 2 | `GT-G5-02` | StrokeChunk for long and large strokes | R3 | REQ-INK-001, REQ-GAP-NFR | D-G5 | GT-G5-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-02/ (planned)` | — | Not Started |
| G5/Task 3 | `GT-G5-03` | Old/new visual bounds and dependency damage | R3 | REQ-GAP-SCENE | D-G5 | GT-G5-01, GT-G5-02 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-03/ (planned)` | — | Not Started |
| G5/Task 4 | `GT-G5-04` | RenderGroup and transform-only reuse | R3 | REQ-GAP-SCENE, REQ-GAP-NFR | D-G5 | GT-G5-03 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-04/ (planned)` | — | Not Started |
| G5/Task 5 | `GT-G5-05` | TileKey, signed TileGrid and ScaleBucket contracts | R3 | REQ-GAP-SCENE | D-G5 | GT-G5-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-05/ (planned)` | — | Not Started |
| G5/Task 6 | `GT-G5-06` | TileManager and local raster source | R3 | REQ-GAP-SCENE | D-G5 | GT-G5-05 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-06/ (planned)` | — | Not Started |
| G5/Task 7 | `GT-G5-07` | Tile/non-tile golden and seam corpus | R3 | REQ-GAP-SCENE, REQ-GAP-VER | D-G5 | GT-G5-06 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-07/ (planned)` | Must compare with the accepted G3 NonTiledReferenceRenderer | Not Started |
| G5/Task 8 | `GT-G5-08` | L1 TileCache and eviction | R3 | REQ-GAP-NFR, REQ-GAP-SCENE | D-G5 | GT-G5-06, GT-G5-07 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-08/ (planned)` | — | Not Started |
| G5/Task 9 | `GT-G5-09` | Priority, deadline, cancel and generation scheduler | R3 | REQ-GAP-NFR | D-G5 | GT-G5-08 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-09/ (planned)` | — | Not Started |
| G5/Task 10 | `GT-G5-10` | ResourceBudgetCoordinator | R3 | REQ-GAP-NFR | D-G5 | GT-G5-08, GT-G5-09 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-10/ (planned)` | — | Not Started |
| G5/Task 11 | `GT-G5-11` | Performance Playground | R3 | REQ-GAP-VER, REQ-GAP-NFR | D-G5 | GT-G5-01..GT-G5-10 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-11/ (planned)` | — | Not Started |
| G5/Task 12 | `GT-G5-12` | History-slope and G5 evidence | R3 | REQ-GAP-VER, REQ-GAP-NFR | D-G5 | GT-G5-01..GT-G5-11 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G5/<commit>/GT-G5-12/ (planned)` | POC-03 Windows D3D12 p95/p99 historical Fail must remain in lineage | Not Started |


### G6

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G6/Task 1 | `GT-G6-01` | RichText and TextEditSession | R2 | REQ-TEXT-001..004 | D-G6 | G5 Pass | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-01/ (planned)` | — | Not Started |
| G6/Task 2 | `GT-G6-02` | TextInputAdapter and FakeIME | R2 | REQ-TEXT-002..004 | D-G6 | GT-G6-01 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-02/ (planned)` | — | Not Started |
| G6/Task 3 | `GT-G6-03` | Tier-A text adapters | R2,R3 | REQ-TEXT-002..004, REQ-GAP-PLAT | D-G6 | GT-G6-02 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-03/ (planned)` | Source omits iOS/iPadOS product adapters; accepted platform contracts are required | Not Started |
| G6/Task 4 | `GT-G6-04` | Clipboard through command and Operation | R2 | REQ-EDIT-HIST-001, REQ-GAP-PLAT | D-G6 | GT-G6-01, GT-G6-02 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-04/ (planned)` | — | Not Started |
| G6/Task 5 | `GT-G6-05` | Controlled ExternalSurface | R3 | REQ-GAP-PLAT | D-G6 | G5 Pass | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-05/ (planned)` | — | Not Started |
| G6/Task 6 | `GT-G6-06` | Platform lifecycle finite-state machine | R3 | REQ-GAP-PLAT, REQ-GAP-NFR | D-G6 | G5 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-06/ (planned)` | Accepted platform lifecycle contracts are required | Not Started |
| G6/Task 7 | `GT-G6-07` | Device and surface recovery | R3 | REQ-GAP-PLAT, REQ-GAP-NFR | D-G6 | GT-G6-05, GT-G6-06 | Modify | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-07/ (planned)` | — | Not Started |
| G6/Task 8 | `GT-G6-08` | Editing Demo 0.1 | R2,R3 | REQ-OBJ, REQ-TEXT, REQ-GAP-PLAT | D-G6 | GT-G6-01..GT-G6-07, GT-G6-10, GT-G6-11 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-08/ (planned)` | — | Not Started |
| G6/Task 9 | `GT-G6-09` | G6 Gate evidence | R2,R3 | REQ-OBJ, REQ-TEXT, REQ-GAP-VER | D-G6 | GT-G6-01..GT-G6-08, GT-G6-10, GT-G6-11 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-09/ (planned)` | — | Not Started |
| Repository reconciliation task | `GT-G6-10` | Connector, Group, Frame, Sticky and PDF product behavior | R2,R3 | REQ-FUNC-OBJ-004..007 | D-G6 | GT-G6-01..GT-G6-04 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-10/ (planned)` | Confirmed scope is absent from the latest G6 source task list | Not Started |
| Repository reconciliation task | `GT-G6-11` | iOS/iPadOS IME, lifecycle and recovery coverage | R2,R3 | REQ-TEXT-002..004, REQ-GAP-PLAT | D-G6 | GT-G6-02, GT-G6-05..GT-G6-07 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G6/<commit>/GT-G6-11/ (planned)` | Confirmed Tier-A Apple scope is absent from the latest G6 source task list | Not Started |


### G7

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G7/Task 1 | `GT-G7-01` | Establish Shared Data Runtime package and public/core boundary | R2 | REQ-GAP-DATA | D-G7 | G6 Pass | Conflict | Blocked | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-01/ (planned)` | Requires RFC for implementation language, physical owner, package and release form | Blocked |
| G7/Task 2 | `GT-G7-02` | AxiomDocumentPort opaque Operation/Snapshot bridge | R2 | REQ-GAP-DATA | D-G7 | GT-G7-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-02/ (planned)` | Requires accepted G1 public Operation/Snapshot contracts | Not Started |
| G7/Task 3 | `GT-G7-03` | InMemoryStore reference | R2 | REQ-GAP-DATA, REQ-GAP-VER | D-G7 | GT-G7-01, GT-G7-02 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-03/ (planned)` | — | Not Started |
| G7/Task 4 | `GT-G7-04` | Operation journal and durability frontier | R2 | REQ-GAP-DATA | D-G7 | GT-G7-02, GT-G7-03 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-04/ (planned)` | — | Not Started |
| G7/Task 5 | `GT-G7-05` | Snapshot and checkpoint manager | R2 | REQ-GAP-DATA | D-G7 | GT-G7-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-05/ (planned)` | Snapshot/manifest/blob atomicity and Blob closure need a clarifying ADR | Not Started |
| G7/Task 6 | `GT-G7-06` | Native structured store | R2 | REQ-GAP-DATA, REQ-GAP-PLAT | D-G7 | GT-G7-03..GT-G7-05 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-06/ (planned)` | — | Not Started |
| G7/Task 7 | `GT-G7-07` | Web IndexedDB/OPFS store | R2 | REQ-GAP-DATA, REQ-GAP-PLAT | D-G7 | GT-G7-03..GT-G7-05 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-07/ (planned)` | — | Not Started |
| G7/Task 8 | `GT-G7-08` | DocumentSession open, restore and close | R2 | REQ-GAP-DATA | D-G7 | GT-G7-04..GT-G7-07 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-08/ (planned)` | — | Not Started |
| G7/Task 9 | `GT-G7-09` | Persistence fault corpus | R2 | REQ-GAP-DATA, REQ-GAP-VER | D-G7 | GT-G7-03..GT-G7-08 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-09/ (planned)` | — | Not Started |
| G7/Task 10 | `GT-G7-10` | Persistence Demo 0.1 | R2 | REQ-GAP-DATA | D-G7 | GT-G7-01..GT-G7-09, GT-G7-12 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-10/ (planned)` | — | Not Started |
| G7/Task 11 | `GT-G7-11` | G7 Gate evidence | R2 | REQ-GAP-DATA, REQ-GAP-VER | D-G7 | GT-G7-01..GT-G7-10, GT-G7-12 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-11/ (planned)` | — | Not Started |
| Repository reconciliation task | `GT-G7-12` | Page repository and Page Collection custody contract | R2 | REQ-FUNC-CAN-001, REQ-GAP-DATA | D-G7 | GT-G7-01..GT-G7-08 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G7/<commit>/GT-G7-12/ (planned)` | Must define `PageId → DocumentId`, create/duplicate/delete/rename/reorder/open/close/recovery and cross-Document isolation while keeping product Page Collection ownership above Axiom and forbidding a synthetic root. Duplicate gets a new DocumentId/revision/frontier/history; delete retention/tombstone and crash atomicity are explicit; open/close cannot change digest; editing Page A cannot change Page B revision/history/digest | Not Started |


### G8

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G8/Task 1 | `GT-G8-01` | Durable Outbox | R4 | REQ-GAP-COL | D-G8 | G7 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-01/ (planned)` | Requires the accepted G7 journal contract | Not Started |
| G8/Task 2 | `GT-G8-02` | DurableInbox persist-first/apply-second | R4 | REQ-GAP-COL | D-G8 | G7 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-02/ (planned)` | Requires accepted G7 store contracts and G1 semantic idempotency | Not Started |
| G8/Task 3 | `GT-G8-03` | Deterministic FakeSyncServer | R4 | REQ-GAP-COL, REQ-GAP-VER | D-G8 | G7 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-03/ (planned)` | Reuses the accepted G0 verification infrastructure | Not Started |
| G8/Task 4 | `GT-G8-04` | Sync lifecycle | R4 | REQ-GAP-COL | D-G8 | GT-G8-01..GT-G8-03 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-04/ (planned)` | — | Not Started |
| G8/Task 5 | `GT-G8-05` | Gap detection and catch-up | R4 | REQ-GAP-COL | D-G8 | GT-G8-02..GT-G8-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-05/ (planned)` | — | Not Started |
| G8/Task 6 | `GT-G8-06` | Snapshot bootstrap and merge seam | R4 | REQ-GAP-COL | D-G8 | GT-G8-02..GT-G8-05 | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-06/ (planned)` | Collision/merge and conflict authority remain Open | Blocked |
| G8/Task 7 | `GT-G8-07` | Transport dedupe and semantic idempotency | R4 | REQ-GAP-COL | D-G8 | GT-G8-01..GT-G8-03 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-07/ (planned)` | Requires accepted G1 semantic idempotency contract | Not Started |
| G8/Task 8 | `GT-G8-08` | Blob Cloud Closure | R4 | REQ-GAP-COL, REQ-GAP-DATA | D-G8 | GT-G8-01, GT-G8-02, GT-G8-05 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-08/ (planned)` | Requires accepted resource-closure contract | Not Started |
| G8/Task 9 | `GT-G8-09` | Real AXTP adapter | R4 | REQ-GAP-COL | D-G8 | GT-G8-01..GT-G8-08 | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-09/ (planned)` | AXTP, retry, ACK and public-state contract remain Open | Blocked |
| G8/Task 10 | `GT-G8-10` | Sync and recovery fault corpus | R4 | REQ-GAP-COL, REQ-GAP-VER | D-G8 | GT-G8-01..GT-G8-09 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-10/ (planned)` | — | Not Started |
| G8/Task 11 | `GT-G8-11` | Sync Recovery Demo | R4 | REQ-GAP-COL | D-G8 | GT-G8-01..GT-G8-10 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-11/ (planned)` | — | Not Started |
| G8/Task 12 | `GT-G8-12` | G8 Gate evidence | R4 | REQ-GAP-COL, REQ-GAP-VER | D-G8 | GT-G8-01..GT-G8-11 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G8/<commit>/GT-G8-12/ (planned)` | — | Not Started |


### G9

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G9/Task 1 | `GT-G9-01` | Integrated product facade and composition shell | R5 | REQ-CAN, REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | D-G9 | G8 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-01/ (planned)` | All accepted G0–G8 contracts remain mandatory inputs | Not Started |
| G9/Task 2 | `GT-G9-02` | IntegratedScenario schema | R5 | REQ-GAP-VER | D-G9 | GT-G9-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-02/ (planned)` | Reuses the accepted G0 verification contract | Not Started |
| G9/Task 3 | `GT-G9-03` | FailureInjector through approved seams | R5 | REQ-GAP-VER, REQ-GAP-NFR | D-G9 | GT-G9-01 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-03/ (planned)` | Reuses the accepted G0 fault-injection contract | Not Started |
| G9/Task 4 | `GT-G9-04` | Standard 100K integrated scenario | R5 | REQ-CAN, REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT, REQ-GAP-DATA, REQ-GAP-COL | D-G9 | GT-G9-01..GT-G9-03 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-04/ (planned)` | All accepted G0–G8 contracts and Gate Reports remain required | Not Started |
| G9/Task 5 | `GT-G9-05` | Windows RNW integrated host | R5 | REQ-GAP-PLAT | D-G9 | GT-G9-04 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-05/ (planned)` | Latest source still carries native/Tauri-era Windows wording | Not Started |
| G9/Task 6 | `GT-G9-06` | Web WASM integrated host | R5 | REQ-GAP-PLAT | D-G9 | GT-G9-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-06/ (planned)` | — | Not Started |
| G9/Task 7 | `GT-G9-07` | Android RN integrated host | R5 | REQ-GAP-PLAT | D-G9 | GT-G9-04 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-07/ (planned)` | — | Not Started |
| G9/Task 8 | `GT-G9-08` | Evidence bundle collector | R5 | REQ-GAP-VER | D-G9 | GT-G9-02..GT-G9-07, GT-G9-13..GT-G9-15 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-08/ (planned)` | — | Not Started |
| G9/Task 9 | `GT-G9-09` | Cross-platform comparison | R5 | REQ-GAP-VER, REQ-GAP-PLAT | D-G9 | GT-G9-05..GT-G9-08, GT-G9-13..GT-G9-15 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-09/ (planned)` | — | Not Started |
| G9/Task 10 | `GT-G9-10` | Two-hour mixed editing and recovery soak | R5 | REQ-GAP-NFR | D-G9 | GT-G9-04..GT-G9-08, GT-G9-13..GT-G9-15 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-10/ (planned)` | — | Not Started |
| G9/Task 11 | `GT-G9-11` | G0–G8 regression sweep | R5 | REQ-GAP-VER | D-G9 | G8 Pass | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-11/ (planned)` | Re-runs all accepted G0–G8 Gate suites | Not Started |
| G9/Task 12 | `GT-G9-12` | G9 Gate Report and Internal Alpha | R5 | REQ-GAP-VER, REQ-GAP-NFR | D-G9 | GT-G9-01..GT-G9-11, GT-G9-13..GT-G9-15 | Missing | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-12/ (planned)` | — | Not Started |
| Repository reconciliation task | `GT-G9-13` | iOS/iPadOS RN integrated hosts and physical evidence | R5 | REQ-GAP-PLAT, REQ-GAP-NFR | D-G9 | GT-G9-04 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-13/ (planned)` | Confirmed Tier-A Apple scope is absent from the latest G9 source task list | Not Started |
| Repository reconciliation task | `GT-G9-14` | Multi-Page product collection integration and recovery | R5 | REQ-FUNC-CAN-001, REQ-GAP-DATA | D-G9 | G7 Pass, GT-G9-04 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-14/ (planned)` | Uses the accepted G7 custody/journal/checkpoint/recovery contract. Verify create/duplicate/delete/rename/reorder/open/close, restart recovery, Page-to-Document mapping and digest/history/resource isolation without a multi-Page Axiom Document | Not Started |
| Repository reconciliation task | `GT-G9-15` | Windows RNW screen-annotation special host and physical evidence | R5 | REQ-PLAT-WIN-ANNOTATION-001, REQ-GAP-PLAT, REQ-GAP-NFR | D-G9 | G8 Pass, GT-G9-05 | Conflict | Not Started | Not Started | Not Started | `verification/evidence/gates/G9/<commit>/GT-G9-15/ (planned)` | Requires accepted G3/G4 screen-host surface/input/Arc evidence. POC-05 proves RNW/native host and controlled Overlay feasibility only; product evidence must cover transparent topmost composition, click-through/draw-mode switching, multi-monitor/DPI, focus/pen capture, resize/display change, suspend/resume, surface loss, Arc fallback and hot-path isolation | Not Started |


### R5-B

| Notion Task ID / source locator | Gate Task ID | Task | R contribution | Requirement | ADR/RFC/Contract | Dependencies | Disposition | Design | Implementation | Validation | Evidence path | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| No Notion task page | `GT-R5B-01` | Compatibility window and migration plan | R5 | REQ-GAP-NFR | D-R5B | G9 Pass | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-01/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-02` | Threat model and untrusted-input limits | R5 | REQ-GAP-NFR | D-R5B | G9 Pass | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-02/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-03` | Diagnostics, crash reporting and support bundle | R5 | REQ-GAP-NFR | D-R5B | G9 Pass | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-03/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-04` | Fuzz and security validation | R5 | REQ-GAP-NFR | D-R5B | GT-R5B-01, GT-R5B-02 | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-04/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-05` | Editing and collaboration soak | R5 | REQ-GAP-NFR | D-R5B | GT-R5B-01..GT-R5B-03 | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-05/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-06` | SBOM, provenance, signing and dependency review | R5 | REQ-GAP-NFR | D-R5B | G9 Pass | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-06/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-07` | Release candidate, upgrade and rollback rehearsal | R5 | REQ-GAP-NFR | D-R5B | GT-R5B-01..GT-R5B-06 | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-07/ (planned)` | Authoritative R5-B task list is missing | Blocked |
| No Notion task page | `GT-R5B-08` | R5-B Gate Report and release acceptance | R5 | REQ-GAP-VER, REQ-GAP-NFR | D-R5B | GT-R5B-01..GT-R5B-07 | Blocked | Blocked | Not Started | Not Started | `verification/evidence/gates/R5-B/<commit>/GT-R5B-08/ (planned)` | Authoritative R5-B task list is missing | Blocked |


## 5. 更新协议

完成任一任务后，必须在同一次变更中：

1. 更新该行的 Design、Implementation、Validation、Final、Blocker 和真实 Evidence 路径；
2. 更新 Gate 总状态与 Gate Report；
3. 更新 [R 里程碑状态表](R_MILESTONE_STATUS.md) 中所有受影响条目；
4. 更新 Requirement/Decision/Implementation/Validation/Evidence 追踪；
5. 保留旧 Fail/Pending Evidence，并用 lineage 指向重测，不覆写历史；
6. 若产生 Open 决定，记录问题、选项、影响、owner 和建议验证；阻塞正确实现时置为 Blocked。

任务只有在实际 implementation 与适用验证完成、Evidence 可复现且无未关闭阻塞项时才能
标为 Pass。Gate 只有所有必需任务 Pass、跨任务集成测试和 Gate 退出条件通过时才能 Pass。
