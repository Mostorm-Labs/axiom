# R1～R5 产品里程碑状态表

> 状态：Tracking baseline / 2026-08-23
>
> 规范性：R1～R5 只提供产品交付汇总，不形成执行或 Gate 晋级顺序。唯一晋级路线见
> [AR-0 架构对账报告](AR0_RECONCILIATION_REPORT.md)。

## 1. 状态规则

R 条目的贡献状态使用任务状态词：`Not Started`、`Analyzing`、`Ready`、`Implementing`、
`Validating`、`Pass`、`Fail`、`Blocked`。R 阶段最终只有在以下三类条件全部满足后，才可以
额外标记为 `Accepted`：

1. 映射到该里程碑的全部必需 Gate Task 均为 `Pass`；
2. 该里程碑的跨任务、跨 Gate、跨平台集成测试和适用物理设备测试通过；
3. [总路线](AXIOM_GATES_AND_STAGES.md) 中该 R 阶段的可量化退出条件有可复现 Evidence。

单任务 Pass 不自动使 Gate Pass；Gate Pass 也不自动使 R 阶段 Accepted。历史 POC/RF Evidence
可以作为输入，但不计作产品 Gate Task Pass。

## 2. R1 — Runtime Foundation

GT-G0-13 的 iPhone 与 iPadOS 独立物理 Evidence 和 commit-bound Evidence 均已通过。GT-G0-14
的正常 hosted PR DAG、四类故意失败归层和 commit-bound Evidence 均已通过。

| R 阶段条目 | 来源 Gate/Task | 状态 | Evidence | 剩余条件 |
| --- | --- | --- | --- | --- |
| Apple platform adapter foundation | `GT-G0-13` | Pass | [GT-G0-13 Apple XCTest-style Adapter Evidence](../quality/evidence/g0/gt-g0-13-apple-xctest-adapter-20260824.md) | — |
| Verification Foundation | `GT-G0-00..17` | Pass | [GT-G0-00～GT-G0-16 Evidence](../quality/evidence/g0/)；[package-bound mobile physical authority](../quality/evidence/g0/gt-g0-17/mobile-physical-package-bound-20260817.md)；[GT-G0-17 revalidation](../quality/evidence/g0/gt-g0-17-revalidation-20260826.md)；`verification/evidence/g0/gt-g0-17/` | G0 已通过；G1 可开始。 |
| Semantic foundation | `GT-G1-01`、`GT-G1-01R`、`GT-G1-02`、`GT-G1-03..08` | Validating | `GT-G1-01R`：[reconciliation Evidence](../../verification/evidence/gates/G1/42562d90a9403d8c3286cd0fa9972bac79a60bae/GT-G1-01R/)；`GT-G1-02`：`verification/evidence/gates/G1/1058311e47a2af68e4a444a76444dd6cf2354975/GT-G1-02/` + hosted revalidation；`GT-G1-03` repair Evidence planned | GT-G1-03 repair/revalidation 和其余 G1 任务、Operation-only Semantic Kernel、oracle、replay 与 Gate Report 通过。 |
| RuntimeScene foundation | `GT-G2-01..07` | Not Started | `verification/evidence/gates/G2/<commit>/`（planned） | G1 Pass；Full/Incremental、Linear/baseline spatial、100K candidate gate 通过。 |
| Canonical canvas foundation | `GT-G3-01..10` | Not Started | `verification/evidence/gates/G3/<commit>/`（planned） | G2 Pass；Headless、Web、RNW、Android、iOS/iPadOS canonical path 与 cross-platform golden 通过。 |
| R1 cross-task integration and exit | G0～G3 的 R1 贡献任务 | Not Started | `verification/evidence/milestones/R1/<commit>/`（planned） | 所有必需任务 Pass；ABI/dependency/build/sanitizer/POC migration 退出条件通过。 |

**R1 当前状态：Validating；Accepted：否。** `GT-G0-00` 已通过仓库基线对账，但 G0 及其余
R1 贡献任务尚未完成；已有 POC/RF 结果只作为任务对账输入。

## 3. R2 — V1 Local Visual Document Runtime

| R 阶段条目 | 来源 Gate/Task | 状态 | Evidence | 剩余条件 |
| --- | --- | --- | --- | --- |
| Semantic Object/Operation/Snapshot | `GT-G1-01`、`GT-G1-01R`、`GT-G1-02`、`GT-G1-03..08` | Validating | `GT-G1-01R` reconciliation、`GT-G1-02` codec/differential 已通过；`GT-G1-03` repair Evidence planned | G1 Pass；V1 schema、Operation、History 和 Snapshot 语义闭合。 |
| View/canonical interaction foundation | `GT-G3-04,08` | Not Started | `verification/evidence/gates/G3/<commit>/`（planned） | Camera/View/Hit/Select 与跨平台 structural evidence 通过。 |
| Interaction, Ink and local editing | `GT-G4-01..09` | Not Started | `verification/evidence/gates/G4/<commit>/`（planned） | Arc required/fallback、Selection/Lasso/Align/Snap 和三条 erase 路径通过。 |
| RichText, complex objects and editing lifecycle | `GT-G6-01..04,08..11` | Not Started | `verification/evidence/gates/G6/<commit>/`（planned） | RichText/IME/Clipboard 及 Connector/Group/Frame/Sticky/PDF 产品行为通过。 |
| Local data runtime, Page repository and recovery | `GT-G7-01..12` | Blocked | `verification/evidence/gates/G7/<commit>/`（planned） | 先接受 Shared Data Runtime 物理形态 RFC，并通过 journal/checkpoint/recovery/fault gate；Page Collection 产品语义仍由上层产品层拥有。 |
| R2 cross-task integration and exit | G1、G3、G4、G6、G7 的 R2 贡献任务 | Not Started | `verification/evidence/milestones/R2/<commit>/`（planned） | 全部必需任务 Pass；每种 V1 节点、Undo/Redo、save/replay/recovery 与数据丢失门禁通过。 |

