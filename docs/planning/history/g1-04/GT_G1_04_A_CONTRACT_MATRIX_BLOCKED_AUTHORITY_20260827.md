# GT-G1-04-A Contract Matrix

> 状态：`Blocked — BLOCKED_AUTHORITY`
>
> Gate Task：`GT-G1-04-A`
>
> Notion Task ID：`G1/Task 4`
>
> Authority baseline：`origin/main @ 39d44d289680e4ddaa5ae48a06e24aa579ee6326`
>
> Reconciliation commit：`68aba58b3cdb5292127c9e36f0128b290fc3fbe1`
>
> 对账日期：2026-08-27

## 术语

- **typed payload（类型化载荷）**：用 C++ 封闭 `std::variant` 表示 Operation 的 15 个分支，
  不暴露 Protobuf DTO。
- **结构验证**：不读取 ObjectStore 便可确定的 ID、版本、枚举、数值、集合和嵌套叶子规则。
  目标存在性、对象类型兼容、层级环和 resulting-state 不属于本工作包。
- **Authority gap（权威缺口）**：当前 manifest 允许消费的冻结来源没有给出某个必须行为；该行为
  不能由实现或测试临时决定。

## 1. 结论

当前 `operation.proto`、Operation Registry 和已冻结 leaf schema 足以物化 15 个 encoding-neutral
payload 分支，也足以实现已明确的通用 Normalize：有限 f64/f32、`-0 → +0`、PropertyBag 的
FieldId 升序/唯一性，以及持久 EraseMask 的 MaskId 无符号字典序/唯一性。

但是当前 authority 不能完整定义 GT-G1-04-A 所要求的每个 operation 的结构验证。缺口包括：

1. Operation `schemaVersion` 与 `payloadVersion` 的 V1 接受矩阵、零值/缺失语义；
2. 除 PropertyBag 和持久 `erase_masks` 外，Operation repeated payload 的空集、同一目标重复、
   排序或集合语义；
3. 完整 ObjectKind → accepted `kind_version` 表；
4. VectorPath 命令序列语法、NormalizedRect 精确域、RichText step 结构与九级 weight 的实际域；
5. 人工审阅的 15-operation negative intent。**这是历史上与 A 混列的 verification 缺口，
   已由下方 Re-entry Review 纠正并移交 GT-G1-04-C**；现有 semantic conformance 文档仍是
   Draft / Freeze Candidate，不能把实现诊断升格为 protocol outcome。

因此本任务不得写出会固化这些选择的 normalizer 或 payload validator；状态为
`BLOCKED_AUTHORITY`。未创建、未修改产品 C++、schema、registry 或 codec truth。

## 2. Authority baseline

| 议题 | 当前可消费来源 | 已知结论 |
| --- | --- | --- |
| 15 branch identity | `operation-payload-validation-v0.1.md`、`operation_registry_v1.yaml`、`operation.proto` | tag `1..15` 已冻结。 |
| Operation-only / staged boundary | `07-03-operation-semantic-document-v0.1.md`、ADR-0025 | A 只到 Normalize → Envelope → Payload structural validation。 |
| 通用数值 | Common Wire、`canonical_profile_v1.yaml`、ADR-0026 | f64/f32 有限，`-0 → +0`，不得 clamp。 |
| 已冻结 collection | `canonical_profile_v1.yaml`、`field_registry_v1.yaml` | PropertyBag 按 FieldId 升序且唯一；持久 EraseMask 按 MaskId 唯一有序。 |
| 已冻结 limits | `protocol_hard_limits_v1.yaml` | bytes、UTF-8、generic keyed batch、EraseMask、geometry、OrderKey 限制已定义。 |
| 必须递延 | `07-03-operation-semantic-document-v0.1.md` | Idempotency、reference/kind/resulting-state、cascade closure 和 ApplyPlan 属于 B 或后续。 |

## 3. Typed Operation Envelope

目标 semantic domain envelope：

```text
Operation {
  operation_id: OperationId
  document_id: DocumentId (strong semantic identity)
  schema_version: uint32
  payload_version: uint32
  payload: closed typed union
}
```

OperationKind 必须从 payload variant 派生，不能在有效 semantic Operation 内独立写入。DTO 只可
通过 `codec / mapper → semantic Operation` 进入这个边界；public header 不得公开 protobuf。

