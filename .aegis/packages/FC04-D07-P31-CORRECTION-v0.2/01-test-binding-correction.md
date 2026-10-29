# D07 Test-Binding Correction — Explicit Intent Boundary

## Defect
The frozen requirements conflate independent command submissions with one explicitly grouped intent:
- `TwoIndependentCommandsNeedTwoUndoUnits`: two independent commands require two Undo actions.
- `NaiveSequentialFallbackCannotSatisfyOneUndo_RED`: currently bound to the same standalone call style but expects one Undo to reverse both.

These are incompatible without an explicit intent/group boundary. This is a P31 verification-binding defect, not permission to change independent command semantics.

## Correct binding

### Independent commands
Keep `TwoIndependentCommandsNeedTwoUndoUnits` unchanged in meaning:
- two independent command submissions, no shared IntentPlan/UndoIntent/HistoryGroup;
- first Undo reverses only the second command;
- second Undo reverses the first;
- redo cursor behavior remains correct.

### Naive sequential fallback
Rebind `NaiveSequentialFallbackCannotSatisfyOneUndo_RED` as a negative-control/characterization test:
- execute the same standalone sequential pattern;
- prove naive sequential calls cannot satisfy grouped-intent one-Undo semantics;
- do not require independent commands to become grouped;
- preserve historical results and test identity mapping; record the corrected expectation in a new versioned binding.

### Explicit grouped intent
Add a distinct positive oracle, e.g. `ExplicitIntentGroupProducesOneUndoUnit`:
- use the real explicit IntentPlan/UndoIntent/HistoryGroup entry point;
- submit multiple typed Operations under one explicit intent/group boundary;
- assert one coherent local success and exactly one user-visible Undo unit;
- one Undo reverses the whole intent; Redo restores forward meaning with fresh operation IDs;
- do not infer grouping from two standalone calls or temporal adjacency.

If the real explicit-intent entry point is missing, keep D07 blocked and report the exact missing owner. Do not add a UI-only flag or adjacency heuristic.

## Preserve OOM and failure containment
Keep all OOM/failure-injection assertions blocking. Reserve required HistoryGroup/UndoIntent, inverse/preimage, result and publication memory before mutation wherever feasible. Failure at any forward-operation boundary must not expose partial local Scene/observer publication or strand canonical state without History.

## Correct acceptance mapping
- Independent commands: two Undo units.
- Explicit grouped intent: one Undo unit.
- Naive fallback: negative control showing it cannot meet grouped semantics.
- OOM/failure containment: independent blocking obligation.
- Cross-client/crash atomicity out of scope.
- No global canonical Transaction or new wire semantics.
- D06-B remains separately OPEN.
