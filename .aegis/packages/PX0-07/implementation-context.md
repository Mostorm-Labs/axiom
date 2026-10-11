# PX0-07 — Closure / Evidence

PX0-07 is the final work package of the PX0 Debug Control Plane track.

Repository identities:

- repository: `github:Mostorm-Labs/axiom`
- accepted PX0-06 production repair: `6c734f76fec203e7802fbfa0be660d7148e73ab7`
- accepted PX0-06 evidence head: `06964a48400d748bf22a8fef3818bd7c428fe388`
- PX0-06 P34 closure / PX0-07 task anchor: `7e3772d747cf3f7e158fb0917e5169a56065a08b`

## Current closure reality

Most Task 15 architecture boundaries are already partially or fully represented in existing tests:

- `runtime_contract_test.cpp` already asserts RuntimeFacade is not derived from RuntimeDiagnostics.
- `panel_registry_test.cpp` already constructs the exact DebugPanelContext boundary and proves six workspace/slot validation.
- `snapshot_assembler_test.cpp` already proves DebugSnapshot has no `capabilities` member, provider degradation, and explicit MixedGeneration coherence.
- `workbench_state_test.cpp` already exercises context-disabled Fit Selection/Fit Primary behavior.
- typed Axiom debug queues exist under `canvas::runtime`; the forbidden item is the old generic `canvas::debug_ui::DebugCommand` lane, not the typed owner queue.
- DebugActivityLog is bounded in production but needs an explicit final closure regression if one is not already present.

Do not rewrite already-sufficient tests merely to make the diff larger. Task 15 is to close residual proof gaps.

## Synthetic extension proof

Add test-only contributions using existing `PanelRegistry` APIs:

```text
Control / Feature
Inspect / Object   (or Inspect / Interaction)
```

The synthetic render functions may emit fixed diagnostic text or flip a test-local flag. They must not call RuntimeFacade, DebugControlRouter product/debug mutation, Document/Scene internals, or introduce Shape types. The proof is about extension mechanics only.

## Qualification identity model

PX0-07 intentionally uses two identities:

```text
Task 15 test/CMake commit
        |
        +--> qualification_subject_revision
              |
              +-- Windows build + physical observation
              +-- Web build + physical observation
              |
              +--> evidence-only/docs commit(s)
                    |
                    +--> evidence_head
```

The README records `qualification_subject_revision`. Do not try to make a README commit contain its own commit SHA as the subject; that creates an endless revision-chasing loop.

## Physical observation status vocabulary

Each checklist row uses one of:

- `PASS` — expected available behavior was physically exercised and behaved correctly.
- `DISABLED_BY_CONTEXT` — control is intentionally disabled in the current product context; record how the context was established.
- `UNSUPPORTED` — current owner/build intentionally does not implement the capability.
- `FAIL` — expected available behavior did not work.
- `NOT_APPLICABLE` — only when the checklist item truly does not apply to that platform, with explanation.

Do not use `PASS` as a synonym for `UNSUPPORTED`.

For Fit Selection / Fit Primary Object, first record the disabled no-selection state, then use the real Canvas interaction path to establish a valid selection if the current host supports it. Do not create a direct debug/test mutation path solely for qualification.

For Web Surface engineering controls, current architecture may expose them as Unsupported because Web has no PlatformDebugControl owner. That is correct if the UI accurately reports Unsupported.

## README minimum content

`docs/quality/evidence/px0-debug-control-plane/README.md` must include:

1. package ID and exact `qualification_subject_revision`;
2. task anchor and evidence head/reference;
3. Windows configure/build command and environment identity;
4. Web configure/build/serve command and browser/WebGL identity;
5. Windows checklist table;
6. Web checklist table;
7. explicit Available / DisabledByContext / Unsupported distinctions;
8. paths to automated logs/evidence;
9. known non-blocking findings;
10. a sentence that official PX0/P34/physical PASS is not issued by this evidence document.

Screenshots are optional corroboration.

## Execution behavior when GUI/browser physical access is unavailable

Do not stop early before completing executable automated closure work.

Complete Task 15 tests, full native suite, structural contracts and both hosted builds. Commit and push the exact `qualification_subject_revision`. Prepare the README structure without fabricating physical observations.

Then return:

`BLOCKED_MISSING_INPUT`

with:

- exact qualification subject revision;
- Windows executable path;
- Web hosted output path;
- exact commands to launch/serve;
- the untouched physical checklist requiring observation.

The physical observations can then be supplied in a later P33 continuation without replaying Task 15.

## First incomplete action

Run repository/package/binding preflight and ImplementationDesignPreflight. Inventory which Task 15 boundaries are already covered, add only missing closure assertions, observe the required new RED where applicable, and proceed through automated closure. Continue into physical qualification only if the execution surface has real GUI/browser observation capability; otherwise follow the explicit BLOCKED_MISSING_INPUT rule above.
