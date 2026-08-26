# GT-G1-04 Authority 与任务对账

> 状态：`Ready`（实施前对账完成；最终验证语料尚未物化）
>
> Gate Task：`GT-G1-04`
>
> Notion Task ID：`G1/Task 4`
>
> 对账基线：`origin/main @ 39d44d289680e4ddaa5ae48a06e24aa579ee6326`
> 对账日期：2026-08-27

## 术语

- **Normalize（规范化）**：将同一语义的允许输入收敛为唯一的 canonical 值；例如将
  `-0` 变为 `+0`，并按注册表排序集合。它不会提交 Document 状态。
- **Validate（验证）**：按固定阶段拒绝无效 envelope、payload、类型、引用和 resulting
  state。被拒绝的 Operation 不得改变 ObjectStore、索引、generation 或 ChangeSet。
- **Idempotency（幂等）**：相同 `OperationId` 与 canonical payload 的重放返回
  `AlreadyApplied`；相同 ID 而 payload 不同是协议损坏。
- **ApplyPlan（应用计划）**：在不修改 Document 的前提下，解析目标、规范化值、删除级联、
  before-image 和实际写入集合；只有后续 Atomic Apply 能消费它。

## 1. 目标与非目标

本任务将 15 个已冻结 Operation 的 `Normalize → Validate → Idempotency → Prepare
ApplyPlan` 前提交路径物化为 Semantic Core。它为 R1/R2 的 `REQ-OBJ`、`REQ-EDIT`、
`REQ-INK`、`REQ-TEXT` 提供共同输入边界。

本轮对账不实现 `SemanticDocument` 的 Atomic Apply、generation 推进、ChangeSet 发布、
Snapshot、replay、Scene、Arc 或产品 Shell。这些分别属于 GT-G1-05 及后续 Gate。

全局约束保持不变：一个 Product Page 对应一个独立 Axiom Document；没有 Page ObjectKind
或 `DocumentRoot → Page*` synthetic root；Operation 是唯一 canonical mutation，且没有
全局 `Transaction → operations[]`。

## 2. 当前 authority 结论

| 议题 | 当前 authority / 机器产物 | 对 G1-04 的结论 |
| --- | --- | --- |
| 操作集合 | `operation-payload-validation-v0.1.md`、`operation_registry_v1.yaml`、`operation.proto`：15 kinds、oneof tag `1..15` | 已冻结，可直接消费。 |
| 阶段顺序 | `07-03-operation-semantic-document-v0.1.md`：Decode → Normalize → Envelope → Payload → OperationId Idempotency → Reference/Kind/Invariant → Prepare → Atomic Apply | 已冻结。幂等 gate 必须在 payload 验证之后、reference/kind/invariant 之前。 |
| 原子性 | `operation-payload-validation-v0.1.md`、ADR-0025 | 已冻结：一个 Operation 全有或全无；不是多 Operation Transaction。 |
| numeric / collection 规范化 | Common Wire、`canonical_profile_v1.yaml`、ADR-0026、field registry | 已冻结：finite f64、`-0 → +0`、PropertyBag 按 FieldId 排序且重复拒绝。 |
| 注册表与 hard limit | field/operation registry、`protocol_hard_limits_v1.yaml` | 已冻结；实现须从集中表读取，不能在 C++/TS 各自复制规则。 |
| resulting-state / connector delete | `07-03-operation-semantic-document-v0.1.md`、connector contract、semantic conformance source | 已冻结：验证 resultant state；delete 必须预先解析到 connector fixed-point cascade closure。 |
| 负例的稳定错误代码 | `semantic-conformance-golden-corpus-v0.1.md` 仍是 Draft / Freeze Candidate；现有 corpus 仅有 codec BG/BG-N 与 `seed-v0.1`，没有 15-operation apply case set | 不能把实现自定义 code 升格为协议 authority。内部诊断可稳定，但 GT-G1-04 最终 golden 需新增人工审阅的 case intent、stage/path/outcome。 |

### 已修正的计划漂移

旧 G1 包在 Wave 3 中写成
`... → Reference → Kind → Invariant → Idempotency → Prepare`。这与当前 07-03 的
OperationId gate 顺序冲突。正确路径是：

```text
Decode / typed view
→ Common Wire Normalize
→ Envelope Validation
→ Payload Structural Validation
→ OperationId Idempotency Classification
→ Reference / ObjectKind / resulting-state Invariant Validation
→ Prepare ApplyPlan
→ (GT-G1-05) Atomic Apply → generation + ChangeSet
```

因此相同 ID + canonical-equal payload 在不读取/计算不必要的引用约束前即可返回
`AlreadyApplied`；相同 ID + 不同 payload 返回 protocol corruption；两者均不得改变状态。

## 3. 仓库现状对账

