# RichText + Stroke Semantic / Wire Reconciliation Matrix v0.1

> Source: Notion `RichText + Stroke Semantic / Wire Reconciliation Matrix v0.1`
>
> Notion Page ID: `3c94c57a-590c-8172-a7ad-ec6bb5f7bd91`
>
> Source URL: <https://app.notion.com/p/3c94c57a590c8172a7adec6bb5f7bd91?pvs=204>
>
> Snapshot/publication date: 2026-08-27
>
> Repository status: `current-reconciliation-authority`
>
> Verdict: `MACHINE_PROJECTION_DRIFT_CONFIRMED`
>
> Architecture changed: `false`

## Status and scope

本页是 `GT-G1-04` 阻塞调查的正式 Semantic ↔ Wire Reconciliation Matrix。它把 RichText 与
Stroke / Brush 的关键字段从 semantic authority 一路追到 Reference IDL、Generated Proto
Baseline 与当前 repository Proto，并记录 authority locator、expected/actual
descriptor、field/tag/type、protobuf wire type、冲突类别、影响范围及后续验证闭环。

本页不重新设计 RichText / Stroke semantic contract，不以当前 repository `.proto` 反向覆盖
Leaf Authority，也不改变 canonical ownership、ObjectKind、Operation vocabulary、
BrushFamily、Pressure/Tilt、font 或 renderer/runtime boundary。

冻结分类：

- Architecture Changed: `NO`
- Semantic Contract Changed: `NO`
- Machine Projection Refreeze: `REQUIRED`
- GT-G1-02 historical evidence: `PRESERVE`
- GT-G1-02R: `REQUIRED`
- GT-G1-04-A: `BLOCKED_BY_SEMANTIC_MACHINE_PROJECTION_RECONCILIATION`
- GT-G1-05: `NOT AUTHORIZED`

## 1. Authority stack and ownership

Leaf Schema owns semantic detail; Reference IDL owns protobuf spelling, numeric tag and scalar
width; Generated Proto Baseline owns repository-level `.proto` materialization discipline. The
actual repository Proto is a machine projection and cannot override those layers.

| Layer | RichText | Stroke / Brush | Role |
| --- | --- | --- | --- |
| Leaf semantic owner | `RichTextDocument + RichTextDelta Wire Schema v0.1` | `BrushDescriptor + StrokeRecord Wire Schema v0.1` | Canonical field meaning and structural semantics |
| Release closure | `RichText Font Semantic Baseline V1 Release v0.1` | `BrushFamily Registry V1 Release v0.1` and `Pressure + Tilt Canonical Input Contract V1 Release v0.1` | Closes release-specific open semantics |
| Wire reconciliation | `Reference IDL Integration + Leaf Schema Reconciliation v0.1` | Canonical wire names, tags, scalar widths and collection classification | Integrated spelling and wire mapping |
| Machine baseline | `Generated Proto Baseline + Canonical Codec Freeze v0.1` | Expected repository `.proto` projection and codegen boundary | Repository materialization target |
| Final schema classification | `Axiom Semantic Schema V1 Release Candidate Final Gate v0.1` | V1 semantic schema lock / execution gate classification | Release-level precedence |
| Audited machine artifacts | `schema/axiom/v1/proto/auditoryworks/axiom/v1/rich_text.proto` | `schema/axiom/v1/proto/auditoryworks/axiom/v1/brush_stroke.proto` | Current projection under audit |

### Audit baseline and method

The audited GitHub baseline is `main@39d44d289680e4ddaa5ae48a06e24aa579ee6326`.

- `rich_text.proto` blob: `6f4a4f98d50c82f04339f2630443d5df44841816`
- `brush_stroke.proto` blob: `b108a60d9429538b21a32c0fb416711b9cce550f`
- Both files entered the current lineage in `d857bd342fd63b922a048f5348b0be6864196ad8`
  (`GT-G1-01 — establish semantic target and canonical type boundary`).

Comparison order:

```text
Leaf Semantic Authority
        ↓
Release Closure
        ↓
Reference IDL Reconciliation
        ↓
Generated Proto Baseline
        ↓
Actual Repo Proto
        ↓
Descriptor / Codec / Golden Coverage
```

Conflict classes used by this authority are:

