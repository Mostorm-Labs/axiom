# Platform Scenario File Format + PlatformObservation JSON Schema v0.1

> Source page: https://app.notion.com/p/3c44c57a590c816c825ac93d1cc168a6
> Source page id: `3c44c57a-590c-816c-825a-c93d1cc168a6`
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Machine-readable G3 Platform Verification Contract
> Migration note: expanded during MR-10-04 to preserve the source field-level machine contract at implementation-useful fidelity.

## Scope / ownership

This page freezes **Verification artifact/tooling representation**, not Platform physical realization. Direct upstream owners include Cross-platform Parity, 08 Lifecycle, Surface/GPU, ABI/Bridge, Semantic Projection/Result and CI Governance.

Inherited boundaries remain strict:

- semantic golden remains single-source; platform scenarios reference canonical fixtures rather than creating per-platform semantic truth;
- platform adapters report observed facts only; coordinator/comparator owns PASS/FAIL;
- parity compares observable contract, not physical implementation identity;
- OPEN platform realization remains observation, never a schema-selected Windows/Android/Apple/Web winner;
- pixel bit-exactness is outside this contract;
- requirement status remains `SPEC_REQUIREMENT / FREEZE_CANDIDATE / BENCHMARK_TARGET / EXPERIMENTAL_TARGET / OPEN`.

## Core machine contract set v0.1

All top-level contracts use JSON Schema Draft 2020-12, fixed `format`, `formatVersion = 1`, and top-level `additionalProperties: false`.

Core schema set:

```text
verification/schemas/
├── platform-suite.schema.json
├── platform-scenario.schema.json
├── platform-profile.schema.json
├── platform-trace.schema.json
├── platform-observation.schema.json
└── platform-result.schema.json
```

URNs:

```text
urn:auditoryworks:axiom:verification:platform-suite:v1
urn:auditoryworks:axiom:verification:platform-scenario:v1
urn:auditoryworks:axiom:verification:platform-profile:v1
urn:auditoryworks:axiom:verification:platform-trace:v1
urn:auditoryworks:axiom:verification:platform-observation:v1
urn:auditoryworks:axiom:verification:platform-result:v1
```

Shared JSON rules inherit Semantic Verification: UTF-8 without BOM; checked-in JSON uses LF, two-space indentation and final newline; artifact paths are root-relative POSIX paths, never absolute and never `..` escaping.

`platform-suite` is completed by the later 10-09 source correction; see `platform-scenario-seed-adapter-v0.1.md`.

## Shared platform conventions

Platform families:

```text
WEB
WINDOWS
ANDROID
APPLE
```

`APPLE` is the G3 family; concrete iOS/iPadOS distinction is profile metadata. macOS is not silently promoted to a V1 required family.

Participant policy:

```text
REQUIRED
REQUIRED_WHEN_CAPABLE
```

There is no `OPTIONAL_BUT_IGNORE_FAILURE`.

Generation values use exact Verification u64 tagging:

```text
^u64:[0-9a-f]{16}$
```

V1 explicitly separates:

- `surfaceGeneration` — host-slot / presentable-target binding identity;
- `metricsGeneration` — normalized size/scale/orientation/visibility revision;
- `deviceGeneration` — GPU device/context recovery generation.

This disambiguates Verification trace vocabulary; it does not freeze the exact PlatformSurfaceAdapter ABI.

## PlatformScenario v1

Top-level logical shape:

```json
{
  "format": "axiom-platform-scenario-v1",
  "formatVersion": 1,
  "id": "PLAT-SURFACE-LOST-REBIND-001",
  "requirementStatus": "SPEC_REQUIREMENT",
  "requirementIds": ["PC-I05", "PC-I06"],
  "authorityRefs": ["<upstream-authority-url>"],
  "canonicalFixtureRef": "REPLAY-MIXED-OPERATIONS-001",
  "targets": [],
  "preconditions": {},
  "steps": [],
  "expected": {},
  "capture": {}
}
```