**R2 当前状态：Blocked；Accepted：否。** 阻塞项是 Shared Data Runtime 语言/物理 owner/包与
Bridge RFC 尚未接受；它不允许实现者临时选择 TS-first。

## 4. R3 — Production Rendering and Shells

| R 阶段条目 | 来源 Gate/Task | 状态 | Evidence | 剩余条件 |
| --- | --- | --- | --- | --- |
| Canonical render and Tier A hosts | `GT-G3-01..10` | Not Started | `verification/evidence/gates/G3/<commit>/`（planned） | G3 Pass；所有 Tier A canonical hosts 与 shared Core conformance 通过。 |
| Ink/Arc product path | `GT-G4-01..09` | Not Started | `verification/evidence/gates/G4/<commit>/`（planned） | G4 Pass；真实设备 latency/Human Ink、handoff 和 Canonical-only fallback 通过。 |
| Large-canvas production optimization | `GT-G5-01..12` | Not Started | `verification/evidence/gates/G5/<commit>/`（planned） | G5 Pass；Reference parity、100K、frame-time、memory、tile/cache/scheduler 门禁通过。 |
| Rich editing, Overlay and lifecycle | `GT-G6-03,05..11` | Not Started | `verification/evidence/gates/G6/<commit>/`（planned） | Tier A IME/lifecycle、ExternalSurface、device loss 和复杂对象集成通过。 |
| R3 cross-task integration and exit | G3～G6 的 R3 贡献任务 | Not Started | `verification/evidence/milestones/R3/<commit>/`（planned） | 全部必需任务 Pass；Tier A 用户流、性能、内存、2 小时稳定性与物理设备门禁通过。 |

**R3 当前状态：Not Started；Accepted：否。** POC-03 Windows D3D12 p95/p99 历史 Fail 仍是
G5 的阻断 Evidence，不能因文档再基线而消失。

## 5. R4 — Collaboration MVP

| R 阶段条目 | 来源 Gate/Task | 状态 | Evidence | 剩余条件 |
| --- | --- | --- | --- | --- |
| Durable sync and recovery | `GT-G8-01..05,07..08,10..12` | Not Started | `verification/evidence/gates/G8/<commit>/`（planned） | G7 Pass；Outbox/Inbox、persist-first apply、catch-up、blob closure、fault/demo/report 通过。 |
| Snapshot bootstrap and conflict policy | `GT-G8-06` | Blocked | `verification/evidence/gates/G8/<commit>/GT-G8-06/`（planned） | collision/merge/conflict authority 必须先形成 Accepted ADR/Contract。 |
| AXTP adapter and public sync state | `GT-G8-09` | Blocked | `verification/evidence/gates/G8/<commit>/GT-G8-09/`（planned） | AXTP、ACK/retry 和 public state contract 必须先接受。 |
| R4 cross-task integration and exit | G8 全部任务 | Blocked | `verification/evidence/milestones/R4/<commit>/`（planned） | G8 Pass；3/5 副本 100K Operations 收敛、5 客户端 soak 和 Presence 隔离通过。 |

**R4 当前状态：Blocked；Accepted：否。** Open policy 只能产生 Blocked，不能由实现者选择
CRDT/OT、local-wins 或到达顺序来换取通过。

## 6. R5 — Hardening and Release

| R 阶段条目 | 来源 Gate/Task | 状态 | Evidence | 剩余条件 |
| --- | --- | --- | --- | --- |
| Integrated Product Gate / Internal Alpha | `GT-G9-01..15` | Not Started | `verification/evidence/gates/G9/<commit>/`（planned） | G0～G8 Pass；Tier A integrated scenario、多 Page Collection、Windows 屏幕批注 special host、failure injection、comparison、soak 和 regression 通过。 |
| Hardening and release work package | `GT-R5B-01..08` | Blocked | `verification/evidence/gates/R5-B/<commit>/`（planned） | G9 Pass；先形成权威 R5-B 任务清单，再完成安全、迁移、诊断、fuzz/soak、SBOM 和 RC 演练。 |
| R5 cross-task integration and exit | G9 + R5-B 全部必需任务 | Blocked | `verification/evidence/milestones/R5/<commit>/`（planned） | 无静默数据丢失/未归类 divergence；性能回归、升级、恢复、签名和回滚门禁全部通过。 |

**R5 当前状态：Blocked；Accepted：否。** G9 尚未开始，且 Notion Master 下没有独立 R5-B
任务页；仓库当前只保存显式标为非 Notion 来源的阻塞占位任务。

## 7. 更新纪律

每个 Gate Task 状态改变时，必须同步更新本文件中所有受影响行。贡献条目全部 Pass 仍不等于
R 阶段 Accepted；还必须附该 R 阶段的集成 Evidence、退出条件检查和人工批准记录。历史
Fail/Pending 通过 lineage 追加重测，不得原地改成 Pass。