- `MATCH`: semantic field, tag and wire carrier agree;
- `NAMING_ONLY`: wire bytes may remain equivalent while source/type naming differs;
- `DESCRIPTOR_DRIFT`: generated API or descriptor identity differs;
- `MISSING_FIELD`: an expected authority field cannot be emitted;
- `SEMANTIC_DRIFT`: a similar wire carrier has different meaning;
- `TAG_COLLISION`: a frozen numeric tag is occupied by another semantic field/branch;
- `WIRE_TYPE_COLLISION`: the same tag uses a different protobuf wire type;
- `STALE_PLACEHOLDER`: a pre-reconciliation placeholder survived in the projection.

Severity in this review: **P0** cannot faithfully represent or interpret the frozen V1 contract,
or lets identical bytes/tags acquire different meaning; **P1** is descriptor/generated API drift
or a non-byte-breaking structural mismatch that still violates the locked machine contract.

## 2. RichText field-level reconciliation

### 2.1 Canonical state

| Field / type | Expected V1 | Actual repository | Verdict |
| --- | --- | --- | --- |
| `ParagraphAlignment` | enum: `0 Invalid / 1 Left / 2 Center / 3 Right / 4 Justify` | No enum type | `DESCRIPTOR_DRIFT` |
| `TextStyle.font_resource_id = 1` | `Id128` | `Id128` | `MATCH` |
| `TextStyle.font_size = 2` | `double` | `double` | `MATCH` |
| `TextStyle.weight = 3` | `uint32` | `uint32` | `MATCH` |
| `TextStyle.italic = 4` | `bool` | `bool` | `MATCH` |
| `TextStyle.underline = 5` | `bool` | `bool` | `MATCH` |
| `TextStyle.color = 6` | `ColorValue` | `ColorValue` | `MATCH` |
| `ParagraphStyle.alignment = 1` | `ParagraphAlignment` | `uint32` | `DESCRIPTOR_DRIFT` |
| `ParagraphStyle.line_height = 2` | `double` | absent | `MISSING_FIELD` |
| `ParagraphStyle.spacing_before = 3` | `double` | absent | `MISSING_FIELD` |
| `ParagraphStyle.spacing_after = 4` | `double` | absent | `MISSING_FIELD` |
| `TextRun.text = 1` | `string` | `string` | `MATCH` |
| `TextRun.style = 2` | `TextStyle` | `TextStyle` | `MATCH` |
| `Paragraph.paragraph_id = 1` | `Id128` | `Id128` | `MATCH` |
| `Paragraph.style = 2` | `ParagraphStyle` | same carrier with stale nested shape | carrier `MATCH`, nested drift |
| `Paragraph.runs = 3` | repeated `TextRun` | same | `MATCH` |
| `RichTextDocument.paragraphs = 1` | repeated `Paragraph` | same | `MATCH` |
| `RichTextContent.document = 1` | `RichTextDocument` | same | `MATCH` |

### 2.2 Mutation types

| Field / type | Expected V1 | Actual repository | Verdict |
| --- | --- | --- | --- |
| `InsertTextStep` fields 1..4 | `paragraph_id / scalar_offset / text / style` | same | `MATCH` |
| `DeleteTextStep.1` | `paragraph_id` | same | `MATCH` |
| `DeleteTextStep.2` | `start_scalar` | same | `MATCH` |
| `DeleteTextStep.3` | `scalar_count` | `end_scalar` | **`SEMANTIC_DRIFT`** |
| `SplitParagraphStep` fields 1..3 | `paragraph_id / scalar_offset / new_paragraph_id` | same | `MATCH` |
| `MergeParagraphStep` fields 1..2 | `first_paragraph_id / second_paragraph_id` | same | `MATCH` |
| `SetInlineStyleStep.1` | `paragraph_id` | `SetTextStyleStep.paragraph_id` | `STALE_PLACEHOLDER` |
| `SetInlineStyleStep.2` | `start_scalar` | same carrier | stale type |
| `SetInlineStyleStep.3` | `scalar_count` | `end_scalar` | **`SEMANTIC_DRIFT`** |
| `SetInlineStyleStep.4` | `TextStyle style` | same carrier | stale type |
| `SetParagraphStyleStep` fields 1..2 | `paragraph_id / ParagraphStyle` | same carrier | carrier `MATCH`, nested drift |

### 2.3 RichTextStep numeric tag registry

