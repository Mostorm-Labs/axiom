# GT-G1-02R Machine Refreeze Contract Map

> 任务：`GT-G1-02R — Pre-release V1 Machine Projection Refreeze`  
> Authority baseline：`e7a0581706b4e4370fd397ccffef81aa84e48a27`  
> 主依据：`docs/notion/authority/04-semantic-schema/04-reference-idl/richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md`  
> Notion page：`3c94c57a-590c-8172-a7ad-ec6bb5f7bd91`

## 结论与边界

本任务修复已经冻结的 V1 语义到 Proto、descriptor、DTO/codec 映射和验证语料之间的机器投影差异。
当前 `.proto` 是待修复对象，不能反向成为语义依据。它不授权 Operation apply、ObjectStore mutation、RichText
语义校验、BrushFamily 解释器或任何 G1-04 行为实现。

| 判定项 | 结论 |
| --- | --- |
| Architecture Gap | **NO** |
| Semantic Contract Change | **NO** |
| Wire Machine Artifact Change | **YES** |
| Descriptor Change | **YES** |
| Golden Change | **YES**，新增独立 leaf corpus；既有 BG 不改写 |
| Historical Golden Rewrite | **NO** |

共同 authority stack 为：V1 Final Gate、Reference IDL、Leaf Schema Reconciliation、Generated Proto
Baseline、RichText/Font、Brush/Stroke、BrushFamily、Pressure/Tilt、Common Wire 与 Golden Authoring
Authority。每个条目的 descriptor/golden 都须证明字段身份，而不仅是 protobuf 的可解码性。

## RichText 缺陷闭合表

| ID | Authority / expected machine projection | 当前投影 | 所需 Proto / descriptor 改动 | domain / codec 与 golden 要求 |
| --- | --- | --- | --- | --- |
| `RT-D01` | `ParagraphStyle.alignment=1` 是 `ParagraphAlignment`；enum：`INVALID=0`、`LEFT=1`、`CENTER=2`、`RIGHT=3`、`JUSTIFY=4`；wire 0 | `uint32 alignment=1`，没有 enum；wire 0 | 添加 enum，并把 tag 1 的 field type 改为 enum；分类 `DESCRIPTOR_IDENTITY_CORRECTION` | domain 使用具名 enum；golden 覆盖已知 enum 及现有 fail-closed unknown-enum policy |
| `RT-D02` | `ParagraphStyle.line_height=2 double`、`spacing_before=3 double`、`spacing_after=4 double`；均 wire 1 | 三字段缺失 | 新增 2、3、4；分类 `MISSING_FIELD_MATERIALIZATION` | domain 需要完整 ParagraphStyle；golden 覆盖四字段、明确零/固定 64 位值 |
| `RT-D03` | `DeleteTextStep.scalar_count=3 uint32`；wire 0，含义是计数 | 同 tag/type 叫 `end_scalar`，含义变成绝对位置 | 重命名为 `scalar_count`；分类 `SEMANTIC_NAME_CORRECTION` | DTO/projection 输出 `scalarCount`；非零 start/count golden 防止把相同 bytes 解释为 end |
| `RT-D04` | `SetInlineStyleStep`，tags 1 paragraph、2 start、3 scalar_count、4 `TextStyle`；oneof tag 5 | `SetTextStyleStep`，tag 3 是 end，且 branch 占 tag 3 | 以 `SetInlineStyleStep` 替代；恢复 branch tag 5；分类 `DESCRIPTOR_IDENTITY_CORRECTION` 与 `WIRE_BREAKING_PRE_RELEASE_REPAIR` | DTO/projection 命名和 count 语义；golden 使用 tag 5、非零 range、完整 TextStyle |
| `RT-D05` | `RichTextStep`: 1 Insert、2 Delete、3 Split、4 Merge、5 SetInlineStyle、6 SetParagraphStyle；均 wire 2 | 3 SetTextStyle、4 Split、5 Merge | oneof 按冻结 tag 重排；分类 `WIRE_BREAKING_PRE_RELEASE_REPAIR` | golden 覆盖 tag 1..6，production observation 输出实际 branch identity |
| `RT-D06` | `RichTextDelta.delta_version=1 uint32` wire 0；`steps=2 repeated RichTextStep` wire 2 | `steps=1`，无 version | 添加 version、把 steps 移至 2；分类 `WIRE_BREAKING_PRE_RELEASE_REPAIR` / `MISSING_FIELD_MATERIALIZATION` | domain/projection 包含 `deltaVersion` 与有序 steps；golden 覆盖单步及多步顺序 |

