# FC04-D07 Functional P31 v0.3 — Execution Closure

Status: P31 materialization for control review. Product-source edits are not authorized until the four package preflights pass in the declared worktree.

## Authority and purpose

This package narrows the next implementation attempt to D07 functional atomicity on the existing Axiom owners. It resumes the partial Scene/Intent work at cursor `b5072f2c7dd796b2ad7aed3a6bfa5d86b6058cad`; it does not restart from the original feature anchor and must preserve valid work.

The user has explicitly prioritized completing D07 functional behavior before deep Protobuf/Abseil remediation. This package does not waive or pass the known descriptor-initialization OOM hang. It separates:
- **Functional scope result:** `FUNCTIONAL_SCOPE_COMPLETE_READY_FOR_CONTROL_REVIEW`, only if every frozen functional obligation passes on the exact result revision.
- **Overall D07/release:** remains `BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG` until the real fault-sweep hang is resolved or a safe containment is independently reviewed and accepted.

## Required implementation

1. Finish the production explicit Intent path for every operation kind eligible under the accepted D07 contract. The existing SetTransforms + PatchProperties example is a minimum, not the full supported-kind matrix.
2. Complete Scene/RuntimeScene prepare, commit and abort for real participants, including record store, render scene, spatial index, damage/invalidation and runtime projection.
3. Before canonical mutation, validate all forward operations and target preconditions against a coherent intent basis; prepare inverses/preimages/expectations; reserve HistoryGroup/UndoIntent and all result/publication resources.
4. Commit the whole intent as one coherent local result and exactly one user-visible Undo unit. Preserve real per-operation identities, generations/stamps, ChangeSets and ordered eligible DataBridge records.
5. Undo must target the exact original intent even after current selection changes. Redo preserves forward meaning with fresh operation identities.
6. Abort/rejection/failure before commit leaves Document, Scene/index publication and generation, History cursor/size, Redo branch and observer-visible state unchanged.
7. Preserve independent command semantics: two independent submissions remain two Undo units. Do not introduce a canonical global Transaction, wire operation list, new OperationKind/ObjectKind/FieldId, public ABI or schema change.
8. Preserve the existing single-command apply/synchronize/recover behavior and the previously fixed single-command receipt-growth OOM containment.

## Forbidden

- No Protobuf/Abseil source edits, upstream patch edits, SDK producer recipe changes, dependency-lock changes, installed SDK replacement or release promotion.
- Do not edit `deps.lock.json`, `semantic-sdk.lock.json`, or `r1-full-skia-sdk.lock.json`.
- Do not move, disable, prewarm around, weaken or relabel the real descriptor OOM fault sweep. Do not repeatedly run the known-hanging sweep in the same process.
- No public ABI, schema, canonical Transaction, new registry, D06-B, Group/Ungroup, Snap/Alignment or multi-select property UI expansion.
- No main merge, overall D07 PASS, Gate PASS or release-ready claim.

## Allowed source surface

Existing source owners and focused verification paths are enumerated in `implementation-context.json`. New private C++ helper files may be added only under `runtime/interaction/src/` and must remain internal. Any source path outside the explicit allowlist, or a need for a new cross-module owner, is a terminal `TASK_PACKAGE_DEFECT` / `AUTHORITY_CONFLICT`; stop before editing.

## Verification and evidence

Required exact IDs and oracles are in `test-matrix.json`; execution and raw evidence rules are in `evidence-contract.md`. All 14 frozen original D07 IDs and all 160 Selection regression IDs remain required, plus the new real-owner Intent and Scene tests. No skipped or zero-match tests pass.

The known Protobuf/Abseil descriptor-initialization OOM sweep is a separate overall D07 blocker, not a functional-scope pass. Preserve its exact report, fault stack and hang stack. Do not claim overall D07 closure while it remains unresolved.

## Terminal success

Return `FUNCTIONAL_SCOPE_COMPLETE_READY_FOR_CONTROL_REVIEW` only when:
- every required functional test in the frozen matrix passes at the exact revision;
- the full eligible Operation-kind matrix and SCENE-PREP-01..06 have real-owner evidence;
- all 14 original D07 IDs and all 160 Selection IDs pass without skips;
- required Windows/native and actual Chrome/WASM qualification is recorded;
- ABI/scope checks pass; no forbidden change exists;
- exact result revision and reviewer-accessible evidence are materialized.

Overall D07/release remains blocked by the known OOM hang. If safe precommit atomicity cannot be achieved in current Axiom owners, stop and return a terminal blocker; do not expand into upstream SDK changes.
