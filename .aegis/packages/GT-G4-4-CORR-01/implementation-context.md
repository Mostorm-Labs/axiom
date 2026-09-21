# GT-G4-4-CORR-01 implementation context

## Package repair

P31 v0.2 supersedes v0.1 for execution. Current scope is AutoIntent only.
MultiInk and GesturePriority remain deferred design ideas and are not authorized
implementation or verification work. No new blocking physical-device round is
required by this corrective.

## Repository identity

- Repository: Mostorm-Labs/axiom
- Canonical branch: main
- Task anchor: e5d0f418816b85a652f457a8d2235130c65f0a52
- Required relation: anchor is ancestor of accepted starting revision
- Execution branch: codex/g4-4-positiononly-corrective
- Downstream branch to preserve: codex/g4-5-programmable-brush

## Current repository reality

The accepted G4.4 baseline already has PointerKey generation fencing, keyed
InkEngine sessions, keyed PreviewModel state, keyed InteractionSessionManager
state, an exclusive per-pointer GestureArbiter primitive, and a two-finger
centroid/distance/scale helper.

The missing boundary is common AutoIntent arbitration. Pointer phase is not
currently carried as a runtime-neutral common fact; generic platform Down paths
immediately begin Ink sessions; Android additionally computes pinch
independently, while Windows/Web behave as immediate multi-Ink.

## Target ownership

Platform adapters report facts only. Common Axiom interaction runtime owns
PendingContact, DrawActivation, InteractionCohort, atomic viewport claim and
AutoIntent second-pointer semantics.

Current/default/only supported behavior in this corrective is AutoIntent.

## First incomplete action

Add PointerPhase/capability validity and implement the common AutoIntent
coordinator under RED acceptance tests before rewiring platform ingress.

## Preserved work

Do not rewrite PointerKey/generation fencing, keyed multi-stroke storage, keyed
Arc preview/handoff, canonical operation semantics, existing G4.5 programmable
brush work, or completed historical physical evidence.

## P32 design preflight

Before production mutation, record current flow, target coordinator flow,
affected types/files, obligation mapping and confirm:
- unresolved_semantic_decisions: []
- authority_conflict: false
- verification_conflict: false
- package_scope_conflict: false

Critical behavior oracles should be RED on the accepted baseline before
implementation: AutoIntent Pending->Viewport, PositionOnly pinch without
pressure/contact geometry, AutoIntent Ink-lock->later independent Ink, and
atomic group claim.
