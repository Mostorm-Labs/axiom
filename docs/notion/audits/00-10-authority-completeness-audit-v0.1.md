# Axiom 00–10 Authority Completeness Audit v0.1

> Audit date: 2026-08-24
> Branch: `docs/notion-bridge-bootstrap`
> Scope: compare current Notion architecture authority against repository snapshot under `docs/notion/authority/` and assess whether Codex can work from GitHub without requiring Notion access.

## Audit states

- **Complete** — repository snapshot contains the implementation-relevant authority at sufficient detail for offline consumption.
- **Partial** — repository contains a useful reconciliation/snapshot, but important source contracts remain only in Notion.
- **Index-only** — repository primarily contains an index or summary; the detailed authority remains in Notion.
- **Missing** — expected authority layer has no repository snapshot and no clearly defined source authority root.
- **Intentionally blank** — the Notion authority root itself is blank; repository records that fact and does not invent authority.

## Executive verdict

The migration is **not yet complete as an offline implementation authority set**.

Current shape:

| Layer | Audit state | Codex offline-ready? | Main finding |
|---|---|---:|---|
| 00 Product Research | **Missing** | No | Target layer exists in the migration design, but no formal top-level Notion authority root was identified and the GitHub directory is absent. |
| 01 Capability Traceability | **Complete** | Yes | Single primary authority snapshot is present. |
| 02 Product Object Model | **Complete** | Yes | Single primary authority snapshot is present. |
| 03 Interaction / Behavior | **Complete** | Yes | Single primary authority snapshot is present. |
| 04 Semantic Schema / Operation Model | **Complete** | Yes | Detailed Markdown authority, proto baseline, registries, descriptor lock and golden seed are materialized. |
| 05 Runtime Capability Architecture | **Partial** | Mostly | Reconciled implementation-facing baseline exists, but several source documents remain Notion-only. |
| 06 Module Detailed Design | **Partial** | No for full subsystem implementation | Closure is migrated, but the seven subsystem designs and Render Optimization 01–08 are not individually materialized. |
| 07 Runtime Data Flow | **Index-only** | No | Notion contains 07-00 through 07-15 plus Final Closure; GitHub currently has only an authority index/summary. |
| 08 Platform Contract | **Index-only** | No | Eight detailed platform matrices/decision documents remain Notion-only. |
| 09 Engineering | **Intentionally blank** | N/A | The Notion `09 Engineering` root is blank; GitHub correctly records this instead of inventing content. |
| 10 Verification | **Index-only** | No | Detailed conformance/harness/governance plans remain Notion-only even though some machine-readable verification assets already exist in the repo. |

## Layer-by-layer findings

### 00 — Product Research

**State: Missing.**

The migration target previously reserved `docs/notion/authority/00-product-research/`, but the current repository authority tree begins at `01-capability-traceability`. The Notion architecture baseline does not expose a formal top-level `00 Product Research` authority root. A page named `00 Overview` exists, but it belongs inside `04 Semantic Schema / Operation Model`, not product research.

**Required closure:** either define the formal 00 source set (for example Vibe capability research / competitive comparison / source requirement tables) and snapshot it, or explicitly remove 00 from the authority contract and classify product research as non-authoritative source material.

### 01 — Product Capability Traceability

**State: Complete.**

Repository snapshot:

- `docs/notion/authority/01-capability-traceability/axiom-product-capability-traceability-v0.1.md`

The Notion root resolves to the same primary authority document. This is sufficient as the product-capability-to-object/operation/module/platform bridge.

### 02 — Product Object Model

**State: Complete.**

Repository snapshot:

- `docs/notion/authority/02-product-object-model/axiom-product-object-model-v0.1.md`

The Notion root resolves to the same primary authority document. No additional source set is required for baseline offline consumption.

### 03 — Interaction / Behavior Model

**State: Complete.**

Repository snapshot:

- `docs/notion/authority/03-interaction-behavior/axiom-interaction-behavior-model-v0.1.md`

The detailed runtime realization is intentionally a 06 concern; the 03 semantic interaction authority itself is present.

### 04 — Semantic Schema / Operation Model

**State: Complete / machine-readable closure achieved.**

The repository contains:

- schema freeze/final-gate records;
- object schema and registries;
- operation payload/validation authority;
- Common Wire Rules and OrderKey;
- Reference IDL / codec mapping;
- leaf wire schemas and release closures;
- semantic hard limits;
- proto baseline under `schema/axiom/v1/proto/`;
- registries/canonical profiles;
- descriptor lock;
- binary golden seed and conformance bootstrap under `verification/`.

