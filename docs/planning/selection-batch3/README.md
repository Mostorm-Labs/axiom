# Axiom Selection Batch 3 — Selection Summary & UI Consumption

This package defines the third selection implementation batch. It is limited to the unified, read-only selection summary and its Debug UI/action-bar consumers.

## Contents
- `01-scope-and-behavior.md` — behavior contract and scope boundaries
- `02-source-audit.md` — inspected source facts and required reconciliation
- `03-implementation-plan.md` — implementation slices, tests and exit criteria
- `04-codex-prompt.md` — execution handoff

## Baseline caution
Before editing, Codex must resolve actual checkout HEAD and inspect implementation branches/results from batches 1 and 2. Do not assume either prior batch is merged or complete.

## Explicit non-goals
No bulk property mutation, full Group/Ungroup operations, Wrap in Section commands, Snap/Alignment or new object semantics. Never present an action as executable unless the real command path and capability exist.