| 能力 | 当前成果 | 分类 | G1-04 动作 |
| --- | --- | --- | --- |
| Operation/ObjectKind identity | `operation.hpp`、`operation.proto`、生成 Proto；9 ObjectKinds / 15 OperationKinds 已在 G1-01/02 固化 | Reuse | 将 `Operation` 从仅有 `id + kind` 的 bootstrap 升级为可验证 typed payload boundary。 |
| Canonical numeric helper | `canonical_numeric.hpp` 已验证 finite 和 `-0` | Modify | 扩展为完整 Operation/leaf normalizer；拒绝和输出值不能原地改写 store record。 |
| Strict wire decode | G1-02 `SemanticCodec` 与 BG/BG-N corpus | Reuse | 将其 typed decode result 接入同一 Normalize/Validate 入口；不复制 codec preflight。 |
| Object lookup / index | `ReferenceObjectStore`、`IndexedObjectStore`，GT-G1-03 已通过 | Reuse | 只读查询作为 reference/kind/invariant 和 ApplyPlan 的 state oracle。 |
| Private mutation seam | `internal::ObjectStoreMutator` | Reuse with restriction | 仅由 GT-G1-05 的 Atomic Apply 消费；G1-04 任意阶段不得调用。 |
| Normalizer / validator / idempotency / ApplyPlan 公共 API | 不存在 | Missing | 新建四个 focused headers/sources；公共 header 不引入 Scene、Skia、Arc、平台、存储或网络。 |
| Document/generation/ChangeSet publish | 只有类型定义，未有 SemanticDocument | Missing, downstream | 不在 G1-04 伪实现；由 GT-G1-05 实现。 |
| Apply conformance corpus | 无 15-operation positive/negative/replay/no-mutation case set | Missing | 首先创建 human-reviewed authority-manual case intent，再由 verification-only fixture compiler 派生数据。 |
| 旧 Task 状态 | G1 计划和 R 表仍显示 G1-02 Blocked、G1-03 Not Started/planned | Conflict | 本次同步为已通过的 commit-bound Evidence，避免 G1-04 从错误依赖状态启动。 |

## 4. Gate Task Matrix

| Notion Task ID | Gate Task ID | 子工作包 | R 贡献 | 关联 Requirement | ADR / Contract | 依赖 | 分类 | 设计 | 实现 | 验证 | Evidence | 阻塞项 | 最终状态 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G1/Task 4 | GT-G1-04-A | typed operation input、normalizer 与 envelope/payload validation | R1,R2 | REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | D-G1；ADR-0025、ADR-0026；Common Wire | G1-02, G1-03 | Modify | Ready | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-04/operation-matrix.json` | — | Not Started |
| G1/Task 4 | GT-G1-04-B | idempotency classifier、reference/kind/resulting-state validation 与 read-only ApplyPlan | R1,R2 | REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | 07-03；operation validation；connector/field/leaf contracts | GT-G1-04-A | Missing | Ready | Not Started | Not Started | `negative-results.json`、`no-mutation-results.json` | Apply semantics only; no commit/publication before G1-05 | Not Started |
| G1/Task 4 | GT-G1-04-C | 15-operation golden intent、independent fixtures、negative stage/path/result records | R1,R2 | REQ-GAP-VER；REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | semantic conformance；golden authoring pipeline | GT-G1-04-A/B | Missing | Analyzing | Not Started | Blocked | `summary.json` | human-reviewed case intent / frozen semantic error outcomes are not yet materialized in repository | Blocked |

`GT-G1-04` 的实施可从 A 开始；在 C 的审阅式语料存在并被 runner 消费前，整个任务不得标记
`Pass`。这不是由 production codec 自动 bless 语料可以绕过的条件。

## 5. 验证与 Evidence 设计

G1-04 的实现提交必须至少产出：

- `operation-matrix.json`：15 个 operation 的 positive、structural-negative、reference/kind/invariant-negative、replay 分类覆盖；
- `negative-results.json`：固定 stage、input path、authority-backed category（仅在 authority 已冻结处记录 code）；
- `no-mutation-results.json`：每个 rejection 前后的 ObjectStore projection、index invariant、generation 与 ChangeSet 均未改变；
- `summary.json`：source commit、authority baseline、registry/hash、case counts、CTest、Python/hosted run、first divergence 与 blocker。

Golden 仍遵循：`authority/manual intent → verification-only fixture compiler → derived binary/provenance → checked-in corpus → production observation → differential comparison`。不得从 Semantic Core 的当前输出生成 expected bytes、projection 或错误答案。

## 6. 下一最小工作包

`GT-G1-04-A — Typed Operation / Normalizer / Envelope-Payload Validation`：

1. 以当前 `operation_registry_v1.yaml`、field registry、hard-limit profile 和 generated proto
   建立单一的 typed input/validation data boundary；
2. 先写 RED 测试：zero/missing IDs、schema/payload version、unknown operation、non-finite/
   negative-zero、duplicate canonical key、FieldId/value type、N-1/N/N+1 hard-limit；
3. 仅实现 Normalize、envelope 与 payload structural stage；证明任何失败都不调用
   `ObjectStoreMutator`，并且不引入 SemanticDocument / ChangeSet publication；
4. 运行 semantic CTest、registry/contract tests 和 docs/manifest validation，保存新提交的
   commit-bound Evidence。

该工作包不实现 reference/invariant closure，也不进入 GT-G1-05。
