# ObjectKind Version Registry V1 Release v0.1

> Source: Notion Current Authority publication
> Source page id: `3c94c57a-590c-8147-9aa2-cd70f507c441`
> Source status: **Current Authority — G1-04 Semantic Freeze V1**
> Repository status: current
> Approval token: `APPROVE_G1_04_SEMANTIC_FREEZE_V1`

> 状态：**CURRENT AUTHORITY — V1 SEMANTIC FREEZE**
>
> 这是 `ObjectRecord.kind_id` / `kind_version` 的最小 gap closure 冻结规则，不是 ShapeKind、
> OperationKind、BrushFamily 或产品 Page 的 registry。

## 1. 目的与边界

current ObjectContent、Field Registry、Reference IDL 与 operation registry 已一致地给出九种
V1 ObjectKind identity，但尚无一份明确的 `kind_version` accepted table。`ObjectContent` oneof
tag 或当前 C++ enum 都不足以自行推导 version policy；本当前 authority只提出该缺失表。

一个 Product Page 仍恰好对应一个独立 Axiom Document。这里不会新增 Page ObjectKind，也不会
新增 `DocumentRoot → Page*` synthetic root。

## 2. 提议 registry

`G104-S08`：**FROZEN BY HUMAN APPROVAL**。所有 released V1 ObjectKind 的
`current_version = 1`、`accepted_versions = [1]`；0、missing 和未知 version 一律 fail closed。
已存在 target 的 kind compatibility 仍是 B；本表只检查 payload 中 ObjectRecord 的自描述
identity 与 content branch 是否一致。

| kind_id | 名称 | ObjectContent branch | current version | accepted versions | unknown / missing / zero | 适用静态结构 | provenance |
| --- | --- | --- | ---: | --- | --- | --- | --- |
| 1 | Shape | `shape` | 1 | `[1]` | reject | ShapeKind registry、有限正 size | ObjectContent、ShapeKind、Reference IDL |
| 2 | Image | `image` | 1 | `[1]` | reject | ImageContent presence/geometry/mode | ObjectContent、Image release、Reference IDL |
| 3 | VectorPath | `vector_path` | 1 | `[1]` | reject | VectorPath grammar 与 FillRule | ObjectContent、VectorPath、`G104-S09` |
| 4 | RichText | `rich_text` | 1 | `[1]` | reject | RichText leaf与 delta/state carrier | ObjectContent、RichText authority |
| 5 | VectorStroke | `vector_stroke` | 1 | `[1]` | reject | StrokeRecord vector representation | ObjectContent、Brush/Stroke authority |
| 6 | DabStroke | `dab_stroke` | 1 | `[1]` | reject | StrokeRecord dab representation | ObjectContent、Brush/Stroke authority |
| 7 | Connector | `connector` | 1 | `[1]` | reject | endpoint/anchor/routing static syntax | ObjectContent、Connector release |
| 8 | Sticky | `sticky` | 1 | `[1]` | reject | finite positive local dimensions | ObjectContent / semantic gap closure |
| 9 | Group | `group` | 1 | `[1]` | reject | empty content carrier only | ObjectContent / semantic gap closure |

## 3. 规范化与 reject

- `kind_id`、`kind_version` 与 active `ObjectContent` branch 必须形成表中唯一的三元组；不允许
  alias、fallback 或 implicit conversion。
- `kind_id` 为 0、缺失、未知，`kind_version` 为 0、缺失、未知，或 active branch 不匹配时，
  在 A3 reject。
- `kind_version` 不是“由 oneof tag 推断”的默认值。当前 protobuf scalar 未携带 presence 时，
  WirePreflight/DTO boundary 必须给 A3 以区分 missing 的事实；若无法做到，实施必须停止，而非
  默认为 1。
- ShapeKind、BrushFamily/version、FieldId/version 和 OperationKind/version 继续由各自 registry
  拥有，不能挪入本表。

## 4. 兼容性与迁移

该冻结规则的 fail-closed 行为使未来 kind 的发布成为显式 schema evolution：先发布新 registry
version、machine projection、golden 与 migration，再接受相应 ObjectRecord。它不会破坏现有
九个 V1 carrier，也不需要变更 oneof tags。

## 5. 未纳入事项

- 既有对象能否被某种 Operation 修改：B。
- Group hierarchy、Sticky primary child、connector graph：B。
- Page collection、PageId → DocumentId：产品层 / 后续 G7。
- byte-level error outcome 与兼容 case：C。