| Tag | Expected V1 | Actual repository | Verdict |
| --- | --- | --- | --- |
| 1 | `InsertText` | `InsertText` | `MATCH` |
| 2 | `DeleteText` | `DeleteText` | `MATCH` |
| 3 | `SplitParagraph` | `SetTextStyle` | **`TAG_COLLISION`** |
| 4 | `MergeParagraph` | `SplitParagraph` | **`TAG_COLLISION`** |
| 5 | `SetInlineStyle` | `MergeParagraph` | **`TAG_COLLISION`** |
| 6 | `SetParagraphStyle` | `SetParagraphStyle` | `MATCH` |

### 2.4 RichTextDelta envelope

| Tag | Expected V1 | Expected wire | Actual repository | Actual wire | Verdict |
| --- | --- | --- | --- | --- | --- |
| 1 | `uint32 delta_version` | wire 0 / varint | `repeated RichTextStep steps` | wire 2 / length-delimited | **`WIRE_TYPE_COLLISION + TAG_COLLISION`** |
| 2 | `repeated RichTextStep steps` | wire 2 | absent | — | **`MISSING_FIELD`** |

RichText semantic rules that are not Proto defects and remain validator/normalizer concerns:

- `paragraphs` is an `OrderedSequence`; a document has at least one paragraph;
- `ParagraphId` is non-zero and document-unique; an empty document is one empty paragraph;
- an empty paragraph has `runs=[]`; non-empty run text is non-empty;
- adjacent canonical-equal `TextStyle` runs must merge;
- text is valid UTF-8 with no Unicode normalization; CR/LF is not a paragraph boundary;
- positions are Unicode Scalar Value indices, not UTF-8 bytes, UTF-16 units or graphemes;
- `RichTextDelta.steps` is an ordered sequence applied against staged state, and `EditRichText`
  is atomic;
- `TextStyle` and `ParagraphStyle` are complete value replacements, not property patches;
- font identity/presence/weight/fallback follows the RichText Font V1 release.

## 3. RichText defect registry

### RT-D01 — ParagraphAlignment / ParagraphStyle type identity

- **Severity:** P1
- **Authority locator:** RichText Leaf §8 `ParagraphStyle`; Generated Proto Baseline RichText
  source; Reference IDL reconciliation scalar/tag rules.
- **Expected descriptor:** `ParagraphStyle.alignment = 1` typed as `ParagraphAlignment`, with
  `INVALID=0, LEFT=1, CENTER=2, RIGHT=3, JUSTIFY=4`.
- **Actual descriptor:** `uint32 alignment = 1`; no `ParagraphAlignment` enum.
- **Wire:** both enum and uint32 use wire 0 for known values.
- **Conflict class:** `DESCRIPTOR_DRIFT`.
- **Blast radius:** generated C++/TS API, enum identity, unknown-value validation, descriptor
  fingerprint and semantic-to-wire mapping.
- **Required golden:** known-enum descriptor case, unknown-enum fail-closed case and descriptor
  lock assertion for enum identity.

### RT-D02 — ParagraphStyle missing line / spacing fields

- **Severity:** P0
- **Authority locator:** RichText Leaf §8; Reference IDL f64 scalar-width matrix; Generated
  Proto Baseline `ParagraphStyle`.
- **Expected descriptor:** `line_height=2 double`, `spacing_before=3 double`,
  `spacing_after=4 double`.
- **Actual descriptor:** fields 2..4 absent.
- **Wire:** each expected `double` is wire 1 / fixed64.
- **Conflict class:** `MISSING_FIELD`.
- **Blast radius:** the current writer cannot express complete canonical `ParagraphStyle`; snapshot
  and edit round-trip can lose semantic facts; descriptor/generator and SetParagraphStyle are
  incomplete.
- **Required golden:** complete ParagraphStyle bytes; document snapshot round-trip;
  SetParagraphStyle round-trip; finite/range and `-0→+0` semantic negatives.

### RT-D03 — DeleteText range semantic collision

- **Severity:** P0
- **Authority locator:** RichText Leaf §6 position unit and §13 DeleteText; Generated Proto
  Baseline `DeleteTextStep`.
- **Expected descriptor:** tag 3 is `scalar_count` (`uint32`).
- **Actual descriptor:** tag 3 is `end_scalar` (`uint32`).
- **Wire:** both use wire 0 / varint, so protobuf cannot detect the mismatch.
- **Conflict class:** `SEMANTIC_DRIFT`.
- **Blast radius:** identical bytes can mean count or absolute end; replay and cross-language apply
  can silently diverge.
