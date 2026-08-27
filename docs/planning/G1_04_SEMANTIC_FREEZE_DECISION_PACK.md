# G1-04 Semantic Freeze Human Review Decision Pack

> 状态：**PROPOSED / NOT CURRENT / NOT MANIFEST AUTHORITY**
>
> 目的：把 G1-04-A 的全部新语义选择显式交给人工审核。批准本包不会自动实施产品代码，
> 不会使 G1-04 overall Pass，也不会授权 G1-05。

## 1. 本次审阅的边界

本包仅审阅：Operation structural semantics、ObjectKind version registry 与真正缺失的 stateless
leaf structural validation。它刻意不重开已由 `GT-G1-02R` 固化的 RichText/Stroke machine
projection。

Phase 0 已完成的 A/C 分类修正 commit：
`b7e9966269fbb4022e18aa332830ef682b6ecb38`。

审计结果见 [G1-04 语义缺口最小化审计](G1_04_SEMANTIC_GAP_MINIMIZATION_AUDIT.md)。提案正文：

- [Operation structural closure](authority-drafts/g1-04/operation-structural-semantics-v1-closure-v0.1.md)
- [ObjectKind version registry](authority-drafts/g1-04/objectkind-version-registry-v1-release-v0.1.md)
- [Leaf structural closure](authority-drafts/g1-04/semantic-leaf-structural-validation-closure-v1-v0.1.md)
- [A closure composition gate](authority-drafts/g1-04/g1-04-a-semantic-authority-closure-gate-v0.1.md)

## 2. Human decision table

| Decision ID | 问题 | existing authority | Proposed V1 answer | 可选方案 | compatibility consequence | Codex recommendation | Human status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `G104-S01` | schema version 如何接受？ | 需要 version compatibility，未给 exact matrix | 仅 `1`；missing/0/unknown reject | 0/missing 默认 1 | 老 payload 须显式升级 | 采用显式 fail-closed | Pending |
| `G104-S02` | payload version 如何接受？ | 同上 | 仅 `1`；missing/0/unknown reject | 0/missing 默认 1 | payload family evolution 不再隐式 | 采用显式 fail-closed | Pending |
| `G104-S03` | Insert/Delete/Restore batches | 有 15 payload，缺逐字段集合 profile | keyed canonical set；nonempty；unique object ID；unsigned ID sort | 保留输入 sequence | 减少同义 bytes 与 duplicate ambiguity | keyed set | Pending |
| `G104-S04` | placement/transform/size batches | carrier 与 OrderKey 已有 | keyed canonical set；nonempty；target unique；unsigned target ID sort | sequence / last-write-wins | 同 target 二次更新必须拆为两个 Operation | keyed set | Pending |
| `G104-S05` | property batch 与 SET/CLEAR | Field registry 已有 | keyed `(objectId,fieldId)` set；nonempty；SET requires value；CLEAR forbids value | 允许 ordered multi-patch | 去除隐含覆写顺序 | keyed set + strict presence | Pending |
| `G104-S06` | stroke split / replacements | split carrier 已有 | source unique；splits/replacements nonempty；replacement IDs globally unique；ID sort | replacement empty 表示 delete | whole erase 继续使用 Delete，split 专注 fragments | reject empty replacements | Pending |
| `G104-S07` | add/remove erase-mask batches | mask identity/geometry 已有 | outer target 与 inner mask 都是 nonempty keyed sets；duplicates reject；ID sort | sequence 或 empty no-op | 避免同一 payload 的 mask 冲突 | keyed set | Pending |
| `G104-S08` | ObjectKind version registry | 9 identity 已锁定，version table 不完整 | 9 kind 均 version 1、只接受 `[1]`、unknown/missing/0 reject | 由 branch/tag 推断 version | 将 future kind evolution 显式化 | complete fail-closed registry | Pending |
| `G104-S09` | VectorPath grammar | carrier/finite/pre-apply 已有 | nonempty；MoveTo 开始；active subpath；合法 Close；multiple subpaths；two FillRule；finite | 空 path / implicit MoveTo | 防止 renderer-specific reinterpretation | 明确 grammar，保留 point subpath | Pending |
| `G104-S10` | RichTextDelta version / steps | leaf truth与 02R wire 已有，version/empty 未物化 | version 1 only；missing/0/unknown reject；nonempty ordered steps；one active branch | empty delta as no-op | no-op 改为不产生 Operation | reject empty | Pending |
| `G104-S11` | stroke cardinality / Dab domain | family/pressure/tilt 已有 | vector samples 与 dab dabs各至少 1；Dab center finite、size >0、rotation finite、opacity `[0,1]` | minimum 2 / allow empty | 合法 tap/dot 保持；空内容拒绝 | minimum 1 | Pending |

