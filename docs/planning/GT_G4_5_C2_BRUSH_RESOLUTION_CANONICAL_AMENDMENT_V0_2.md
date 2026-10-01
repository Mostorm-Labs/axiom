# C2 Brush Resolution & Canonical Reconstruction — P12/P13 proposal v0.2

Status: **PROPOSED_PENDING_CONTROL_REVIEW**. Primary owner: `aegis-modeling`.
This is a semantic design proposal, not Current Authority, a P31 package,
an independent review verdict, a schema release or implementation permission.

Repository: `Mostorm-Labs/axiom`; branch: `codex/g4-5-common-brush-processing`.
Inspected baseline: `b3138f5af25d2fdbca08e4f95d3666e3218de7dc`.
Responds to `C2-AMEND-B01/B02/B03` in
`.aegis/results/GT-G4-5-C2/p31-amendment-v0.2-preflight.json`.

## 1. Decision requested

Approve a **narrow, additive, versioned canonical semantic extension** for the new
VectorStroke execution profile. Retain all existing v1 wire fields, interpreters,
operation identities and golden behavior. Do not disguise this extension as a
schema-free payload change.

The alternative of putting all new values into the old BrushDescriptor is rejected
as a recommendation: it cannot losslessly represent independent thinning,
streamline, smoothing, cap, pressure-simulation and algorithm binding values.
An external brush catalog or overloaded pressure curve is not a durable substitute.

This proposal completes the semantic choices rather than asking P32 to choose
them. Acceptance would still require P20 verification design and P21/P23 review of
the affected authority before P31 regeneration. It does not itself supersede them.

## 2. Source-of-truth map

All repository paths below are bound to the inspected baseline above.

| Source | Classification and treatment |
| --- | --- |
| `.aegis/packages/GT-G4-5/authority.lock.json` | Accepted package binding: one common BrushRuntime; v1 canonical behavior preserved; authoring BrushDefinition is not a canonical document object. Preserve these invariants. |
| `.aegis/packages/GT-G4-5-09V/authority.lock.json` | Recorded Vector-first authority binding, not a claim that 09V implementation passed. Its no-schema-expansion restriction conflicts with the requested new durable fields; request a scoped supersession for the new profile only. |
| `.aegis/packages/GT-G4-5-C2/authority.lock.json` and `execution-contract.json` | v0.1 execution contract remains blocked by TASK_PACKAGE_DEFECT. Do not resume or edit it to claim executable status. |
| Brush wire/family/interpreter documents under `docs/notion/authority/04-semantic-schema/05-leaf-schemas/` | Frozen v1 snapshots. Preserve v1 semantics; additions require explicit release/compatibility review. |
| `docs/adr/0026-semantic-v1-numeric-contract.md` | Accepted numeric separation: canonical values are not renderer float storage; preserve finite checks and negative-zero normalization. |
| `.aegis/packages/GT-G4-5-R01/reference.lock.json` | Exact R1 upstream/reference binding; accepted result `4c4a2a727ff6a61374f6b216de7fa4e51c85ba7c`. Do not change its kernel or limitations. |
| `docs/notion/authority/03-interaction-behavior/axiom-interaction-behavior-model-v0.1.md` | Proposed snapshot, not authority to override a frozen schema. |
| `runtime/semantic/**`, `runtime/ink/**`, `schema/axiom/v1/proto/**` | Implementation reality used to locate gaps, not authority by mere existence. |
| User amendment request | Authorizes preparation of this proposal; does not constitute approval of the new field values, wire layout or supersession decisions proposed here. |

No upstream Notion page was changed or independently refreshed in this step.

## 3. Entities and lifetimes

* BrushDefinition: versioned authoring value, not a new Document ObjectKind.
* ResolvedBrushExecutionParameters: immutable value produced by BrushCompiler.
  Its execution-relevant projection is durable for the new profile; it contains no
  program pointer, function pointer, GPU handle or mutable catalog reference.