- **Required golden:** exact `(start_scalar, scalar_count)` projection; non-zero-start apply;
  negative bounds; cross-language semantic differential rather than byte equality alone.

### RT-D04 — SetInlineStyle stale SetTextStyle model

- **Severity:** P0
- **Authority locator:** RichText Leaf §16; RichText tag freeze RT-12; Generated Proto Baseline.
- **Expected descriptor:** `SetInlineStyleStep`, range `start_scalar + scalar_count`, branch tag 5.
- **Actual descriptor:** stale `SetTextStyleStep`, range `start_scalar + end_scalar`, branch tag 3.
- **Wire:** inner scalar fields are wire 0, but branch tag and range semantics differ.
- **Conflict class:** `STALE_PLACEHOLDER + SEMANTIC_DRIFT + TAG_COLLISION`.
- **Blast radius:** inline-style identity, delta replay, oneof decode, staged-state validation and
  generated APIs.
- **Required golden:** exact tag-5 bytes; non-zero start/count; complete TextStyle replacement;
  invalid range; staged-apply differential.

### RT-D05 — RichTextStep tags 3/4/5 collision

- **Severity:** P0
- **Authority locator:** Reference IDL reconciliation numeric tag freeze §6.2; RichText Leaf §11;
  Generated Proto Baseline.
- **Expected descriptor:** 3 `SplitParagraph`, 4 `MergeParagraph`, 5 `SetInlineStyle`.
- **Actual descriptor:** 3 `SetTextStyle`, 4 `SplitParagraph`, 5 `MergeParagraph`.
- **Wire:** all oneof message branches use wire 2; the tag chooses a different semantic message.
- **Conflict class:** `TAG_COLLISION`.
- **Blast radius:** cross-version/cross-language decode can reinterpret a valid branch; descriptor
  and canonical bytes no longer represent the frozen registry.
- **Required golden:** one exact vector for each tag 1..6, branch identity checks and unknown-step
  fail-closed negative.

### RT-D06 — RichTextDelta version / steps layout collision

- **Severity:** P0
- **Authority locator:** RichText Leaf §10; integrated RichText IDL; Generated Proto Baseline.
- **Expected descriptor:** `delta_version=1 uint32`, `steps=2 repeated RichTextStep`.
- **Actual descriptor:** `steps=1 repeated RichTextStep`, no `delta_version`.
- **Wire:** expected tag 1 wire 0 vs actual tag 1 wire 2; expected tag 2 wire 2 is absent.
- **Conflict class:** `WIRE_TYPE_COLLISION + TAG_COLLISION + MISSING_FIELD`.
- **Blast radius:** canonical incompatibility, unenforceable version policy and invalid
  `EditRichText` payload identity.
- **Required golden:** version 1 exact bytes with one/multiple tag-2 steps; missing/zero/unknown
  version negatives; ordered multi-step staged-state case.

## 4. Stroke / Brush field-level reconciliation

### 4.1 Pressure, Tilt and shared types

| Field / type | Expected V1 | Actual repository | Verdict |
| --- | --- | --- | --- |
| `CurvePoint01.x = 1` | `float` | type absent | `MISSING_TYPE` |
| `CurvePoint01.y = 2` | `float` | type absent | `MISSING_TYPE` |
| `PiecewiseLinearCurve01.points = 1` | repeated `CurvePoint01` | type absent | `MISSING_TYPE` |
| `PressureMapping.enabled = 1` | `bool` | `bool` | `MATCH` |
| `PressureMapping.size_curve = 2` | `PiecewiseLinearCurve01` | `float size_influence` | **`WIRE_TYPE_COLLISION`** |
| `PressureMapping.opacity_curve = 3` | `PiecewiseLinearCurve01` | `float opacity_influence` | **`WIRE_TYPE_COLLISION`** |
| `TiltMapping.enabled = 1` | `bool` | `bool` | `MATCH` |
| `TiltMapping.size_influence = 2` | `float` | `float` | `MATCH` |
| `TiltMapping.angle_influence = 3` | `float` | `float` | `MATCH` |
| `SmoothingSettings.amount = 1` | `float` | `float` | `MATCH` |
| `SpacingSettings.normalized_spacing = 1` | `float` | type absent | `MISSING_TYPE` |

### 4.2 BrushDescriptor

