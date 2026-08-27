# G1-04 语义缺口最小化审计

> 状态：**PROPOSED / NOT CURRENT / NOT MANIFEST AUTHORITY**
>
> 目的：为 `GT-G1-04-A` 区分真正尚缺的无状态语义、后续状态相关实现，以及独立的验证语料工作。
> 本文是人工冻结前的审计与建议，不修改既有 Current Authority，也不授权产品实现。
>
> 审计基线：`origin/main@26dcee011f4f35cfca7cd3f1d9a6c115e46853e2`；分类修正见
> `codex/gt-g1-04-operation-apply@b7e9966269fbb4022e18aa332830ef682b6ecb38`。

## 1. 术语和判定方法

- **A0**：编码无关的类型化 `Operation` 表达。
- **A1**：只根据输入值完成的规范化，不读取 `ObjectStore`。
- **A2**：Operation envelope 的无状态检查。
- **A3**：payload 的无状态结构、版本、集合和叶子值检查。
- **B**：必须读取当前 `SemanticDocument` / `ObjectStore` 或推演结果状态的检查、幂等和
  `ApplyPlan`；本轮不实现。
- **C**：人工审阅的 case intent、稳定的预期 outcome、独立 fixture 与 differential
  evidence；本轮不物化。

判断问题只有一个：规则能否仅凭一条输入 Operation 决定？可以的才可能属于 A；需要已有
对象、引用、层级或历史记录的属于 B；只是在说“如何证明”或“失败应报告什么”的属于 C。

本审计只消费 `docs/notion/manifest.yaml` 所列 current authority 及其 current 依赖。旧 POC、
历史 prompt 和当前 Proto 只作为 provenance 或 carrier，不反向成为语义 authority。

## 2. 先前声称缺口的去向

| 先前问题 | 审计结论 | 依据与含义 |
| --- | --- | --- |
| RichText / Stroke 的 descriptor、tag、字段投影 | `MACHINE_PROJECTION_ALREADY_CLOSED_BY_GT_G1_02R` | `GT-G1-02R` 已把该 machine projection refreeze 到当前 main；本轮不得重新设计 RichText 或 Stroke truth。 |
| 15 个 Operation 的人工审阅正/负例、稳定 stage/path/outcome | `MOVED_TO_GT_G1_04_C` | `semantic-conformance-golden-corpus-v0.1.md` 与 Golden Authoring 规则定义的是验证责任；它不是 A 的 semantic unblock 条件。 |
| target 是否存在、对象种类是否可被修改、层级/引用闭包、connector 可连接性 | `MOVED_TO_GT_G1_04_B` | 这些规则需要当前 store 或 resulting state；由 `07-03-operation-semantic-document-v0.1.md` 的 Reference/Kind/Invariant 与 Prepare 阶段负责。 |
| C++ variant、15 个 payload tag、稳定 ID 的 carrier | `CLOSED_BY_EXISTING_CURRENT_AUTHORITY` | Reference IDL、generated proto baseline、operation registry 已锁定 15 种 payload 与 16-byte ID carrier。 |
| 有限数、`-0 → +0`、PropertyBag FieldId 排序、持久 erase mask ID 排序 | `CLOSED_BY_EXISTING_CURRENT_AUTHORITY` | Common Wire、canonical profile、Field Registry 已给出。 |
| `schema_version` / `payload_version` 的 V1 接受与缺失策略 | `TRUE_SEMANTIC_AUTHORITY_GAP` | current authority 规定版本必须遵守兼容规则，却未发布 V1 exact accepted/missing/zero/unknown matrix。 |
| Operation-level repeated payload 的空集、重复键、比较器、规范顺序 | `TRUE_SEMANTIC_AUTHORITY_GAP` | current authority明确有 collection canonicalization 责任，但未逐字段完整物化。 |
| 完整 `ObjectKind → accepted kind_version` 表 | `TRUE_SEMANTIC_AUTHORITY_GAP` | 九个 ObjectKind identity 已锁定；`kind_version` 的完整 accepted registry 尚未发布。 |
| VectorPath 命令文法 | `TRUE_SEMANTIC_AUTHORITY_GAP` | current leaf authority要求结构校验和 renderer-neutral geometry，但尚未给出完整 command grammar。 |
| RichTextDelta version/empty steps 的精确策略 | `TRUE_SEMANTIC_AUTHORITY_GAP` | RichText semantic truth和 02R wire shape 已锁定；`delta_version` accepted/missing/zero/unknown 与空 steps 策略尚未单独闭合。 |
| Stroke / Dab 的最小 cardinality 与 Dab intrinsic 域 | `TRUE_SEMANTIC_AUTHORITY_GAP` | family/representation、pressure/tilt、wire shape 已锁定；样本/点最小数及 `DabInstance` 基础域仍须明确。 |
| Image 的 crop、尺寸、mode、资源二进制可用性 | `CLOSED_BY_EXISTING_CURRENT_AUTHORITY` / `MOVED_TO_GT_G1_04_B` | Image release 已拥有 `sourceRect`/full-rect/presence、有限正尺寸和 mode；资源 bytes availability 不属于 A apply 前置。 |
| Connector endpoint / anchor syntax | `CLOSED_BY_EXISTING_CURRENT_AUTHORITY` | Connector release 已拥有 oneof endpoint、free point、AutoPerimeter 与 StablePort、routing 的静态语义；target existence/connectability 仍是 B。 |
| 当前 Proto 具体如何编写 validator | `IMPLEMENTATION_DECISION_ONLY` | Proto 只说明可携带的值；错误对象、类分解和内部 API 不得反向定义语义。 |

