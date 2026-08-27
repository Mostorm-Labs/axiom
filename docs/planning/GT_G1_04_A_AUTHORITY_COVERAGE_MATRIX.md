# GT-G1-04-A Rule-Level Authority Coverage Matrix (P35/P36)

> Fresh rule-level audit for the P34 re-entry. Current Authority is read from
> `docs/notion/manifest.yaml`; implementation and machine projection are
> evidence under review, never sources of semantic truth. Historical source/
> evidence and Android visual-smoke data are immutable and out of scope.

| Authority Rule ID | Source Authority | Normative Rule | Owner | Carrier | Implementation location | Test / Oracle | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| G104-S01 | Operation Structural Semantics §2 | `schema_version` present and exactly `1`; missing/zero/unknown reject | A2 | `OperationFieldPresence` + `uint32` | `validator.cpp::validateEnvelope` | `EnvelopeValidation.AcceptsOnlyExplicitVersionOne` | COVERED |
| G104-S02 | Operation Structural Semantics §2 | `payload_version` present and exactly `1`; missing/zero/unknown reject | A2 | `OperationFieldPresence` + `uint32` | `validator.cpp::validateEnvelope` | `EnvelopeValidation.AcceptsOnlyExplicitVersionOne` | COVERED |
| G104-S03 | Operation Structural Semantics §3–4 | Insert/Delete/Restore batches non-empty keyed canonical sets, non-zero unique IDs, ascending unsigned-lexicographic order | A1/A3 | typed vectors | `normalizer.cpp`, `validator.cpp` | keyed collection regressions | COVERED |
| G104-S04 | Operation Structural Semantics §4 | Placement/transform/size item batches keyed by `object_id`, non-empty/unique/ascending | A1/A3 | typed item vectors | `normalizer.cpp`, `validator.cpp` | keyed collection regressions | COVERED |
| G104-S04-SIZE | User P35 scope + object size rules | `SetObjectSize.width` and `.height` finite and strictly positive; target-kind applicability is B | A3/B | `ObjectSizeItem` | `validator.cpp` | zero/negative/NaN/Inf tests | COVERED |
| G104-S05 | Operation Structural Semantics §5 + Field Registry | Patch key `(object_id,field_id)` unique/ascending; SET value required; CLEAR value forbidden; field/value registry-valid | A1/A3 | `PropertyPatch` | `normalizer.cpp`, `validator.cpp` | property patch tests | COVERED |
| FIELD-DEFAULT-01 | Field Registry V1 | explicit registry defaults are elided from canonical property bag | A1 | `PropertyBag.entries` | `normalizer.cpp` | default-elision tests | COVERED |
| G104-S06 | Operation Structural Semantics §4 | Split outer/replacement sets non-empty, ascending; replacement IDs unique operation-wide | A1/A3 | `StrokeSplit` vectors | `normalizer.cpp`, `validator.cpp` | split uniqueness tests | COVERED |
| G104-S07 | Operation Structural Semantics §4 | Erase-mask outer/inner sets non-empty, ascending; mask IDs unique operation-wide | A1/A3 | `EraseMask*` vectors | `normalizer.cpp`, `validator.cpp` | erase uniqueness tests | COVERED |
| G104-S08 | ObjectKind Version Registry V1 | Exact released `(kind_id, kind_version=1, active ObjectContent branch)` IDs 1–9; unknown fail closed | A3 | `ObjectRecord.kind/kind_version/content` | `validObjectKindTriple` | object-kind matrix tests | COVERED |
| G104-S09 | Semantic Leaf Structural Validation §2 | VectorPath commands non-empty, first MoveTo, active-subpath/close grammar, exact oneof, finite coords, FillRule NON_ZERO/EVEN_ODD | A3 | `VectorPathGeometry` | `validVectorPath` | vector-path leaf tests | COVERED |
| G104-S10 | Semantic Leaf Structural Validation §3 + RichText/Font | RichTextDelta version exact 1, non-empty ordered exact-oneof steps; UTF-8/font/style domains | A3 | `RichTextDelta` / `RichTextStep` | rich-text validators | rich-text leaf tests | COVERED |
| G104-S10-LIMIT | Hard Limits + RichText authority | Generic strings each <=1 MiB; EditRichText InsertText aggregate <=8 MiB; complete document total is not constrained by that aggregate | A3 | runs vs delta inserts | rich-text validators | aggregate/document regression | COVERED |
| G104-S11 | Semantic Leaf Structural Validation §4 + Brush/Stroke | Vector samples/dab dabs non-empty ordered; dab finite-positive domain; released family/representation/pressure/tilt rules | A3 | `StrokeRecord` | `validStrokeRecord` | stroke/pressure/tilt tests | COVERED |
| IMG-01 | ImageContent V1 Release | resource ID non-zero; intrinsic/display dimensions finite and positive | A3 | `ImageContent` | `validImage` | image geometry tests | COVERED |
| IMG-02 | ImageContent V1 + Geometry | sourceRect absent means full image; explicit full unit rect canonicalizes/elides; partial crop positive/in-unit | A1/A3 | `optional<NormalizedRect>` | normalizer/validator | crop tests | COVERED |
| IMG-05 | ImageContent V1 Release | enum identity `INVALID=0`, `FIT=1`, `FILL=2`, `STRETCH=3`; 0/unknown reject | A0/A3 | semantic/generated enum | object proto, semantic enum, validator | static identity + wire mapping tests | COVERED |
| CN-V1-01 | Connector Anchor Contract | free-point endpoint finite; routing only Straight/Orthogonal | A3 | `ConnectorContent` | `validConnector` | connector leaf tests | COVERED |
| CN-V1-05 | Connector Anchor Contract | AutoPerimeter hint absence differs from presence; present hint finite/unit-range and non-center; `-0` normalizes | A0/A1/A3 | `optional<Vec2>` hint | object carrier/normalizer/validator | presence/range tests | COVERED |
| CN-V1-06 | Semantic Leaf Structural Validation §6 + Connector Anchor Contract | StablePort static V1 namespace is `1..4`; current-target applicability remains B | A3/B | `StablePortAnchor.port_id` | `validator.cpp::validConnector` | StablePort `0/1/4/5/UINT32_MAX` boundary test | COVERED |
| GEO-V1-01 | Geometry Types + Common Wire | geometry scalars finite; `-0 → +0`; no clamping | A1/A3 | geometry carriers | normalizer/validator | numeric tests | COVERED |
| GEO-V1-02 | Current Transform2D structural rule | determinant `a*d-b*c` exact non-zero; no epsilon | A3 | `Transform2D` | `validTransform2D` | identity/rotation/singular/NaN/Inf tests | COVERED |
| ORDER-V1-01 | OrderKey RFC + canonical profile | opaque unsigned-byte lexicographic key, length 1..32; trailing zero invalid | A1/A3 | `OrderKey` | `order_key.hpp` | boundary/trailing-zero tests | COVERED |
| PAINT-V1-01 | Paint/Style + Field Registry | Color channels finite [0,1]; SolidStroke width finite >0 | A3 | paint value variants | validator | paint tests | COVERED |
| PAINT-V1-02 | Paint/Style Current Authority | StrokeCap Butt/Round/Square; StrokeJoin Miter/Round/Bevel; Miter limit finite >=1 | A3 | `SolidStroke` | validator | cap/join tests | COVERED |
| PAINT-V1-03 | Paint/Style Current Authority | DashPattern >=2 even finite-positive segments; finite offset (negative allowed); SolidDash accepted | A3 | `DashPattern` | validator | dash tests | COVERED |
| ERASE-V1-01 | EraseMask Geometry + Common Wire | erase geometry finite; persistent masks sorted/unique and bounded | A1/A3 | `EraseMaskRecord` | normalizer/validator | erase geometry tests | COVERED |
| ERASE-V1-02 | Reference IDL ObjectRecord | non-empty `erase_masks` only for VectorStroke or DabStroke; other kinds reject | A3 | kind + erase masks | validator | kind-applicability tests | COVERED |
| WIRE-V1-01 | Hard Limits + Common Wire | operation encoded bytes <= 33,554,432 before expensive decode/allocation | A2 | raw wire bytes | codec wire preflight | exact/plus-one tests | COVERED |
| WIRE-V1-02 | Hard Limits + Reference IDL | nested ObjectRecord encoded message <=16,777,216 from raw nested length, never typed-memory guess | A2/A3 | raw length-delimited field | codec preflight seam | nested boundary/seam test | COVERED |
| WIRE-V1-03 | Hard Limits | generic UTF-8 string <=1,048,576 bytes | A2/A3 | raw string carrier + typed strings | codec preflight + validator | raw 1MiB/1MiB+1 and typed string boundaries | COVERED |
| WIRE-V1-04 | Hard Limits | EditRichText inserted aggregate <=8,388,608 bytes only | A3 | `RichTextDelta` | validator | aggregate/document tests | COVERED |
| WIRE-V1-05 | Hard Limits + Geometry Aggregate Accounting | keyed batch <=65,535; masks/object <=65,535; exact geometry aggregate <=2,000,000 | A1/A2/A3 | vectors/raw wire | normalizer/validator/codec | raw collection limits + independent geometry boundaries | COVERED |
| GAA-V1-01 | Geometry Aggregate Accounting Contract §GAA-01..05 | exact atom weights: path `1/1/2/3/0`, sample `1`, dab `3`, erase segment `6`; operation-payload-only checked aggregation | A2/A3 | `OperationPayload` geometry carriers | `validator.cpp::geometryUnits` | weighted carrier + `N-1/N/N+1` geometry tests | COVERED |
| A0-CARRIER-01 | Reference IDL + generated Proto baseline | nested protobuf payloads map completely or fail closed; enum/descriptor identity matches authority | A0/A2 | generated DTO → typed mapper | `decodeProtobufOperation` | protobuf ON projection/descriptor tests | COVERED |
| A0-CARRIER-02 | Operation Structural Semantics §2 | raw version-field occurrence preserved before scalar default erasure | A2 | `OperationFieldPresence` | codec preflight | protobuf presence test | COVERED |
| B-REF-01 | Connector/ObjectKind/Operation Validation | target existence/current kind/applicability, parent/reference/connectability, current mask/text state | B | stateful ObjectStore | not implemented in A | linkage poison tests | DEFERRED_B |
| B-REF-02 | Operation Payload Validation | idempotency, cycles, delete cascade, ApplyPlan/before-image | B | stateful apply seam | not implemented in A | linkage poison tests | DEFERRED_B |
| C-OUTCOME-01 | Verification/G1-04-C authority | stable stage/path/category, reviewed intent, independent golden differential | C | independent oracle/corpus | not production validator | C review artifacts | DEFERRED_C |