| Tag | Expected V1 | Actual repository | Verdict |
| --- | --- | --- | --- |
| 1 | `uint32 brush_family_id` | same | `MATCH` |
| 2 | `uint32 brush_version` | same | `MATCH` |
| 3 | `ColorValue color` | same | `MATCH` |
| 4 | `double nominal_size` | same | `MATCH` |
| 5 | `float opacity` | same | `MATCH` |
| 6 | `PressureMapping pressure` | same carrier, incompatible nested schema | nested **P0** |
| 7 | `TiltMapping tilt` | same | `MATCH` |
| 8 | `SmoothingSettings smoothing` | same | `MATCH` |
| 9 | `SpacingSettings spacing` | `double spacing` | **`WIRE_TYPE_COLLISION`** |
| 10 | `Id128 texture_resource_id` | `BrushBlendMode blend_mode` | **`TAG + WIRE TYPE COLLISION`** |
| 11 | `BrushBlendMode blend_mode` | `Id128 texture_resource_id` | **`TAG + WIRE TYPE COLLISION`** |

### 4.3 Stroke data and record

| Field / type | Expected V1 | Actual repository | Verdict |
| --- | --- | --- | --- |
| `StrokeSample.position = 1` | `Vec2` | `Vec2` | `MATCH` |
| `StrokeSample.pressure = 2` | `float` | `float` | `MATCH` |
| `StrokeSample.tilt = 3` | `Vec2` with Edition presence | same | carrier `MATCH` |
| `VectorStrokeData.samples = 1` | repeated `StrokeSample` | same | `MATCH` |
| Canonical dab type | `DabInstance` | `Dab` | `DESCRIPTOR_DRIFT` |
| Dab tag 1 | `Vec2 center` | `Vec2 position` | `NAMING / DESCRIPTOR_DRIFT` |
| Dab tag 2 | `double size` | same | wire `MATCH` |
| Dab tag 3 | `float rotation` | same | wire `MATCH` |
| Dab tag 4 | `float opacity` | same | wire `MATCH` |
| `DabStrokeData.dabs = 1` | repeated `DabInstance` | repeated `Dab` | `DESCRIPTOR_DRIFT` |
| `StrokeRecord.brush = 1` | `BrushDescriptor` | same carrier | carrier `MATCH` |
| `StrokeRecord.deterministic_seed = 2` | `fixed64` | `uint64` | **`WIRE_TYPE_COLLISION`** |
| `StrokeRecord.vector = 3` | `VectorStrokeData` | same | `MATCH` |
| `StrokeRecord.dab = 4` | `DabStrokeData` | same | `MATCH` |

Stroke semantic rules that are not Proto defects and remain release-validator/golden concerns:

- `brushFamilyId + brushVersion` is the immutable interpreter key; V1 publishes only `(1,1)`,
  `(2,1)` and `(3,1)`;
- FinePen / Highlighter use `VectorStrokeData`; TexturedMarker uses `DabStrokeData`;
- FinePen NORMAL and Highlighter HIGHLIGHTER have no texture; TexturedMarker NORMAL requires a
  non-zero texture;
- disabled pressure is present `enabled=false` with curves absent and `effectivePressure=1`;
- enabled pressure requires two curves with at least two ordered points, strict x increase,
  domain/range `[0,1]` and the staged f32 evaluator;
- V1 released families have tilt disabled and canonical `StrokeSample.tilt` absent;
- vector samples and dab dabs are ordered sequences;
- `StrokeRecord` excludes raw input, predicted tail, renderer mesh, `SkPath` and GPU state.

Edition 2024 explicit presence can carry the required presence distinction. The current source not
spelling the `proto3 optional` keyword is not itself a defect in this reconciliation.

## 5. Stroke / Brush defect registry

### ST-D01 — Pressure curve schema replaced by influence scalars

- **Severity:** P0
- **Authority locator:** Brush/Stroke Leaf §6; Pressure+Tilt Release §5–6; reconciliation
  scalar-width/collection matrix; Generated Proto Baseline.
- **Expected descriptor:** tag 2 `PiecewiseLinearCurve01 size_curve`, tag 3
  `PiecewiseLinearCurve01 opacity_curve`; each curve owns repeated `CurvePoint01{x,y}`.
- **Actual descriptor:** tag 2 `float size_influence`, tag 3 `float opacity_influence`; curve types
  absent.
- **Wire:** expected message wire 2; actual float wire 5 / fixed32.
- **Conflict class:** `WIRE_TYPE_COLLISION + STALE_PLACEHOLDER`.
- **Blast radius:** frozen piecewise-linear pressure semantics and presence rules cannot be encoded;
  descriptor, generator, codec, golden and BrushFamily interpretation are affected.
