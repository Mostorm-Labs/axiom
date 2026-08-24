# MR-10-05 Platform Corpus / Harness Materialization — Closure Audit v0.1

> Audit date: 2026-08-25
> Branch: `docs/notion-bridge-bootstrap`
> Layer: `10-verification`
> Status: **IN PROGRESS — PHASE A/B MATERIALIZED; PHASE B CI VERIFIED; PHASE C G0 RECONCILIATION ACTIVE**

## 1. Purpose

MR-10-05 turns the already-authoritative Platform verification design into repo-local executable corpus/harness artifacts. It consumes, but does not redesign, 10-09 through 10-12, the MR-10-04 closed core-six schemas, or 08 Platform physical-realization authority.

Source pages remain Freeze Candidate / `proposed-freeze`.

Three corpora remain independently governed:

```text
60 semantic vectors
28 platform scenarios
56 harness protocol vectors
```

## 2. Phase A — Platform Corpus Namespace + Reference Resolution

Materialized:

```text
verification/platform/v1/suites/platform-seed-v0.1.json
verification/conformance/coordinator/test_platform_corpus_contracts.py
verification/conformance/coordinator/platform_contracts.py
```

The suite contains exactly the 28 stable 10-09 IDs. Semantic references resolve against the checked-in Semantic Golden namespace. Missing top-level `canonicalFixtureRef`, `SEMANTIC` step references, and `DATA_BRIDGE/APPLY_EXTERNAL` semantic fixture references fail closed.

Evidence chain includes test-first RED and subsequent GREEN implementation commits:

```text
a62aeb8a... test-first corpus namespace
68ef69fe... materialize platform seed namespace
e839a20d... resolve platform semantic fixture references
f512ca5d... test DataBridge semantic fixture resolution
64e91ed4... fix DataBridge external semantic fixture resolution
```

## 3. Phase B — 28 Scenario Body Materialization

Exactly one shared body per stable ID is materialized at:

```text
verification/platform/v1/scenarios/<PLAT-...>/scenario.json
```

No `scenario.web.json`, `scenario.windows.json`, `scenario.android.json`, or `scenario.apple.json` authority copies exist.

Phase B preserves:

- exact 10-09 requirement status and requirement IDs;
- ALL / ORIENTATION / NATIVE_ARC participation policy;
- explicit create → attach host → attach document → replay bootstrap where required;
- shared `REPLAY-MIXED-OPERATIONS-001` semantic golden;
- `OP-SETTRANSFORMS-VALID-001` for DataBridge external apply;
- OPEN physical realization only as profile/open-observation evidence;
- no correctness sleeps or platform-private semantic expected truth.

Test-first body commit:

```text
31178a1c6b79f0a8a4beb48e7393086e21bd2678
```

Observed RED was a missing first scenario body. A staging branch then materialized 28 files; compare proved `ahead_by: 28`, `behind_by: 0`, exactly 28 added scenario files only. The authority branch was fast-forwarded to scenario corpus head `0db1f91668c87cb8a1795abc01cdf8242a1251ae`.

Local Draft 2020-12 preflight reported `28/28` valid. User-provided GitHub Actions evidence subsequently showed the Phase B chain green at run number `#42`, head commit `64e91ed4334270dea1d8959b72388c6d6a2900aa`, after the expected RED test runs. Phase B is therefore recorded **CI VERIFIED**.

## 4. Phase C scope changed after repository reconciliation

The prior plan described Phase C as a greenfield `Normalized Trace Comparator Closure`, followed by fresh protocol/schema/runner/adapters phases.

Repository inspection of `main` showed that this would duplicate substantial existing G0 implementation. `main` already contains:

- TypeScript verification workspace;
- protocol package and envelope codec;
- Reference Platform Runner;
- scripted adapter and transport;
- exactly 56 protocol vectors;
- protocol trusted-root CI;
- verification native hooks/common host assets;
- 28 earlier-generation platform scenarios;
- Web reference adapter;
- Windows native adapter and Windows CI.

Therefore Phase C is now:

> **Existing G0 Implementation Reconciliation + Comparator Authority Alignment**

Detailed audit:

```text
docs/notion/audits/mr-10-05-phase-c-existing-g0-reconciliation-v0.1.md
```

## 5. Phase C reconciliation verdict

Existing implementation is split into three dispositions.

### REUSE

High-value existing components whose core semantics align with 10-10/10-11:

- HarnessEnvelope / session epoch model;
- ActionRequest / Receipt / Completion separation;
- CommandSequencer / EventSequencer;
- Action / Completion / Session registries;
- SourceLease tracking;
- deterministic Fault and LateEventFence core;
- in-process + serialized-loopback transport concept;
- protocol first-divergence collector;
- 56-vector protocol seed structure;
- protocol trusted-root CI concept;
- verification-only downstream native dependency direction.

