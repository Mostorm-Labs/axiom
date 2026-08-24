# 07 Runtime Data Flow — Authority Index

> Source: Notion `07 Runtime Data Flow`
> Source page: https://app.notion.com/p/3c44c57a590c80a6b37ef389b5d6f9e4
> Snapshot date: 2026-08-24
> Repository status: full-authority-migrated

07 is an Authority Hub. It does not redesign 00–06; it defines where runtime data originates, who owns/transforms it, when it becomes canonical/derived/durable/rendered/presented, and where thread/language boundaries occur.

## Migrated authority set

- [07-00 Runtime Data Flow Overview](07-00-runtime-data-flow-overview-v0.1.md) — Current Direction
- [07-01 Pointer → Preview](07-01-pointer-preview-v0.1.md) — Current Direction
- [07-02 Pointer / Intent → Canonical Operation](07-02-intent-canonical-operation-v0.1.md) — Current Direction
- [07-03 Operation → Semantic Document](07-03-operation-semantic-document-v0.1.md) — Proposed Freeze
- [07-04 Document → RuntimeScene](07-04-document-runtime-scene-v0.1.md) — Proposed Freeze
- [07-05 RuntimeScene → Render → Presented](07-05-render-presented-v0.1.md) — Proposed Freeze
- [07-06 Commit → Local Storage → Cloud](07-06-commit-local-storage-cloud-v0.1.md) — Proposed Freeze
- [07-07 Remote → Apply](07-07-remote-apply-v0.1.md) — Proposed Freeze
- [07-08 Open / Restore / Catch-up](07-08-open-restore-catchup-v0.1.md) — Proposed Freeze
- [07-09 Special Flows](07-09-special-flows-v0.1.md) — Proposed Freeze
- [07-10 Runtime Data Model Matrix](07-10-runtime-data-model-matrix-v0.1.md) — Proposed Freeze
- [07-11 SemanticGeneration / ChangeSet / ApplyBatchResult](07-11-generation-changeset-apply-batch-v0.1.md) — Proposed Freeze
- [07-12 Sync Revision / Cursor / ACK + RemoteApplied Frontier](07-12-sync-frontier-ack-v0.1.md) — Proposed Freeze
- [07-13 Blob / Resource Materialization](07-13-resource-materialization-v0.1.md) — Proposed Freeze
- [07-14 Interaction Conflict / Multi-Operation Intent](07-14-interaction-conflict-multi-op-v0.1.md) — Proposed Freeze
- [07-15 Presented Proof / CanonicalVisible / Arc Handoff](07-15-presented-proof-arc-handoff-v0.1.md) — Proposed Freeze
- [Final Closure / OPEN-to-Owner Handoff](final-closure-open-owner-handoff-v0.1.md) — Proposed Freeze

## Runtime invariants

- Semantic Document = only canonical truth.
- RuntimeScene / Bounds / Spatial / Tile / GPU state = derived and rebuildable.
- Pointer / Preview hot path bypasses Shared Data Runtime and React state.
- Local semantic commit and external apply are distinct; remote/replay/recovery is no-echo.
- Canonical committed != rendered != presented != CanonicalVisible(token).
- Local Durable != Cloud Synced.
- Local Ready does not depend on Cloud Ready.
- Remote canonical apply implies DurableInbound; dispatch also requires OrderReady.
- Operation is the canonical mutation unit; no global Transaction is restored.
- Resource identity, bytes, materialization and cloud availability are separate facts.
- Active InteractionSession cannot veto canonical apply.

## Final closure

07 reports no remaining blocking architecture gap in the runtime data-flow trunk. Remaining OPEN items are explicitly handed to 08 Platform Contract, collaboration/sync semantics, or 10 Verification.

The repository now contains implementation-facing snapshots for the complete 07-00..07-15 + Final Closure set, so Codex no longer needs Notion access to recover the 07 logical data-flow authority.