### Envelope validation matrix

| 检查 | 状态 | 理由 |
| --- | --- | --- |
| OperationId 为 16 bytes 且非零 | 可实现 | Common Wire / canonical profile 已冻结。 |
| DocumentId 为 16 bytes 且非零 | 可实现 | Common Wire / canonical profile 已冻结。 |
| payload 恰好一个、tag 为 1..15 | 可实现 | Operation oneof / registry 已冻结。 |
| schemaVersion accepted V1 value 与缺失/零值策略 | **Blocked** | 当前 authority 未给接受矩阵。 |
| payloadVersion accepted V1 value 与缺失/零值策略 | **Blocked** | 当前 authority 未给接受矩阵。 |

## 4. Fifteen payload variants

| Tag / Operation | C++ typed branch | 当前可确认的无状态规则 | 尚缺的最小 closure |
| --- | --- | --- | --- |
| 1 `InsertObjects` | `InsertObjectsOp{vector<ObjectRecord>}` | ObjectRecord ID、ObjectKind、有限数值、PropertyBag、EraseMask 通用规则。 | 空集、同 ID 重复、ObjectKind version 表。 |
| 2 `DeleteObjects` | `DeleteObjectsOp{vector<ObjectId>}` | 每个 ID 非零；batch 上限可消费。 | 空集、重复 ID、sequence/set 语义。 |
| 3 `RestoreObjects` | `RestoreObjectsOp{vector<ObjectRecord>}` | 同 InsertObjects。 | 同 InsertObjects；restore 内部顺序。 |
| 4 `SetPlacements` | `SetPlacementsOp{vector<PlacementItem>}` | target ID 非零；OrderKey 1..32、尾字节非零；parent ID 如有则非零。 | 空集、同 target 重复/排序；cycle 属于 B。 |
| 5 `SetTransforms` | `SetTransformsOp{vector<TransformItem>}` | target ID 非零；6 个 f64 有限并规范化。 | 空集、同 target 重复/排序。 |
| 6 `PatchProperties` | `PatchPropertiesOp{vector<PropertyPatch>}` | target ID 非零；FieldId 在 registry；value branch 匹配类型；action 已知。 | set/clear value presence；同 key 重复/排序；空集。 |
| 7 `SetObjectSize` | `SetObjectSizeOp{vector<ObjectSizeItem>}` | target ID 非零；尺寸有限；Shape 的 released size 为严格正。 | 其他 kind 域、完整尺寸域、重复/空集。 |
| 8 `SetVectorPathGeometry` | `SetVectorPathGeometryOp{ObjectId, VectorPathGeometry}` | target ID 非零；FillRule 已知；路径数值有限。 | command grammar、空 path、count/closed policy。 |
| 9 `SetImageContent` | `SetImageContentOp{ObjectId, ImageContent}` | target/resource ID 非零；mode 已知；数值有限。 | NormalizedRect 精确域、尺寸域、presence policy。 |
| 10 `AddStroke` | `AddStrokeOp{ObjectRecord}` | ObjectRecord 通用规则；BrushFamily `(family,version)` 与 representation 可查。 | sample cardinality、pressure/tilt/opacity 域、kindVersion 表。 |
| 11 `SplitStrokes` | `SplitStrokesOp{vector<StrokeSplit>}` | source/replacement ID 与 ObjectRecord 通用规则。 | 空集、重复 ID、排序/set 语义。 |
| 12 `AddEraseMasks` | `AddEraseMasksOp{vector<EraseMaskAddItem>}` | object/mask ID 非零；geometry 数值有限。 | payload item/mask 空集、重复、排序继承规则。 |
| 13 `RemoveEraseMasks` | `RemoveEraseMasksOp{vector<EraseMaskRemoveItem>}` | object/mask ID 非零；batch 上限可消费。 | 重复、顺序、空集语义。 |
| 14 `EditRichText` | `EditRichTextOp{ObjectId, RichTextDelta}` | target/paragraph ID 非零；step oneof；font resource semantic-required；数值有限。 | step required fields、delta cardinality/order、scalar range、weight 表。 |
| 15 `SetConnectorContent` | `SetConnectorContentOp{ObjectId, ConnectorContent}` | target ID 非零；点数值有限；attached target 非零；endpoint/anchor oneof；routing 已知。 | anchor/port 域与 presence；target connectability 属于 B。 |

