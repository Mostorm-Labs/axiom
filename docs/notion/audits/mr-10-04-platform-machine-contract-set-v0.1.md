# MR-10-04 Platform Machine Contract Set — Authority Closure Audit v0.1

> Audit date: 2026-08-24
> Branch: `docs/notion-bridge-bootstrap`
> Layer: `10-verification`
> Status: **CLOSED — CORE SIX MATERIALIZED / CI VERIFIED**

## 1. Purpose

MR-10-04 closes the gap between the narrative G3 Platform Verification design and the six repo-local machine-readable platform artifact contracts required before the 28-case platform seed can be materialized.

This is an authority migration / representation closure. It does **not** select a physical Surface primitive, GPU backend, thread topology, process topology, bridge implementation, device farm, timeout policy or pixel-render oracle.

## 2. Source authorities reviewed

Primary 10 Verification sources:

- 10-07 `Cross-platform Parity + Platform Conformance Matrix v0.1`
- 10-08 `Platform Scenario File Format + PlatformObservation JSON Schema v0.1`
- 10-09 `Platform Scenario Seed Set + Harness Adapter Skeleton v0.1`
- 10-10 `Platform Harness Execution Protocol + Fault Hook Contract v0.1`
- 10-11 `Platform Harness Reference Runner + Protocol Test Vectors v0.1`
- 10-12 `Platform Harness Implementation Plan + CI Job Wiring v0.1`

Relevant 08 owners reviewed:

- `08 Platform Contract` index
- `Cross-platform Contract Matrix v0.1`
- `Lifecycle Matrix v0.1`
- `OPEN Platform Decisions v0.1`
- source dependencies identified by 10-08: Surface/GPU, ABI/Bridge and Thread matrices.

All 10-07 through 10-12 source pages remain Freeze Candidate. 08 uses its own `Accepted / Current Direction / Proposal / Superseded / OPEN` vocabulary. Materialization does not promote either layer.

## 3. Core six contract inventory

| Contract | Primary source owner | 08 dependency | Status |
|---|---|---|---|
| `platform-suite` | 10-09 correction | scenario authority only | **MATERIALIZED + VERIFIED** |
| `platform-scenario` | 10-08 + 10-09 correction | references 08 invariants; no physical winner | **MATERIALIZED + VERIFIED** |
| `platform-profile` | 10-08 | OPEN realization stays metadata | **MATERIALIZED + VERIFIED** |
| `platform-trace` | 10-08 + 10-09 event correction | normalized observable vocabulary only | **MATERIALIZED + VERIFIED** |
| `platform-observation` | 10-08 | fact-only evidence; realization may differ | **MATERIALIZED + VERIFIED** |
| `platform-result` | 10-08 | evaluates scenario authority, not OPEN realization | **MATERIALIZED + VERIFIED** |

Repository files:

```text
verification/schemas/platform-suite.schema.json
verification/schemas/platform-scenario.schema.json
verification/schemas/platform-profile.schema.json
verification/schemas/platform-trace.schema.json
verification/schemas/platform-observation.schema.json
verification/schemas/platform-result.schema.json
```

All six use JSON Schema Draft 2020-12, fixed verification URNs, fixed format/version identifiers and top-level `additionalProperties: false`.

## 4. Why 08 OPEN decisions do not block schema materialization

08 explicitly separates stable semantic/ownership/lifecycle meaning from unresolved platform realization. Still-OPEN examples include Windows composition profile, Android Surface primitive/backend, Apple exact layer hierarchy, Web realm/worker/backend choice, exact native input executor, runtime/render/worker topology, special-host process topology and exact PlatformSurfaceAdapter descriptor ABI.

MR-10-04 does not choose any of them. 10-08 provides explicit non-authoritative channels:

1. `PlatformProfile.realization` — observed/profile metadata;
2. `openObservations[]` — explicit realization observations.

Therefore schema materialization freezes **how evidence is represented**, not the physical implementation winner.

A regression test explicitly confirms that arbitrary profile-local experimental backend/surface strings remain structurally valid realization metadata rather than becoming enum-frozen platform authority.

## 5. Source evolution / correction precedence

10-08 establishes the six-contract set and field-level model. 10-09 provides later corrections that are incorporated here:

- independent `platform-suite.schema.json` using `axiom-platform-suite-v1`;
- `PREVIEW_CLEAR_REQUESTED` and `PREVIEW_CLEARED` normalized events;
- seed-required capability vocabulary for state/ownership/stale-generation/completion/source/fence/source-attempt evidence.

These are refinements, not architecture conflicts.

## 6. Machine-readable rules represented

The materialized schemas/validator preserve at minimum:

- Platform families `WEB / WINDOWS / ANDROID / APPLE`;
- participant policy `REQUIRED / REQUIRED_WHEN_CAPABLE`;
- requirement status vocabulary shared with Verification;
- exact `u64:` + 16 lowercase hex generation/event sequence representation;
- PlatformScenario target/precondition/step/expected/capture envelope;
- event-bound waits instead of correctness sleeps;
- normalized lifecycle/surface/bridge event vocabulary;
- 10-09 Arc preview-clear event correction;
- fact-only PlatformObservation;
- opaque target binding evidence and no native-handle leakage policy;
- PlatformResult status separation, including OPEN result restrictions;
- profile `realization` as non-authoritative open metadata;
- safe run-relative artifact paths.

## 7. Semantic validation layer materialized

`verification/conformance/coordinator/platform_contracts.py` owns the minimal MR-10-04 semantic-validator boundary rather than expanding the semantic coordinator into a platform runtime.

