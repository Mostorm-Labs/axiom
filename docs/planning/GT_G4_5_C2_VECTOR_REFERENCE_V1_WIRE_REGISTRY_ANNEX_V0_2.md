# GT-G4-5-C2 VectorReferenceV1 — P12/P13 Wire, Registry and Compatibility Annex v0.2

Status: **PROPOSED_PENDING_P21_P23_REVIEW**. This annex is the concrete semantic
input for the C2 amendment. It is not a schema release, a P31 package, or
permission to change production code.

## 1. Scope and preserved identities

This is an additive, versioned profile for `VectorStroke` only. It preserves:

- `ObjectKind.VectorStroke = 5` and the existing `AddStroke = 10` operation;
- all v1 `BrushDescriptor`, `StrokeRecord`, `VectorStrokeData`, `DabStrokeData`
  wire fields and canonical projections;
- the R1 perfect-freehand reference commit
  `176e00f2399f4969e1b0965c5921d96a3e50ce9f`;
- existing semantic numeric rules: finite values, `-0 -> +0`, no clamp/default
  substitution, and checked collection/geometry limits.

No new `ObjectKind`, `OperationKind`, GPU resource, `SkPath`, Arc state,
prediction state, or renderer cache is canonical data.

## 2. Registry identities

The following IDs are proposed for one atomic registry release:

| Registry | ID | Symbol | Version | Meaning |
|---|---:|---|---:|---|
| Brush family | `4` | `VECTOR_REFERENCE` | `1` | explicit VectorReference execution profile |
| Execution profile | `1` | `VECTOR_REFERENCE_V1` | `1` | typed payload below |
| Algorithm | `1` | `PERFECT_FREEHAND_R1` | `1` | pinned R1 interpreter identity |
| Easing | `1` | `LINEAR` | n/a | the only accepted easing |
| Taper policy | `1` | `DISABLED` | n/a | fixed profile policy, not an authoring default |

Unknown IDs and unsupported `(family, version)`, `(profile, version)` or
`(algorithm, version)` pairs fail closed. IDs are not inferred from display
names or platform brush selectors.

The proposed brush-family entry is:

```yaml
- family_id: 4
  symbol: VECTOR_REFERENCE
  brush_version: 1
  representation: VectorReferenceV1
  blend_modes: [NORMAL, HIGHLIGHTER]
  texture_resource_id: absent
  pressure: explicit_sample_presence
  tilt: unsupported
```

For this family the legacy descriptor remains the color/opacity/blend carrier.
Its `nominal_size` must equal the resolved payload `size`. In the new profile the
descriptor's pressure curves, tilt, smoothing and spacing are neutral and do
not supply execution parameters; the typed payload is authoritative.

## 3. Concrete wire layout

The existing `StrokeRecord` keeps fields 1--4 unchanged and adds one oneof
member at tag 5. No existing tag is reused.

```proto
message VectorReferenceSample {
  Vec2 position = 1;            // f64 x/y, finite and content-space
  optional double pressure = 2; // absent is distinct from present 0.0
}

message VectorReferenceV1 {
  Id128 definition_id = 1;
  uint32 definition_version = 2;
  uint32 execution_profile_id = 3; // VECTOR_REFERENCE_V1 = 1
  uint32 algorithm_version = 4;    // PERFECT_FREEHAND_R1 = 1
  double size = 5;
  double thinning = 6;
  double smoothing = 7;
  double streamline = 8;
  uint32 easing_id = 9;             // LINEAR = 1
  bool start_cap = 10;
  bool end_cap = 11;
  bool simulate_pressure = 12;
  uint32 taper_policy_id = 13;      // DISABLED = 1
  repeated VectorReferenceSample samples = 14;
}

message StrokeRecord {
  BrushDescriptor brush = 1;       // existing field, v1 unchanged
  fixed64 deterministic_seed = 2;  // the single canonical seed source
  oneof data {
    VectorStrokeData vector = 3;   // existing v1 representation
    DabStrokeData dab = 4;         // existing v1 representation
    VectorReferenceV1 vector_reference = 5;
  }
}
```

`VectorReferenceV1` does not repeat `deterministic_seed`: field 2 of
`StrokeRecord` remains the single source and is copied into `BrushCommitIntent`.
The payload does not contain `last`; `last=false` is active-session state and
`last=true` is supplied only to the sealed/reconstruction evaluator.

Presence and validation are exact:

- every scalar above is required by semantic presence rules, even where a
  generated protobuf API has a scalar default;
- `samples` is non-empty and order/duplicates are preserved;
- present pressure is finite in `[0,1]`; absent pressure is passed through the
  R1 missing-pressure adapter sentinel and is never encoded as NaN;