## 5. Normalization matrix

下表只记录当前 manifest 允许消费的 authority 已经明确的规范化。没有明确
canonical comparator、presence 语义或数值域的字段保持 `Blocked`；本任务不得以
实现选择补齐规则。

| 字段族 | Authority | Normalize action | Reject condition | 是否需要状态查询 |
| --- | --- | --- | --- | --- |
| `OperationId` / `DocumentId` | Common Wire、`canonical_profile_v1.yaml` | 保持 16-byte identity；不做重写 | 长度不是 16 bytes 或全零 | 否 |
| 所有可达 `f32` / `f64` | Common Wire、canonical profile、对应 leaf authority | `-0 → +0`；其余有限值保持语义值 | NaN、±Infinity；超出已明确 leaf 域 | 否 |
| `OrderKey` | Order Key RFC、canonical profile、hard limits | 保持 unsigned-byte lexical 表示 | 长度不在 1..32 或尾字节为零 | 否 |
| `PropertyBag` entries | Field Registry、canonical profile | 按 `FieldId` 升序；显式等于默认值的 entry 按 registry 规则省略 | `FieldId` 重复；未知 FieldId；value branch 与 registry 类型不匹配 | 否（ObjectKind applicability 递延） |
| 持久 `erase_masks` | canonical profile、EraseMask authority | 按 `MaskId` unsigned-byte lexical 升序 | `MaskId` 重复 | 否 |
| UTF-8 字符串 | Common Wire、hard limits | 不做 Unicode/locale 重写；保留语义字节 | 非法 UTF-8 或超过对应字节上限 | 否 |
| released enum / registry identity | Common Wire、operation/shape/brush/field registry | 不把未知值映射为平台 fallback；保留已知 identity | 未知或不支持的 enum、kind/version、brush pair | 否（target applicability 递延） |
| 一般 repeated payload entries | Operation Payload authority | **不排序、不去重**；等待该字段的 sequence/set 语义 closure | 仅在 authority 明确重复/空集规则后拒绝 | 否 |
| VectorPath / Image / Connector / RichText leaf 数值与集合 | 对应 leaf authority | 仅递归应用已明确的有限值与 `-0` 规则 | 未冻结的 grammar、presence、精确域不能在 A 中推断 | 否 |

## 6. Explicitly deferred to GT-G1-04-B

以下检查需要当前 Document/ObjectStore、跨对象关系或 resulting-state，因此不属于
A；A 不应创建占位实现或查询 `ObjectStore`：

- target `ObjectId` 是否存在，以及重复 create/restore 是否与当前状态冲突；
- target 的 `ObjectKind` / `kind_version` 是否与 operation 兼容；
- parent 是否存在、placement 是否形成层级环或违反整批层级约束；
- connector endpoint 的 target、connectability、anchor/port 是否与当前对象状态兼容；
- RichText delta 是否适用于当前文档内容与游标/段落状态；
- split source stroke 是否存在、类型是否匹配、replacement 是否与现有对象冲突；
- mask target 是否支持 erase masks，以及 Add/Remove 对当前 mask 集合的影响；
- DeleteObjects 的 connector cascade fixed-point closure；
- resulting-state invariant、跨对象 atomic apply 计划与 before-image；
- OperationId 幂等分类（`AlreadyApplied` / collision）及其持久去重状态；
- `ApplyPlan`、`SemanticDocument`、`SemanticGeneration`、`ChangeSet`、History、Snapshot、Replay
  或任何 canonical mutation。

## 7. Error / diagnostic authority

### 已冻结、可作为规范依据的内容

- 阶段顺序：Decode / wire preflight → Envelope → Payload → 后续 B 阶段；
- `protocol_hard_limits_v1.yaml` 中的安全类别：`WIRE_SIZE_LIMIT_EXCEEDED`、
  `OBJECT_SIZE_LIMIT_EXCEEDED`、`COLLECTION_LIMIT_EXCEEDED`、`GEOMETRY_LIMIT_EXCEEDED`、
  `STRING_LIMIT_EXCEEDED`、`DECODE_BUDGET_EXCEEDED`、`INTEGER_OVERFLOW`、
  `TRUNCATED_LENGTH_DELIMITED_FIELD`；
