# Operation Structural Semantics V1 Closure v0.1（提案）

> 状态：**PROPOSED / NOT CURRENT / NOT MANIFEST AUTHORITY**
>
> 本文只提出 `GT-G1-04-A` 所需的无状态 Operation 结构语义。它不发布 Current
> Authority、不修改 wire format，也不定义 ObjectStore、idempotency、ApplyPlan 或错误 golden。

## 1. 范围与来源

本提案组合当前的 Operation Payload、Common Wire、OrderKey、Reference IDL、generated Proto
baseline、Field Registry 与 hard-limit authority。它补足这些 authority 已要求、但尚未逐字段
物化的 V1 policy。所有 `G104-Sxx` 决定均为 **PROPOSED UNTIL HUMAN APPROVAL**。

`Operation` 是唯一 canonical mutation。一次 Operation 的完整 pipeline 仍为：wire preflight →
decode/typed view → normalize → envelope → payload structural → B 的 stateful validation →
prepare → atomic apply。任一 A 阶段 reject 均不产生 mutation、generation、ChangeSet 或输出。

## 2. Envelope version matrix

| Decision | 字段 | current evidence | 选项 | 建议 V1 决定 | 兼容性后果 |
| --- | --- | --- | --- | --- | --- |
| `G104-S01` | `schema_version` | Common Wire 要求版本兼容行为，但未给 V1 exact matrix | A: 仅 1；B: missing/0 默认 1 | **接受且仅接受 `1`；missing、0、未知值均 reject** | 旧的未标版本 payload 不能静默进入 V1；升级必须显式版本化。 |
| `G104-S02` | `payload_version` | 同上 | A: 仅 1；B: 0 视为 1 | **接受且仅接受 `1`；missing、0、未知值均 reject** | 防止某个 payload family 以默认值绕过未来不兼容变更。 |

`schema_version` 和 `payload_version` 在当前 protobuf carrier 中是 scalar。WirePreflight 必须保留
field occurrence；“missing”与“explicit zero”都 reject，但 C 将来是否对二者冻结不同的
stage/category 是验证 authority 的责任，不由本文决定。此建议不需要 Proto change。

## 3. 共通 batch 规则

除 leaf authority 明确为 sequence 的字段外，以下 Operation-level batch **提案**采用
`KEYED_CANONICAL_SET`：

- 空集合 `REJECT`，不能形成 silent no-op；
- key 重复 `REJECT`，不能通过“last write wins”暗改 payload 的意义；
- normalize 后按声明 key 的 unsigned bytes lexicographic comparator 排序；复合 key 从左至右
  比较；
- canonical writer 按规范化顺序编码；输入顺序不构成语义；
- 所有 ID 必须是有效非零 16-byte identity，适用的 hard limits 必须先于 allocation 执行。

这是对 operation batch 的提案，不改变 OrderedSequence leaf 的含义。

## 4. 逐字段 collection profile

