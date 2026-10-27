# Codex Handoff — Selection Batch 3: Unified Summary & UI Consumption

## User-authorized intent
Implement the third selection batch: provide a unified, read-only selection summary and connect Debug UI/action-bar consumers. This prepares for future multi-selection editing, Group, Wrap in Section and Alignment but does not implement those mutations.

## Repository and starting state
Repository: `Mostorm-Labs/axiom`

Before editing:
1. Verify repository identity, current branch, HEAD and working tree.
2. Find and inspect the actual Batch 1 and Batch 2 implementation branches/PRs/results.
3. Reconcile which work is merged and which remains in progress.
4. Use the current trusted integrated baseline. Do not blindly use a stale SHA or reimplement completed work.
5. If the actual owner or accepted contract needed for a summary field is missing, report the narrow blocker rather than inventing a parallel semantic model.

## Source landmarks
- `runtime/interaction/include/canvas/interaction/selection_state.hpp`
- `runtime/interaction/include/canvas/interaction/selection_session.hpp`
- `runtime/interaction/src/selection_session.cpp`
- `runtime/scene/include/canvas/scene/scene_query.hpp`
- `runtime/scene/include/canvas/scene/scene_types.hpp`
- `runtime/scene/src/scene.cpp`
- `runtime/interaction/src/canvas_editing_runtime.cpp`
- `runtime/composition/src/canvas_runtime_section.cpp`
- `runtime/render/include/canvas/render/editing_overlay.hpp`

These are discovery points, not a hardcoded allowlist. Confirm actual owners and include only necessary paths in the execution record.

## Objective
Expose a deterministic summary based on the existing Runtime selection owner and current Scene generation, and make Debug UI/action-bar read it without duplicating semantics.

Required output:
- selected operation-root count;
- per-kind root counts;
- primary root validated as a member of the root set;
- aggregate bounds when computed consistently from Scene/BoundsSystem;
- typed capability state/reason for supported actions;
- uniform/mixed/partial/unavailable property aggregates for properties supported by existing read APIs;
- freshness/revision information only where backed by real owner state.

Keep root count separate from descendants. Never count a selected container's descendants as additional selected roots.

## Implementation sequence
1. Reconcile Batch 1/2 and record the exact source-to-contract map.
2. Extend existing `SelectionSummary` or accepted equivalent; do not create a second selection owner.
3. Normalize selected roots using existing Group/Section scope and resolution rules.
4. Derive bounds from Scene/BoundsSystem with generation consistency.
5. Project only real command capabilities. Unimplemented future actions remain disabled/not implemented.
6. Aggregate only properties with an authoritative applicability/read contract.
7. Connect Debug UI/action-bar as read-only consumers through existing facade/composition ownership.
8. Add focused tests and run affected builds/regressions.
9. Continue through all executable items in this package until success or a precise terminal blocker.

## Hard constraints
- No new canonical object/schema/operation.
- No bulk property mutation.
- No Group/Ungroup implementation.
- No Wrap-in-Section mutation.
- No Snap/Alignment implementation.
- No new global registry or second selection state.
- No UI-side authoritative selected-ID list.
- No silent fallback to primary-only mutation under multi-selection.
- No selection persistence.
- No test/oracle weakening.
- Do not claim full object coverage or platform acceptance without corresponding evidence.
- Do not merge to `main` automatically.

## Required regression properties
- selection/summary/UI refresh changes no Document content and creates no History entry;
- primary belongs to normalized roots;
- root counts and per-kind counts are consistent;
- parent+descendant do not double count;
- deleted/stale roots cannot leak into summary;
- stale Scene generation does not produce falsely authoritative bounds;
- mixed/partial properties are represented honestly;
- unimplemented actions remain disabled;
- existing selection, Section, image crop, transform and connector workflows remain intact.

## Terminal report
Return actual base/final HEAD, branch and changed files, Batch 1/2 reconciliation result, tests/builds run with exact commands and exit codes, evidence refs and unrun hosted/physical tests, remaining limitations and next smallest task.

Do not stop after writing another plan if code can safely proceed. Do not broaden scope into Group, Snap/Alignment or bulk editing.