Required semantic roles:

- stable, non-reused scenario `id`;
- requirement status + unique requirement IDs;
- at least one upstream authority reference;
- at least one target policy;
- logical preconditions;
- ordered invocation steps;
- expected assertions/oracles;
- explicit capture policy.

`canonicalFixtureRef` is optional. When present it references an existing Semantic Golden case ID; platform corpus does not duplicate snapshot/opstream/expected projection.

## Target policy / capability requirements

A target binds `platformFamily`, `policy`, and `requiredCapabilities[]`.

Capability IDs are lowercase namespaced Verification/runtime capability names. Source-defined V1 capabilities include semantic projection capture; surface/metrics generation; surface/device fault injection; presentation feedback; Public Facade/DataBridge/callback trace; pointer batch; Arc preview; present-completion hold; state/ownership capture; stale-generation injection; completion token, source lease, late-event fence and source-attempt tracing.

A capability ID is not a Product feature flag. Unknown required capabilities are never silently treated as supported.

## Preconditions v1

Verification start-state axes are independent and do not require Product APIs to expose the same enums:

```text
appState:       FOREGROUND | BACKGROUND
canvasState:    NOT_CREATED | CREATED | MOUNTED | RUNNING | SUSPENDED | DESTROYED
hostState:      DETACHED | ATTACHED
documentState:  NONE | AVAILABLE | ATTACHED
surfaceState:   UNBOUND | BOUND | UNAVAILABLE | REBINDING
deviceState:    READY | LOST | RECOVERING
arcPreviewMode: DISABLED | ENABLED | REQUIRED_IF_CAPABLE
```

## ScenarioStep v1

Every step has a stable `stepId` and exactly one logical action variant.

Kinds:

```text
CONTROL
APP_LIFECYCLE
METRICS_UPDATE
SEMANTIC
BRIDGE
INPUT
FAULT
WAIT
```

The steps array defines **invocation order**, not total ordering of asynchronous platform events. Observable ordering is expressed through `partialOrder[]`.

CONTROL operations:

```text
CREATE_CANVAS
DESTROY_CANVAS
ATTACH_HOST
DETACH_HOST
PROVIDE_SURFACE_REBIND
ATTACH_DOCUMENT
DETACH_DOCUMENT
SUSPEND_CANVAS
RESUME_CANVAS
```

APP lifecycle actions are `BACKGROUND / FOREGROUND`.

METRICS_UPDATE carries normalized logical/physical size, deviceScale, orientation (`IDENTITY / ROTATE_90 / ROTATE_180 / ROTATE_270`), visibility and occlusion. Mirror/color-space/HDR are not silently frozen in V0.1 correctness schema.

SEMANTIC actions are fixture-referencing `RESTORE_CANONICAL_FIXTURE / APPLY_CANONICAL_FIXTURE / REPLAY_CANONICAL_FIXTURE`.

BRIDGE actions use logical `PUBLIC_FACADE / DATA_BRIDGE` contracts and logical operations; exact WASM/JSI/JNI/ObjC++ method names are excluded.

INPUT V1 logical action is `DELIVER_POINTER_SAMPLE_BATCH` using Verification fixtures conforming to the existing PointerSampleBatch meaning.

WAIT binds to a normalized EventSelector. `sleep(N)` / arbitrary wall-clock delay is not a correctness oracle.

## Step completion

Completion modes:

```text
DISPATCH_ONLY
WAIT_FOR_ACTION_COMPLETION
WAIT_FOR_EVENT
```

`DISPATCH_ONLY` supports non-blocking contract proof. `WAIT_FOR_EVENT` requires an EventSelector. Timeout is run policy/evidence, not semantic platform parity.

## Capture / checkpoint contract

Capture can request normalized lifecycle, surface, bridge and semantic evidence plus diagnostics/state/ownership checkpoints.

Checkpoint IDs are scenario-unique. A checkpoint is located by exactly one of step or event selector; schema provides structural exclusivity and semantic validation resolves references.

