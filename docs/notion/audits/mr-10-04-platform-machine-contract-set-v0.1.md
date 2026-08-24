# MR-10-04 Platform Machine Contract Set — Authority Closure Audit v0.1

> Audit date: 2026-08-24
> Branch: `docs/notion-bridge-bootstrap`
> Layer: `10-verification`
> Status: **AUTHORITY RECONCILIATION COMPLETE / CORE SIX READY FOR MATERIALIZATION**

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

All 10-07 through 10-12 source pages remain Freeze Candidate. 08 uses its own `Accepted / Current Direction / Proposal / Superseded / OPEN` status vocabulary. This closure does not promote either layer.

## 3. Core six contract inventory

| Contract | Primary source owner | Source completeness | 08 dependency | MR-10-04 verdict |
|---|---|---|---|---|
| `platform-suite` | 10-09 correction | format/id/version + ordered scenario membership + runner/scenario format requirements defined | none beyond referenced scenario authority | **READY** |
| `platform-scenario` | 10-08, corrected by 10-09 | field-level target/precondition/step/expected/capture model defined | references 08 requirement/invariant authority; does not choose physical realization | **READY** |
| `platform-profile` | 10-08 | family/variant/capability/realization shape defined | physical realization values deliberately remain profile-local / OPEN metadata | **READY** |
| `platform-trace` | 10-08, event correction in 10-09 | envelope, trace kind, global eventSeq, event/generation/correlation semantics defined | normalized observables derive from 08 lifecycle/ownership; exact OS callback/API excluded | **READY** |
| `platform-observation` | 10-08 | fact-only profile/scenario execution, steps, artifacts, semantic/state checkpoints, target binding, realization, diagnostics defined | observed implementation metadata may differ while 08 is OPEN | **READY** |
| `platform-result` | 10-08 | result statuses, participants/checks/open observations/divergence and deterministic comparison order defined | evaluates scenario authority; must not convert OPEN realization into winner | **READY** |

No unresolved 08 physical decision prevents structural materialization of the six core schemas.

## 4. Why 08 OPEN decisions do not block schema materialization

08 explicitly separates stable semantic/ownership/lifecycle meaning from unresolved platform realization.

Examples of still-OPEN physical decisions include:

- Windows DComp/DXGI/surface composition profile;
- Android SurfaceView/TextureView/SurfaceControl and GPU backend;
- Apple exact CAMetalLayer hierarchy;
- Web main-realm vs Worker / future backend;
- exact native input APIs/executors;
- ordered-runtime/render/worker thread topology;
- special-host process topology;
- PlatformSurfaceAdapter exact descriptor ABI.

MR-10-04 does not need to choose any of them. 10-08 already provides two explicit non-authoritative channels:

1. `PlatformProfile.realization` — observed/profile metadata;
2. `openObservations[]` — explicit P3 realization requests.

Those values are not blocking cross-platform expected equality. A Spec/Freeze scenario may PASS while different profiles report different OPEN realization metadata.

Therefore materializing a schema for the **container and vocabulary** is not equivalent to accepting a physical implementation choice.

## 5. 08 semantics that *are* valid schema inputs

The following inputs are sufficiently owned upstream to be represented by Verification:

- Web/Windows/Android/Apple family roles;
- one canonical semantic authority and canonical surface ownership by Axiom;
- optional Arc preview ownership separation;
- Host attachment and Document attachment orthogonality;
- App / Canvas / Document / Surface / GPU lifecycle separation;
- background/suspend != destroy;
- resize/DPI/orientation do not mutate Semantic Document;
- surface/device loss preserves semantic continuity and rebuilds derived state;
- stale generation must not publish valid presentation;
- bridge/input observable semantics are compared independently of physical JNI/JSI/WASM/ObjC++ implementation;
- OPEN/Current Direction requirements remain explicitly non-promoted through requirement status and observation channels.

Where upstream status is Current Direction rather than Accepted, 10-07 maps resulting parity evidence to Freeze Candidate rather than silently promoting it to Spec Requirement.

## 6. Source evolution / correction precedence

10-08 establishes the six-contract set and field-level model.

10-09 is later and provides three corrections that MR-10-04 must apply:

- independent `platform-suite.schema.json` with `axiom-platform-suite-v1` manifest;
- `PREVIEW_CLEAR_REQUESTED` and `PREVIEW_CLEARED` normalized events for Arc handoff proof;
- seed-required capability IDs for state/ownership/stale-generation/completion/source/fence/source-attempt evidence.

These are refinements to the same machine contract, not a conflict.

## 7. Core six vs harness/protocol schemas