- overflow、truncate、clamp、partial apply 和 duplicate canonical key 的拒绝原则；
- Operation kind/tag `1..15`、ID 编码、canonical numeric 规则和已列出的 registry comparator。

### 仅限实现内部的诊断

实现可以为测试返回结构化的 `stage`、`path`、`category` 和内部诊断文本，以保证确定性
和可调试性；但在当前没有审阅式 15-operation negative corpus 的情况下，这些内部代码、
路径优先级和文本不是 protocol authority，也不能成为 golden expected output。

### GT-G1-04-C verification lane（不属于 A semantic blocker）

`semantic-conformance-golden-corpus-v0.1.md` 仍是 Draft / Freeze Candidate。因而每个
operation 的 negative intent、稳定的 stage/path/outcome 组合、同一阶段多个错误的优先级，
都必须先经过人工审阅并物化到 GT-G1-04-C verification authority；不能从 production validator
的输出反向生成 expected answer，也不能用 protobuf exception string 充当 semantic oracle。
这项缺口仍然阻塞 GT-G1-04 的最终验证，但不阻塞已经由 A semantic authority 定义完整的
A0–A3 实现。

## 8. Required authority closure

要解除本 A 包的 `BLOCKED_AUTHORITY`，最小闭环不是实现更多代码，而是由 Architecture /
Semantic Authority 发布并进入 manifest/current mirror 的下列**语义**决定。15-operation
reviewed corpus 不在本清单中；它由 GT-G1-04-C 单独拥有：

1. Operation `schemaVersion` 与 `payloadVersion` 的 V1 accepted matrix，以及 missing/zero 的明确策略；
2. 除 PropertyBag 和持久 `erase_masks` 外，每个 repeated operation payload 的空集、重复 target/key、
   sequence 或 canonical set 语义，以及 set 的 comparator；
3. 完整的 `ObjectKind → accepted kind_version` 表（不能把当前部分 registry 推断为完整表）；
4. VectorPath command grammar、空 path、闭合规则、count policy；
5. `NormalizedRect` 的精确数值域、presence/default 规则与边界行为；
6. RichTextStep 每个 oneof 分支的 required fields、delta cardinality/order、scalar range 和九级
   font-weight 数值表；
7. AddStroke 的 sample cardinality、pressure/tilt/opacity 精确域与 kind/version 适用性；
8. SetConnectorContent 的 anchor/port/presence 规则（target existence 仍留给 B）；
9. （不属于 A）人工审阅的 15-operation positive/negative case intent，以及冻结的
   stage/path/outcome，供独立 verification-only fixture compiler 和 runner 消费；该项保留在
   GT-G1-04-C，不作为 A 的 unblock 条件。

在前 1–8 项 semantic closure 到位前，继续实现 normalizer 或 payload validator 会把未批准
的行为写入公共语义边界，因此本工作包在 Contract Matrix 阶段停止。第 9 项只决定 C 的
verification materialization，不改变 A 的 semantic readiness。

## Re-entry Review after GT-G1-02R

### 时间线与复审边界

本节不替换上述初始对账，也不将历史 `GT-G1-02` 的通过记录改写为失败。复审时间线为：

```text
初始 GT-G1-04-A 对账
→ 识别 authority 缺口并阻塞
→ RichText / Stroke machine projection drift
→ authority publication
→ GT-G1-02R machine projection refreeze
→ GT-G1-02R exact-source hosted validation
→ main integration
→ 本次 G1-04-A re-entry classification
```

