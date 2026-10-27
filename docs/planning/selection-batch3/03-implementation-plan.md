# Batch 3 — Implementation Plan

## Suggested branch
`codex/selection-summary-batch3`

Create it from the actual trusted baseline after reconciling Batch 1/2 status. Do not use a stale SHA just because it appeared in earlier conversation.

## Slice 0 — Reconcile prior work
- Inspect actual HEAD and working tree.
- Inspect Batch 1/2 branches, PRs, tests and accepted handoffs.
- Determine current selection owner, summary consumer and public facade boundary.
- Write a short source-to-contract map before editing.
Exit: no duplicated implementation and no unowned semantic decision.

## Slice 1 — Typed summary model
- Extend existing `SelectionSummary` or its authoritative equivalent; do not create a parallel summary owner.
- Add per-kind root counts and clear count semantics.
- Include revision/generation freshness only when backed by real owner state.
- Respect existing public API boundaries.
- Test empty, one-root, multi-root, duplicate/invalid and mixed-kind cases.
Exit: deterministic summary with no selection/document mutation.

## Slice 2 — Bounds and hierarchy summary
- Resolve selected operation roots through existing SelectionSession/container rules.
- Derive aggregate bounds from Scene/BoundsSystem for one consistent scene generation.
- Test nested Group/Section normalization, parent+descendant duplicate protection, deleted/stale IDs, transform changes and unavailable bounds.
- Do not count descendants as roots.
Exit: root count, per-kind counts and bounds agree with normalized roots.

## Slice 3 — Capability projection
- Extend existing capability resolution/command owner if available.
- Cover only actions with real command paths and known preconditions.
- Group/Ungroup, Wrap in Section, bulk property edit, Snap/Alignment remain disabled or `not_implemented` until real command paths exist.
- UI must not enable actions based on selection count alone.
- Add no mutations or operation semantics.
Exit: every action state maps to a real command capability or grounded unavailable reason.

## Slice 4 — Property aggregation
- Reuse existing read APIs for properties already supported.
- Implement uniform/mixed/partial/unavailable states.
- Define applicability through existing object/property capability rules, not UI heuristics.
- If no authoritative applicability source exists, omit that property from aggregation rather than inventing rules.
Exit: mixed and partial values cannot be misreported as uniform.

## Slice 5 — Debug UI/action-bar consumer
- Read summary through existing RuntimeFacade/composition owner.
- Render root count, per-kind counts and primary identity.
- Display aggregate bounds only when valid.
- Render capability-driven actions with accurate disabled state/reason.
- Render property aggregate states without performing mutation.
- Refresh through existing events/revisions; do not make UI state authoritative.
Exit: UI reflects Runtime state and refresh creates no Operation/History entry.

## Required tests
1. Empty selection: count 0, no primary, no uniform properties.
2. Single root: count 1, primary belongs to roots, correct kind count.
3. Mixed roots: per-kind counts sum to root count.
4. Parent and descendant: no duplicate root/count/bounds contribution.
5. Section plus child: obey accepted Section scope semantics.
6. Deleted selected object: summary updates without stale IDs.
7. Scene generation mismatch: bounds/property aggregate unavailable or consistently recomputed.
8. Uniform property: same value across applicable roots.
9. Mixed property: differing values produce `mixed`.
10. Partial property: unsupported roots do not inherit another object's value.
11. Capability: unavailable future command remains disabled.
12. UI refresh: selection changes update summary; refresh creates no Operation/History entry.
13. Multi-selection safety: no action silently applies only to primary.
14. Batch 1/2, Section, image crop, transform and connector regressions remain intact.

## Verification
Run focused unit tests, affected CTest targets, relevant host build, UI/build tests and `git diff --check` as applicable. Record exact commands and exit codes. Mark hosted/physical verification pending if unavailable.

## Terminal result
Return exact branch/HEAD, changed files, tests/results, evidence refs, unsupported actions and next step. Stop only on a precise missing Authority/owner/verification blocker. Do not expand into Batch 4.
