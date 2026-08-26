# MR-10-05 Phase C — Existing G0 Implementation Reconciliation + Comparator Authority Alignment v0.1

> Audit date: 2026-08-25
> Branch under authority migration: `docs/notion-bridge-bootstrap`
> Existing implementation baseline inspected: `main`
> Layer: `10-verification`
> Status: **RECONCILIATION COMPLETE / IMPLEMENTATION ALIGNMENT NOT YET EXECUTED**

## 1. Purpose

Phase C is no longer a greenfield comparator implementation task. Repository inspection found that `main` already contains a substantial G0 verification workspace: protocol schemas/packages, a Reference Platform Runner, scripted transport, a 56-vector protocol seed, protocol trusted-root CI, Web reference adapter, Windows native adapter/host and evidence generators.

This audit reconciles that existing implementation against the current 10-08 through 10-12 authority migrated on `docs/notion-bridge-bootstrap`.

The rule is:

```text
current 10-08..10-12 authority
        ↓
inspect existing main implementation
        ↓
REUSE | ADAPT | MISSING
        ↓
minimal alignment only
```

Implementation behavior is evidence, not specification authority. A passing historical G0 task does not override a newer Freeze Candidate machine contract.

## 2. Authority inputs

The reconciliation uses these migrated sources:

- `platform-scenario-observation-schema-v0.1.md` — structured PlatformScenario / Observation / Result contract and deterministic comparison order;
- `platform-scenario-seed-adapter-v0.1.md` — exactly 28 stable scenarios, one shared corpus, explicit bootstrap, adapter facts-only boundary;
- `platform-harness-execution-fault-v0.1.md` — execution protocol, source lease, completion, fault and LateEventFence semantics;
- `platform-harness-reference-runner-protocol-vectors-v0.1.md` — trusted-root Reference Runner and exactly 56 protocol vectors;
- `platform-harness-implementation-ci-v0.1.md` — package ownership and CI dependency order.

All remain Freeze Candidate / `proposed-freeze`. This audit does not promote them.

## 3. Existing `main` implementation inventory

Repository inspection confirmed the following existing implementation:

```text
verification/
├── package.json / package-lock.json
├── packages/
│   ├── platform-harness-protocol/
│   ├── platform-harness-runner/
│   ├── platform-harness-scripted-adapter/
│   ├── platform-harness-transport/
│   ├── platform-harness-web/
│   └── platform-conformance-cli/
├── platform/protocol/v1/
│   ├── suites/protocol-seed-v0.1.json
│   └── vectors/<56 HPR vectors>/vector.json
├── platform/v1/scenarios/<28 scenario bodies>/scenario.json
├── native/platform/windows/
└── tools/
```

`main` also contains:

- `.github/workflows/g0-platform-protocol-seed.yml`;
- `.github/workflows/g0-windows-native-adapter.yml`.

The Gate tracker records GT-G0-00..11 as Pass, while Android, Apple, PR DAG, nightly/release and Gate aggregation remain unfinished.

## 4. Reconciliation matrix