- **Required golden:** enabled curve with ≥2 points; disabled mapping with curves absent; missing
  curve negative; duplicate/non-increasing x negative; exact f32 evaluator vectors; FinePen,
  Highlighter and TexturedMarker cases.

### ST-D02 — SpacingSettings flattened to double

- **Severity:** P0
- **Authority locator:** Brush/Stroke Leaf §9; reconciliation scalar-width matrix; Generated Proto
  Baseline.
- **Expected descriptor:** tag 9 `SpacingSettings`, nested tag 1 `float normalized_spacing`.
- **Actual descriptor:** tag 9 `double spacing`.
- **Wire:** expected outer message wire 2; actual double wire 1 / fixed64; nested value should be
  float/wire 5.
- **Conflict class:** `WIRE_TYPE_COLLISION`.
- **Blast radius:** normalized-ratio semantics and family-specific spacing validation cannot bind to
  the current shape.
- **Required golden:** exact SpacingSettings bytes, positive-domain negatives and representative
  FinePen/Highlighter/Marker vectors.

### ST-D03 — texture_resource_id / blend_mode tag inversion

- **Severity:** P0
- **Authority locator:** Brush/Stroke Leaf §§4, 10, 11; integrated BrushDescriptor IDL;
  Generated Proto Baseline; BrushFamily V1 constraints.
- **Expected descriptor:** tag 10 `Id128 texture_resource_id`; tag 11 `BrushBlendMode blend_mode`.
- **Actual descriptor:** tag 10 blend enum; tag 11 texture `Id128`.
- **Wire:** expected tag 10 wire 2 vs actual wire 0; expected tag 11 wire 0 vs actual wire 2.
- **Conflict class:** `TAG_COLLISION + WIRE_TYPE_COLLISION`.
- **Blast radius:** BrushDescriptor canonical bytes are incompatible and texture/blend family rules
  attach to the wrong tags.
- **Required golden:** FinePen NORMAL/no texture, Highlighter HIGHLIGHTER/no texture,
  TexturedMarker NORMAL with non-zero texture, exact tag-10/tag-11 bytes and family negatives.

### ST-D04 — deterministic_seed fixed64 vs uint64

- **Severity:** P0
- **Authority locator:** Brush/Stroke Leaf §12; reconciliation scalar-width matrix; Generated Proto
  Baseline.
- **Expected descriptor:** `fixed64 deterministic_seed = 2`.
- **Actual descriptor:** `uint64 deterministic_seed = 2`.
- **Wire:** expected wire 1 / fixed64; actual wire 0 / varint.
- **Conflict class:** `WIRE_TYPE_COLLISION`.
- **Blast radius:** every seeded StrokeRecord changes canonical bytes and descriptor fingerprint;
  cross-language writers cannot conform to the frozen baseline.
- **Required golden:** seeds 0, 1, high-bit and multi-byte values with exact fixed64 bytes;
  varint-form canonicality rejection or differential evidence; TexturedMarker deterministic
  interpreter vector bound to the same seed identity.

### ST-D05 — DabInstance / center descriptor identity drift

- **Severity:** P1
- **Authority locator:** Brush/Stroke Leaf §15; reconciliation canonical naming; Generated Proto
  Baseline.
- **Expected descriptor:** `DabInstance { Vec2 center = 1; double size = 2; float rotation = 3;
  float opacity = 4; }`.
- **Actual descriptor:** `Dab { Vec2 position = 1; ... }`.
- **Wire:** audited four-field tag/type layout remains byte-compatible.
- **Conflict class:** `NAMING_ONLY + DESCRIPTOR_DRIFT`.
- **Blast radius:** descriptor fingerprint, generated API names and semantic-wire mapping; less
  severe than ST-D01..04 but still violates released machine identity.
- **Required golden:** descriptor identity assertion for `DabInstance.center` and one DabStroke
  exact binary round-trip after source-name correction.

## 6. Outer integration seam audit

The outer semantic registry is not the source of these defects:

- `ObjectContent` tags: 4 RichText / 5 VectorStroke / 6 DabStroke agree with the V1 registry;
- Operation tags: 10 AddStroke / 14 EditRichText agree with the V1 Operation vocabulary;
- `EditRichTextOp { object_id = 1; RichTextDelta delta = 2; }` has the expected outer carrier.

