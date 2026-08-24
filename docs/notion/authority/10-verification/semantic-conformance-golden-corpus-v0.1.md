# Semantic Conformance + Golden Corpus v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81d9a2eaf77cc4fb67c2
> Snapshot date: 2026-08-24
> Source status: Draft / Freeze Candidate — Semantic Verification Contract

## Scope

Converts Common Wire, OrderKey, Reference IDL and the 15 Operation families into executable golden corpus and C++/TS/WASM differential conformance. Corpus follows upstream authority and must not become an implicit schema designer.

## Required proof

Same canonical value must decode/normalize/validate consistently; same Snapshot + Operation Stream must reach canonical-equivalent semantic result; canonical protobuf bytes must match exact golden where applicable; malformed/unknown/oversized inputs must agree at the frozen semantic error granularity; whole-op atomicity/idempotency/OperationId collision must agree; replay must locate first semantic divergence.

## Pipeline and oracle hierarchy

Input bytes/semantic fixture → Decode → Normalize → Validate → Apply → Canonical Semantic Projection → Canonical Encode → ConformanceResult. Stages must be independently testable.

Oracle priority: canonical semantic projection equality; canonical protobuf bytes; semantic checkpoints; digest only as comparison acceleration. Hash choice is not semantic protocol authority unless separately frozen.

## Golden corpus contract

Corpus is versioned and shared by all implementations. Stable vector metadata includes id, authority refs, requirement status, category, input kind/artifacts, optional initial snapshot/opstream, expected decode/normalize/validation/apply outcome, canonical projection/bytes/final state, OPEN blocking and notes. Stable IDs appear directly in CI regression reports.

## Common Wire families

- Id128: exactly 16 bytes; zero invalid; unsigned lexicographic order; host-endian independent.
- Numeric: finite f64; -0→+0; NaN/Inf reject; committed normalized scalar exact equality, no epsilon.
- UTF-8: valid accepted, invalid rejected, no implicit NFC/NFKC, limits by UTF-8 bytes where authority says so.
- Presence: absent differs from present(default) unless registry explicitly says otherwise; Clear differs from Set(default).
- Collections: OrderedSequence preserves order; CanonicalSet sorts/uniques; CanonicalMap sorts/uniques; PropertyBag by unsigned FieldId; keyed operation batches by ObjectId; PatchProperties by `(ObjectId,FieldId)`; duplicate canonical targets reject.
- Root placement: absent parent = root; zero ObjectId parent invalid.
- Registry/version: known/unknown kind/field/schema/payload/reserved/deprecated behavior follows authority.
- Hard limits: generate N-1/N/N+1 only when both limit and accounting unit are frozen; otherwise mark blocked OPEN.

## Canonical protobuf exact-byte rules

Normalize before encode; singular field once; numeric tag order; canonical map represented as sorted repeated entries rather than relying on map iteration; keyed/set collections sorted; packed form fixed by schema; -0 encoded +0; NaN/Inf rejected; UTF-8 valid; oneof exactly-one; unknown semantic kind/field/version does not enter canonical editor output; presence preserved. Library deterministic mode is not a substitute for the Axiom canonical profile.

## OrderKey

Validate length 1..32, reject empty/33/trailing 00, allow internal zero, unsigned byte semantics. Comparator examples include `20 < 21`, `20 < 20 80`, `20 7F < 20 80`, `20 FF 80 < 21`; equal keys tie-break by ObjectId lexicographically. Allocation golden covers between 20/40, 20/21, 20 FF/21, before-first, after-last, prefix-neighbor and near-exhaustion. Exhaustion requires explicit rebalance/SetPlacements path; window size is tuning, not semantic golden.

## Operation conformance family

Every one of the 15 operations uses: `VALID_APPLY`, `INVALID_PAYLOAD`, `INVALID_REFERENCE`, `WRONG_OBJECT_KIND`, `DUPLICATE_TARGET`, `ATOMIC_REJECTION`, `REPLAY`, `IDEMPOTENT_SAME_ID_SAME_PAYLOAD`, `ID_COLLISION_SAME_ID_DIFFERENT_PAYLOAD`, `UNDO_DATA_PRESERVATION` where supported.

Operations: InsertObjects, DeleteObjects, RestoreObjects, SetPlacements, SetTransforms, PatchProperties, SetObjectSize, SetVectorPathGeometry, SetImageContent, AddStroke, SplitStrokes, AddEraseMasks, RemoveEraseMasks, EditRichText, SetConnectorContent.

Mandatory special cases include staged insert references/cycles; cascade connector delete; restore validity; resulting hierarchy cycles; numeric normalization; PropertyBag applicability; image/content whole-op validation; stroke/mask integrity; ordered RichText delta and font semantics; Connector free/attached endpoints, AutoPerimeter hint, StablePortId, routing and whole-content atomicity.

## Connector V1 family

Release suite `CN-G01..CN-G32` covers endpoint validation, connectable kinds Shape/Image/Sticky, AutoPerimeter absent/present inequality and geometry, deterministic coincident fallback, StablePortId 1..4 and unknown rejection, atomic SetConnectorContent, derived anchor changes without semantic content mutation, cascade delete, restore dependencies and staged target+connector insertion.

## 07 intake additions

Conformance must also cover whole-op atomic apply, source/idempotency/no-echo behavior, identity namespace non-aliasing where observable, and replay/checkpoint evidence needed by later 07 recovery and runtime equivalence tests. These additions inherit 07 authority; corpus may not invent unresolved collaboration policy.