- IDs and registry values are validated before expensive evaluation;
- missing, unknown, non-finite, out-of-range, overflowing, or non-neutral values
  reject the whole object/operation atomically.

The v2 descriptor constraints are:

```text
brush_family_id = 4
brush_version   = 1
texture_resource_id = absent
brush.nominal_size == vector_reference.size   (exact normalized f64 equality)
pressure curves = absent
tilt.enabled = false; tilt influences = +0
smoothing.amount = +0; spacing.normalized_spacing = +0
```

Color, opacity and blend mode remain in `BrushDescriptor`; they are not copied
into the typed payload. `NORMAL` and `HIGHLIGHTER` are explicit descriptor
values, not inferred from a profile name.

## 4. Version and compatibility matrix

The version dispatch below is proposed as the early rejection boundary. It must
be reviewed against the existing object/operation/snapshot version authorities
before implementation:

| Input | v1 reader | v2 reader |
|---|---|---|
| VectorStroke kind v1 + `vector`/`dab` | accept under existing rules | accept unchanged |
| VectorStroke kind v2 + `vector_reference` | reject before apply | accept after full validation |
| Operation payload v2 | reject before apply | accept supported pairs only |
| Snapshot schema v2 | reject before restore | accept after object/version validation |
| kind v2 with old `vector`/`dab` branch | reject | reject |
| unknown profile/algorithm/easing/taper | reject | reject |
| missing typed payload/required field | reject | reject |

Insert, Restore and Split validate this matrix for every contained object.
Delete, transform, erase-mask and property operations retain their existing
mutation semantics and must not downgrade, erase or reinterpret a v2 stroke.
Unknown protobuf fields are not a compatibility mechanism: old readers reject
the explicit version before apply, and no reader may silently drop tag 5 and
reinterpret the stroke as v1.

New readers support v1 data without conversion. There is no opening-time
rewrite, catalog lookup, or best-effort fallback from v2 to v1.

## 5. Initial complete authoring/qualification profile

The first qualification fixture is an ID-addressed complete profile, not a
brush-name preset and not an implementation guess:

```yaml
definition_id: 00000000000000000000000000000001
definition_version: 1
execution_profile_id: 1
algorithm_version: 1
size: 8.0
thinning: 0.5
smoothing: 0.5
streamline: 0.5
easing_id: 1
start_cap: true
end_cap: true
simulate_pressure: true
taper_policy_id: 1
descriptor:
  brush_family_id: 4
  brush_version: 1
  nominal_size: 8.0
  opacity: 1.0
  blend_mode: NORMAL
  pressure_enabled: false
  tilt_enabled: false
  smoothing_amount: 0.0
  normalized_spacing: 0.0
```

The seed is supplied per stroke and may be zero. A different profile must carry
a different definition ID/version or a complete replacement set; no partial
override or platform selector is a resolver input.

## 6. Canonical projection and digest order

Existing v1 projection/digest bytes are unchanged. The new projection is
`axiom.vector-reference-projection-v1` and is selected only for a validated
v2 VectorStroke. It uses existing canonical protobuf rules, numeric-tag order,
unsigned-byte ID ordering for object collections, and little-endian IEEE-754
binary64 bits after `-0 -> +0` normalization.

Within the new stroke projection, fields are emitted in this exact order:

```text
object.id, kind_id, kind_version, placement, transform, properties,
erase_masks,
stroke.brush (descriptor fields 1..11 in tag order),
stroke.deterministic_seed (fixed64 field 2),
vector_reference.definition_id,
definition_version, execution_profile_id, algorithm_version,
size, thinning, smoothing, streamline,
easing_id, start_cap, end_cap, simulate_pressure, taper_policy_id,
samples.count,
for each sample in input order:
  position.x, position.y, pressure_presence, pressure_if_present
```

The digest input is canonical projection bytes prefixed by ASCII domain
`axiom.semantic.digest.v1` and a little-endian `uint32` projection version.
Digest algorithm is SHA-256. Every resolved option, identity, seed and confirmed
sample is included. Prediction, pointer IDs/timestamps, preview revisions, Arc,
SkPath, mesh, GPU, catalog contents and renderer caches are excluded. Pressure
absence and present `0.0` are distinct canonical states.

## 7. Operation and replay impact

`BrushCommitIntent` carries this complete typed payload plus the single seed and
confirmed samples. Runtime Core's Operation Builder emits existing `AddStroke`
with the reviewed version pair; Brush Engine never creates an Operation.
`StrokeRecord` is sufficient for fresh replay without Brush catalog, BrushSession,
Arc preview or GPU state. Unsupported versions fail before canonical mutation,
leaving document revision and digest unchanged.

This annex must be reviewed together with the C2 amendment and verification
annex before package regeneration. No generated schema, codec, validator,
projection or production file is changed by this document alone.