## Stroke / Brush 缺陷闭合表

| ID | Authority / expected machine projection | 当前投影 | 所需 Proto / descriptor 改动 | domain / codec 与 golden 要求 |
| --- | --- | --- | --- | --- |
| `ST-D01` | `CurvePoint01{x=1 float,y=2 float}`，`PiecewiseLinearCurve01.points=1 repeated`；`PressureMapping.enabled=1 bool,size_curve=2 message,opacity_curve=3 message` | tag 2/3 为 float `size_influence` / `opacity_influence`，wire 5 | 添加两个 curve types，2/3 改为 length-delimited message；分类 `WIRE_BREAKING_PRE_RELEASE_REPAIR` | domain 采用 typed curves；golden 覆盖多 point 和 disabled mapping，且不实现曲线语义 validator |
| `ST-D02` | `SpacingSettings.normalized_spacing=1 float`；`BrushDescriptor.spacing=9 SpacingSettings`，outer wire 2、inner wire 5 | tag 9 是 `double spacing`，wire 1 | 添加 `SpacingSettings`，tag 9 改为 message；分类 `WIRE_BREAKING_PRE_RELEASE_REPAIR` | domain/projection 用 normalized spacing；golden 覆盖嵌套 wire shape |
| `ST-D03` | `BrushDescriptor.texture_resource_id=10 Id128` wire 2；`blend_mode=11 BrushBlendMode` wire 0 | 两个 semantic field 互换占用 10/11 | 复位 field/tag/type；分类 `WIRE_BREAKING_PRE_RELEASE_REPAIR` | DTO/projection 证明 texture 与 blend 分别来自正确 tag；golden 覆盖无 texture/highlighter/marker |
| `ST-D04` | `StrokeRecord.deterministic_seed=2 fixed64`，wire 1 | `uint64`，wire 0 | 改为 `fixed64`；分类 `WIRE_BREAKING_PRE_RELEASE_REPAIR` | encoding-neutral domain 继续 `uint64_t`；golden 以 0、1、多字节与 high-bit 值证明固定 8-byte carrier |
| `ST-D05` | `DabInstance.center=1 Vec2`、2 size double、3 rotation float、4 opacity float；`DabStrokeData.dabs` 重复 `DabInstance` | `Dab.position=1` | 将 type/name 纠正为 `DabInstance.center`；分类 `DESCRIPTOR_IDENTITY_CORRECTION` | domain/projection 命名同步；golden 覆盖 Dab stroke，不改变 tag/wire shape |

## Descriptor 审查契约

`descriptor-refreeze-diff.json` 必须将每一条差异映射到上表中的 `RT-D01..RT-D06` 或
`ST-D01..ST-D05`，并使 `unmappedChanges=[]`。允许的分类只有：

- `WIRE_BREAKING_PRE_RELEASE_REPAIR`
- `SEMANTIC_NAME_CORRECTION`
- `DESCRIPTOR_IDENTITY_CORRECTION`
- `MISSING_FIELD_MATERIALIZATION`

不得改动 ObjectContent 的 9 个既有 tag、Operation 的 15 个既有 tag、BrushFamily ID、renderer/runtime
语义，也不得重写既有 BG corpus。新 leaf golden 由 verification-only generic wire fixture compiler 根据当前
authority 的人工审阅输入物化；production codec 只提供 observation/differential，不生成 expected bytes。