## 3. 所有 15 个 Operation 的 A/B/C 责任矩阵

矩阵中的“提案”只表示本工作包建议由冻结文档闭合；并非现在已接受的规则。

| Operation | A0 类型表示 | A1 规范化 | A2 envelope 依赖 | A3 无状态结构 | B 状态/结果检查 | C 验证责任 |
| --- | --- | --- | --- | --- | --- |
| InsertObjects | 已有 typed branch | object/leaf canonicalization；提案按 object ID 排序 | 共享 envelope version 规则（提案） | objects 非空、ID 唯一、ObjectKind/version、叶子结构 | staged insert、引用与 hierarchy cycle | insert 正/负、原子拒绝、canonical bytes |
| DeleteObjects | 已有 typed branch | 提案按 object ID 排序 | 共享 envelope version 规则（提案） | IDs 非空、非零、唯一 | target existence、connector cascade、最终删除集 | delete/cascade/no-mutation |
| RestoreObjects | 已有 typed branch | object/leaf canonicalization；提案排序 | 共享 envelope version 规则（提案） | objects 非空、ID 唯一、ObjectKind/version、叶子结构 | restore admissibility、引用和 hierarchy | restore/reject/replay |
| SetPlacements | 已有 typed branch | 提案按 target ID 排序；OrderKey 已有 comparator | 共享 envelope version 规则（提案） | items 非空、target 唯一、placement carrier 有效 | target existence、parent/hierarchy/cycle、结果顺序 | placement/cycle/no-mutation |
| SetTransforms | 已有 typed branch | finite、`-0`；提案按 target ID 排序 | 共享 envelope version 规则（提案） | items 非空、target 唯一、transform 有限 | target existence、kind applicability | transform numeric boundaries |
| PatchProperties | 已有 typed branch | Field Registry default elision；提案按 `(object_id, field_id)` 排序 | 共享 envelope version 规则（提案） | patches 非空、key 唯一、SET/CLEAR presence、field/value carrier | target existence、field applicability | property SET/CLEAR/duplicate/no-mutation |
| SetObjectSize | 已有 typed branch | finite、`-0`；提案按 target ID 排序 | 共享 envelope version 规则（提案） | items 非空、target 唯一、尺寸结构域 | target existence、kind may resize | size/kind/no-mutation |
| SetVectorPathGeometry | 已有 typed branch | finite、`-0`、Path grammar（提案） | 共享 envelope version 规则（提案） | target ID 与 geometry presence、FillRule、commands grammar | target existence、VectorPath kind | grammar/canonical geometry |
| SetImageContent | 已有 typed branch | Image release 的 presence/full crop canonicalization | 共享 envelope version 规则（提案） | target ID、ImageContent 静态域 | target existence、Image kind；资源 lifecycle 不在 A | image crop/mode/atomic reject |
| AddStroke | 已有 typed branch | stroke canonicalization；leaf cardinality（提案） | 共享 envelope version 规则（提案） | ObjectRecord、kind/version、representation/brush/leaf结构 | new object collision、placement/reference constraints | vector/dab/family cases |
| SplitStrokes | 已有 typed branch | 提案按 source ID 与 replacement ID 排序 | 共享 envelope version 规则（提案） | splits 非空、source 唯一、replacements 非空且 ID 无冲突 | source exists/is Stroke、fragment semantics、resulting state | segment erase / atomicity |
| AddEraseMasks | 已有 typed branch | 提案 outer target、inner mask ID 排序 | 共享 envelope version 规则（提案） | items/masks 非空、target/mask key 唯一、mask geometry | target exists/is erasable、mask relation | mask add/no-mutation |
| RemoveEraseMasks | 已有 typed branch | 提案 outer target、inner mask ID 排序 | 共享 envelope version 规则（提案） | items/mask IDs 非空、target/mask key 唯一 | target/mask existence、erasable kind | mask removal/no-mutation |
| EditRichText | 已有 typed branch | 已有 RichText normalization；delta policy补充（提案） | 共享 envelope version 规则（提案） | target ID、delta version、ordered nonempty steps、每 step oneof | paragraph/range/staged text validity | ordered delta/negative outcome |
| SetConnectorContent | 已有 typed branch | endpoint/anchor/routing静态 canonicalization | 共享 envelope version 规则（提案） | target ID、endpoint oneof、有限点、port/routing枚举 | target/attached target existence、connectability、graph | endpoint/anchor/no-mutation |

