# Codex Execution Prompt — Selection Batch 2

Please execute Selection Batch 2 using the adjacent files:
- `01-scope-and-behavior.md`
- `02-source-audit.md`
- `03-implementation-plan.md`

Repository: `Mostorm-Labs/axiom`
Preferred branch: `codex/selection-hierarchy-coverage-batch2`

## Start safely

1. Inspect the actual repository identity, current branch, exact HEAD and working tree. Preserve any uncommitted user work.
2. Locate the first-batch implementation branch/PR/commit supplied by the user or present in repository refs. Do not assume `main` contains the partial implementation.
3. Reconcile the first-batch scope against current code and report what is already done, what is missing, and what Batch 2 can safely build upon.
4. If repository identity is ambiguous, fail closed. If Batch 1 prerequisites are absent and necessary to implement this package, return `BLOCKED_UPSTREAM_BATCH1_INCOMPLETE` instead of silently reimplementing the whole first batch.
5. Run baseline focused tests before editing.

## Implement

Follow work slices A–D in `03-implementation-plan.md`. Prefer narrow changes in interaction/scene/runtime and their tests. Reuse the existing Scene query/hit-test and semantic identity rules. Do not introduce a second selection model.

Core requirements:
- Deterministic, non-redundant operation roots for Group/Section hierarchy.
- Section background/bounds must not swallow valid selection of enclosed/overlapping content.
- Precise region selection for currently delivered object types; AABB is broad phase only.
- Correct user-facing identity for Sticky/RichText and Group/container behavior.
- Respect image crop and connector route/arrowhead geometry.
- Preserve prior selection on gesture cancellation; do not globally change legacy clear-selection semantics without the first-batch contract authorizing it.
- Selection-only actions must not create canonical Operations, mutate Document, or add History entries.
- Preserve existing object dragging, handles, Section boundary interaction, Connector creation, Crop, and pan/zoom.

## Do not do

- Do not create new canonical ObjectKinds or modify semantic schema.
- Do not implement full multi-object transform/property editing.
- Do not add Snap/Alignment, Table, OCR, auto-layout, or broad platform rewrites.
- Do not weaken, skip, or relabel tests to manufacture PASS.
- Do not merge into `main` or claim Gate PASS.

## Required completion report

Return:
- exact repository, branch, base SHA, result SHA, and PR URL if created;
- first-batch reconciliation (done / missing / not applicable) and any blocker;
- exact changed paths and why each is in scope;
- test commands with exit codes and platform/environment;
- coverage table by object kind and geometry oracle;
- semantic generation / operation count / history invariants;
- known failures, unrun platform checks, and residual unsupported coverage;
- explicit statement that no Gate PASS or merge is claimed.

Continue through all executable slices until terminal success or a specific terminal blocker is reached.