## Mechanical gate

Before fresh Evidence, every row owned by A0/A1/A2/A3 must be `COVERED`;
`MISSING`, `WRONG_ORACLE`, `UNOWNED`, and `BLOCKED_CARRIER` must be zero.
`DEFERRED_B`/`DEFERRED_C` are permitted only where their exact owner is shown.

## P34 Review Finding Lineage

| Finding ID | Review source | Evidence source | Closure authority/rule | Closure source | Status |
| --- | --- | --- | --- | --- | --- |
| P34-R09..R17 | External P34 verdict reference absent from repository | Historical Evidence `c073054..ac69fa8` | Normative rows above; no semantic rule inferred from missing verdict text | P35/P36 matrix review | PROVENANCE_ONLY |
| P34-R18 | P34 review | Historical Evidence `ac69fa8` | `CN-V1-06` StablePort static namespace | Semantic Leaf Structural Validation §6 | CLOSED_IN_P36 |
| P34-R19 | P34 review | Historical Evidence `ac69fa8` | `WIRE-V1-01..05` raw-known limit preflight | Semantic Hard Limits + Common Wire | CLOSED_IN_P36 |
| P34-R20 | P34 review | Historical Evidence `ac69fa8` | Mechanical coverage accounting | This matrix + checker | CLOSED_IN_P36 |