## 4. Operation-level repeated collection 逐项审计

以下表只处理 Operation payload 中的 batch collection。`RichTextDelta.steps`、
`VectorPathGeometry.commands`、`VectorStrokeData.samples` 和 `DabStrokeData.dabs` 是各自 leaf
的 sequence，不应错误并入本表。

| 字段 | current evidence | 尚缺部分 | 建议归属 |
| --- | --- | --- | --- |
| `InsertObjects.objects` | Operation payload 的 staged insert；ObjectRecord 语义 | 空集、key、跨 item duplicate、排序 | `G104-S03` |
| `DeleteObjects.object_ids` | Delete 是整对象 mutation | 空集、key、duplicate、排序 | `G104-S03` |
| `RestoreObjects.objects` | Restore 以 ObjectRecord 为载荷 | 空集、key、duplicate、排序 | `G104-S03` |
| `SetPlacements.items` | Placement 是 `(parentId, orderKey)` 原子语义 | target uniqueness 与 batch canonical order | `G104-S04` |
| `SetTransforms.items` | Transform 为独立 semantic carrier | target uniqueness 与 batch canonical order | `G104-S04` |
| `PatchProperties.patches` | Field registry 与 property bag 已锁定 | `(objectId, fieldId)` key 与完整 batch policy | `G104-S05` |
| `SetObjectSize.items` | size carrier 已存在 | target uniqueness 与 batch canonical order | `G104-S04` |
| `SplitStrokes.splits` | source/replacement 的领域含义已知 | source key、empty、排序、跨 replacement 冲突 | `G104-S06` |
| `StrokeSplit.replacements` | fragment ObjectRecord carrier 已存在 | empty、object ID uniqueness、canonical order | `G104-S06` |
| `AddEraseMasks.items` | object-local mask 语义与 mask ID 已有 | target batch key/empty/order | `G104-S07` |
| `EraseMaskAddItem.masks` | persistent `erase_masks` 已有 ID comparator | inner empty、重复与 order 的 operation 明示规则 | `G104-S07` |
| `RemoveEraseMasks.items` | object-local remove carrier 已存在 | target batch key/empty/order | `G104-S07` |
| `EraseMaskRemoveItem.mask_ids` | mask identity 已有 | inner empty、重复与 order 的 operation 明示规则 | `G104-S07` |

## 5. 叶子最小化结果

| 领域 | A 审计结论 | 不属于 A 的部分 |
| --- | --- | --- |
| VectorPath | command vocabulary/grammar 是 true gap；renderer tessellation 永不进入本 closure | target kind、derived SkPath/mesh |
| NormalizedRect / Image | 无新 A 规则；已由 Image release 与 Geometry Types 拥有 | Resource bytes availability 与 resource lifecycle |
| RichText | 只补 `delta_version` 与 empty steps；不重开 tag/ParagraphStyle/字体/UTF-8/相邻 run 合并 | scalar range、paragraph existence、staged state |
| Stroke / Brush | 只补 minimum cardinality 和 Dab intrinsic domain；不重开 02R 或 family/pressure/tilt/seed truth | source object / resulting fragment state |
| Connector | 无新 A 规则；endpoint、anchor、routing 静态语义已拥有 | attached target existence/connectability/graph |

## 6. 真正的 A semantic gaps

1. `G104-S01`：Operation `schema_version` V1 matrix。
2. `G104-S02`：Operation `payload_version` V1 matrix。
3. `G104-S03..S07`：所有 Operation-level batch 的 collection contract。
4. `G104-S08`：九种 ObjectKind 的 version registry。
5. `G104-S09`：VectorPath stateless command grammar。
6. `G104-S10`：RichTextDelta version 与 steps 最小结构。
7. `G104-S11`：Vector/Dab stroke minimum cardinality 与 Dab intrinsic domain。

## 7. 非 A blocker

- `GT-G1-02R` machine projection：**closed**。
- 15-operation reviewed corpus：**GT-G1-04-C**。
- target/reference/resulting-state/idempotency/ApplyPlan：**GT-G1-04-B**。
- validator class、error object、日志与诊断格式：**implementation decision only**，但不得违背未来 C
  人工冻结的 outcome。

## 8. 审计结论

提案文档能够把 A 的缺口限制在上述 11 个可见决策，不得扩大为重写 04 Semantic Schema。批准前
`GT-G1-04-A` 仍为 `BLOCKED_AUTHORITY`；`GT-G1-04-C` 仍为
`BLOCKED_VERIFICATION_MATERIALIZATION`；`GT-G1-05` 仍未获授权。
