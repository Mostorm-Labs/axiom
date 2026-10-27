# FC-04 D07 Source Audit & Test Execution Handoff v0.1

Status: SOURCE STRUCTURE REVIEWED / EXECUTION EVIDENCE STILL REQUIRED. This handoff is scoped to proving or disproving local multi-operation intent atomicity and one user-visible Undo. It does not authorize production source changes.

## Exact subject

- Repository: Mostorm-Labs/axiom
- Branch: main
- HEAD: 2fee75f66a7f96f6aac834228ede372a9abda59a
- Relevant source:
    - runtime/interaction/include/canvas/interaction/editor_history.hpp
    - runtime/interaction/src/editor_history.cpp
    - runtime/interaction/src/undoable_command.cpp
    - runtime/interaction/tests/editor_history_test.cpp
    - runtime/semantic/include/canvas/semantic/operation_engine.hpp
    - runtime/semantic/src/operation_engine.cpp
- Relevant contracts:
    - 07-02 Pointer / Intent → Canonical Operation Runtime Data Flow v0.1
    - 07-14 Interaction Conflict / Multi-Operation Intent Integration Contract v0.1
    - FC-00X-C Capability Resolution Contract v0.1 — Capability Semantics Foundation
    - FC-04 Multi-selection Editing — Source Matrix & Closure Findings v0.2

## Current source facts

- EditorHistory::PreparedEntry contains one forward Operation, a vector of inverse Operations, and before/after expectations.
- HistorySubmitPort has a submit API accepting a span of Operations, plus conditional and admitted submission hooks.
- EditorHistory::executeUndoable submits an admitted forward Operation and commits the prepared history entry after accepted submission.
- Undo/Redo use the stored inverse/forward operations and expectation checks.
- Existing editor_history_test.cpp has focused tests for compensation identity, rejected Undo retryability, and typed inverse/redo paths.
- These facts show useful primitives; they do not prove that a multi-operation user intent is coordinated end-to-end as one atomic visible result and one History unit.

## Required audit questions

1. Where is the actual user-intent coordinator / IntentPlan / UndoIntent / HistoryGroup implementation in the current checkout? Record exact source paths and callers.
2. Does it preflight all Operations against one coherent current-state basis before any canonical apply?
3. Is History capacity and all inverse/preimage data reserved before the first canonical mutation?
4. What are the observable semantics if operation N fails after operations 1..N-1 were applied?
5. Does compensation restore Document, Scene/index publication, semantic generation, ChangeSet/event observers, History cursor and redo branch to the accepted local contract?
6. Does a successful multi-operation intent produce exactly one user-visible Undo unit? Verify Undo then Redo after the selection has changed.
7. Does any test only fake the submit port or assert the coordinator call count without verifying real Document/History state? Mark those as unit evidence, not end-to-end proof.
8. Is cross-client/crash atomic visibility required? Current 07-14 scope does not claim it; do not expand the requirement without a separate authority decision.

## Required test matrix

- D07-01: all forward operations preflight successfully, commit succeeds, one HistoryGroup/Undo unit is visible.
- D07-02: preflight rejection before any apply leaves Document, Scene publication, generation, History cursor and redo branch unchanged.
- D07-03: inject failure at every forward-operation boundary; assert zero mutation or accepted fully contained compensation, no successful Undo unit, and exact observer-visible outcome.
- D07-04: fail History reservation/inverse preparation before first apply; assert zero mutation.
- D07-05: fail publication/receipt preparation at each permitted boundary; assert behavior matches the accepted 07-14 contract.
- D07-06: successful Undo after changing current selection restores the original committed targets, not the current selection.
- D07-07: Redo replays the accepted forward meaning with fresh operation identities and cursor behavior.
- D07-08: redo branch and cursor are unchanged after rejected/failed intent.
- D07-09: verify semantic generations/ChangeSets and Scene/index publication around compensated failure; document whether intermediate states can be observed.
- D07-10: single-operation multi-target SetTransformsOp and PatchPropertiesOp are tested separately from multi-operation intent grouping.

For every test, record exact command, exit code, target, raw output, and whether it is unit, integration, or end-to-end evidence. Run the existing focused tests plus the new tests on the actual checkout. Do not mark a test passed from source inspection alone.

## Decision rule

- If all required local guarantees are demonstrated by actual tests and match accepted 07-14 scope, close D07 for Phase 2 without inventing a global Transaction.
- If a required guarantee is missing, prepare the smallest D07/History authority amendment and keep the affected multi-operation command unavailable.
- Do not claim cross-client/crash atomicity unless separately authorized.
- Do not modify product implementation as part of this audit-only task. If a test must be added to prove a gap, add only the test harness/evidence in a separately bounded verification change, not the production fix.

## Terminal report

Return exact HEAD, coordinator paths, test commands/results, failure-injection coverage, visible generation/event behavior, and a clear PASS/BLOCKED conclusion for D07. No Gate PASS and no Phase 2 implementation authorization is implied.