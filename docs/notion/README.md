# Axiom Notion Architecture Bridge

This directory contains versioned, implementation-facing snapshots derived from the Axiom architecture maintained in Notion.

## Source of Truth

Notion is the living source of truth for:

- product modeling
- architecture decisions
- semantic contracts
- runtime architecture
- verification design

Files under `docs/notion/authority/` are frozen snapshots published from that source of truth for implementation use.

They are **not** an independently editable second source of truth.

## Ownership

### `docs/notion/authority/`

Owned by Architecture / Human + ChatGPT.

Codex and other implementation agents must not independently redesign or modify frozen authority documents while implementing a task.

### `docs/notion/implementation/`

Contains approved implementation packages derived from frozen architecture.

Implementation agents should use these packages as the entry point for implementation work.

### `docs/evidence/`

Contains implementation evidence produced by code, tests, benchmarks, demos, conformance runs, and other executable validation artifacts.

`docs/evidence/` remains part of the repository's existing documentation structure and is intentionally not mirrored under `docs/notion/`.

## Authority Status

Authority snapshots use the following lifecycle states:

- `draft` — exploratory; do not treat as implementation contract
- `proposed` — candidate direction; reference only
- `proposed-freeze` — pending freeze review; may be consumed only when an implementation package explicitly allows it
- `frozen` — authoritative implementation contract
- `superseded` — historical; do not use for new implementation

## Architecture Conflict Rule

If implementation reveals a contradiction with a frozen authority:

1. Do not silently modify the authority.
2. Record the architecture blocker.
3. Stop the affected implementation path.
4. Resolve the issue in the architecture workflow.
5. Publish a new authority snapshot.
6. Resume implementation against the new revision.

## Entry Point

Before consuming architecture snapshots, read:

`docs/notion/manifest.yaml`

The manifest is the machine-readable index of published authority snapshots and implementation-package roots.