| Decision | 字段 | 语义 kind | empty | key | duplicate | comparator / canonical encoding order | 证据、替代与建议理由 |
| --- | --- | --- | --- | --- | --- | --- |
| `G104-S03` | `InsertObjects.objects` | **提案** keyed canonical set | reject | `ObjectRecord.id` | reject | `id` unsigned 16-byte 升序 | 批量 insert 无输入顺序语义；替代为 sequence 会把无关入参顺序写入 canonical bytes。 |
| `G104-S03` | `DeleteObjects.object_ids` | **提案** keyed canonical set | reject | `object_id` | reject | `object_id` unsigned 16-byte 升序 | delete 目标的次序不改变最终集合；替代为 sequence 无可观察价值。 |
| `G104-S03` | `RestoreObjects.objects` | **提案** keyed canonical set | reject | `ObjectRecord.id` | reject | `id` unsigned 16-byte 升序 | restore 是对象集回填，不以输入顺序定义语义。 |
| `G104-S04` | `SetPlacements.items` | **提案** keyed canonical set | reject | `PlacementItem.object_id` | reject | `object_id` unsigned 16-byte 升序 | 每个 target 一次 atomic placement；`OrderKey` 是 item value，不是 batch comparator。 |
| `G104-S04` | `SetTransforms.items` | **提案** keyed canonical set | reject | `TransformItem.object_id` | reject | `object_id` unsigned 16-byte 升序 | 同 target 的两个 transform 没有明示 sequential semantics，应 reject 而非覆写。 |
| `G104-S05` | `PatchProperties.patches` | **提案** keyed canonical set | reject | `(object_id, field_id)` | reject | 先 `object_id` unsigned 16-byte，再 unsigned `field_id` 升序 | Field Registry 按 FieldId 管理 property；同一对象同一 field 双 patch 会引入隐式 order effect。 |
| `G104-S04` | `SetObjectSize.items` | **提案** keyed canonical set | reject | `ObjectSizeItem.object_id` | reject | `object_id` unsigned 16-byte 升序 | 同 target 双 size 没有独立可见的 sequential contract。 |
| `G104-S06` | `SplitStrokes.splits` | **提案** keyed canonical set | reject | `StrokeSplit.source_stroke_id` | reject | `source_stroke_id` unsigned 16-byte 升序 | 每个 source stroke 只能在同一 Operation 被 split 一次；替代为 sequence 会允许冲突修复。 |
| `G104-S06` | `StrokeSplit.replacements` | **提案** keyed canonical set | reject | `ObjectRecord.id` | reject；同一 Operation 任意 split 间也 reject ID 冲突 | `id` unsigned 16-byte 升序 | segment split 应保留未擦除 fragment；空 replacement 会与 whole-object Delete 语义重叠。 |
| `G104-S07` | `AddEraseMasks.items` | **提案** keyed canonical set | reject | `EraseMaskAddItem.object_id` | reject | `object_id` unsigned 16-byte 升序 | 同一对象的 mask 添加在本 Operation 只表达一个集合。 |
| `G104-S07` | `EraseMaskAddItem.masks` | **提案** keyed canonical set | reject | `EraseMaskRecord.mask_id` | reject；同一 Operation 跨 item 也 reject ID 冲突 | `mask_id` unsigned 16-byte 升序 | mask 是 stable identity，不能依赖 insertion order 或重复覆盖。 |
| `G104-S07` | `RemoveEraseMasks.items` | **提案** keyed canonical set | reject | `EraseMaskRemoveItem.object_id` | reject | `object_id` unsigned 16-byte 升序 | 每对象一次 removal 集合，无顺序 effect。 |
| `G104-S07` | `EraseMaskRemoveItem.mask_ids` | **提案** keyed canonical set | reject | `mask_id` | reject | `mask_id` unsigned 16-byte 升序 | 相同 mask 的多次 remove 不得被当成幂等/状态问题而留给 A；payload 本身应无重复。 |

`RichTextDelta.steps`、`VectorPathGeometry.commands`、`VectorStrokeData.samples`、
`DabStrokeData.dabs` 不在本表：它们是 leaf-owned ordered sequence，分别由
`G104-S09..S11` 或已有 leaf authority 解释。

## 5. PropertyPatch action / value presence

`G104-S05` 同时提出以下无状态规则：

| action | `value` | 建议 |
| --- | --- | --- |
| `PROPERTY_PATCH_SET` | 必须 present | `value` 必须是 Field Registry 中该 `field_id` 的有效 tagged value；缺失 reject。 |
| `PROPERTY_PATCH_CLEAR` | 必须 absent | 带 value 的 CLEAR reject；clear 的实际 default/removal 语义继续由 Field Registry 决定。 |
| `PROPERTY_PATCH_INVALID` 或未知 | 任意 | reject；不得回退为 SET/CLEAR。 |

Field 是否适用于**既有** target ObjectKind 是 B；但 field ID 是否已发布、value branch 是否匹配
registry 是 A3。

## 6. A 与 B 的明确边界

本文 A 阶段可以决定：版本、oneof presence、ID carrier、有限数、静态 enum/registry、batch
empty/duplicate/order、ObjectRecord/leaf structure 与 `PropertyPatch` presence。

本文不决定：target 是否存在；插入是否与现有对象碰撞；restore 是否允许；parent/reference 是否
存在；connector target 是否 connectable；hierarchy cycle；mask 是否已存在；OperationId 是否已见；
以及任何 `ApplyPlan`。这些由 `GT-G1-04-B` 按当前 state 处理。

本文也不定义 error code、stage/path 文本或 golden expected。它们由 `GT-G1-04-C` 人工审阅后
冻结。

## 7. 批准后的实现影响（非授权实施）

批准后可生成的 machine projection 仅应表达本页的 version matrix、13 个 operation batch
collection profile 和 PropertyPatch presence。若 implementation 发现当前 carrier 无法区分批准的
语义，必须停止并报告 `WIRE_SCHEMA_CHANGE_REQUIRED`，不得自行修改 Proto。
