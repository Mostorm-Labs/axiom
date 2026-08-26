# Axiom Notion Architecture Bridge

This directory is the repository mirror of Axiom architecture and implementation authority maintained in Notion.

## Authority relationship

Notion is the living human-facing source of architecture and contract authority. `docs/notion/authority/` is a controlled, versioned, Codex-facing materialization of that authority. It is not an independently editable second source of truth.

The repository mirror exists so implementation agents do not need to rediscover current authority by searching historical Notion pages.

## Mandatory Codex start rule

Before architecture-sensitive implementation work:

1. Read `docs/notion/manifest.yaml` first.
2. Read only documents listed as current by that manifest for the scope being implemented.
3. Treat Superseded / Historical / Stale material as provenance or comparison oracle only.
4. Do not infer a missing current contract from an older document.
5. If current authority is incomplete or contradictory, stop the affected implementation path and report an authority gap.
6. Generated or derived artifacts must never fill an authority gap by invention.

## Ownership

### `docs/notion/authority/`

Owned by the architecture/governance workflow. Codex and other implementation agents must not redesign or silently modify these snapshots while implementing a task.

### `docs/notion/implementation/`

Contains approved execution packages derived from current authority. An implementation package may narrow scope, but it must not override architecture authority.

### Verification evidence

Executable evidence belongs in the repository verification/evidence structure. A verification specification is not evidence; an old PASS is not a PASS for a superseding authority revision.

## Lifecycle classes

- `current` / `frozen` — current normative authority for its declared scope.
- `proposed-freeze` — current implementation contract candidate; consumable only where the execution package explicitly permits it.
- `derived` — materialization blueprint derived from authority; cannot redefine source semantics.
- `verification-specification` — defines what must be proven; is not proof itself.
- `superseded` / `historical` / `stale-evidence` — provenance only; MUST NOT drive new implementation.

## Conflict rule

If implementation reveals a contradiction with current authority:

1. Do not silently patch the authority from implementation code.
2. Record the blocker and stop the affected path.
3. Resolve the issue in the architecture/governance workflow.
4. Publish a new current snapshot and supersede the prior version when the change is material.
5. Invalidate/re-run derived materialization and verification evidence affected by the change.
6. Resume implementation against the new manifest revision.

## Current reconciliation boundary — 2026-08-26

The repository mirror is being reconciled from the current `main` branch. The historical `docs/notion-bridge-bootstrap` branch is a migration source only. Its `schema/`, `verification/`, workflow and runtime changes are not imported by this reconciliation.

The current First-Batch Implementation Contract source set is pinned in `docs/notion/manifest.yaml` and currently resolves to:

- ICF-00 v0.3
- ICF-01 v0.1
- ICF-02 v0.2
- ICF-03 v0.2
- ICF-04 v0.2
- ICF-05 v0.1

Current gate: `Source Contract Closure Ready -> Repo Rematerialization / Reverification Pending`.
