# Semantic Leaf Structural Validation Closure V1 v0.1

> Source: Notion Current Authority publication
> Source page id: `3c94c57a-590c-8173-bf5f-c1418f76764a`
> Source status: **Current Authority — G1-04 Semantic Freeze V1**
> Repository status: current
> Approval token: `APPROVE_G1_04_SEMANTIC_FREEZE_V1`

> 状态：**CURRENT AUTHORITY — V1 SEMANTIC FREEZE**
>
> 这是一个补洞文件，不重写任何现有 leaf semantic owner。每个新规则均已由批准 token 冻结。

## 1. 使用方式

本页只关闭 `GT-G1-04-A3` 不能仅靠 current authority 机械决定的最小无状态结构问题。它不
supersede RichText、Brush/Stroke、Pressure/Tilt、BrushFamily、Connector 或 VectorPath 的既有
semantic owner；批准后应作为它们的 composition/gap closure。

数值共同前提保持不变：拒绝 NaN / Infinity，`-0` 规范化为 `+0`，并在适用处执行已发布的
hard limit。需要读取 store 才能判断的规则明确留给 B。

## 2. VectorPath — `G104-S09`

| 当前 authority 已拥有 | 真正缺口 | 建议 closure | A/B |
| --- | --- | --- | --- |
| renderer-neutral carrier、`MoveTo/LineTo/QuadTo/CubicTo/ClosePath` oneof、finite geometry、FillRule carrier、structural validation before apply | command grammar、empty path、subpath/close state、enum exact policy | **冻结规则**：commands 非空；首命令必须 `MoveTo`；可有多个 subpath；`LineTo`/`QuadTo`/`CubicTo` 只能出现在 active subpath；`ClosePath` 只能关闭当前尚未关闭的 subpath；关闭后下一绘制命令前需新的 `MoveTo`；每个 command 恰好一个 oneof；FillRule 仅 `NON_ZERO`/`EVEN_ODD`；所有坐标有限；已发布 hard limit 生效。 | A3 |

该冻结规则允许只含一个 `MoveTo` 的点状 subpath，不把 renderer fill/tessellation、stroke expansion 或
hit testing 写入 semantic grammar。

## 3. NormalizedRect / Image

**NO NEW RULES REQUIRED。** current Image release、Geometry Types 和 Image-related current
authority 已拥有：

- `sourceRect` absent 表示 full image；显式 `[0,0,1,1]` canonicalize/elide 为 absent；
- partial crop 须正面积、完全位于 unit square；
- resourceId、intrinsic/display width/height 的有限正域；
- `contentMode` required，只接受 Fit/Fill/Stretch，absence/0/unknown reject；
- resource binary availability 不属于 semantic apply 的 A 前置。

目标是否为 Image 及 Resource lifecycle 由 B 或后续 Resource/Data Runtime gate 处理。

## 4. RichText — `G104-S10`

GT-G1-02R 已关闭 RichText descriptor/tag/ParagraphStyle machine projection，既有 RichText 与
Font authority 已拥有 ordered mutation、paragraph identity、UTF-8、run/style canonicalization、
完整 style replacement、font required 和 weight domain。本当前 authority不得重开它们。

| 真正缺口 | 建议 closure | A/B |
| --- | --- | --- |
| `RichTextDelta.delta_version` 的 accepted/missing/zero/unknown matrix；`steps` 空集策略 | **冻结规则**：仅 `delta_version = 1` 可接受；missing、0、未知 reject；`steps` 为非空 OrderedSequence，保留输入顺序，不排序、不去重；每个 `RichTextStep` 必须恰有一个 active oneof branch。 | A3 |

每个 paragraph 是否已存在、scalar range 是否落在当前文本、step 对 staged state 是否有效，都需要
Document state，属于 B。

## 5. Stroke / Brush — `G104-S11`

GT-G1-02R 已关闭 PressureMapping、SpacingSettings、texture/blend tag、fixed64 seed 和
`DabInstance` descriptor identity；BrushFamily、pressure/tilt 与 deterministic interpreter 仍是
现有 owner。

| 真正缺口 | 建议 closure | A/B |
| --- | --- | --- |
| Vector stroke samples / Dab stroke dabs 的最小 cardinality | **冻结规则**：`VectorStrokeData.samples` 与 `DabStrokeData.dabs` 都至少一个；一个 sample/dab 合法，以保留 tap/dot stroke。两者都是 OrderedSequence，输入顺序不得排序。 | A3 |
| Dab intrinsic numeric domain | **冻结规则**：每个 Dab 的 `center` 坐标有限；`size` 有限且严格大于 0；`rotation` 有限；`opacity` 有限且在 `[0,1]`。 | A3 |

已有 Vector sample pressure `[0,1]`、V1 tilt absence/representation、family/version 对
Vector/Dab representation 的约束仍由当前 BrushFamily / Pressure+Tilt authority 执行。Object
content 的 kind 与 stroke data representation 是否匹配可以在 A3 的 payload identity 做静态
检查；与**既有** target 的兼容性是 B。

## 6. Connector

**NO NEW RULES REQUIRED。** Connector release 已拥有 tagged endpoint、free point finite、
AutoPerimeter hint presence/normalized range/center reject、StablePort V1 `1..4`、routing required
且仅 Straight/Orthogonal、unknown enum reject。attached target 是否存在、是否 connectable、其
kind compatibility 与最终 graph 结果均是 B。

## 7. 非目标

- 不建立 renderer command、SkPath、mesh、GPU 或 Arc 规则。
- 不为 C 指定 expected error stage/path/category、fixture 或 golden bytes。
- 不为 Resource bytes、font bytes 或 image decode texture 建立 A 规则。