This is currently the strongest migrated authority layer and can be consumed by Codex without Notion access.

### 05 — Runtime Capability Architecture

**State: Partial.**

Repository currently contains:

- `README.md`
- `runtime-capability-architecture-v0.1.md`
- `axiom-architecture-baseline-v0.3.md`

The reconciled snapshot correctly captures the current five-domain split and the newer Platform Host identity. However, the Notion source root also contains detailed `Ownership Matrix`, `Dependency Graph`, `Public Boundary Contract`, and the historical Platform Host Runtime document. Those detailed contracts are not individually snapshotted.

**Risk:** Codex can understand the top-level architecture but may miss precise negative-dependency and boundary rules when implementing modules outside G0.

**Closure priority: P1.** Materialize at least Ownership Matrix, Dependency Graph, and Public Boundary Contract. Keep the historical host document explicitly superseded/annotated rather than treating it as current authority.

### 06 — Module Detailed Design

**State: Partial, high-impact gap.**

Repository currently contains only:

- `README.md`
- `module-design-closure-v0.1.md`

Notion contains the full detailed set:

- Axiom Semantic Core
- Axiom Interaction Runtime
- Axiom Scene Core
- Axiom Render Core
- Arc Runtime
- Shared Data Runtime
- Platform Host
- Render Optimization 01–08
- final cross-module closure

The closure document is valuable for ownership and dependency reconciliation, but it is not a substitute for the subsystem-specific lifecycle/API/error/resource/thread contracts.

**Risk:** implementation agents can preserve macro ownership while still inventing or drifting local module contracts.

**Closure priority: P0 before G1/G2/G3 production work expands.** Migrate the seven subsystem designs first; Render Optimization 01–08 can follow as a dedicated 06 subsection because G5 depends heavily on them.

### 07 — Runtime Data Flow

**State: Index-only, critical gap.**

Notion defines 07 as an authority hub with:

- 07-00 overview
- 07-01 Pointer → Preview
- 07-02 Pointer / Intent → Canonical Operation
- 07-03 Operation → Semantic Document
- 07-04 Document → RuntimeScene
- 07-05 RuntimeScene → Render → Presented
- 07-06 Commit → Local Storage → Cloud
- 07-07 Remote → Apply
- 07-08 Open / Restore / Catch-up
- 07-09 Special Flows
- 07-10 Runtime Data Model Matrix
- 07-11 through 07-15 integration contracts
- Final Closure / OPEN-to-Owner Handoff

GitHub currently contains only `README.md` summarizing/indexing these contracts.

**Risk:** this is the most dangerous implementation gap because ordering, no-echo, persist-first, generation, presentation proof, recovery and cross-thread/cross-language rules live here. An implementation agent without Notion access can easily build locally plausible but globally incorrect flows.

**Closure priority: P0.** Materialize 07-00–07-15 + Final Closure, preserving source status labels.

### 08 — Platform Contract

**State: Index-only.**

Notion contains eight detailed authority documents:

- Cross-platform Contract Matrix
- ABI / Bridge Matrix
- Surface / GPU Matrix
- Thread Matrix
- Lifecycle Matrix
- Text / IME Matrix
- ExternalSurface Matrix
- OPEN Platform Decisions

GitHub currently contains only `README.md`.

**Risk:** G0 platform harness and later platform implementation can validate against an index but not the actual physical realization constraints.

**Closure priority: P0 for platform-facing G0/G6 work; otherwise P1.** At minimum migrate ABI/Bridge, Thread, Lifecycle, Surface/GPU and OPEN Decisions first.

### 09 — Engineering

**State: Intentionally blank.**

The Notion `09 Engineering` page is blank. The GitHub `09-engineering/README.md` correctly records this rather than silently relocating Render Optimization or Verification authority into 09.

**Required decision:** leave 09 blank until a real authority scope is defined, or formally repurpose it later through an architecture decision. Do not fill it merely to complete numbering.

### 10 — Verification

**State: Index-only, critical for evidence-gated implementation.**

Notion contains a substantial verification authority set including:

- Verification Strategy + Matrix
- Semantic Conformance + Golden Corpus
- Cross-language Conformance Runner + Golden Vector File Format
- Golden Corpus Seed + Conformance CLI
- projection/result JSON schema design
- fixture authoring pipeline
- CI gates / corpus governance
- cross-platform parity/conformance
- platform scenario schemas/seed
- harness execution/fault-hook protocol
- reference runner/protocol vectors
- harness implementation/CI wiring
- Implementation Backlog / IH-00–IH-15
- Evidence-Gated G0–G9 design

