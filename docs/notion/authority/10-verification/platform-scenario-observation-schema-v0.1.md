# Platform Scenario File Format + PlatformObservation JSON Schema v0.1

> Source page: https://app.notion.com/p/3c44c57a590c816c825ac93d1cc168a6
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Machine-readable G3 Platform Verification Contract

## Machine contract

JSON Schema Draft 2020-12; fixed format + formatVersion=1; top-level additionalProperties=false. Contracts: platform suite, scenario, profile, trace, observation and result. Semantic golden remains single-source; platform scenarios reference canonical fixtures rather than copying platform-specific semantic expected.

Platform families: WEB, WINDOWS, ANDROID, APPLE. Participant policy: REQUIRED / REQUIRED_WHEN_CAPABLE only. Generations use exact tagged u64 and are disambiguated as `surfaceGeneration`, `metricsGeneration`, `deviceGeneration`.

## PlatformScenario

Stable fields: id, requirementStatus, requirementIds, authorityRefs, optional canonicalFixtureRef, targets, preconditions, ordered steps, expected assertions and capture policy. Stable IDs are never reused.

Capability IDs are verification/runtime capability names such as semantic projection capture, surface/metrics generation, surface/device fault injection, presentation feedback, bridge traces, pointer batch, Arc preview, present-completion hold, stale-generation injection, completion/source/fence/source-attempt harness capabilities. Unknown required capability cannot be silently treated as supported.

## Preconditions

Logical start state covers app foreground/background, canvas not-created/created/mounted/running/suspended/destroyed, host detached/attached, document none/available/attached, surface unbound/bound/unavailable/rebinding, device ready/lost/recovering and Arc preview disabled/enabled/required-if-capable. These are verification states, not required Product enums.

## Steps

Each step has stable stepId and exactly one action variant. Kinds: CONTROL, APP_LIFECYCLE, METRICS_UPDATE, SEMANTIC, BRIDGE, INPUT, FAULT, WAIT. Array defines invocation order, not total ordering of async events; expected partial-order assertions define required event ordering.

Control covers create/destroy canvas, attach/detach host/document, surface rebind, suspend/resume. Semantic actions reference canonical fixture. Bridge actions address logical PUBLIC_FACADE or DATA_BRIDGE operations, never exact JNI/JSI/WASM/ObjC++ method names. Input delivers normalized pointer sample batch. WAIT binds to normalized event selector; arbitrary sleep is not correctness synchronization.

Completion modes: DISPATCH_ONLY, WAIT_FOR_ACTION_COMPLETION, WAIT_FOR_EVENT. Timeout is run policy, not semantic parity.

## Expected/capture

Expected may require semantic projection, events, partial order, forbidden events, state assertions and OPEN observations. Capture may request lifecycle/bridge/semantic/surface traces and diagnostics. Adapter does not read expected and does not decide PASS/FAIL.

## Observation/result

PlatformObservation contains profile/scenario/run identity, capability realization, normalized trace, semantic checkpoints, artifacts and diagnostics. Comparator produces PlatformConformanceResult against scenario authority. Physical backend metadata is observation, not equality requirement unless upstream explicitly says so.

## Presentation proof

Trace schema must preserve frame/canvas/surface/metrics correlation needed to reject stale PresentedFeedback. Qualified presentation evidence is separate from submission/approximate evidence; CanonicalVisible eligibility is evaluated against recorded frame coverage and live generations.
