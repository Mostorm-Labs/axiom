# GT-G1-04-C / C8

## P31 Implementation Task Package

Status: READY_FOR_P32

## Authority

Current Authority:
- GT-G1-04-C P20 Verification Design Reconciliation v0.1
- GT-G1-04-C P20 TerminalPhase Classification Addendum v0.1
- GT-G1-04-C P30 Implementation Plan v0.1

## Task Anchor

```yaml
task_anchor:
  revision: 34c8db4f247849c5850e16226b0e556f57497053
  relation: ancestor
```

## Purpose

Close the remaining C7 lifecycle gap:

- exact-source CI execution binding
- durable reviewer-resolvable evidence materialization

This package does not change semantic validation, provider behavior, or GT-G1-05 scope.

## Accepted Dependencies

```yaml
p36_source_ref: 492d2f914f078a6e4ac8b567e07f7ec813c10107
p36_materialized_ref: 9b73be589ae070bc602b8989f83d89745a54774e
c7_r2_source_ref: 4abd5a472c84457cfecd763957e68a6dc06c18d3
c7_r2_materialized_ref: 34c8db4f247849c5850e16226b0e556f57497053
```

## Authorized Scope

### Included

1. Add exact-source CI contract.
2. Bind CI execution identity to the verified source SHA.
3. Capture executed commands, commit identity, workflow identity, and result metadata.
4. Define durable evidence materialization format.
5. Add fail-closed handling for source/evidence mismatch.

### Explicit Non-goals

Do not:

- modify AUTHORITY_MANUAL truth
- consume provider output as expected truth
- alter semantic validators
- modify ObjectStore/apply semantics
- implement Atomic Apply
- implement SemanticGeneration
- implement ChangeSet
- implement CanonicalCommitStamp
- implement post-apply publication

## CI Execution Contract

Required properties:

- clean checkout of exact source ref
- execution result bound to immutable source SHA
- no evidence claiming verification of a different revision
- stale or mismatched evidence fails closed

## Evidence Contract

Durable evidence must contain:

- source_ref
- materialization revision
- CI workflow/run identity
- executed command list
- pass/fail result
- artifact locations
- verification timestamp metadata

Local-only logs are insufficient.

## Tests / Oracles

Required verification:

- source SHA binding check
- evidence/source consistency check
- existing GT-G1-04-C semantic corpus execution
- fixture reproducibility preservation
- no mutation invariant preservation

## Exit Criteria

P31 package is complete when:

- implementation branch descends from 34c8db4f247849c5850e16226b0e556f57497053
- package is immutable and reviewable
- P32 execution surface is defined
- GT-G1-05 remains not started

## Downstream Guard

```yaml
p32: NOT_STARTED
gt_g1_05: NOT_STARTED
```
