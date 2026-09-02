# GT-G1-04-A Contract Matrix

> 状态：`READY_FOR_A0_A3_IMPLEMENTATION`
>
> Gate Task：`GT-G1-04-A`
>
> Current authority baseline：`origin/main @ 17ff844f3b72e1976dee248550aa80f59ad38990`
>
> 历史 `BLOCKED_AUTHORITY` matrix 已归档到 `docs/planning/history/g1-04/`。

## 1. Execution Boundary

A 只拥有不读取当前 ObjectStore 即可确定的 semantic input contract：

`typed Operation → normalize → envelope validation → payload structural validation`

A 不拥有 idempotency、reference/kind/resulting-state、ApplyPlan、Atomic Apply 或 golden expected
outcome。上述边界分别属于 B、G1-05 与 C。

## 2. A0 — Typed Operation Domain

Semantic envelope：

```text
Operation {
  operation_id: Id128
  document_id: Id128
  schema_version: uint32
  payload_version: uint32
  payload: closed variant<15 operation payloads>
}
```

规则：

- `operation_id` / `document_id`：16 bytes、非零；
- payload exactly-one；tag 仅 `1..15`；
- semantic `OperationKind` 从 active payload branch 派生，不维护第二份可漂移 discriminator；
- public semantic header 不暴露 protobuf DTO；
- WirePreflight / mapper 必须能向 A2 提供 version field occurrence fact。

## 3. A1 — Common Normalization

| 范围 | Canonical rule |
| --- | --- |
| f32/f64 | NaN/Infinity reject；`-0 → +0`；不得 clamp。 |
| Id128 | 保持 16-byte identity；zero reject。 |
| PropertyBag | 按 FieldId 升序、唯一；value branch 必须匹配 registry。 |
| ObjectRecord persistent erase masks | 按 MaskId unsigned-byte lexicographic 升序、唯一。 |
| Operation keyed collections | 按本页第 5 节声明 key 做 canonical ascending order；空集和重复 key reject。 |
| RichText steps / VectorPath commands / vector samples / dab dabs | leaf-owned OrderedSequence；保留输入顺序，不排序、不去重。 |
| UTF-8 | validate；不做 locale/Unicode semantic rewrite。 |

## 4. A2 — Envelope Validation

| Field | Accepted | Missing | Zero | Unknown |
| --- | --- | --- | --- | --- |
| `schema_version` | `[1]` | reject | reject | reject |
| `payload_version` | `[1]` | reject | reject | reject |

当前 scalar Proto carrier 本身不能表达 presence 时，presence fact 必须来自 WirePreflight/DTO
boundary。若做不到，停止并报告 `WIRE_SCHEMA_CHANGE_REQUIRED`；不得默认成 V1，也不得在 A2
自行改变 Proto。

## 5. A3 — Operation Collection Profiles

所有下表字段均为 `KEYED_CANONICAL_SET`：empty reject、duplicate reject、normalize 后按 key
升序 canonical encode。ID comparator 为 unsigned 16-byte lexicographic；复合 key 从左到右比较。

| Path | Key | Additional invariant |
| --- | --- | --- |
| `payload.insert_objects.objects` | `ObjectRecord.id` | — |
| `payload.delete_objects.object_ids` | `object_id` | — |
| `payload.restore_objects.objects` | `ObjectRecord.id` | — |
| `payload.set_placements.items` | `object_id` | OrderKey 是 value，不是 batch comparator。 |
| `payload.set_transforms.items` | `object_id` | — |
| `payload.patch_properties.patches` | `(object_id, field_id)` | PropertyPatch presence 见第 6 节。 |
| `payload.set_object_size.items` | `object_id` | — |
| `payload.split_strokes.splits` | `source_stroke_id` | 每 source 同 Operation 只能出现一次。 |
| `payload.split_strokes.splits[].replacements` | `ObjectRecord.id` | replacement id 在整个 Operation 跨 splits 唯一。 |
| `payload.add_erase_masks.items` | `object_id` | — |
| `payload.add_erase_masks.items[].masks` | `mask_id` | mask id 在整个 Operation 跨 items 唯一。 |
| `payload.remove_erase_masks.items` | `object_id` | — |
| `payload.remove_erase_masks.items[].mask_ids` | `mask_id` | — |

