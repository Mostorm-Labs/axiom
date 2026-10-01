# GT-G4-5-C2 P12/P13 Authority Amendment v0.3

Status: **CONTROL-ACCEPTED FOR P31 REGENERATION; NOT P32 AUTHORITY**.

This amendment closes the prior P21/P23 findings without changing released V1
object, operation, or brush-family identities. It is the semantic authority for
the first Vector brush profile only.

## 1. Preserved released identities

* `ObjectKind.VectorStroke = 5`, exact version `(5,1)`.
* `OperationKind.AddStroke = 10`, operation envelope `schema_version=1`,
  `payload_version=1`.
* Brush family `FINE_PEN = 1`, brush version `1`.
* Existing `BrushDescriptor`, `VectorStrokeData`, `DabStrokeData`, and all
  released V1 validation behavior remain unchanged.
* R01 Perfect Freehand commit `176e00f2399f4969e1b0965c5921d96a3e50ce9f` is
  the evaluator oracle.

No new ObjectKind, OperationKind, or brush-family registry entry is allocated.
The former proposal for family 4 and kind/operation/snapshot version 2 is
superseded and must not be implemented.

## 2. Versioned VectorReference representation

The new profile is an additive `StrokeRecord.data` representation extension,
not a new object or operation version:

```text
StrokeRecord.data = vector_reference_v1 (extension tag 5)
profile_id       = 1 (VECTOR_REFERENCE_V1)
algorithm_id     = 1 (PERFECT_FREEHAND_R1)
```

The extension is carried by the existing AddStroke/ObjectRecord envelope. Old
readers that know only `vector`/`dab` see no recognized oneof member and reject
the StrokeRecord before mutation; they must not reinterpret it as V1. New
readers accept it only after full profile validation. Unknown fields are never
a forward-compatibility mechanism.

The extension uses the existing descriptor family `(1,1)` and requires
`descriptor.nominal_size == vector_reference.size` after canonical numeric
normalization. The descriptor remains the color/opacity/blend carrier.

### Logical wire fields (tag allocation is an additive schema-release input)

```proto
message VectorReferenceSample {
  Vec2 position = 1;            // finite content-space f64
  optional double pressure = 2; // presence distinguishes absent from 0.0
}
message VectorReferenceV1 {
  Id128 definition_id = 1;
  uint32 definition_version = 2;
  uint32 profile_id = 3;        // 1
  uint32 algorithm_id = 4;      // 1
  double size = 5;
  double thinning = 6;
  double smoothing = 7;
  double streamline = 8;
  uint32 easing_id = 9;         // 1 LINEAR
  bool start_cap = 10;
  bool end_cap = 11;
  bool simulate_pressure = 12;
  uint32 taper_policy_id = 13;  // 1 DISABLED
  repeated VectorReferenceSample samples = 14;
}
```

The runtime package may define the typed in-memory contract, but a future
schema release must allocate and publish the tag-5 oneof before persistence is
enabled. Until then the profile is not a released wire format.

## 3. Deterministic resolution and canonical projection

Resolution order is `EngineDefaults → explicit BrushPackage values →
whitelisted RuntimeStrokeOverrides`. Names, catalog position, platform
capabilities, locale, clocks, and renderer state are not inputs.

`ResolvedVectorStrokeParameters` contains exactly: `size`, `thinning`,
`smoothing`, `streamline`, `easing_id`, `start_cap`, `end_cap`,
`simulate_pressure`, `taper_policy_id`, algorithm/profile/version identities,
and the normalized missing-pressure policy. Values are finite; `-0` normalizes
to `+0`; invalid ranges, unknown IDs, and descriptor-size mismatch reject.

`axiom.vector-reference-semantic-projection.v1` is a named semantic projection,
separate from existing canonical protobuf bytes. Its field order is:

```text
object identity/kind/version/placement/transform/properties/erase masks;
descriptor tags 1..11;
deterministic_seed;
definition_id, definition_version, profile_id, algorithm_id;
size, thinning, smoothing, streamline, easing_id, start_cap, end_cap,
simulate_pressure, taper_policy_id;
sample_count;
for each sample: position.x, position.y, pressure_presence,
pressure_if_present.
```

Each field uses an explicit type marker and little-endian canonical scalar
encoding; IDs use unsigned-byte order; collections use the existing canonical
key order. Every numeric scalar is normalized before encoding. Equality of this
projection is the semantic oracle. `SHA-256(domain="axiom.semantic.digest.v1"
|| u32le(1) || projection_bytes)` is a derived acceleration/checksum only and
never replaces the projection or the recoverable fields.

## 4. Replay and Operation boundary

`BrushCommitIntent` carries the resolved typed profile, confirmed samples,
algorithm/profile identities, canonical representation and seed. Runtime Core's
Operation Builder mechanically emits existing `AddStroke`; Brush Engine never
creates or mutates an Operation. Operation replay validates and restores the
same StrokeRecord/projection without consulting BrushPackage catalog, current
defaults, Arc, prediction, or GPU state.

Prediction is preview-only and never enters the intent. Cancel before seal
emits no intent. Undo/redo and collaboration carry the opaque AddStroke.

## 5. Explicit compatibility matrix

| Input | Released V1 reader | Amended reader |
|---|---|---|
| `(VectorStroke,1)` + `vector`/`dab` | accept existing rules | accept unchanged |
| `(VectorStroke,1)` + `vector_reference_v1` | reject before apply | accept after full validation |
| operation envelope v1 with invalid profile | reject atomically | reject atomically |
| operation/snapshot envelope v2 | reject (unsupported) | reject (not part of this amendment) |
| unknown profile/algorithm/easing/taper | reject | reject |
| missing pressure presence or malformed samples | reject | reject |

The matrix applies to AddStroke, Insert, Restore, Split replacements and
snapshot restore. No opening-time conversion or best-effort fallback exists.

## 6. Package/resource boundary

The initial Vector profile uses no Shape/Grain/WetMix resource. Resource
identity/content-addressing and lazy-load rules remain frozen as inactive-stage
behavior: inactive resources are not loaded; a required-but-unready resource
rejects begin; no substitute resource is selected. WetMix is explicitly
unsupported/non-activatable in this profile.

