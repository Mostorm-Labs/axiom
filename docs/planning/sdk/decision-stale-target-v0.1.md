# SDK-01 concurrent property edits — user decision v0.1

On 2026-10-29 the user chose **reject the stale request, refresh and retry** when a property edit was queued and another property on the same object changed before the queued edit executes. The user was explicitly offered field-level automatic merging as the alternative. This records the user decision; it does not supersede accepted semantic Authority, accept unrelated SDK design proposals, or by itself authorize SDK-01 P32.

For the reviewed existing selected-appearance lane:

1. A semantic-generation mismatch causes the queued request to finish with `kStaleTarget`; it must not change canonical state or overwrite the concurrently updated property.
2. A caller may read a fresh snapshot, prepare the desired field update against it, and explicitly submit a **new request identity**. The refreshed request must preserve other fields and return its real terminal receipt. No automatic, hidden retry or stale-success receipt is implied.
3. The independent prior `FC00XRuntimeFacade` expectation of automatic application in this scenario is superseded as a test expectation by the user's explicit choice. Its original failure, source and diagnostic evidence remain at `.aegis/results/AXIOM-SDK-01-REVIEW-002/`. The updated regression must check stale receipt, no stale mutation, explicit fresh retry, unchanged concurrent stroke width and applied opacity. Other regressions and requirements remain in force.

The currently existing `CanvasControlService` guard applies a conservative global semantic-generation fence. This decision does not authorize weakening that fence, adding a new field-conflict protocol or claiming the full SDK/public multi-selection interface is implemented. The native and Web public SDK consumers must eventually observe an equivalent truthful stale result and perform any retry at the caller's explicit request.
