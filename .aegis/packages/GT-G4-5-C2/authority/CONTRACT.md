# Brush Engine v0.4 — P12/P13 frozen Vector contract

Role: semantic modeling. Authority: user decision of 2026-09-24, new Brush
Semantic Kernel direction and explicit read-only V1 compatibility.
Objective: complete package → snapshot → AddStroke → reload semantics.
Non-goals: Dab, WetMix, scripts, archive importer, automatic V1 migration.
Required analysis: version rejection, presence, canonical recovery and scope.
Required output: this contract, schema, examples and exact binding to P31.
Quality/evidence gate: P20 obligations; design checks do not prove runtime behavior.
Handoff: P14/P16, P20 and P31 explicitly requested as this task's sequence.

## Format and parameter registry

Editable package contains `manifest.json` and `pipeline.json`; no executable
content. P32 implements directory/embedded JSON loading; ZIP import/export is a
successor, not a prerequisite. Maximum JSON file size 64 KiB, depth 16. UTF-8,
unique keys, no comments, no NaN/Infinity, no unknown keys. JSON Schema files in
this directory define shapes; duplicate keys and size/depth are parser checks.
Metadata is display-only. Manifest package ID is 32 lowercase hex digits,
nonzero; revision is u32 in [1, 4294967295]. Schema/pipeline/defaults/profile
versions are exactly 1. No legacy family ID participates in resolution.

Versioned defaults are: size=16 content units, thinning=0.5, smoothing=0.5,
streamline=0.5, pressureSource=simulated, missingPressure=reject,
easing=linear, startCap=endCap=true, startTaper=endTaper=0;
paint rgba=[0.05,0.1,0.2,1], opacity=1, blend=source-over, colorSpace=srgb.
Size range (0,4096], thinning [-1,1], smoothing/streamline/rgba/opacity [0,1].
All numbers are binary64 finite; normalize signed zero to positive zero.
Taper distances are exactly 0 for this profile. No custom easing is accepted.
JSON `default` is documentation; resolver must actually insert each absent value.
Explicit zero/false is never treated as absent. Merge field-by-field:
defaults v1 → package values → runtime overrides (size, rgba, opacity only).
Other override keys reject. Mid-stroke edits affect the next stroke only.

Pressure source simulated (wire 1) ignores device pressure and passes *absent*
pressure to pinned R1 with simulate_pressure=true. Source device (wire 2) passes
present normalized [0,1] pressure with simulate_pressure=false. Missing policy
reject (wire 1) rejects a device sample lacking pressure atomically; half (wire 2)
inserts 0.5 before evaluator use. For simulated source missing policy must be
reject; absent is its normal input, not an error. Capability selection is only a
pre-begin support check; it cannot silently change source. An application may
explicitly select a different package before begin. No outer pressure curve,
streamlining or smoothing; those are owned solely by the Vector node.
Samples contain content-space binary64 positions bounded by absolute 1e9.
A device sample's pressure presence is preserved; simulated samples persist
absent pressure. Timestamps, sequence IDs and prediction provenance are transient.
The sample list preserves confirmed event order including repeated positions.
Maximum 1,000,000 samples; fail the batch before accepting an overflow.

## Constrained stage registry

Stage IDs/order: input=1, vector=2, taper=3, shape=4, grain=5,
rendering=6, wetMix=7. Mode IDs: off=1, auto=2, on=3. Each stage appears exactly
once, sorted by ID. Omitted authoring stage inserts the default mode:
input=on, vector=on, rendering=on; others=off.

Released nodes: input=(1,1), vector=(2,1), solid-output=(3,1).
Graph edges input→vector→rendering. Profile=1 requires these three nodes;
`auto` for these nodes activates by dependency/output demand; `off` rejects.
Taper/shape/grain/wetMix accept only off or neutral auto (no parameters/resources),
both inactive with node_id=node_version=0. Explicit on rejects
UNSUPPORTED_STAGE. No arbitrary edges/nodes are authorable. The compiler derives
active nodes [1,2,3], never runs inactive nodes and allocates zero inactive state.
Dependency on an off/missing provider rejects, never overrides the declaration.
A supported config cannot contain a cycle; attempts to add edges reject as an
unknown field. Future graph expansion requires another registry/profile version.
Requested modes and derived active/node tuples all enter the snapshot; changing
on to auto may preserve pixels but changes the declared snapshot identity.

Node 2 binds reference commit 176e00f2399f4969e1b0965c5921d96a3e50ce9f,
with existing R01 source/fixture lock. Its adapter alone constructs StrokeOptions:
size/thinning/smoothing/streamline/caps from resolved values; easing linear;
all taper values 0, all taper-enabled/full-length flags false;
simulate_pressure=(pressureSource==simulated); last=false during preview,
last=true at seal. `last` is lifecycle state, not an authoring override.

## Snapshot, canonical geometry and replay

The normative field/tag allocation is `brush_engine.proto`. Every singular
field is required and encoded once (including 0 and false), except sample
pressure, whose absence is meaningful. Vec2 requires both scalars. Id128 uses
existing 16-byte nonzero convention. Empty repeated fields emit no tag. Repeated
stage entries have exactly seven entries; resources must be empty for profile 1.
Snapshot version/pipeline/defaults/profile/signal-schema are 1. Seed is u64,
including 0, passed by caller before begin, stored exactly, unused by Vector v1.
No PRNG consumption and no initial WetMix state exist in this profile.
Paint wire blend=1, color_space=1, easing_id=1. Other enum values reject.