Materialized checks include:

- six-schema inventory + Draft 2020-12/URN validation;
- suite/scenario/capability/assertion identity uniqueness where locally decidable;
- duplicate step-ID rejection;
- non-empty Spec/Freeze correctness oracle;
- exact eventSeq parsing and per-trace strict monotonic uniqueness;
- event-specific required facts for surface/metrics/device/DataBridge/callback/input/stale-generation observations;
- requested trace artifact presence;
- profile/scenario/observation identity consistency;
- native-handle leakage rejection for `bindingTag` evidence;
- OPEN scenario cannot emit semantic PASS;
- REQUIRED missing/not-capable participant must produce `FAIL_CAPABILITY_MISSING`.

The validator deliberately does **not** enum-freeze `PlatformProfile.realization` values.

## 8. Meta-contract tests materialized

`verification/conformance/coordinator/test_platform_machine_contracts.py` covers the MR-10-04 self-test layer needed before platform corpus materialization, including:

```text
core-six schema inventory
META-PLATFORM-SCENARIO-UNKNOWN-FIELD-REJECT
META-PLATFORM-SCENARIO-DUPLICATE-STEP-ID-REJECT
META-PLATFORM-SCENARIO-SPEC-WITHOUT-ORACLE-REJECT
META-PLATFORM-TRACE-DUPLICATE-EVENTSEQ-REJECT
META-PLATFORM-TRACE-NONMONOTONIC-EVENTSEQ-REJECT
META-PLATFORM-TRACE-SURFACE-BOUND-WITHOUT-GENERATION-REJECT
META-PLATFORM-OBS-MISSING-REQUESTED-TRACE-REJECT
META-PLATFORM-OBS-REAL-HANDLE-IN-BINDINGTAG-REJECT-BY-POLICY
META-PLATFORM-RESULT-PASS-WITH-DIVERGENCE-REJECT
META-PLATFORM-RESULT-OPEN-SCENARIO-PASS-REJECT
META-PLATFORM-RESULT-REQUIRED-NOTCAPABLE-WITHOUT-FAIL-REJECT
OPEN physical realization remains metadata
```

Two source meta-contract families intentionally remain cross-artifact/downstream rather than being faked during schema-only closure:

- `META-PLATFORM-SCENARIO-MISSING-FIXTURE-REJECT` needs the discovered semantic/platform corpus namespace;
- `META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL` needs actual normalized trace artifacts/comparator execution.

They become executable during MR-10-05 corpus/harness materialization. Their authority is already preserved in the 10-08 snapshot and is not forgotten.

## 9. Core six vs harness/protocol schemas

MR-10-04 remains bounded to the six platform evidence contracts.

10-10 / 10-11 introduce a separate Shared Runner protocol family:

```text
platform-harness-envelope
platform-harness-session
platform-fault-hook
platform-late-event-fence
platform-protocol-suite
platform-protocol-vector
platform-protocol-meta-result
```

Those support the 56-vector protocol trusted root and belong MR-10-05 / a subordinate harness-protocol closure. They are not aliases for the core six.

## 10. Verification evidence

TDD evidence retained:

- initial test-first commit: `a6b49c09f3e0613ecabd0930e2b3f0db00717239`;
- local RED reproduced: `test_core_six_platform_schemas_are_materialized` failed specifically because `platform-suite.schema.json` did not exist;
- six schema files were then materialized;
- targeted local GREEN exercised schema inventory, a valid PlatformScenario/PlatformTrace and the MR-10-04 negative policy checks.

Final CI evidence:

- GitHub Actions run: `https://github.com/Mostorm-Labs/axiom/actions/runs/32742201698`
- workflow: `Axiom V1 Conformance Seed Bootstrap`
- run number: `35`
- head branch: `docs/notion-bridge-bootstrap`
- head commit: `9d408191a760a2f769015b355b9cd67347f512a7`
- run status: `completed`
- run conclusion: `success`
- job `validate-seed-contract`: `success`
- step `Validate verification artifact contracts`: `success`
- JSON syntax, frozen descriptor compilation, 60 seed identity, bootstrap adapter safety, fail-closed run and whitespace verification: all `success`.

This is the final evidence required to close MR-10-04.

## 11. Explicitly out of MR-10-04

Downstream scope remains:

- full `platform-run.json` environment/device-farm schema;
- concrete browser/Windows SKU/Android OEM/Apple device matrix;
- numeric timeout policy;
- CI/device farm provider orchestration;
- physical Surface/GPU/backend/thread/process winner;
- pixel bit-exact rendering;
- 28 scenario file materialization;
- four real platform adapters;
- 56 protocol vectors/reference runner;
- deterministic fault implementation hooks;
- cross-artifact fixture discovery and actual partial-order comparison execution.

## 12. Closure verdict

**MR-10-04 — CLOSED.**

Exit criteria are all satisfied:

```text
Authority reconciliation                    PASS
Core six Draft 2020-12 schemas              MATERIALIZED
Platform semantic-validator boundary         MATERIALIZED
Source-defined schema/meta-contract tests    MATERIALIZED
Targeted RED/GREEN evidence                  PASS
Full GitHub Actions CI                       PASS — run 32742201698
08 OPEN realization preserved as OPEN        PASS
```

Source authority remains Freeze Candidate / `proposed-freeze`; this machine-readable closure does not promote 08 or 10 architecture status.

The next closure is **MR-10-05 — Platform Corpus / Harness Materialization**.
