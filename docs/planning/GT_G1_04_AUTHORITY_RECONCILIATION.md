# GT-G1-04 Authority 与任务对账

> 状态：`PASS_REENTRY_READY_A0_A3_IMPLEMENTATION_NOT_STARTED`
>
> Gate Task：`GT-G1-04`
>
> 当前 re-entry baseline：`origin/main @ 17ff844f3b72e1976dee248550aa80f59ad38990`
>
> 对账日期：2026-08-27

## 1. Lineage 与本轮边界

此前 `codex/gt-g1-04-operation-apply@b7e9966269fbb4022e18aa332830ef682b6ecb38`
正确停止在 `PASS_REENTRY_BLOCKED_AUTHORITY`：当时 Operation/version、ObjectKind version 与
stateless leaf structural semantics 尚未成为 Current Authority。该历史结论保存在
`docs/planning/history/g1-04/`，不被本页改写。

随后完成：

- human semantic authority：`8adcf8599775dcdf8852699f5f8725a7add1864e`；
- publication evidence：`2b9506077e5f76ef7a71e037f51bc7222754ffa6`；
- machine-projection correction source：`b62fb8bab950a88641721da9db2b96faea385b24`；
- correction evidence + current main：`17ff844f3b72e1976dee248550aa80f59ad38990`。

本轮只重新执行 `GT-G1-04-A / A0–A3` authority re-entry。没有实现产品 C++、没有修改 Proto、
没有开始 ObjectStore/stateful validator、ApplyPlan、golden corpus 或 `GT-G1-05`。

## 2. 当前 Authority Stack

A0–A3 必须组合消费以下 Current Authority / machine projection，而不是从实现反推规则：

1. `schema/axiom/v1/proto/auditoryworks/axiom/v1/*.proto` 与 current descriptor/codegen baseline；
2. `schema/axiom/v1/registry/operation_registry_v1.yaml`；
3. `schema/axiom/v1/registry/object_kind_registry_v1.yaml`；
4. `schema/axiom/v1/canonical/canonical_profile_v1.yaml`；
5. `schema/axiom/v1/canonical/operation_structural_profile_v1.yaml`；
6. `schema/axiom/v1/canonical/semantic_leaf_constraints_v1.yaml`；
7. Field / Shape / Brush registries 与 protocol hard limits；
8. 对应 Notion Current Authority repository mirrors。

若 human authority、Proto/descriptor 与 machine projection 再次出现不一致，实施必须停止并记录
Authority / Projection Drift；不得以 runtime 输出修补 authority。

## 3. A0–A3 Re-entry Classification

| 子阶段 | 当前状态 | 当前结论 |
| --- | --- | --- |
| A0 Typed Operation domain | `READY` | 15 个 payload branch、OperationId/DocumentId、version carrier 与 encoding-neutral typed boundary 已有完整输入。 |
| A1 Common normalization | `READY` | finite、`-0 → +0`、canonical keyed sets、PropertyBag、persistent erase masks 与 leaf ordered-sequence ownership 已闭合。 |
| A2 Envelope validation | `READY_WITH_WIRE_PREFLIGHT_REQUIREMENT` | `schema_version = 1`、`payload_version = 1` 是唯一接受值；missing/0/unknown 全部 reject。WirePreflight/DTO boundary 必须保留 scalar field occurrence。 |
| A3 Payload structural validation | `READY` | 13 个 operation batch profile、ObjectKind version registry、PropertyPatch presence、VectorPath/RichText/Stroke stateless structural rules均已冻结。 |

因此 `GT-G1-04-A` 已从历史 `BLOCKED_AUTHORITY` 转为：

`READY_FOR_A0_A3_IMPLEMENTATION`。

这不是 `GT-G1-04-A Pass`，更不是 `GT-G1-04 Pass`；implementation 和 validation 尚未开始。

## 4. A / B / C 的硬边界

### A0–A3 本轮允许

- 建立 encoding-neutral typed Operation domain；
- normalize canonical values；
- envelope validation；
- 不读取 ObjectStore 的 payload structural validation；
- 只消费已发布 registry / limits / leaf rules；
- 证明 rejection 不产生 state mutation。

### GT-G1-04-B 仍未授权

不得提前实现或冻结：

- OperationId idempotency storage/classification；
- target existence / existing ObjectKind compatibility；
- parent/reference/connectability；
- hierarchy cycle / resulting-state invariant；
- mask/current-text/current-stroke state checks；
- read-only ApplyPlan 与 before-image/cascade closure。

### GT-G1-04-C 仍未授权

不得从 production validator 输出反向生成：

- 15-operation reviewed positive/negative intent；
- stable expected stage/path/category；
- golden fixtures / differential oracle。

### GT-G1-05 仍未授权

不得实现 Atomic Apply、generation advance、ChangeSet publication、History、Snapshot 或 Replay。

## 5. Wire Presence Stop Condition

Current Proto 中 `schema_version` / `payload_version` 是 scalar carrier，而 Current Authority 要求
missing 与 explicit zero 都 reject。A2 实现必须在 WirePreflight/DTO mapping 边界保留 field
occurrence，使 semantic validator 能确认字段是否真实出现。

如果当前 codec boundary 无法提供 occurrence fact：

`STOP = WIRE_SCHEMA_CHANGE_REQUIRED`

此时不得把 missing 默认为 1，也不得在本任务自行修改 Proto。

## 6. Implementation Re-entry Inputs

下一实施批次只能从 A0 开始，并按以下顺序推进：

`A0 typed domain → A1 normalize → A2 envelope → A3 payload structural`

每一阶段必须先有 RED case，再实现最小 GREEN；A3 完成并形成 commit-bound Evidence 后才允许
重新评估是否授权 B。A0–A3 不应创建假的 ObjectStore 或 ApplyPlan 来“提前打通”后续路径。

## 7. Re-entry Exit Result

- Authority gap：`CLOSED_FOR_A0_A3`；
- Machine projection review：`CLOSED`；
- Implementation：`NOT_STARTED`；
- Validation：`NOT_STARTED`；
- B：`NOT_AUTHORIZED`；
- C：`NOT_AUTHORIZED`；
- G1-05：`NOT_AUTHORIZED`。

本页是当前 implementation-branch re-entry authority reconciliation；历史 blocked 版本只用于
lineage，不再作为当前执行状态。