`GT-G1-02R` 关闭的范围仅为 RichText / Stroke 从 semantic authority 到 Proto、descriptor、
codec 与 golden 的机器投影一致性。其 source commit 为
`ac92939e70f0bbbf85f7ae126595f0e5522d4f7d`，durable Evidence commit 为
`26dcee011f4f35cfca7cd3f1d9a6c115e46853e2`；该 Evidence 绑定的 hosted run 为
[G1 Semantic Codec #33041434455](https://github.com/Mostorm-Labs/axiom/actions/runs/33041434455)。
当前 main 是 `26dcee011f4f35cfca7cd3f1d9a6c115e46853e2`；本分支以正常 merge
`aa90bd65427b98fec584dcc23079985a739123d6` 吸收该基线。

它**没有**关闭 Operation 的 version、repeated payload、ObjectKind version 或其他 leaf
结构语义。因而不能由“02R 已集成”推导出 `GT-G1-04-A` 已 Ready。

### 当前分类

| 子包 / 议题 | 当前分类 | 复审结论 |
| --- | --- | --- |
| GT-G1-02R RichText / Stroke machine projection | `CLOSED_BY_GT_G1_02R` | Proto、descriptor、codec、BG 与 23-case leaf differential 的投影一致性已闭合。 |
| A0 Typed Operation Domain | `READY` | 可以继续维护 encoding-neutral typed domain 的设计边界；本轮未实现产品代码。 |
| A1 Common Frozen Normalization | `PARTIALLY_READY` | finite、`-0`、PropertyBag、持久 EraseMask 已冻结；一般 repeated payload 仍无语义 closure。 |
| A2 Envelope Validation | `BLOCKED_AUTHORITY` | `schemaVersion` / `payloadVersion` 的 accepted、missing、zero policy 尚未发布。 |
| A3 Payload Structural Validation | `BLOCKED_AUTHORITY` | repeated semantics、`ObjectKind → kind_version` 与多个 leaf structural rule 尚未闭合。 |
| VectorPath grammar | `OPEN_SEMANTIC_AUTHORITY` | command grammar、空 path、closed/count policy 未冻结。 |
| Image / NormalizedRect | `OPEN_SEMANTIC_AUTHORITY` | 精确数值域、presence/default 与边界行为未冻结。 |
| RichText | `OPEN_SEMANTIC_AUTHORITY` | 02R 已关闭 machine projection；完整 required/presence/cardinality/scalar-range validation 仍无 closure。 |
| Stroke | `OPEN_SEMANTIC_AUTHORITY` | 02R 已关闭 machine projection；sample cardinality、pressure/tilt/opacity 域及 kind-version applicability 仍无 closure。 |
| Connector | `OPEN_SEMANTIC_AUTHORITY` | tagged endpoint/static outer structure 已有；anchor/port/presence 仍待 A closure，target existence/connectability 属于 B。 |
| 15-operation reviewed corpus | `MOVED_TO_GT_G1_04_C` / `BLOCKED_VERIFICATION_MATERIALIZATION` | 它是 GT-G1-04-C 的 verification materialization 前置，不再冒充 A0 architecture prerequisite。 |

### 尚未物化的最小 authority closure

截至当前 manifest/current authority，以下三项目标 semantic authority 文件或等价 current
manifest 条目均未 materialize；历史、superseded 与 draft 内容只能作为 provenance，不能补齐
缺口。G1-04-A closure gate 是它们的组合复核，不另行引入一套语义：

1. `Operation Structural Semantics V1 Closure`：定义 envelope version policy 与每类 repeated
   payload 的 sequence/set、空集、重复和 comparator 语义。
2. `ObjectKind Version Registry V1 Release`：发布完整 `ObjectKind → accepted kind_version`
   registry 与适用性边界。
3. `Semantic Leaf Structural Validation Closure V1`：关闭 VectorPath、NormalizedRect、RichText、
   Stroke 与 Connector 的尚缺 structural validation 规则。
4. `G1-04 Semantic Authority Closure Gate`：作为三项 semantic authority 的组合复核；不得
   把 GT-G1-04-C 的 reviewed intent、negative stage/path/outcome 当作 A 的输入。

最小恢复动作是先由 authority 发布并在 `docs/notion/manifest.yaml` 的 current set 中列出这三
项 semantic closure（或其等价规范），随后重新执行 A0–A3 对账。此时才可决定 A 是否从
`BLOCKED_AUTHORITY` 变为 `READY`。C 的 reviewed corpus 仍需另行物化，但不回阻 A；在此之前，
下一步是 authority closure，**不是 C++**。

### Re-entry 最终状态

`GT-G1-04-A = BLOCKED_AUTHORITY`。本次复审的 overall status 为
`PASS_REENTRY_BLOCKED_AUTHORITY`：02R 的集成和 re-entry 审计均已完成，但 A 的实现路径按
manifest authority-gap policy 必须停止。