## 6. PropertyPatch Presence

| action | value | A3 result |
| --- | --- | --- |
| `PROPERTY_PATCH_SET` | present and registry-compatible | accept structurally |
| `PROPERTY_PATCH_SET` | absent | reject |
| `PROPERTY_PATCH_CLEAR` | absent | accept structurally |
| `PROPERTY_PATCH_CLEAR` | present | reject |
| invalid / unknown action | any | reject |

Field 是否适用于**既有 target ObjectKind** 属于 B，不在 A3 读取 store 判断。

## 7. ObjectKind Version Registry

V1 只接受以下 exact triple：`kind_id + kind_version + active ObjectContent branch`；不允许 alias、
fallback 或 implicit conversion。

| kind_id | kind_version | ObjectContent branch |
| ---: | ---: | --- |
| 1 | 1 | `shape` |
| 2 | 1 | `image` |
| 3 | 1 | `vector_path` |
| 4 | 1 | `rich_text` |
| 5 | 1 | `vector_stroke` |
| 6 | 1 | `dab_stroke` |
| 7 | 1 | `connector` |
| 8 | 1 | `sticky` |
| 9 | 1 | `group` |

`kind_id` / `kind_version` missing、0、unknown 或 content branch mismatch 均 reject。没有发布
额外 reserved range。Page 仍不是 ObjectKind。

## 8. Stateless Leaf Constraints

### VectorPath

- commands `min_items = 1`；
- first command 必须 `MoveTo`；
- multiple subpaths allowed；
- `LineTo/QuadTo/CubicTo` 需要 active subpath；
- `ClosePath` 只能关闭当前 open subpath；
- Close 后下一 drawing command 前必须新的 `MoveTo`；
- 每个 PathCommand exactly-one branch；
- MoveTo-only subpath 合法；
- FillRule 只接受 `NON_ZERO` / `EVEN_ODD`；
- coordinates finite。

### RichTextDelta

- `delta_version = 1` only；missing/0/unknown reject；
- `steps` non-empty OrderedSequence；不排序、不去重；
- 每个 step exactly-one active branch；
- paragraph/current-text range 等需要 state 的检查递延 B。

### Stroke

- `VectorStrokeData.samples` 至少 1 个，OrderedSequence；
- `DabStrokeData.dabs` 至少 1 个，OrderedSequence；
- Dab center finite；size finite and `> 0`；rotation finite；opacity `[0,1]`；
- BrushFamily/version 与 representation 的既有 frozen rules 继续生效。

### Image / Connector

本次 semantic freeze 没有新增规则；继续消费既有 Image/Connector Current Authority。resource
availability、attached target existence/connectability 不是 A3 条件。

## 9. Explicitly Deferred to B

A0–A3 不得实现或查询：

- target/current object existence；
- target current ObjectKind applicability；
- duplicate create/restore against current store；
- parent/reference existence、hierarchy cycle；
- connector connectability / resulting graph；
- split source/current mask/current rich-text state；
- OperationId replay/collision；
- cascade closure / before-image / ApplyPlan。

## 10. Explicitly Deferred to C

A0–A3 可以产生确定性的内部 diagnostics，但不得把当前实现输出升级为 protocol oracle。以下仍需
C 的独立 human-reviewed materialization：

- 15-operation positive/negative intent；
- stable expected stage/path/category；
- independent fixtures；
- differential first-divergence oracle。

## 11. A0–A3 Entry Criteria

当前 entry criteria：`PASS`。

实施必须：

1. A0→A1→A2→A3 顺序推进；
2. 每一步 RED test 先于 production implementation；
3. rejection 路径不得调用 ObjectStore mutation seam；
4. 不引入 B/C/G1-05 行为；
5. 产出 source-commit keyed Evidence 后，才可请求下一 lane 授权。

当前状态仍是 `IMPLEMENTATION_NOT_STARTED`。