## PlatformTrace v1

Common envelope:

```json
{
  "format": "axiom-platform-trace-v1",
  "formatVersion": 1,
  "scenarioId": "PLAT-SURFACE-LOST-REBIND-001",
  "profileId": "android-reference",
  "traceKind": "SURFACE",
  "events": []
}
```

Trace kinds include lifecycle, surface and bridge normalized traces. All normalized events inside one PlatformObservation share one run-local exact-u64 `eventSeq` namespace.

`eventSeq` rules:

- globally unique within an observation;
- strict ascending inside each trace file;
- not a wall clock and not a Product sequence number;
- optional monotonic time is diagnostic/benchmark evidence and not default G3 equality.

Comparator checks scenario-declared required/forbidden/partial-order facts; it does not demand four platforms emit identical total event streams.

A PlatformTraceEvent records the stable normalized event plus relevant step/canvas/target/generation/correlation facts. Platform-private extension data belongs only in diagnostic `details`; comparator must not turn private details into hidden correctness authority.

Normalized event vocabulary derives from 08 observable semantics, not OS callback names. Important categories include canvas/host/document lifecycle, surface bound/unavailable/rebound, metrics changes, app foreground/background, suspend/resume, device loss/recovery, Arc degradation/recovery, facade/DataBridge issue/completion, local canonical publish, input batch delivery, canonical/preview submit/present, canonical-visible acknowledgement, stale-generation rejection and destroy/callback evidence.

The later 10-09 correction adds `PREVIEW_CLEAR_REQUESTED` and `PREVIEW_CLEARED` so Arc handoff ordering can be proven without changing production ABI.

## Event selectors / assertions

EventSelector selects normalized facts by trace kind, event, occurrence and typed match keys. ScenarioExpected supports:

- required events with count bounds;
- forbidden events;
- `HAPPENS_BEFORE` partial-order assertions;
- state / semantic / generation / ownership assertions;
- OPEN realization observation requests.

At least one oracle/assertion is required for a Spec/Freeze correctness scenario. An empty-oracle Spec Requirement scenario is invalid evidence.

## OPEN realization observation

Physical realization is captured only through explicit observation requests. Source keys include examples such as surface primitive, render backend, GPU device/queue sharing, bridge mechanism and execution topology.

These values do **not** enter blocking cross-platform equality. A Spec/Freeze scenario may PASS while profiles report different OPEN realization metadata; the G3 aggregator may surface `PASS_WITH_OBSERVATIONS`.

## PlatformProfile v1

Representative shape:

```json
{
  "format": "axiom-platform-profile-v1",
  "formatVersion": 1,
  "profileId": "android-reference",
  "platformFamily": "ANDROID",
  "platformVariant": "ANDROID_NORMAL_CLIENT",
  "capabilities": [
    "surface.generation",
    "surface.loss.inject",
    "semantic.projection.capture"
  ],
  "realization": {
    "renderBackend": "vulkan",
    "surfacePrimitive": "platform-private",
    "bridgeMechanism": ["JNI", "JSI"],
    "arcPreviewEnabled": false
  }
}
```

`realization` is observed/profile metadata, not expected oracle. Backend/primitive spelling may remain profile-local while 08 is OPEN; schema materialization must not enum-freeze an unaccepted physical winner.

## PlatformObservation v1

PlatformObservation is fact-only and cannot contain semantic PASS/FAIL judgment.

Top-level responsibilities include:

- format/version + scenario/profile/platform identity;
- execution metadata;
- terminal driver outcome;
- ordered step observations;
- requested artifact references;
- semantic checkpoints;
- state checkpoints;
- target bindings / ownership evidence;
- realization metadata;
- diagnostics.

Terminal driver outcomes include completed, not-supported, harness-error and implementation-error forms. `COMPLETED` only means the driver reached terminal execution; it does not mean conformance PASS.

Step outcomes similarly report observed completion/rejection/not-supported/error facts. Comparator decides whether an observed rejection is correct for the scenario.

