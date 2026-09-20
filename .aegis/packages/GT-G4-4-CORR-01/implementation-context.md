# GT-G4-4-CORR-01 implementation context

## Repository identity

- Repository: Mostorm-Labs/axiom
- Canonical branch: main
- Task anchor: e5d0f418816b85a652f457a8d2235130c65f0a52
- Required relation: anchor is ancestor of accepted starting revision
- Execution branch: codex/g4-4-positiononly-corrective
- Downstream branch to preserve: codex/g4-5-programmable-brush

## Current repository reality

The accepted G4.4 baseline already has PointerKey generation fencing, keyed InkEngine sessions, keyed PreviewModel state, keyed InteractionSessionManager state, an exclusive per-pointer GestureArbiter primitive, and a two-finger centroid/distance/scale helper.

The missing boundary is common multi-contact arbitration. Pointer phase is not currently carried as a runtime-neutral common fact; generic platform Down paths immediately begin Ink sessions; Android additionally computes pinch independently, while Windows/Web behave as immediate multi-Ink.

## Target ownership

Platform adapters report facts only. Common Axiom interaction runtime owns PendingContact, DrawActivation, MultiContactPolicy, InteractionCohort, atomic viewport claim and second-pointer semantics.

Default MultiContactPolicy is AutoIntent.

## First incomplete action

Add PointerPhase/capability validity and implement the common coordinator under RED acceptance tests before rewiring platform ingress.

## Preserved work

Do not rewrite PointerKey/generation fencing, keyed multi-stroke storage, keyed Arc preview/handoff, canonical operation semantics, or G4.5 programmable brush work.

## P32 design preflight

Before production mutation, record current flow, target coordinator flow, affected types/files, obligation mapping and confirm:
- unresolved_semantic_decisions: []
- authority_conflict: false
- verification_conflict: false
- package_scope_conflict: false

Critical behavior oracles should be RED on the accepted baseline before implementation: AutoIntent default, PositionOnly Pending->Viewport, GesturePriority late-second ignore and atomic group claim.