The defect boundary is therefore:

```text
ObjectContent / Operation outer registry
        ✅
        │
        ├── RichText leaf internal wire       ❌
        └── Brush / Stroke leaf internal wire ❌
```

## 7. Consolidated defect summary

| ID | Defect | Severity | Primary class |
| --- | --- | --- | --- |
| RT-D01 | ParagraphAlignment enum / ParagraphStyle alignment identity | P1 | `DESCRIPTOR_DRIFT` |
| RT-D02 | ParagraphStyle line-height / spacing fields missing | P0 | `MISSING_FIELD` |
| RT-D03 | DeleteText `scalar_count` → `end_scalar` | P0 | `SEMANTIC_DRIFT` |
| RT-D04 | SetInlineStyle → stale SetTextStyle + range drift | P0 | `STALE_PLACEHOLDER / SEMANTIC_DRIFT` |
| RT-D05 | RichTextStep tags 3/4/5 occupied by wrong variants | P0 | `TAG_COLLISION` |
| RT-D06 | RichTextDelta `delta_version` / `steps` layout collision | P0 | `WIRE_TYPE_COLLISION` |
| ST-D01 | Pressure curves replaced by float influences | P0 | `WIRE_TYPE_COLLISION` |
| ST-D02 | SpacingSettings flattened to double | P0 | `WIRE_TYPE_COLLISION` |
| ST-D03 | texture / blend tags 10/11 inverted | P0 | `TAG + WIRE TYPE COLLISION` |
| ST-D04 | deterministic_seed fixed64 → uint64 | P0 | `WIRE_TYPE_COLLISION` |
| ST-D05 | `DabInstance.center` → `Dab.position` | P1 | `DESCRIPTOR_DRIFT` |

## 8. Root-cause classification

Audited evidence supports `REPO_MACHINE_MATERIALIZATION_DEFECT`, not a newly discovered
RichText/Stroke product-semantic ambiguity. The field-level Leaf Authority, Release Closure,
Reference IDL Reconciliation and Generated Proto Baseline agree on the listed P0 fields; the
current repository Proto is the outlier.

The evidence pattern is:

```text
Aug 22–23
Leaf Semantic / Release / Reconciliation / Generated Proto target
already contains current RichText + Stroke forms
        ↓
Aug 26
GT-G1-01 materializes repo Proto
        ↓
current rich_text.proto / brush_stroke.proto contain stale/pre-reconciliation shapes
```

This page does not claim that a particular generator/template is the sole root cause. The
refreeze implementation must continue tracing the `d857bd3` materialization inputs. It does
confirm that the machine projection is inconsistent with current semantic/wire authority.

## 9. Verification coverage gap

The Final Gate composes `richtext-font-v1`, `brush-family-v1`, `brush-interpreter-v1` and
`pressure-tilt-v1` into V1 release conformance, but the repository suite files remain `candidate`
with `vectors: []`. Existing GT-G1-02 / RC evidence proves descriptor reproducibility and codec
self-consistency against the current machine schema; it does not independently prove that current
RichText or Stroke leaf tags, fields and exact bytes match semantic authority.

Classification: `LEAF_VERIFICATION_COVERAGE_GAP = CRITICAL`.

## 10. Authority publication / mirror completeness gap

Before this publication, GitHub's `05-leaf-schemas/richtext-wire-schema-v0.1.md` and
`brush-stroke-wire-schema-v0.1.md` were summaries while `docs/notion/manifest.yaml` did not expose
the complete field-level 04 chain. That combination made the stale repository Proto appear more
specific than the Codex-facing authority:

```text
Notion field-level authority = detailed
GitHub leaf mirror           = summary
Actual repo Proto            = detailed but stale
Manifest current set         = incomplete for the full 04 field-level chain
```

Classification: `AUTHORITY_PUBLICATION_GAP = YES` and
`GITHUB_AUTHORITY_MIRROR_COMPLETENESS_DEFECT = YES`. This page closes the publication gap for the
reconciliation source set; it does not silently rewrite leaf semantic owners.

## 11. Final Gate reclassification

This reconciliation does not overturn the V1 Semantic Design Lock. The semantic sources and
release closures support one another, while V1 Machine Projection / Execution Evidence is reopened:

```text
V1 Semantic Design Lock
        remains credible

V1 Machine Projection / Execution Evidence
        REOPEN REQUIRED
```

