# Compact stroke v1 implementation binding

The user's instruction to implement INK-V2-01/02/03 authorizes this additive
binding. Source baseline: `a0b20fbb0d31b6da0a536eb5f63fc7594468f415`.

Ownership remains Input -> CanvasEditingRuntime -> BrushSession ->
BrushCommitAdapter -> OperationEngine. Semantic owns the persisted value;
Ink owns effective signals and evaluation; Scene/Render own disposable display.

Allocate ObjectKind VectorStroke (5), kind_version 3, ObjectContent
`compact_stroke` tag 14, AddStroke payload_version 3. Existing versions and tags
are unchanged. Snapshot schema 3 requires `compact-stroke-v1`, reader bit 2048.
Readers without this capability reject before restoring or mutating a document.

The record contains config version 1, evaluator version 1, signal version 1,
material version 1, family, round mode, pressure provenance, paint, size,
material parameters, uint64 seed, and one spine. Spine positions use interleaved
little-endian binary64 x/y; effective pressure uses parallel uint16 LE, decoded
as q/65535. No raw samples, timestamps, dabs, outlines or per-stroke bitmap are
persisted. Knot count is 1..262144 subject to the smaller existing aggregate
limit. Byte lengths and finite coordinates are validated before allocation/apply.

Families: round=1, marker=2, chalk=3, crayon=4, watercolor-lite=5; laser=6 is
transient and is rejected by the persisted record validator. Round mode:
regular=1, handdrawn=2. Pressure source: simulated=1 (default), device=2.
Device requires captured pressure capability and finite [0,1] pressure in every
accepted sample. Explicit zero is valid. Config freezes at begin.

`speed-pressure-v1`: initial .5; pMin=.2, pMax=.8, vLow=80, vHigh=1200,
tau=.032 seconds. Target = .8-.6*smoothstep(clamp((speed-80)/1120));
filter uses 1-exp(-dt/tau). Equal timestamps do not invent elapsed time.
Predictive evaluation forks confirmed state and never advances it.

Default size 16, RGBA [.05,.1,.2,1], opacity 1, round handdrawn,
marker angle pi/4 and flatness .25, grain density .65, texture scale 8,
deposition .8, wet edge .4. Bounded values and exact supported versions reject
invalid or unknown input. Laser defaults: fade 1000ms, length 300 world units.

Public config/readback are owned values through the existing facade/Session;
IDs and seeds use lossless decimal strings in JS/JSON. Commands keep existing
owner-thread, non-reentrant and receipt semantics. Changes affect the next
session. SDK assets must be rebuilt together. This binding does not authorize
publication or change a historical reference corpus.

Material binding (INK-V2-02/03 implementation increment): compact presets are
`marker-compact-v1` (package 54), `chalk-compact-v1` (55), `crayon-wax-v1` (56),
`watercolor-lite-v1` (57), each revision 1. Historical marker/chalk revisions
retain their old interpretation. Material version 1 uses analytic ellipse sweep
and a fixed procedural paper field; it requires no external image/shape resource.
The immutable evaluator source is the built-in resource, identified in the
record by `ink-compact-material-v1`, its source SHA-256 and sampling version 1.
Unknown/missing bindings reject before mutation. No per-stroke resource blob is
created. Seed offsets are local to the object; current canvas and clock are not
inputs. Scalar inward coverage combines by max, then material and whole-stroke
opacity are applied once. Preview's view-bounded coverage cache is disposable;
canonical rendering uses 256x256 device tiles. The paper/pores, wax threshold,
and watercolor edge equations are frozen by the versioned evaluator source.
