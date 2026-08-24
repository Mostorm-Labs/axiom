# 07 Runtime Data Flow — Authority Index

> Source: Notion `07 Runtime Data Flow`
> Source page: https://app.notion.com/p/3c44c57a590c80a6b37ef389b5d6f9e4
> Snapshot date: 2026-08-24
> Repository status: proposed-freeze

07 is an Authority Hub. It does not redesign 00–06; it defines where runtime data originates, who owns/transforms it, when it becomes canonical/derived/durable/rendered/presented, and where thread/language boundaries occur.

## Source document set

- 07-00 Runtime Data Flow Overview — Current Direction
- 07-01 Pointer -> Preview — Current Direction
- 07-02 Pointer / Intent -> Canonical Operation — Current Direction
- 07-03 Operation -> Semantic Document — Proposed Freeze
- 07-04 Document -> RuntimeScene — Proposed Freeze
- 07-05 RuntimeScene -> Render -> Presented — Proposed Freeze
- 07-06 Commit -> Local Storage -> Cloud — Proposed Freeze
- 07-07 Remote -> Apply — Proposed Freeze
- 07-08 Open / Restore / Catch-up — Proposed Freeze
- 07-09 Special Flows — Proposed Freeze
- 07-10 Runtime Data Model Matrix — Proposed Freeze
- 07-11 SemanticGeneration / ChangeSet / ApplyBatchResult Integration Contract — Proposed Freeze
- 07-12 Sync Revision / Cursor / ACK + RemoteApplied Frontier — Proposed Freeze
- 07-13 Blob / Resource Materialization State Machine — Proposed Freeze
- 07-14 Interaction Conflict / Multi-Operation Intent Integration Contract — Proposed Freeze
- 07-15 Presented Proof / CanonicalVisible / Arc Handoff ABI Integration Contract — Proposed Freeze
- Final Closure / OPEN-to-Owner Handoff — Proposed Freeze

## Runtime invariants

- Semantic Document = only canonical truth.
- RuntimeScene / Bounds / Spatial / Tile / GPU state = derived and rebuildable.
- Pointer / Preview hot path bypasses Shared Data Runtime and React state.
- Local semantic commit and external apply are distinct; remote/replay is no-echo.
- Canonical committed != rendered != presented.
- Local Durable != Cloud Synced.
- Local Ready precedes/does not depend on Cloud Ready.
- Operation is the canonical mutation unit; no global Transaction is restored.

## Final closure

07 reports no remaining blocking architecture gap in the runtime data-flow trunk. Remaining OPEN items are handed to 08 Platform Contract for physical realization, collaboration/sync authority for conflict semantics, and 10 Verification for executable evidence/benchmarks/fault scenarios.

Implementation may choose physical schema/container/API details inside owner modules only if they preserve this logical authority.
