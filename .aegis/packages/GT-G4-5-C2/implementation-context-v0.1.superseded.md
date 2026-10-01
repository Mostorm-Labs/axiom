# GT-G4-5-C2 implementation context

## Purpose

Make routed Ink pass through one common Brush hot path before preview/canonical fanout. Preserve C1 ingress, C0 routing, and the accepted R1 one-shot perfect-freehand evaluator.

## Trusted starting point

Repository: Mostorm-Labs/axiom
Execution ref: codex/g4-5-common-brush-processing
Task anchor: 85cd05961243dcf5fa56179fcd309638a1c87d2a
Accepted C1 result: 5087ca34b3b956844d0899c88ff2f4320d491e70
Accepted R01 result: 4c4a2a727ff6a61374f6b216de7fa4e51c85ba7c

## Current gap

BrushRuntime append stores all samples and re-evaluates the full session history. Web and Android still own local BrushRuntime sessions. Windows still derives Arc preview geometry from raw pointer samples.

## Frozen thread model

C1 ingress -> C0 route -> C2 incremental Brush runs as one ordered preview-critical logical path through BrushPreviewDeltaReady. No mandatory inter-module thread hop is allowed.

After BrushPreviewDeltaReady:
- preview uses a bounded high-priority async mailbox;
- superseded non-terminal preview revisions may coalesce;
- terminal/cancel/retire control events are reliable.

On seal:
- final preview remains live;
- confirmed-only BrushCommitIntent enters a separate reliable canonical path;
- matching CanonicalVisible retires preview.

## Correctness and performance

The R1 one-shot evaluator remains the correctness oracle. The move-time incremental path must not call it over full history on each append.

The long-stroke structural gate compares 512 vs 2048 samples under the same deterministic local geometry and batch size. Per-append work must not scale with total history.

## First incomplete action

Resolve repository/package identity and hashes, record ImplementationDesignPreflight, add the frozen RED oracles and observe RED, then execute the full closure contract until READY_FOR_D0_PACKAGE_REVIEW or an explicit terminal blocker.
