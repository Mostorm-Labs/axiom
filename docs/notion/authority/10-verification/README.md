# 10 Verification — Authority Index

> Source: Notion `10 Verification`
> Source page: https://app.notion.com/p/3c44c57a590c8029ab4fc016e29c4156
> Source page id: `3c44c57a-590c-8029-ab4f-c016e29c4156`
> Snapshot date: 2026-08-24
> Migration state: Full narrative authority migration complete; machine-readable / executable closure remains partial.

00–09 define what Axiom should be; 10 defines what evidence is sufficient to prove those contracts. Migration into GitHub does **not** upgrade a source document's authority status.

## Reading order / authority mapping

| Local slot | Snapshot | Original source status | Normalized migration status |
|---|---|---|---|
| 10-00 | `verification-strategy-matrix-v0.1.md` | Draft / Freeze Candidate — Verification Architecture | `proposed-freeze` |
| 10-01 | `semantic-conformance-golden-corpus-v0.1.md` | Draft / Freeze Candidate — Semantic Verification Contract | `proposed-freeze` |
| 10-02 | `cross-language-runner-golden-vector-format-v0.1.md` | Freeze Candidate — Verification Tooling Contract | `proposed-freeze` |
| 10-03 | `golden-corpus-seed-cli-v0.1.md` | Freeze Candidate — Seed Corpus + Tooling Scaffold | `proposed-freeze` |
| 10-04 | `semantic-projection-result-schema-v0.1.md` | Freeze Candidate — Machine-readable Verification Contract | `proposed-freeze` |
| 10-05 | `golden-authoring-fixture-pipeline-v0.1.md` | Freeze Candidate — Golden Authoring / Trust Boundary Contract | `proposed-freeze` |
| 10-06 | `conformance-ci-corpus-governance-v0.1.md` | Freeze Candidate — Conformance CI / Corpus Governance Contract | `proposed-freeze` |
| 10-07 | `cross-platform-parity-conformance-v0.1.md` | Freeze Candidate — G3 Platform Gate Contract | `proposed-freeze` |
| 10-08 | `platform-scenario-observation-schema-v0.1.md` | Freeze Candidate — Machine-readable G3 Platform Verification Contract | `proposed-freeze` |
| 10-09 | `platform-scenario-seed-adapter-v0.1.md` | Freeze Candidate — G3 Platform Seed Corpus + Harness Adapter Contract | `proposed-freeze` |
| 10-10 | `platform-harness-execution-fault-v0.1.md` | Freeze Candidate — G3 Harness Execution / Deterministic Fault Contract | `proposed-freeze` |
| 10-11 | `platform-harness-reference-runner-protocol-vectors-v0.1.md` | Freeze Candidate — G3 Harness Reference Runner / Protocol Meta-Conformance Contract | `proposed-freeze` |
| 10-12 | `platform-harness-implementation-ci-v0.1.md` | Freeze Candidate — G3 Platform Harness Implementation / CI Wiring Plan | `proposed-freeze` |
| 10-13 | `implementation-backlog-issue-pack-v0.1.md` | Execution Pack — derived from G3 Freeze Candidate contracts | `proposed` |
| 10-14 | `evidence-gated-vertical-build-g0-g9-v0.1.md` | Design Approved / Freeze Candidate — Implementation Verification Architecture | `proposed-freeze` |

The `10-xx` labels above are a stable local reading order for the migration set. They do not rename source pages and do not imply an acceptance hierarchy absent in Notion.

## Verification principles

- Golden expected is read-only to implementation-under-test; blocking CI has no auto-bless path.
- Verification-only hooks do not enter Product public ABI and production targets must not depend on verification targets.
- Protocol seed must become trustworthy before platform seed is trusted.
- Unsupported platform scenarios are explicit `SKIP` / `UNSUPPORTED`, never false PASS.
- First divergence and deterministic repeat are machine-readable evidence requirements.
- Test harnesses prove runtime authority; they do not redefine it.
- Agreement among implementations cannot override a mismatch against a trusted golden.

## 04 / 07 / 08 authority intake

10 consumes, but does not redefine, the frozen Semantic Schema / Operation Model in `../04-semantic-schema/`, runtime-data-flow contracts in `../07-runtime-data-flow/`, or physical platform ownership/realization decisions in `../08-platform-contract/`.

08 continues to own Accepted / Current Direction / Proposal / OPEN physical realization choices such as Surface primitive/backend/thread/process topology. Verification may encode those as profile/observation evidence but cannot select a winner.

## Machine-readable / executable closure map

### Closed

**MR-10-01 — Semantic Artifact Schema Closure — CLOSED (owner accepted, 2026-08-24).**

Materialized semantic artifact schemas, real Draft 2020-12 validation, frozen-IDL descriptor validation and fail-closed meta-tests. The formerly separate IDL-aware projection item is **SUBSUMED / CLOSED BY MR-10-01**.

### Evidence pending

**MR-10-03 — First-Divergence Result Lock — AUTHORITY MIGRATION + MACHINE SCHEMA LOCK MATERIALIZED / CI EVIDENCE PENDING.**

Primary audit: `docs/notion/audits/mr-10-03-first-divergence-result-lock-v0.1.md`.

**MR-10-04 — Platform Machine Contract Set — CORE SIX MATERIALIZED / TARGETED LOCAL VALIDATION PASS / CI EVIDENCE PENDING.**

Primary audit: `docs/notion/audits/mr-10-04-platform-machine-contract-set-v0.1.md`.

Materialized core set:

```text
verification/schemas/platform-suite.schema.json
verification/schemas/platform-scenario.schema.json
verification/schemas/platform-profile.schema.json
verification/schemas/platform-trace.schema.json
verification/schemas/platform-observation.schema.json
verification/schemas/platform-result.schema.json
verification/conformance/coordinator/platform_contracts.py
verification/conformance/coordinator/test_platform_machine_contracts.py
```

10-08 supplies the field-level base contract; 10-09 supplies the later suite/Arc-event/capability corrections. 08 OPEN physical realization remains `PlatformProfile.realization` / `openObservations` metadata and is not enum-frozen into a winner. 10-10/10-11 execution-protocol schemas remain a separate harness/protocol trusted-root layer.

The source-required cross-artifact checks for missing referenced fixtures and actual partial-order evaluation remain deliberately downstream because they need discovered corpus/trace artifacts; their authority is preserved and they enter MR-10-05 rather than being faked in schema-only closure.

Full GitHub Actions evidence is the remaining MR-10-04 close condition.

### Remaining after MR-10-04

1. MR-10-05 Platform Corpus / Harness Materialization: `verification/platform/v1/`, 28 stable scenarios, four adapters, normalized trace artifacts and deterministic fault hooks.
2. Harness protocol trusted root: execution-protocol schemas + 56 protocol vectors + reference runner meta-conformance.
3. CI/governance lock binding platform evidence to proposed-freeze authority without allowing implementation behavior to become specification.
4. Executable intake of 07 correctness invariants, including Incremental RuntimeScene ≡ Full Rebuild RuntimeScene, persist-first, recovery/no-echo and presentation-generation invariants.
5. G0–G9 evidence-package binding to concrete repo-local authority, oracle, runnable proof and exit evidence.

## Migration / supersession note

The historical `docs/notion/audits/00-10-authority-completeness-audit-v0.1.md` recorded 10 as `Index-only`. That statement describes an earlier repository state and is superseded for layer 10 by the current closure audits. Historical audits are retained rather than rewritten.
