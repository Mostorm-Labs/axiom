# 06 Module Detailed Design — Authority Index

> Source root: Notion `06 Module Detailed Design`
> Source page: https://app.notion.com/p/3c44c57a590c807093b9ce571d163f09
> Snapshot date: 2026-08-24

## Seven subsystem authority set

1. Axiom Semantic Core
2. Axiom Interaction Runtime
3. Axiom Scene Core
4. Axiom Render Core
5. Arc Runtime
6. Shared Data Runtime
7. Platform Host

The source root also contains Render Optimization 01–08 and the final `06 Module Detailed Design Closure / Cross-Module Integration Review v0.1`.

Use `module-design-closure-v0.1.md` as the repository-level cross-module implementation contract. More detailed subsystem behavior remains traceable to the named Notion source pages until all leaf pages are mirrored verbatim.

## Codex rule

Do not introduce an eighth top-level subsystem merely to solve a local implementation problem. The 06 closure found the seven-subsystem ownership/dependency graph coherent; unresolved items are glue/API/physical-layout follow-ups unless a new architecture blocker is demonstrated.
