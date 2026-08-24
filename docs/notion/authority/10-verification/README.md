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

The `10-xx` labels above are a stable local reading order for the migration set. They do not rename the source pages and do not imply an acceptance hierarchy that is absent in Notion.

## Verification principles

- Golden expected is read-only to implementation-under-test; blocking CI has no auto-bless path.
- Verification-only hooks do not enter Product public ABI and production targets must not depend on verification targets.
- Protocol seed must become trustworthy before platform seed is trusted.
- Unsupported platform scenarios are explicit `SKIP` / `UNSUPPORTED`, never false PASS.
- First divergence and deterministic repeat are machine-readable evidence requirements.
- Test harnesses prove runtime authority; they do not redefine it.
- Agreement among implementations cannot override a mismatch against a trusted golden.

## 04 / 07 authority intake

10 consumes, but does not redefine, the frozen Semantic Schema / Operation Model in `../04-semantic-schema/` and the runtime-data-flow contracts in `../07-runtime-data-flow/`.

Priority evidence inherited from 07 Final Closure includes semantic apply/idempotency, identity namespace separation, Incremental vs Full RuntimeScene equivalence, persist-first crash windows, LocalRecoveryClosure, external no-echo, missing resource closure, active-session conflict / multi-op compensation, PresentedFeedback / stale-generation / canonical-coverage behavior, lifecycle late-event rejection and performance candidates.

PlatformQualified presentation proof must ensure `PresentSubmitted`, `Approximate`, stale `SurfaceGeneration` / `MetricsGeneration`, or stale coverage cannot emit `CanonicalVisible(token)`; only qualified evidence with valid coverage may produce the exactly-once canonical-visible handoff.

## Machine-readable / executable closure map

Already materialized in the repository:

- `verification/schemas/{corpus,suite,projection}.schema.json`
- `verification/golden/v1/corpus.json`
- `verification/golden/v1/suites/seed-v0.1.json` — exactly 60 stable semantic seed cases
- `verification/golden/v1/suites/codec-binary-seed-v0.1.json`
- `verification/conformance/adapters/{cpp,wasm,ts}/`
- `verification/conformance/coordinator/`
- `verification/fixture-author/`
- `.github/workflows/conformance-seed-v1.yml`
- `.github/workflows/codec-binary-seed-v1.yml`

Still required for Machine-Readable Authority Closure:

1. Semantic verification artifact schemas required by 10-03 / 10-04 but not yet materialized: `case.schema.json`, `observation.schema.json`, `result.schema.json`, `run.schema.json`.
2. IDL-aware projection validation and schema-backed `ImplementationObservation` / `ConformanceResult` / `DivergenceRecord` validation in the coordinator.
3. Platform verification schemas required by 10-08: `platform-suite`, `platform-scenario`, `platform-profile`, `platform-trace`, `platform-observation`, `platform-result`.
4. `verification/platform/v1/` seed suite + scenario corpus and the platform harness adapters / normalized trace contracts from 10-09 through 10-12.
5. CI lock/governance that binds the above machine contracts to the proposed-freeze authority without allowing implementation behavior to become the specification.
6. G0–G9 evidence-package binding so each Gate points to concrete authority, oracle, runnable proof, performance characterization where applicable, and exit evidence.

## Migration / supersession note

The historical `docs/notion/audits/00-10-authority-completeness-audit-v0.1.md` recorded 10 as `Index-only`. That statement describes an earlier repository state and is superseded for layer 10 by `docs/notion/audits/10-full-authority-migration-closure-v0.1.md`. The historical audit is retained rather than rewritten.
