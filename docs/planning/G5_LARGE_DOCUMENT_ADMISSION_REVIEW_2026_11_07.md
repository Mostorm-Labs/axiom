# Large-document admission review after the DPR4 coverage repair

This is an implementation-boundary review, not new Semantic authority or a scale
Gate. User authorization and the Render repair scope are frozen in
`G5_DPR4_CRITICAL_REPLAY_2026_11_07.json`. Runtime/OperationEngine remain the
canonical owners; SnapshotCodec is the restore boundary. The Render repair does
not change these contracts.

## Current boundaries

| Boundary | Current implementation | Meaning |
| --- | --- | --- |
| Snapshot repeated wire fields | `runtime/semantic/src/codec.cpp`, `kMaxSnapshotRepeatedItems=65535` | Occurrences per repeated wire field per message. A flat ObjectRecord list over 65535 rejects during preflight. This is not a general Runtime document maximum, nor an element-count rule for every packed array. |
| Snapshot ObjectRecord | same file, `kMaxObjectRecordBytes=16MiB` | Serialized ObjectRecord length; not a total snapshot-file or image-resource limit. |
| CompactStroke | `runtime/semantic/include/canvas/semantic/compact_stroke.hpp`, 1–262144 knots | Per stroke. Ten valid100K-knot strokes can have a document total of1M. |
| Derived rendering | `RenderCacheOptions`, ResourceBudgetManager |32MiB optional / 96MiB shared. These limits do not bound canonical store, decoded protobuf, full Scene metadata or complete document RSS. |
| Verification containment | existing G5 corpus | Native 2GiB / browser-family 3GiB / 180s. These are local worker bounds,not published product capacities. |

The prior 49-row matrix at code `c05d3a2f54fcb44c771d797bbe3255f46dcac962`
completed public-SDK 1K/10K object tiers and single-stroke 100K points. 100K-object
snapshot inspect returned SemanticError 7; 1M-object component/fixture workers hit
RSS containment. The independent ten 100K-stroke case completed with 1M stored
points and 16K point centres in the viewport. These are different axes.

The current Render repair leaves the codec/snapshot/CompactStroke sources and
published dependency locks unchanged. The new evidence repeats public-SDK 10K
objects and ten100K strokes as corroborative smoke tests; it does not upgrade
any previously blocked 100K/1M-object tier.

## What should happen next

1. Freeze the intended **product** load range by object count, stored points,
   masks/text/resources, restore time and host memory. Distinguish document
   totals from visible complexity and renderer cache occupancy. Current
   usability and a 1M-point document do not establish 1M-object support.
2. Give the Semantic/admission owner a separate task if the requested product
   range exceeds the existing full-snapshot boundary. Required proof includes
  65,534 / 65,535 / 65,536 record boundaries, ObjectRecord byte boundaries, rejection
   before expensive decode/allocation, digest/capability validation, failed
   restore atomicity, and public-SDK errors/receipts. This review does not choose
   or authorize a new limit/format/streaming protocol.
3. Measure restore peak components separately: source bytes, protobuf decode,
   SemanticSnapshot/store, Scene publication/index and renderer work. Raising
   a wire guard alone does not establish bounded memory; the previous 1M-object
   worker failures remain relevant.
4. Repeat the object-count and document-point matrices only after the intended
   admission profile and oracles are frozen. Keep rejected/capped tiers marked
   BLOCKED_EVIDENCE. A Scene/index component pass cannot replace root-SDK
   restore/render/navigation evidence.

Do not bypass SnapshotCodec by writing stores directly or substituting a stream
of editing Operations while claiming the original atomic snapshot restore was
qualified. Do not increase Render budgets to disguise document admission costs.
Public consumer APIs remain the root JS facade and Windows C/POD contract;
internal stores/Scene/Skia objects are not a proposed consumer escape hatch.

Independent physical input, other-platform packaging, the 66 existing native
failed/not-built cases and formal G5/P34 acceptance remain separate obligations.