* BrushSession: transient, ordered confirmed/predicted state; changes to authoring
  controls affect the next session, not an already-bound session.
* BrushCommitIntent: immutable transient handoff; confirmed-only semantic content
  plus transient receipt correlation. It is not an Operation.
* StrokeRecord: durable semantic value within the existing stroke ObjectRecord.
  ObjectRecord.id remains the authoritative stroke ID, not a duplicate inner ID.
* Operation: the existing semantic mutation unit. Keep `AddStroke` as the operation
  kind. `CreateStrokeOperation` in the request is a conceptual builder action,
  not a new wire OperationKind.

The new profile is VectorStroke only. No DAB, wet-media, texture or laser semantics
are added or migrated. Existing v1 profiles keep their old interpreter.

## 4. Resolution contract: no inferred presets

Use an explicitly discriminated profile `VectorReferenceV1`, separately versioned
from the authoring BrushDefinition version and canonical schema version. Bind it
to upstream perfect-freehand commit
`176e00f2399f4969e1b0965c5921d96a3e50ce9f` and the accepted R1 result above.
Do not reinterpret all definitions with integer version 2 as this profile: older
09F names/versions are not a namespace for this new profile.

For this profile the definition contains a **complete explicit parameter set**.
The resolver validates and canonicalizes that set, then copies it field-for-field.
There are no missing-field defaults, brush-name heuristics, pressure-capability
heuristics, or implicit legacy `pressureSizeInfluence -> thinning` conversion.
Per-stroke edits must first construct a complete new parameter set; partial maps
and override precedence are not part of the resolver contract.

| Resolved field | Source and admissible value | R1 binding |
| --- | --- | --- |
| size | explicit finite f64, strictly positive, content-space units | `size`, unchanged |
| thinning | explicit finite f64 in [-1, 1] | `thinning`, unchanged |
| smoothing | explicit finite f64 in [0, 1] | `smoothing`, unchanged |
| streamline | explicit finite f64 in [0, 1] | `streamline`, unchanged |
| easingId | explicit enum `Linear` only | accepted R1 identity pressure easing; no new callback |
| startCap | explicit boolean | `start_cap` |
| endCap | explicit boolean | `end_cap` |
| simulatePressure | explicit boolean | `simulate_pressure`; not selected by platform |
| startTaper | profile constant `Disabled` | `start_taper=0`, enabled=false, full_length=false |
| endTaper | profile constant `Disabled` | `end_taper=0`, enabled=false, full_length=false |
| last | session completion state, not an authoring knob | false during active evaluation; true on seal/reconstruction |

The `Disabled` taper choice is a **new proposed product restriction**, not a claim
about 09V's previously unspecified fixed taper. Its approval is required. It avoids
silently adding editable or full-length taper while the incremental prefix/tail
contract is being introduced. R1's general taper tests and behavior stay untouched.

No qualification brush preset values are selected here. P31 must carry a reviewed
explicit initial VectorReferenceV1 profile and input binding for platform migration;
executor-chosen Pen/Pencil settings remain prohibited. Failure to supply that
profile blocks P31, not an invitation for P32 to invent one.

Normalize -0 to +0; reject NaN/Infinity, missing/unknown fields, out-of-domain
parameters, unsupported profile/easing/version and output arithmetic overflow.
Do not clamp or substitute values. Existing canonical coordinate/size/hard limits
still apply; P20 must bind their exact identifiers when constructing fixtures.
Numeric/storage/resource limits are validation constraints, not new brush defaults.

For the same complete definition identity/version, profile and parameter set,
resolution produces bit-identical normalized scalar values and enum/boolean fields.
Compare a specified field projection, never C++ struct memory/padding. Resource,
device, timing, locale and platform APIs cannot affect resolution.

