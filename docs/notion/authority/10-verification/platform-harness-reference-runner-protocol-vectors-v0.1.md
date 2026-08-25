# Platform Harness Reference Runner + Protocol Test Vectors v0.1

> Source page: https://app.notion.com/p/3c44c57a590c818d894cf7b73cd0f42a
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — G3 Harness Reference Runner / Protocol Meta-Conformance Contract

## Trusted-root rule

Shared Runner must pass its own `protocol-seed-v0.1` before real platform scenario evidence is trusted. Any blocking protocol meta-test failure makes platform evidence at least INVALID_EVIDENCE.

Protocol vectors use a Scripted Adapter capable of malformed receipts, duplicate completions, stale epochs, lease leaks, illegal fault transitions and fence races. No wall-clock sleep. IN_PROCESS and OUT_OF_PROCESS share oracle; OUT_OF_PROCESS minimum is real UTF-8 JSON serialize/deserialize portability boundary.

## Runner components

HarnessEnvelopeCodec, ProtocolSchemaValidator, AdapterHandshake, SessionRegistry, CommandSequencer, ActionRegistry, CompletionRegistry, EventRecorder, SourceLeaseRegistry, SourceAttemptRecorder, FaultCoordinator, LateEventFenceManager, WaitResolver, CaptureCoordinator, ObservationBuilder, ProtocolViolationCollector and ArtifactWriter. These ownership boundaries prevent components from swallowing violations.

Reference implementation language TS/Node is Experimental tooling choice.

## Handshake

HELLO precedes OPEN_SESSION and reports stable adapterInstanceId, supported protocol versions and actual boundary mode. Required protocol v1 must be supported. Connection reset requires new HELLO; old connection/session messages cannot cross adapter instance/epoch.

## Protocol vector

Stable fields: format/version/id/requirement status/authority refs/focus areas/boundary modes/setup/ordered steps/expected. Focus areas: envelope, session, action receipt, completion token, event draft, source lease/attempt, fault handle, late-event fence, boundary parity, finalization.

Step DSL uses DRIVER/ADAPTER/TRANSPORT/ASSERT actors and explicit connect/open/dispatch/arm fence/close/finalize, inject inbound, disconnect/reconnect, expect outbound/checkpoint operations. Typed selectors use stable protocol keys; no arbitrary sleep or fragile private-field JSONPath oracle.

## Expected/meta result

Expected terminal kinds: RUN_COMPLETED, PROTOCOL_REJECTED, CAPABILITY_FAILED, ADAPTER_FAILED, INVALID_EVIDENCE, plus registry/event/source-attempt/forbidden-outbound assertions.

Stable tooling-only ProtocolErrorCategory includes invalid envelope/message/version/session epoch/reuse/stale/closed, duplicate action/command mismatch, completion token errors, source lease/scope/leak/stale-forward, fault transition/pulse/hook failures, fence not armed/early/open leases/held completions/late forwarded/transport lost/registry incomplete, nonportable boundary payload, eventSeq ownership and unresolved registry.

ProtocolMetaResult records vector/runner/boundary/status/observed terminal/first divergence/artifacts. Meta status is PASS, FAIL_EXPECTATION_MISMATCH, INVALID_VECTOR or RUNNER_ERROR; it is not Product/Platform result status.

## Seed size

The source implementation plan fixes `protocol-seed-v0.1` at **56 stable protocol vectors**. All blocking vectors must pass before platform seed can be evidence.

## Golden governance

Protocol corpus is part of verification trusted root. Runner-under-test cannot bless/update expected. Protocol expected describes how runner interprets verification protocol, never Product semantic truth.