GitHub currently contains only `docs/notion/authority/10-verification/README.md`, while some executable/machine-readable assets already exist under `verification/`.

**Risk:** executable assets without their full authority contract can become self-referential: implementation may accidentally treat current runner behavior as the specification.

**Closure priority: P0 for G0.** Migrate the verification strategy, semantic conformance, cross-language runner format, corpus governance, platform harness protocol, issue pack and G0/G0–G9 evidence design.

## Cross-cutting audit findings

### A. Manifest is materially incomplete

`docs/notion/manifest.yaml` currently enumerates 01–04 in detail and then only the overall architecture baseline at layer 05. It does not enumerate the newly added 05 reconciliation documents, 06–10 snapshots, G0 mapping, or their source page IDs/statuses.

**Impact:** the manifest cannot yet serve as the machine-readable source-of-truth index for migration completeness.

**Required closure: P0.** Extend the manifest to every current authority snapshot and add an explicit `audit_state`/`source_completeness` field or a companion generated inventory.

### B. Source-status preservation is uneven

04 preserves frozen/superseded distinctions well. 05–10 summaries preserve many status labels conceptually, but index-only migration means the per-document `Accepted / Proposed Freeze / Current Direction / Proposal / Superseded / OPEN` status is not available offline at source-document granularity.

**Required closure:** each migrated source document must carry source page ID, snapshot date, source status, authority precedence and known supersession links.

### C. G0 authority mapping is structurally correct but some targets are too shallow

`docs/notion/implementation/gates/G0/authority-map.yaml` now maps G0-00–G0-17 to authority roots. The mapping itself is useful, but references to 07/08/10 currently resolve to index-only snapshots.

**Impact:** the map points to the correct layer but not yet to all necessary offline contract text.

### D. Machine-readable authority is ahead of narrative migration in 04/10

04 has proto/registry/descriptor/golden assets already materialized. 10 has executable verification assets beginning to exist. This is good, but narrative authority and executable authority must be kept paired so that tests do not silently become the new specification.

### E. Branch integration hygiene remains open

At the time of this audit the migration branch has diverged significantly from `main`. The authority audit should be closed before opening the eventual migration PR, then the branch should be reconciled with current `main` and re-audited for path/CI conflicts.

## Closure plan

Migration should not be declared complete until the following are done:

1. **P0 — 07 full authority materialization:** 07-00–07-15 + Final Closure.
2. **P0 — 10 verification authority materialization:** strategy, semantic conformance, runner/vector format, corpus governance, platform harness protocol/seed, issue pack, G0/G0–G9 evidence design.
3. **P0 — 08 implementation-critical matrices:** ABI/Bridge, Surface/GPU, Thread, Lifecycle, OPEN Decisions; then Text/IME, ExternalSurface, cross-platform matrix.
4. **P0/P1 — 06 seven subsystem designs:** Semantic, Interaction, Scene, Render, Arc, Shared Data Runtime, Platform Host; then Render Optimization 01–08.
5. **P1 — 05 detailed ownership/dependency/boundary documents.**
6. **Decision — 00 Product Research:** define a real authority source set or explicitly remove the layer from the authority contract.
7. **P0 — manifest reconciliation:** enumerate every snapshot, status and source page; include 05–10 and G0 mapping.
8. **Final audit:** verify all authority-map targets resolve to implementation-ready local files and no Codex task requires Notion access for a frozen contract.

## Migration exit criteria

The Notion → GitHub authority migration is complete only when:

- every defined 00–10 layer is Complete, Intentionally Blank, or explicitly excluded by a recorded decision;
- every frozen/current implementation contract referenced by an implementation package exists locally in GitHub;
- historical/superseded sources cannot be mistaken for current authority;
- the manifest fully enumerates the snapshot set;
- machine-readable schema/verification assets have a corresponding narrative authority entry;
- G0-00–G0-17 authority mapping resolves to concrete local files, not only Notion links or shallow indexes;
- a clean Codex session with GitHub access but no Notion access can determine inputs, invariants, outputs, verification oracle and exit criteria for its assigned work package.

## Audit verdict

**Overall: PARTIAL / NOT READY TO CLOSE MIGRATION BRANCH.**

01–04 are in good shape. 05 is usable but incomplete. 06 is missing detailed subsystem authority. 07, 08 and 10 are currently index-only and are the main remaining blockers. 09 is correctly blank. 00 requires an explicit authority-scope decision.