Deterministic seed is an explicit uint64 session input, copied unchanged into
commit and canonical storage (zero allowed). It is not derived from wall clock or
host RNG. The new R1 vector profile does not consume random channels; preserve
legacy random-channel rules for legacy profiles. Equal resolved options do not
imply equal seeds if the caller intentionally supplied different seeds.

## 5. Confirmed input and durable stroke payload

The new typed VectorReference payload contains:

* definitionId and definitionVersion (provenance; not a replay-time catalog lookup);
* immutable executionProfileId/algorithmVersion and resolved options;
* ordered confirmed content-space samples: f64 x/y and explicit pressure presence
  with a finite f64 pressure in [0,1] when present;
* deterministic seed, using the existing StrokeRecord seed as the single source;
* canonical color/opacity/blend from the existing descriptor, with no second
  conflicting copy; existing object placement and transform remain authoritative.

Missing pressure is not serialized as NaN. It is converted at the R1 adapter seam
to the accepted reference's missing-pressure sentinel. Real pressure stays exact.
Explicit simulatePressure controls reference behavior on every platform. Prediction,
pointer IDs, timestamps, preview revisions and raw platform packets are excluded.
Preserve confirmed ordering and duplicates required by R1; do not encode a geometry
simplification disguised as serialization.

Canonical samples alone are **insufficient**. They become a reconstruction-complete
payload only together with the persisted resolved options and pinned interpreter
identity. This is not a pointer-event log. No SkPath, triangle mesh, DAB preview,
GPU resource or Arc state is canonical content. The outline is derived by the
accepted reference interpreter; it is not an independently mutable second truth.

The proposed profile uses its resolved size exclusively; any retained descriptor
size mirror must match exactly. Legacy pressure/smoothing/spacing fields are not
applied a second time to this new profile. The wire/registry review must define
their neutral encoding and validate consistency, never overload them with options.
Tilt/resource-dependent interpretation is unsupported for this profile.

## 6. Versioned extension and compatibility

Extend stroke content with a **typed, discriminated VectorReference payload**,
not an opaque sidecar, PropertyBag escape hatch, or new ObjectKind. Keep old
VectorStrokeData/DabStrokeData encodings and existing tags unchanged.

The new payload must be protected by a new supported object/schema version and
Operation payload version wherever the payload can occur. Old readers must reject
unsupported data before apply/restore; protobuf unknown-field dropping must never
turn a new stroke into an apparently valid old stroke.

This affects more than AddStroke: InsertObjects, RestoreObjects, SplitStrokes,
snapshot export/import, projection/digest, equality/fingerprint and all records
containing strokes need consistent version dispatch. Delete/transform/masks retain
their mutation semantics; no operation may erase or silently downgrade the new
execution profile. Partial erase must preserve the profile or reject unsupported
work explicitly, not reconstruct a v1 approximation.

Old data continues to decode/replay under its original interpreter. No automatic
conversion, opening-time rewrite or brush-version migration is authorized. New
readers support old data; old clients cannot edit/round-trip the new version as v1.
Unknown algorithm IDs reject, even if two IDs happen to produce equal images.

This is a semantic proposal, not a frozen wire descriptor. Numeric registry IDs,
new wire tags, presence rules, version compatibility matrix, hard-limit bindings,
canonical projection field order and codec corpus must be issued as an accepted
P12 wire/registry annex before P31 is executable. The proposal does not reserve
tags or modify `semantic-sdk.lock.json`; published dependency policy remains intact.
Generated schema artifacts and any SDK-consumer impact must be evaluated separately
from rebuilding the locked Protobuf toolchain/runtime, which is not authorized here.

## 7. Operation ownership and lifecycle

Only Ink-routed confirmed input reaches the BrushSession. Resolution precedes the
session on the ordered C1 -> C0 -> C2 path. No required queue or wait on Core/Arc/GPU
is introduced before BrushPreviewDeltaReady.

Seal emits a confirmed-only BrushCommitIntent containing stroke identity, complete
new-profile canonical content, resolved options and seed. Cancel before seal emits
none. Receipt session/revision/generation may travel alongside it as transient
correlation, never as document truth.