Observation artifacts reference normalized trace/projection/capture files using safe relative paths.

Target binding evidence uses Verification-only opaque `bindingTag`; it must not contain a real pointer, HWND, ANativeWindow, CALayer, SkSurface or equivalent native handle.

## PlatformConformanceResult v1

Representative top-level shape:

```json
{
  "format": "axiom-platform-conformance-result-v1",
  "formatVersion": 1,
  "scenarioId": "PLAT-SURFACE-LOST-REBIND-001",
  "requirementStatus": "SPEC_REQUIREMENT",
  "result": "PASS",
  "participants": [],
  "checks": [],
  "openObservations": [],
  "divergence": null,
  "diagnostics": []
}
```

Result statuses:

```text
PASS
FAIL_SCENARIO_EXPECTATION
FAIL_CROSS_PLATFORM_DIVERGENCE
FAIL_CAPABILITY_MISSING
FAIL_IMPLEMENTATION_ERROR
BLOCKED_OPEN
OBSERVED_AGREEMENT_OPEN
OBSERVED_DIVERGENCE_OPEN
HARNESS_ERROR
```

Rules:

- PASS means all blocking assertions hold for required/participating profiles;
- scenario-authority mismatch and cross-platform required-equality mismatch remain distinct;
- REQUIRED capability absence is explicit failure;
- driver/runtime crash or malformed observation is implementation/harness evidence, not semantic mismatch;
- an OPEN scenario never becomes semantic PASS merely because implementations agree;
- openObservations on a Spec/Freeze scenario do not change per-scenario PASS; upper G3 aggregation decides whether to surface observations.

Every scenario oracle produces a machine-readable check record. Check status vocabulary is `PASS / FAIL / OBSERVED / NOT_APPLICABLE / BLOCKED_OPEN`.

## Deterministic comparison order

Platform result first-divergence ordering is source-defined, not an implementation preference:

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

Within a group, scenario file array order is authoritative. Cross-platform participants are ordered by `platformFamily + profileId` lexical order.

## Validation layering

A valid JSON object is not sufficient Platform evidence. Required tooling boundary:

```text
validatePlatformScenarioStructure(json)
validatePlatformScenarioSemantics(json, corpus, runnerProtocol)

validatePlatformProfileStructure(json)
validatePlatformProfileSemantics(json)

validatePlatformTraceStructure(json)
validatePlatformTraceSemantics(json, scenario, observation)

validatePlatformObservationStructure(json)
validatePlatformObservationSemantics(json, scenario, profile)

validatePlatformResultStructure(json)
validatePlatformResultSemantics(json, scenario, observations)
```

Semantic validation covers stable ID/reference uniqueness, fixture existence, event-specific required fields, global eventSeq uniqueness/monotonicity, requested capture completeness, generation legality/assertions, capability policy, OPEN scenario behavior, non-empty Spec oracle, path safety and referenced semantic projection validity.

## Meta-contract tests

These tooling self-tests are **not** part of the 28 platform scenario seed:

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

## What v0.1 explicitly does not freeze

Remain OPEN/downstream:

- complete `platform-run.json` device-farm/environment inventory schema;
- CI provider/device-farm orchestration;
- concrete browser / Windows SKU / Android OEM / Apple device matrix;
- numeric timeout policy;
- physical Surface primitive/backend/thread/process winner;
- pixel bit-exact render equality;
- Product/public platform ABI names corresponding to Verification-only event/state vocabulary.

## MR-10-04 authority boundary

MR-10-04 owns materialization of the **six core platform artifact schemas above**. It does not own the additional execution-protocol schema set introduced by 10-10/10-11 (`platform-harness-envelope/session/fault/fence` and protocol-suite/vector/meta-result); those belong the later harness/protocol materialization closure.

08 Platform Contract continues to own physical realization and its Accepted/Current Direction/Proposal/OPEN status. MR-10-04 may encode those values as profile/observation data, but may not turn them into blocking expected enums or select a winner.
