# FC-04 Phase 2 Multi-selection Editing — Execution Closure Contract

Status: P31 package candidate for independent control review. Product-source edits are not authorized until this exact package passes its preflights and is accepted for P32.

## Goal

Complete Phase 2 Multi-selection Editing on the existing Axiom Runtime/Debug UI path. The D06-B eligible-subset policy is now Current Authority. Reuse the D07 explicit-intent path for heterogeneous commands requiring multiple Operations; do not redo D07 or introduce a canonical global Transaction.

## Required changes

1. Produce a real property-by-kind source matrix for the exact cursor. For each property record read owner, semantic applicability owner, capability projection, command/control request, validation, History/Undo mapping, per-target outcome and test.
2. Implement a coherent read-only property projection for normalized selected operation roots. Bind it to selection revision, semantic/document generation and runtime snapshot identity. No UI-owned selection or property truth.
3. Implement accepted D06-B: show eligible/total and typed exclusion reasons before execution; compute Uniform/Mixed over eligible roots and Partial independently; unknown provider/query results are Indeterminate/Unavailable and disable mutation.
4. Implement batch edits over the explicit eligible cohort using existing accepted typed Operation semantics. The eligible cohort is all-or-nothing; applicable-but-locked/stale/denied/invalid/conflicting targets reject the entire eligible cohort before mutation. Return exact changed/skipped target outcomes and reasons.
5. Implement multi-target move/transform and common property editing through the existing owner path. Normalize selected roots; never double-apply an ancestor and descendant. Preserve relative geometry and Group scope semantics.
6. A user intent requiring multiple canonical Operations must go through the accepted D07 explicit Intent path and produce one Undo unit. If a command cannot use that path, keep it unavailable; never sequentially commit operations and pretend they are atomic.
7. Undo targets the exact committed roots/preimages even after selection changes; Redo preserves forward meaning with fresh Operation IDs. Independent commands remain separate History units.
8. Bind FC-02 TextEdit semantics at the actual target/session owner: double-click/Edit Text resolves the actual hit text object; TextEdit replaces the multi-root edit cohort with one owning root and focused text target; commit/cancel leaves that root selected; Escape cancels active composition first and does not also clear selection/exit scope on the same key event.
9. Keep selection/Inspector refresh read-only. Revalidate selection revision, document/semantic generation, target state, capability/applicability, permission/lock and payload at command admission.
10. Add real-owner MSE-01..MSE-27 tests, each independently discoverable or reported as a distinct case with an exact result. Preserve original D07 14 IDs, Selection 160, D07 expanded 30, fault 46, Scene 2 regression suites at this cursor. Do not rerun the known hanging descriptor OOM sweep in the same process; it remains a separate overall D07/release blocker.
11. Complete Windows/native and actual Chrome/WASM automated qualification where supported. Browser automation is not physical touch/stylus/IME acceptance; full integrated real-device validation remains scheduled after Phases 1–3.
12. Preserve ABI manifest, wire schema, FieldId, OperationKind/ObjectKind registry, public RuntimeFacade ABI and dependency locks.

## Non-goals / forbidden changes

- No new FieldId, PropertyValue, OperationKind, ObjectKind, canonical Transaction, wire semantics, global registry or public ABI.
- No Protobuf/Abseil, SDK, producer recipe, dependency lock, installed SDK or upstream patch edits.
- No Group/Ungroup, Wrap/Unwrap, Section lifecycle, Snap/Alignment implementation in this Phase 2 package. Those are Phase 3, and current accepted Group/Section implementation is a protected regression surface only.
- No D06-B reinterpretation or UI-local applicability switch.
- No bypass of D07 explicit Intent, no per-object sequential commits for one atomic command.
- No silent partial success; no guessing Unsupported for unknown/missing provider.
- No overall D07 PASS, merge/release readiness or physical qualification claim.

If a required read/submit path cannot be implemented without changing public ABI, new canonical semantics, or an owner outside the allowed scope, stop with AUTHORITY_CONFLICT; do not extend the package ad hoc.

## Required evidence

- Exact source/property matrix.
- Exact preflight outputs and source/input lock checks.
- MSE-01..MSE-27 results, no missing IDs, no skips or zero-match-as-pass.
- Original D07 14/14, Selection 160/160, expanded D07 30/30, fault 46/46 and Scene 2/2 current-revision regressions.
- Per-target eligibility, skipped reasons, changed outcomes, pre/post Document and Scene generations, History cursor/size and Redo branch.
- Failure injection at capability query, payload validation, Operation preparation, History reservation and Scene publication precommit boundaries; no failpoint moved past canonical mutation.
- Undo after selection change; Redo with fresh IDs; one user intent = one Undo unit; two independent commands = two Undo units.
- Actual-hit TextEdit and Escape/IME precedence tests.
- Windows native build/test and actual Chrome/WASM automated interaction where supported.
- Runtime C ABI manifest unchanged; no wire/schema/registry/lock changes; exact source diff and raw logs.
- Reviewer-accessible result report, manifest hashes, JUnit/test ID lists, commands and exit codes.

## Terminal success

Return PHASE2_FUNCTIONAL_SCOPE_COMPLETE_READY_FOR_CONTROL_REVIEW only if all frozen requirements pass on the exact result revision and evidence is durably materialized. This does not close overall D07/release because the known descriptor OOM hang remains.

## Terminal blockers

AUTHORITY_CONFLICT, MISSING_REQUIRED_INPUT, TASK_PACKAGE_DEFECT, ENVIRONMENT_BLOCKER, FROZEN_VERIFICATION_FAILURE, NEW_HIGH_IMPACT_FAILURE_MODE, EXECUTION_DIVERGENCE, BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG (overall D07/release only).


Overall D07 terminal status remains BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG. This Phase 2 package does not close or waive that risk.