| Area | Existing `main` state | Current authority disposition | Phase C action |
|---|---|---|---|
| Harness envelope/session/action/completion | Implemented | Semantically aligned in core model | **REUSE** after contract tests |
| Command / event sequencing | Implemented | Aligned ownership concept | **REUSE** |
| SourceLease / Fault / LateEventFence | Implemented | Aligned core ownership | **REUSE**, audit SourceAttempt gap |
| In-process + serialized boundary | Implemented | Aligned with 10-11 minimum portability boundary | **REUSE** |
| 56 protocol vector corpus | Implemented | Correct seed cardinality and family split | **REUSE + CONTRACT-HARDEN** |
| Protocol trusted-root CI | Implemented on `main` | Correct dependency concept | **REUSE + REWIRE** into current authority branch later |
| Core six platform schemas | Older / looser generation on `main` | Current 10-08/10-09 machine set is newer | **ADAPT** |
| Platform suite manifest | `scenarioRefs` path list | Authority requires ordered `scenarios` ID list | **ADAPT** |
| 28 platform scenario bodies | Older expected/assertion generation on `main` | Current Phase B bodies are newer authority representation | **ADAPT / REPLACE OLD GENERATION** |
| Platform corpus validator | Tied to old `scenarioRefs`, string events and pair partial-order | Current structured selectors/assertions required | **ADAPT** |
| PlatformObservation builders | Old permissive shape | Current typed execution/terminal/step/artifact/binding shape required | **ADAPT** |
| PlatformConformanceResult generation | Result emitted while comparator is deferred | Comparator must own result judgment | **ADAPT — move result behind comparator** |
| Platform scenario comparator | `compare` CLI reserved / no shared comparator found | Required by 10-08 | **MISSING — IMPLEMENT** |
| Deterministic platform first divergence | Protocol first divergence exists, platform comparator ordering absent | Required by 10-08 | **MISSING — IMPLEMENT** |
| Required / forbidden event evaluation | Old corpus validation only; no observation comparator | Required by 10-08 | **MISSING — IMPLEMENT** |
| Structured partial-order oracle | Old pair representation validation only | Required by 10-08 | **MISSING — IMPLEMENT** |
| State / generation / ownership assertion evaluator | No current shared evaluator found | Required by 10-08 | **MISSING — IMPLEMENT** |
| `META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL` | Not proven | Explicit meta-contract | **MISSING — IMPLEMENT** |
| Web adapter | Exists | Adapter facts-only boundary mostly correct, output contract stale | **ADAPT** |
| Windows native adapter | Exists | Verification-only/native boundary reusable, output contract stale | **ADAPT** |
| Android adapter | Not started | Required later | **DEFER** |
| Apple adapter | Not started | Required later | **DEFER** |
| PR DAG / nightly / Gate aggregator | Not started | Required later | **DEFER** |

## 5. Critical generation drifts

### 5.1 Platform suite drift

Existing `main` schema uses:

```text
scenarioRefs: ["verification/platform/v1/scenarios/.../scenario.json"]
```

Current 10-09 authority uses:

```text
scenarios: ["PLAT-CREATE-CANVAS-001", ...]
```

The authority form is a stable ordered scenario-ID namespace. Physical path resolution belongs semantic validation/tooling, not the authority manifest field.

### 5.2 Scenario oracle drift

Existing `main` scenarios use an older representation such as:

```text
requiredEvents: ["SURFACE_UNAVAILABLE", ...]
partialOrder: [["SURFACE_UNAVAILABLE", "SURFACE_REBOUND"]]
stateAssertions: [{ selector, operator, value }]
```

Current authority uses:

```text
requiredEvents: [{ id, selector, minCount, maxCount? }]
partialOrder: [{ id, relation: HAPPENS_BEFORE, before, after }]
stateAssertions: [{ id, kind, ... }]
openObservations: [{ id, key }]
```

The current Phase B corpus is therefore the representation to preserve; existing G0 scenario bodies must not overwrite it.

### 5.3 Observation drift

Existing Web/Windows outputs include older fields such as:

```text
execution: { adapter, boundary, deterministic }
capabilities: [...]
terminal: { state: ... }
diagnostics: ["..."]
```

Current 10-08 authority requires typed execution metadata, terminal `outcome`, typed step outcomes, requested artifact references, target binding semantics and structured diagnostics. Capabilities are profile responsibility rather than an adapter-created alternate authority channel.

### 5.4 Premature result drift

Existing Web/Windows paths write `PlatformConformanceResult` with `OBSERVED_AGREEMENT_OPEN` while explicitly recording `COMPARATOR_DEFERRED`.

This is not a valid final ownership model. Observation capture can be fact-only, but an agreement/divergence result is coordinator/comparator judgment. Until comparison runs, the adapter/driver path must not manufacture conformance agreement.