MR-10-04 is intentionally bounded to the six platform evidence contracts.

10-10 / 10-11 introduce a second set for the Shared Runner execution protocol:

```text
platform-harness-envelope
platform-harness-session
platform-fault-hook
platform-late-event-fence
platform-protocol-suite
platform-protocol-vector
platform-protocol-meta-result
```

Those schemas prove the harness itself and support the 56-vector protocol trusted root. They are **not** aliases for the six Platform Scenario/Observation contracts and must not be merged into them.

They belong the subsequent platform corpus/harness materialization closure (currently ledgered after MR-10-04).

## 8. Machine contract rules to preserve during materialization

All six core schemas must preserve:

- JSON Schema Draft 2020-12;
- fixed `format` + `formatVersion = 1`;
- top-level `additionalProperties: false`;
- exact tagged u64 generation/event sequence representation;
- safe root-relative POSIX artifact paths;
- adapter observation vs coordinator judgment separation;
- no platform-specific expected semantic copies;
- no arbitrary sleep as correctness synchronization;
- one global normalized `eventSeq` namespace per PlatformObservation;
- scenario-declared partial order instead of full trace total-order equality;
- opaque Verification `bindingTag`, never native handles;
- OPEN scenario cannot produce semantic PASS;
- profile realization metadata cannot silently become correctness oracle.

## 9. Required semantic validation beyond JSON Schema

Source authority explicitly requires a second validation layer. Structural JSON Schema alone cannot prove:

- scenario/step/checkpoint/requirement reference uniqueness;
- referenced semantic fixture existence;
- suite scenario existence and format compatibility;
- event-specific required fields;
- global eventSeq uniqueness and monotonicity across trace artifacts;
- requested capture completeness;
- generation semantic assertions;
- participant capability policy;
- non-empty Spec/Freeze correctness oracle;
- OPEN scenario result restrictions;
- safe bindingTag policy;
- referenced semantic projection schema + IDL-aware validity.

Materialization should therefore preserve a future API boundary equivalent to:

```text
validatePlatformScenarioStructure / Semantics
validatePlatformProfileStructure / Semantics
validatePlatformTraceStructure / Semantics
validatePlatformObservationStructure / Semantics
validatePlatformResultStructure / Semantics
```

## 10. Source-required meta-contract tests

The source already names a minimum machine-contract test set, including:

```text
META-PLATFORM-SCENARIO-UNKNOWN-FIELD-REJECT
META-PLATFORM-SCENARIO-DUPLICATE-STEP-ID-REJECT
META-PLATFORM-SCENARIO-MISSING-FIXTURE-REJECT
META-PLATFORM-SCENARIO-SPEC-WITHOUT-ORACLE-REJECT
META-PLATFORM-TRACE-DUPLICATE-EVENTSEQ-REJECT
META-PLATFORM-TRACE-NONMONOTONIC-EVENTSEQ-REJECT
META-PLATFORM-TRACE-SURFACE-BOUND-WITHOUT-GENERATION-REJECT
META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL
META-PLATFORM-OBS-MISSING-REQUESTED-TRACE-REJECT
META-PLATFORM-OBS-REAL-HANDLE-IN-BINDINGTAG-REJECT-BY-POLICY
META-PLATFORM-RESULT-PASS-WITH-DIVERGENCE-REJECT
META-PLATFORM-RESULT-OPEN-SCENARIO-PASS-REJECT
META-PLATFORM-RESULT-REQUIRED-NOTCAPABLE-WITHOUT-FAIL-REJECT
```

These are tooling self-tests and do not count toward the 28 stable platform scenarios.

## 11. Explicitly out of MR-10-04

Still OPEN/downstream:

- full `platform-run.json` environment/device-farm schema;
- concrete browser/Windows SKU/Android OEM/Apple device support matrix;
- numeric timeout policy;
- CI/device farm provider orchestration;
- physical Surface/GPU/backend/thread/process winner;
- pixel bit-exact rendering;
- 28 scenario file materialization;
- four real platform adapters;
- 56 protocol vectors and reference protocol runner;
- deterministic fault implementation hooks.

## 12. Exit from authority-reconciliation phase

**PASS.** The six core Platform Machine Contracts are source-defined sufficiently to materialize without inventing platform architecture.

Next MR-10-04 substep:

```text
Authority snapshots reconciled
    ↓
materialize six Draft 2020-12 schemas
    ↓
add structural + semantic meta-tests
    ↓
CI lock
    ↓
MR-10-04 CLOSED
```

Any implementation decision that would enum-freeze an 08 OPEN physical winner is a blocker and must return to 08 rather than be invented in Verification.