Runtime Core's Operation Builder validates intent completeness and constructs
existing AddStroke with the correct versioned ObjectRecord. Brush Engine cannot
mutate Document or create/publish Operations. Core allocates/stabilizes OperationId
and preserves it for retries of the same intended mutation. Apply follows the
existing normalize/validate/idempotency/prepare/atomic commit path.

Same OperationId and same canonical payload remains AlreadyApplied; same ID with
different resolved options/algorithm/sample content is a collision. Equality and
digest must include every new semantic field. Rejection leaves canonical state
unchanged. Queue failure is explicit; reliable commit is never latest-wins and
canonical congestion cannot block unrelated move-time preview.

Pen-up, enqueue, serialization, successful apply and RuntimeScene rebuild are not
CanonicalVisible. Keep the final preview until the existing matching visible receipt
arrives; failure or a stale receipt cannot masquerade as visible success.

Undo uses normal compensating mutations. Redo/restore uses the original persisted
profile/options/samples, not today's brush catalog. Local/remote/replay share the
same mutation interpretation and existing no-echo rules. This does not claim that
new profile collaboration or recovery implementation has been qualified.

## 8. Reconstruction semantics for downstream verification

Live path:
`confirmed input -> resolved session -> intent -> AddStroke -> Document -> Scene -> Render`.

Replay path:
`serialized Operation -> fresh decode/apply -> Document -> Scene -> Render`.

Replay must work with no live BrushSession, no authoring catalog, no Arc state and
no unpersisted resolved-parameter side table. Changing/removing the runtime brush
definition after commit cannot change replay.

Proposed semantic equality is exact for normalized options, identities, seed,
ordered canonical samples and typed StrokeRecord. Projection equality is exact in
the same semantic projection version. R1-derived outlines use its accepted 1e-5
coordinate tolerance with exact counts/order, not a tolerance on durable storage.
Resolution equality must not be relaxed to an image comparison.

P20 must independently design V11/V12 oracles and RED fixtures, including parameter
mutation sensitivity, missing-option rejection, catalog removal, codec round-trip,
fresh replay, old-v1 coexistence, unknown versions, malformed presence and prediction
exclusion. Scene equality cannot substitute for a rendered-output check; matching
old fixed-path renders cannot prove R1 geometry is actually consumed.

For render comparison P20 must bind an exact existing renderer/harness, profile,
resource set and equality rule. If the renderer cannot consume this representation,
report that dependency to P14/P31; do not implement R2 render work inside C2 or
label two empty/fallback frames as reconstruction parity. This document is an input
to P20, not a regenerated verification.lock or test evidence.

## 9. Scope, approvals and remaining work

This turn edits this proposal and its provenance/result record only. No production,
schema, package lock, Notion authority or Project State mutation is authorized by it.

Approval is requested for: explicit complete parameters (no inferred presets),
Linear easing and disabled taper for the new profile, typed durable resolved
options plus confirmed samples, existing AddStroke kind, and scoped additive
schema/version extension while preserving v1.

Required sequence after that design decision:

1. Finish the P12/P13 wire/registry/compatibility annex and initial authoring profile.
2. Obtain P21/P23 authority reconciliation/approval, preserving old history and
   explicitly superseding the relevant no-expansion clause only for this profile.
3. Complete P20 V11/V12 proof bindings and resolve canonical-render dependencies.
4. Regenerate P30/P31 scope and all locks; mark ready for P32 entry review only
   after those exact bindings are approved. Scope will include necessary semantic
   codec/validator tests; adding only `runtime/ink/operation_builder.*` is insufficient.
5. Start P32 only under the newly accepted package; no automatic execution here.

Existing C2 mailbox, ordering, incremental parity/complexity, prediction and
CanonicalVisible obligations remain unchanged. No D0/R2/G4.5/Gate PASS is claimed.
