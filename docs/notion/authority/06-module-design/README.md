# 06 Module Detailed Design — Authority Index

> Source root: Notion `06 Module Detailed Design`
>
> Snapshot date: 2026-08-26

## Seven subsystem authority set

1. Axiom Semantic Core
2. Axiom Interaction Runtime
3. Axiom Scene Core
4. Axiom Render Core
5. Arc Runtime
6. Shared Data Runtime
7. Platform Host

The seven-subsystem ownership/dependency graph remains current. Do not introduce an eighth top-level subsystem to solve a local implementation problem unless a new architecture blocker is established through the governance workflow.

## Current cross-module implementation entry

Use `module-design-closure-v0.1.md` for cross-module ownership/integration closure, then enter:

`implementation-contract-freeze/README.md`

for the current First-Batch implementation contract.

The exact current First-Batch set is:

```text
ICF-00 v0.3
ICF-01 v0.1
ICF-02 v0.2
ICF-03 v0.2
ICF-04 v0.2
ICF-05 v0.1
```

The set remains **Proposed Freeze Candidate**, not Frozen v1. Current gate is `Source Contract Closure Ready -> Repo Rematerialization / Reverification Pending`.

## Derived and evidence boundary

- Implementation Contract Skeleton v0.2 is the current derived repo blueprint, not verification evidence.
- Contract Linter / Golden Matrix v0.2 is the current verification specification, not current PASS evidence.
- historical Skeleton/Compile reviews are comparison/provenance only.

## Codex rule

Start at `docs/notion/manifest.yaml`. Consume only the exact current source versions it lists. Do not read Superseded ICF pages to complete type/enum/order/projection semantics. If current authority is insufficient, report the gap rather than deriving an answer from history.