### 5.5 Comparator absence

The existing CLI explicitly reserves:

```text
list
compare
aggregate
```

and returns `NOT_IMPLEMENTED` for them. No shared platform scenario comparator was found in the runner package exports. Protocol `firstDivergence` exists, but it is **protocol meta-conformance divergence**, not Platform Scenario first divergence.

Therefore Phase C still has a real implementation gap: the Platform comparator itself.

## 6. Protocol layer reuse assessment

The protocol trusted-root implementation has high reuse value and should not be rewritten.

Strong alignment already present:

- HELLO / session epoch model;
- action request / receipt / completion separation;
- one-shot completion tracking;
- command and event sequencing;
- source lease tracking;
- deterministic fault state;
- LateEventFence;
- in-process and serialized-loopback boundaries;
- protocol first-divergence collection;
- 56-vector seed;
- read-only protocol corpus CI;
- no bless/update-golden CLI path.

Before declaring 10-10/10-11 machine closure, however, the following must be reconciled rather than assumed:

- protocol vector JSON Schema is currently much shallower than the source stable field contract;
- `SourceAttemptRecorder` is not visible as an explicit runner-owned component in the inspected package exports;
- WaitResolver / CaptureCoordinator / ObservationBuilder / ArtifactWriter are not visible in the inspected runner package;
- exact ProtocolErrorCategory and ProtocolMetaResult field contract still needs field-level comparison.

These are follow-up alignment items, not reasons to discard the working protocol runner.

## 7. Comparator authority alignment

The Platform comparator must implement the 10-08 deterministic order exactly:

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

Within each group, scenario array order is authoritative. Cross-platform participant ordering is lexical `platformFamily + profileId`.

This ordering belongs the current authority. Existing implementation convenience must not reorder it.

## 8. Minimal implementation sequence after reconciliation

Phase C implementation should proceed in this order:

1. **Authority contract adapter layer** — make existing G0 tooling consume the current `scenarios[]` suite and structured Phase B scenario representation without downgrading it.
2. **Normalized observation boundary** — align observation builders/schema usage before comparison.
3. **Comparator core** — implement required events, forbidden events, partial order and first-divergence skeleton against structured selectors.
4. **Meta-contract closure** — prove `META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL` and deterministic ordering.
5. **State/generation/ownership assertions** — add only source-defined assertion kinds.
6. **Result ownership** — generate `PlatformConformanceResult` only after comparator evaluation.
7. **Web/Windows adaptation** — reuse platform seams, but emit current observations and let shared comparator own results.
8. **Protocol layer hardening** — reconcile current 10-10/10-11 field-level schema contract without rewriting the functioning runner.

Do not cherry-pick the full `main/verification` tree over the migration branch. Reconciliation is selective and authority-first.

## 9. Phase C exit criteria

Phase C is not closed until all are true:

- existing G0 implementation has a documented disposition for each authority-owned component;
- current structured Phase B corpus remains source-of-truth representation;
- existing G0 tooling consumes that representation without compatibility shadow authority;
- comparator owns PASS/FAIL and result first divergence;
- `META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL` is executable and green;
- deterministic comparison order is test-locked;
- adapters cannot read `expected` to decide outcome;
- protocol first divergence and platform first divergence remain distinct;
- CI proves the aligned path without weakening the protocol trusted root.

## 10. Verdict

**Existing G0 implementation is valuable and substantially reusable, but it is not authority-equivalent as-is.**

The correct Phase C strategy is:

```text
REUSE protocol execution core
+ ADAPT older platform contract generation
+ IMPLEMENT missing shared comparator
+ KEEP adapters facts-only
+ KEEP current 10-08/10-09 machine authority
```

No current finding requires reopening 04 Semantic Schema, 07 Runtime Data Flow or 08 physical Platform realization.