### ADAPT

Existing platform implementation is older than current 10-08/10-09 machine authority and must be aligned, not copied over it:

- main core-six platform schemas;
- `platform-suite` field `scenarioRefs` → current authority `scenarios` stable-ID list;
- old string-event / pair-partial-order scenario generation → structured EventSelector/assertion generation;
- `validate_platform_scenarios.mjs` old corpus contract;
- Web/Windows observation output shape;
- premature result generation while comparator is deferred.

### MISSING

No current shared implementation was found for:

- Platform Scenario comparator;
- 10-08 deterministic Platform first-divergence ordering;
- required-event evaluator;
- forbidden-event evaluator;
- structured `HAPPENS_BEFORE` evaluator;
- state/generation/ownership assertion evaluator;
- `META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL`;
- CLI `compare` (currently reserved/not implemented).

Protocol `firstDivergence` is not a substitute: it diagnoses protocol meta-conformance, not Platform Scenario expectation divergence.

## 6. Critical authority drift identified

### Suite manifest

Current authority:

```text
scenarios: [stable scenario IDs]
```

Existing main implementation:

```text
scenarioRefs: [physical scenario paths]
```

The current authority form wins. Physical path resolution remains tooling behavior.

### Scenario expected contract

Current authority uses structured:

```text
requiredEvents[{id, selector, minCount, maxCount?}]
forbiddenEvents[{id, selector}]
partialOrder[{id, relation, before, after}]
stateAssertions[{id, kind, ...}]
openObservations[{id, key}]
```

The existing main corpus uses an older string/pair/operator representation. The Phase B corpus on this branch must remain the authority-aligned representation.

### Observation / result ownership

Existing Web/Windows tooling emits an older observation shape and creates `OBSERVED_AGREEMENT_OPEN` results while also declaring `COMPARATOR_DEFERRED`.

That ownership is not retained. Adapter/driver code may emit observed facts. Shared comparator/coordinator must decide agreement, divergence and final result.

## 7. Comparator authority order

Phase C implementation must use the source-defined order:

```text
0 harness / schema / artifact validity
1 required platform/profile/capability availability
2 scenario execution / terminal outcome
3 semantic projection assertions
4 required events
5 forbidden events
6 partial-order assertions
7 state / generation / ownership assertions
8 bridge / input assertions
9 cross-platform required equality
10 OPEN realization observations
```

Within a group, scenario array order is authoritative. Participants are ordered lexically by `platformFamily + profileId`.

## 8. Revised MR-10-05 progression

The remaining sequence is now reconciliation-first:

```text
Phase C1  Current authority ↔ existing G0 contract adapter
Phase C2  Normalize PlatformObservation boundary
Phase C3  Shared comparator core + first divergence
Phase C4  Partial-order meta-contract + state/generation/ownership assertions
Phase C5  Move PlatformConformanceResult ownership behind comparator
Phase C6  Adapt existing Web / Windows paths to current contract
Phase C7  Harden existing 10-10/10-11 protocol schemas/vectors where field-level drift remains
Phase C8  Rewire protocol trusted-root CI into the reconciled graph
then      Android / Apple / PR DAG / nightly / Gate Report aggregator
```

Do not reimplement the existing protocol runner or 56-vector corpus from scratch. Do not cherry-pick the whole `main/verification` tree over current authority artifacts.

## 9. Authority boundaries preserved

MR-10-05 still must not:

- promote 08 OPEN physical realization;
- let adapters read `expected` to decide PASS/FAIL;
- create per-platform semantic golden truth;
- use arbitrary sleep as correctness synchronization;
- expose verification fault/source/fence objects through Product public ABI;
- let implementation behavior become specification authority;
- treat protocol runner first divergence as Platform comparator first divergence;
- bypass protocol trusted-root evidence once the reconciled CI graph is active.

## 10. Current verdict

```text
MR-10-04                                  CLOSED / CI VERIFIED
Phase A corpus namespace                  MATERIALIZED
Phase A semantic reference fail-close     MATERIALIZED
Phase B 28 scenario bodies                MATERIALIZED
Phase B full CI                           VERIFIED (#42 / 64e91ed...)
Phase C G0 reconciliation audit           COMPLETE
Phase C authority-aligned implementation  NOT YET EXECUTED
Existing protocol runner / 56 vectors     REUSE CANDIDATE, NEEDS CONTRACT HARDENING
Existing Web / Windows adapters           ADAPT CANDIDATE
Platform comparator                       MISSING
Android / Apple adapters                  NOT STARTED
PR DAG / nightly / Gate aggregator        NOT STARTED
```

The immediate implementation transition is **Phase C1 — Current Authority ↔ Existing G0 Contract Adapter**, followed by comparator alignment. No upstream runtime architecture is reopened.