Canonical stroke contains the complete immutable begin snapshot, confirmed
samples AND sealed renderer-neutral outline. Outline uses ordered Vec2 polygon
vertices, closed=true, fill_rule=1 (nonzero), at least 3 and at most 64,000,000
finite bounded vertices; no smoothing, refitting, winding normalization or
reordering. The writer omits a redundant repeated closing vertex. Output paint
is the snapshot paint; source-over alpha is rgba.alpha * opacity, applied once to
the filled whole contour. Empty outline at seal is an explicit NO_GEOMETRY
failure and produces no AddStroke. Geometry is CPU canonical data, not a GPU mesh.

Persisting outline makes document replay independent of evaluator availability
and platform libm variation. Operation replay validates and restores stored
snapshot/samples/outline, then derives scene/bounds/hit geometry from the stored
outline. It does NOT recompute it from samples or consult preset catalogs.
Re-evaluation from snapshot/samples is a separate conformance oracle (R01
numerical tolerances); it is never used to overwrite stored geometry on reload.
Unknown node/profile/snapshot versions reject even if an outline exists.

Canonical projection = existing canonical protobuf writer rules applied to
BrushExecutionSnapshot or BrushStrokeRecord: increasing tag order, minimal
varints, standard wire type fixed64 little-endian doubles, length-delimited
nested messages, no maps, all required presence preserved, normalized -0,
reject NaN/Infinity and unknown semantic fields. There is no custom type-marker
encoding. Snapshot semantic identity is exact projection bytes.
SHA256(UTF8("axiom.brush.snapshot.v1") || 0x00 || snapshot_bytes) is a derived
checksum; stroke checksum uses "axiom.brush.stroke.v1" and stroke bytes.
No stored digest is an independent source of canonical truth.

## Atomic schema release and version carriers

This is an explicit scoped replacement of the following V1-only clauses for new
Vector strokes. P32 materializes all schema, registries, codec/validator and
consumers together; a later unspecified schema release is NOT a dependency.
The production protobuf package name stays auditoryworks.axiom.v1 as a namespace;
semantic versions are carried by the fields below, not by that namespace name.

- ObjectKind ID stays 5; release exact pair (5,2), content branch brush_stroke
  tag 10. (5,1)/tag 5 is unchanged; cross-pair branch mixes reject.
- BrushStrokeRecord is separate from old StrokeRecord. No BrushDescriptor,
  old BrushFamily or new brush-family ID is allocated.
- OperationKind AddStroke stays 10, payload tag 10. Envelope schema_version=1.
  payload_version=2 is mandatory for AddStroke/Insert/Restore/Split carrying
  any (5,2) object; payload_version=1 for all others. A version 2 operation with
  no (5,2) record rejects as noncanonical. Mixed Insert/Restore lists use 2.
  Split from a (5,2) source is not offered by the first profile; rejects before
  mutation. New whole-object erasing and generic transforms use existing ops.
- Snapshot schema_version=2 iff it contains at least one (5,2), else 1.
- Old readers reject new operations at payload version before apply, snapshots
  at schema version, and bare new objects at (5,2). Unknown/new versions never
  get silently stripped, downgraded or reinterpreted as (5,1).
- New readers accept all unchanged non-brush V1 behavior and decode old
  VectorStroke/DabStroke through the existing semantic read/validate/render
  facilities. Any document containing legacy V1 strokes opens read-only as a
  whole: viewing, pan/zoom and copy-original export allowed, no mutating command,
  autosave rewrite or new stroke in that document. UI offers a new document;
  migration is not implemented. Protocol V1 replay remains usable only inside
  read-only loading/history reconstruction. Never expose a legacy authoring API.
- Operation builder emits only (5,2) for new strokes. New snapshots and mixed
  protocol fixtures preserve old records without converting them. Generic
  semantic engine tests can still decode/replay V1; this is not UI creation.
- Atomic preflight validates complete batch, versions, lengths, bounds and all
  fields before object store, ledger or scene mutation; reject leaves all three
  unchanged. Existing operation ID dedup and conflict behavior is retained.
  Retry after transport failure uses the exact same operation ID and bytes.
- Undo removes the new object; redo restores the exact (5,2) ObjectRecord via
  Restore payload v2, including identical snapshot/outline, without re-running
  the brush. Snapshot codec, Insert, Restore and history tests are in scope.

## Resources and unavailable capabilities

Resource states: Unloaded→Loading→Ready or Failed, managed during prepare.
A Ready reference includes 16-byte logical ID, 32-byte SHA256 content, decode
version=1, kind shape=1/grain=2, channel alpha=1/luma=2, color space srgb=1/
linear=2, sampling nearest=1/linear=2, wrap clamp=1/repeat=2. No network or
filesystem lookup on append. Any future active resources must be document
owned and hash-verified; eviction never changes content identity. Profile 1
requires empty resource lists: inactive stages must not perform even a read.
WetMix on, live canvas reads, natural-media presets and Dab output reject as
unsupported rather than falling back to old fake grain or old WaterColorLite.
