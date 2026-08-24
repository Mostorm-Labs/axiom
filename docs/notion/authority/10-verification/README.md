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

## 04 / 07 authority intake

10 consumes, but does not redefine, the frozen Semantic Schema / Operation Model in `../04-semantic-schema/` and runtime-data-flow contracts in `../07-runtime-data-flow/`.

Priority evidence inherited from 07 Final Closure includes semantic apply/idempotency, identity namespace separation, Incremental vs Full RuntimeScene equivalence, persist-first crash windows, LocalRecoveryClosure, external no-echo, missing resource closure, active-session conflict / multi-op compensation, PresentedFeedback / stale-generation / canonical-coverage behavior, lifecycle late-event rejection and performance candidates.

PlatformQualified presentation proof must ensure `PresentSubmitted`, `Approximate`, stale `SurfaceGeneration` / `MetricsGeneration`, or stale coverage cannot emit `CanonicalVisible(token)`; only qualified evidence with valid coverage may produce the exactly-once canonical-visible handoff.

## Machine-readable / executable closure map

### Closed

**MR-10-01 — Semantic Artifact Schema Closure — CLOSED (owner accepted, 2026-08-24).**

Materialized and wired:

- `verification/schemas/{corpus,suite,case,projection,observation,result,run}.schema.json`
- real JSON Schema Draft 2020-12 validation via `jsonschema.Draft202012Validator`
- frozen Axiom V1 descriptor compilation and SHA-256 verification in CI
- descriptor-backed `rootType` resolution
- IDL-aware message/scalar/enum/repeated/oneof validation
- canonical tagged scalar validation for f32/f64/i64/u64/bytes plus Id128 / OrderKey projection representations
- schema-backed `ImplementationObservation` / `ConformanceResult` validation
- fail-closed meta-tests in `verification/conformance/coordinator/test_semantic_artifact_contracts.py`

The earlier audit item **MR-10-02 — IDL-aware projection validation** was implemented as part of MR-10-01. It is **SUBSUMED / CLOSED BY MR-10-01** and must not be reimplemented as a parallel validator.

### Current closure

**MR-10-03 — First-Divergence Result Lock — AUTHORITY MIGRATION + MACHINE SCHEMA LOCK MATERIALIZED / CI EVIDENCE PENDING.**

Primary audit: `docs/notion/audits/mr-10-03-first-divergence-result-lock-v0.1.md`.

MR-10-03 is intentionally an authority/schema lock, not premature implementation of the full conformance engine. The following source-owned contracts are now repo-local at implementation-useful fidelity:

- 10-02: fixed semantic-stage order, replay checkpoints, `--stop-after-operation`, long-replay binary-search localization;
- 10-03: `diagnose --first-divergence`, compare-without-rerun behavior and deterministic diagnostic expectation;
- 10-04: current `DivergenceRecord v1`, GOLDEN vs CROSS_IMPLEMENTATION basis, location fields, Semantic Path Grammar v1 and first-divergence comparator order;
- 10-06: failure/observation evidence retention and deterministic-repeat expectations.

The old Runner `expectedArtifact / actualArtifacts` example is explicitly superseded at the result-shape layer by 10-04 `basis + reference? + observed[]`. 10-02 still owns runner/replay behavior.

`verification/schemas/result.schema.json` now machine-locks the structural portion of that source contract, including basis/reference rules, cross-implementation participant count, operation location pairing, semantic-path grammar and byte-offset kind constraints. Meta-tests cover those rules.

**Not part of MR-10-03 migration closure:** implementing recursive semantic diff across every Axiom type, materializing all 60 fixtures/adapters, or building production replay bisection. Those belong to later Codex implementation packages after authority closure.

### Remaining after MR-10-03

1. Platform verification schemas required by 10-08: `platform-suite`, `platform-scenario`, `platform-profile`, `platform-trace`, `platform-observation`, `platform-result`.
2. `verification/platform/v1/` seed suite + scenario corpus and platform harness adapters / normalized trace contracts from 10-09 through 10-12.
3. CI lock/governance binding machine contracts to proposed-freeze authority without allowing implementation behavior to become specification.
4. Executable intake of 07 correctness invariants, including Incremental RuntimeScene ≡ Full Rebuild RuntimeScene, persist-first, recovery/no-echo and presentation-generation invariants.
5. G0–G9 evidence-package binding to concrete repo-local authority, oracle, runnable proof and exit evidence.

## Migration / supersession note

The historical `docs/notion/audits/00-10-authority-completeness-audit-v0.1.md` recorded 10 as `Index-only`. That statement describes an earlier repository state and is superseded for layer 10 by `docs/notion/audits/10-full-authority-migration-closure-v0.1.md`. Historical audits are retained rather than rewritten.
