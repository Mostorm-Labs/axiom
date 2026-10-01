# GT-G4-5-C1 implementation context

## Purpose

Unify Web, Android, Windows, and Apple extracted platform input behind one Axiom platform-input contract after the accepted C0 semantic-routing consolidation.

## Trusted starting point

Repository: Mostorm-Labs/axiom
Execution ref: codex/g4-5-cross-platform-interaction-ingress
Task anchor: 0ced5c44df47fb2aeff4369473df054c7856a1f9
Accepted C0 result: d861e05e18a0665c0c91c7523925b3990bb0c88e

The anchor is a control-only descendant of the accepted C0 result.

## Preserve

Do not redesign AutoIntent, MultiInk, GesturePriority, viewport semantics, radial/path slop, predicted-sample ownership, or P34-NB-01.

Do not modify perfect-freehand.

## Main gap

Platform input entry remains fragmented:
- Web owns generation/stroke/Brush lifecycle in JS.
- Android Java/JNI owns stroke/generation and routes canonical input through Arc InputSource.
- Windows owns generation, ContactDisposition interpretation, Arc input packing, and commit/release decisions.
- Apple has no equivalent InkPlayground extracted-event adapter contract.

## Target

runtime/input owns PlatformPointerBatch, pointer lifecycle/generation, provenance/capabilities, and PassThroughInputCaptureGate.

Platform OS files only extract native events and submit bounded batches.

runtime/input must not link Arc. Any Axiom-to-Arc conversion belongs in common integration/composition code.

## First incomplete action

Resolve package identity and hashes, record ImplementationDesignPreflight, install frozen RED ownership/fixture tests and observe RED, then execute the full closure contract until READY_FOR_D0_PACKAGE_REVIEW or an explicit terminal blocker.
