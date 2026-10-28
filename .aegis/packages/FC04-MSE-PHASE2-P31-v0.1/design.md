# Phase 2 Multi-selection Editing — Design Boundary

## Authority and semantic limits

Current D06-B acceptance: https://app.notion.com/p/4074c57a590c81d9b124cd243e44f205?pvs=204
D07 local atomicity authority: https://app.notion.com/p/4064c57a590c819aabc5e9efbf9a2b53?pvs=204
D07 functional result: https://github.com/Mostorm-Labs/axiom/blob/6bcf2d4bbf3314e5252951190c3c475403f52665/.aegis/results/FC04-D07-FUNCTIONAL-P32-001/report.md

The command operates on normalized selected operation roots, not raw hit candidates. Group ancestor plus selected descendant must not create duplicate target application. Current Group scope rules remain authoritative. Selection, primary, focus, hover and text composition remain transient state.

## D06-B accepted batch property semantics

For normalized selected roots S, resolve exact property applicability by accepted Runtime/feature owner. Let E be the semantically eligible roots and U the inapplicable roots. The UI must show |E|/|S| and typed reasons before commit. Uniform/Mixed values are calculated over E; Partial applicability is a separate dimension.

A root in U may be skipped only because the property is semantically inapplicable. A root in E that is locked, permission-denied, stale, missing, invalid, concurrently changed or conflicted causes the entire eligible batch to reject before mutation, unless a prior accepted command contract defines a safe recovery. Unknown applicability is Indeterminate/Unavailable and never silently shrinks E.

## Command and History path

Use existing typed Operation payloads and CanvasEditingRuntime/RuntimeFacade owner path. One single-property homogeneous edit should use one accepted PatchProperties Operation over the explicit eligible cohort. A heterogeneous user intent that needs multiple canonical Operations must go through the D07 explicit Intent path and create exactly one Undo unit. No canonical Transaction is introduced.

Do not submit multiple independent operations sequentially and then compensate as a substitute for atomicity. Preflight all targets and payloads, capture exact preimages, reserve History and publication resources, then commit through the existing accepted owner. If existing owner APIs cannot represent the request without public ABI/schema changes, keep the action unavailable and return AUTHORITY_CONFLICT.

## Property surface to close

The source matrix must cover these existing V1 properties and mark each SUPPORTED, UNAVAILABLE_WITH_REASON, or BLOCKED_AUTHORITY on the exact cursor:
- Visible (0x1, Bool)
- Locked (0x2, Bool)
- Opacity (0x3, F32_0_1)
- BlendMode (0x4, BlendModeValue)
- FillStyle (0x100, FillStyleValue)
- StrokeStyle (0x101, StrokeStyleValue)
- Connector start/end decoration (0x200/0x201, ConnectorDecorationValue)

No property is declared supported merely because the registry/validator allows it. Each needs read, applicability, capability, write, History and test evidence. Missing providers are Indeterminate/Unavailable.

## FC-02 TextEdit binding

Text editing is a focused single-target session, distinct from multi-root property commands:
- double-click/Edit Text resolves the actual hit text object through accepted TargetRef/session owner;
- entering TextEdit replaces the multi-root edit cohort with one owning root and focused text target;
- commit/cancel leaves the owning object selected;
- Escape cancels composition/session first and cannot clear selection or exit scope in the same key event;
- transient composition/caret/focus never becomes canonical state until accepted text commit.

## UI responsibility

The Inspector/Debug UI consumes one coherent read-only projection. It does not infer applicability from a UI-local kind switch, read primary and pretend it represents all roots, mutate canonical records, or manufacture per-target results. Command admission revalidates all relevant stamps.

## Phase boundary

Phase 2 ends with selection/multi-selection behavior and its MSE tests. Group/Ungroup is Phase 3 and must not be implemented in this package. Existing Group/Section behavior is regression-only.