`RC-E01..E06` must be re-reviewed for RichText/Stroke machine projection after the corrected
descriptor, leaf goldens and differential are bound to a new commit.

## 12. Required repair chain

This publication authorizes only “repair the machine projection; do not reinvent semantic V1” as
the next direction:

```text
Existing Current Semantic Authority
        ↓
THIS Reconciliation Matrix
        ↓
Pre-release V1 Machine Projection Refreeze
        ↓
Authority mirror + manifest closure
        ↓
rich_text.proto correction
brush_stroke.proto correction
        ↓
Descriptor regeneration / lock
        ↓
Generated bindings regeneration
        ↓
RichText leaf binary golden
Stroke / Brush leaf binary golden
        ↓
Semantic + canonical-byte differential
        ↓
GT-G1-02R / Schema Machine Reconciliation
        ↓
G1-04 Authority Closure resume
```

If a repair discovers a real contradiction among upstream current semantic authorities, execution
must stop as `BLOCKED_REFREEZE_DECISION`; the implementation must not weaken semantic authority to
preserve an old descriptor or golden.

## 13. Minimum golden expansion required

Before machine refreeze closes, independently materialize at least:

- `richtext-wire-v1`: complete ParagraphStyle, RichTextDelta version, six step tags,
  Delete/SetInlineStyle count semantics and ordered multi-step delta;
- `richtext-font-v1`: reviewed vectors for TextStyle presence, weight and font identity;
- `brush-wire-v1`: BrushDescriptor tags 1..11, SpacingSettings, texture/blend, StrokeRecord
  fixed64 seed and DabInstance descriptor;
- `pressure-tilt-v1`: reviewed disabled/enabled pressure forms, piecewise curves and V1
  tilt-disabled semantics;
- `brush-family-v1`: family/version to representation, blend and texture constraints;
- `brush-interpreter-v1`: deterministic seed/interpreter cases linked to wire seed representation.

Golden authoring remains:

```text
human-reviewed intent
→ verification-only fixture author/compiler
→ checked-in expected corpus
→ production observation
→ differential
```

Production codec/validator output must not become authoritative expected truth.

## 14. Gate impact and exit criteria

| Gate / evidence | Current impact |
| --- | --- |
| GT-G1-02 historical evidence | `PRESERVE` |
| GT-G1-02 as current complete machine-wire evidence | `SUPERSESSION / RECONCILIATION REQUIRED` |
| GT-G1-02R | `REQUIRED` before RichText/Stroke projection is current |
| GT-G1-03 | Historical PASS remains provenance; rerun only affected regression after corrected leaf mapping |
| GT-G1-04-A | `BLOCKED_BY_SEMANTIC_MACHINE_PROJECTION_RECONCILIATION` |
| GT-G1-04 overall | `NOT PASS` |
| GT-G1-05 | `NOT AUTHORIZED` |

Machine refreeze can close only when all of the following are true:

- RT-D01..RT-D06 and ST-D01..ST-D05 are individually `CLOSED`;
- this field-level authority is mirrored and listed in the manifest;
- corrected Proto agrees with Leaf, Reconciliation and Generated Proto Baseline;
- Edition 2024 compile and descriptor reproducibility pass;
- old→new descriptor diff is human-reviewed and mapped to defect IDs;
- generated C++/TS bindings are reproducible;
- RichText / Stroke / Pressure / BrushFamily goldens are materialized independently;
- canonical encode/decode and semantic differential pass with `first divergence = null`;
- old evidence remains immutable and new GT-G1-02R evidence is commit-bound;
- `architectureChanged = false`, unless a later authority proves a real semantic contradiction.

## 15. Non-goals and hard stops

This authority does not authorize:

- changing RichText or Brush/Stroke product semantics to fit stale Proto;
- entering GT-G1-04-B or GT-G1-05;
- changing Scene, Render, Spatial, Skia, Arc or Platform;
- treating candidate empty suites as release evidence;
- deleting or rewriting old G1-02 evidence;
- treating `d857bd3`'s Proto as higher authority than field-level semantic sources.

## Review verdict

**Verdict: `MACHINE_PROJECTION_DRIFT_CONFIRMED`.** The audit found no need to invent new product
semantics. It found a V1 semantic/wire authority → repository machine-artifact projection defect and
the related publication/verification coverage gap. The next legal step is **Pre-release V1 Machine
Projection Refreeze + GT-G1-02R**; only after that may G1-04 resume.
