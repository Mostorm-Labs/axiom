# G1-04-A Semantic Authority Closure Gate v0.1（提案）

> 状态：**PROPOSED / NOT CURRENT / NOT MANIFEST AUTHORITY**
>
> 这是 `GT-G1-04-A` 的 composition gate；它只有在人工冻结批准后才可成为 Current Authority。

## 1. 目标

将下列三份**已获批准**的最小 closure 作为 A0–A3 implementation readiness 的完整语义输入：

1. `Operation Structural Semantics V1 Closure v0.1`；
2. `ObjectKind Version Registry V1 Release v0.1`；
3. `Semantic Leaf Structural Validation Closure V1 v0.1`。

它们与已发布的 Final Gate、Operation Payload、Common Wire、OrderKey、Reference IDL、Field
Registry、Image、Connector、RichText/Font、Brush/Stroke、BrushFamily、Pressure/Tilt、EraseMask
和 hard limits 一起消费；新文件仅补足审计中确认的 true gaps。

## 2. Gate 判定

| 子阶段 | 需要的 authority | 批准前 | 批准后可达到的状态 |
| --- | --- | --- | --- |
| A0 Typed Operation | 已有 15 payload identity / generated carrier | Ready | Ready |
| A1 Normalize | 既有 numeric/property/mask + operation batch closure | Partially ready | Ready |
| A2 Envelope | operation version matrix | Blocked authority | Ready |
| A3 Structural payload | collection、ObjectKind version、leaf closure | Blocked authority | Ready |

批准不代表实现通过，也不把 `GT-G1-04` overall 变为 Pass。它只允许之后重新进行 A0–A3 对账与
implementation planning。

## 3. 明确排除的 prerequisites

以下内容必须保留为独立 lane，不能再次被列为 A semantic blocker：

- **GT-G1-04-B**：OperationId idempotency、store/reference/kind/resulting-state validation、
  `Prepare ApplyPlan`、atomic apply integration；
- **GT-G1-04-C**：15-operation 的人工审阅 positive/negative intent、稳定 expected outcome、
  independent fixture compiler、production differential 与 no-mutation oracle；
- **GT-G1-05**：SemanticDocument atomic apply 与 ChangeSet；仍未授权。

## 4. 批准后的 machine projection（仍需单独实施/验证）

若人工批准，预计需要按现有 repository YAML 约定产生以下 machine-readable projection：

- `object_kind_registry_v1.yaml`：九种 ObjectKind identity/version；
- `operation_structural_profile_v1.yaml`：version matrix、batch profile 与 PropertyPatch presence；
- `semantic_leaf_constraints_v1.yaml`：仅 `G104-S09..S11` 的 gap-closure constraints。

它们不能成为第二套手写 schema，也不得包含 B state rules 或 C expected outcomes。若严格表达
批准语义需要变更当前 Proto carrier，必须停止为 `WIRE_SCHEMA_CHANGE_REQUIRED`。

## 5. 所需人工批准

本 gate 只接受本轮 Decision Pack 的精确 approval token：

```text
APPROVE_G1_04_SEMANTIC_FREEZE_V1
```

在收到该 token 前，仍为 `AWAITING_HUMAN_AUTHORITY_APPROVAL`：不得发布 Current Authority、
更新 manifest、生成 machine projection、变更 Proto/runtime、合并 authority branch 或把
`GT-G1-04-A` 改为 Ready。
