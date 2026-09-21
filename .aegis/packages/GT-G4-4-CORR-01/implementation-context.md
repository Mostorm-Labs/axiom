# GT-G4-4-CORR-01 implementation context — P31 v0.3

## Reconciliation mode

P31 v0.3 supersedes v0.2. This is not a new coding pass. The corrective runtime
implementation already exists on codex/g4-4-positiononly-corrective.

Current blocking product/runtime guarantee: AutoIntent only.

Existing MultiInk, GesturePriority and mode-switch seams may remain in the branch
as deferred/non-blocking implementation. Do not clean them up and do not expand
them in this corrective.

## Trusted runtime boundary

- runtime result boundary: 3240a3b6567a898452462f4687939e799572c49d
- task anchor: e5d0f418816b85a652f457a8d2235130c65f0a52
- relation: ancestor
- current branch descendants after runtime boundary: .aegis control/package changes only
- common AutoIntent coordinator source has not been semantically rewritten after its primary passing evidence

## Preserved implementation

Preserve:
- PointerPhase
- ContactGeometry/capability availability
- common MultiContactCoordinator
- PendingContact and spatial DrawActivation
- provisional Ink/Preview cancellation before viewport ownership
- common viewport claim
- Windows/Android/Web phase-aware ingress into common host/runtime

## P32 action

Perform repository/evidence reconciliation only. No runtime mutation is
authorized. Resolve current v0.3 package and existing AutoIntent/PositionOnly
evidence, verify current source ownership, materialize reconciliation/result
records, and return READY_FOR_CONTROL_REVIEW if all frozen conditions hold.