所有 Proposed V1 answer 都是 **PROPOSED UNTIL HUMAN APPROVAL**，不是当前 repository contract。

## 3. ObjectKind 完整提议表

| ID | ObjectKind | branch | accepted version |
| ---: | --- | --- | --- |
| 1 | Shape | `shape` | `[1]` |
| 2 | Image | `image` | `[1]` |
| 3 | VectorPath | `vector_path` | `[1]` |
| 4 | RichText | `rich_text` | `[1]` |
| 5 | VectorStroke | `vector_stroke` | `[1]` |
| 6 | DabStroke | `dab_stroke` | `[1]` |
| 7 | Connector | `connector` | `[1]` |
| 8 | Sticky | `sticky` | `[1]` |
| 9 | Group | `group` | `[1]` |

## 4. A / B / C 的最终分离

| Lane | 当前/拟议责任 | 不是它的责任 |
| --- | --- | --- |
| A | payload-only type、normalize、envelope、structural validation | store state、idempotency、ApplyPlan、golden outcome |
| B | OperationId classification、references、kind applicability、resulting state、prepare plan | 修改 schema version、设计 corpus |
| C | reviewed case intent/outcome、fixture、differential、no-mutation evidence | 用 implementation output 生成 authority、阻塞 A readiness |

`GT-G1-04-C` 的独立交接见 [verification handoff](GT_G1_04_C_VERIFICATION_AUTHORITY_HANDOFF.md)。

## 5. 批准后的预计交付物

在**精确批准**后，才可按实际 authority hierarchy 发布 Current pages / GitHub mirror，并更新
manifest。随后才可产生下列 machine-readable projection（仍需独立验证）：

- `schema/axiom/v1/registry/object_kind_registry_v1.yaml`；
- `schema/axiom/v1/canonical/operation_structural_profile_v1.yaml`；
- `schema/axiom/v1/canonical/semantic_leaf_constraints_v1.yaml`。

预期这些规则可使用当前 Proto carrier 表达；但这不是保证。若 materialization 发现 version
presence 或其他批准区分无法表达，停止为 `WIRE_SCHEMA_CHANGE_REQUIRED`，不能在发布包里暗改
Proto。

## 6. 兼容性、迁移与未回答事项

- 新 version policy 是 deliberate fail-closed；历史 missing/0 input 不会静默成为 V1。
- batch canonical set 会把相同集合的不同输入顺序折叠成同一 canonical value；需要依赖顺序的
  工作必须拆成多个 Operation，或由 leaf-owned sequence 明示。
- 本包不选择 B 的 operation error codes、C 的 stable error stage/path/category，也不选择
  idempotency storage、ApplyPlan representation、Resource availability policy 或任何 sync protocol。
- 上述事项并非本 freeze 的隐含决定；它们保持 Open / later-lane，不能由实现者临时选择。

## 7. Required human approval

若接受本包中所有 `G104-S01..S11` 的建议，请发送且只以如下精确 token 表示发布授权：

```text
APPROVE_G1_04_SEMANTIC_FREEZE_V1
```

收到之前，状态必须保持：

```text
AWAITING_HUMAN_AUTHORITY_APPROVAL
```

不得发布 Current Authority、改 `docs/notion/manifest.yaml`、创建 authority machine projection、
变更 Proto/runtime/verification corpus、合并 authority branch，或开始 G1-04-B/C / G1-05。
