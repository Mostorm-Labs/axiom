# 10 Verification — Authority Index

> Source: Notion `10 Verification`
> Source page: https://app.notion.com/p/3c44c57a590c8029ab4fc016e29c4156
> Snapshot date: 2026-08-24

00–09 define what Axiom should be; 10 defines what evidence is sufficient to prove those contracts.

## Authority set

- Verification Strategy + Verification Matrix v0.1
- Semantic Conformance + Golden Corpus v0.1
- Cross-language Conformance Runner + Golden Vector File Format v0.1
- Golden Corpus Seed Set + Conformance CLI Skeleton v0.1
- Semantic Projection Schema + Conformance Result JSON Schema v0.1
- Golden Corpus Authoring Rules + Fixture Generation Pipeline v0.1
- Conformance CI Gates + Corpus Governance v0.1
- Cross-platform Parity + Platform Conformance Matrix v0.1
- Platform Scenario File Format + PlatformObservation JSON Schema v0.1
- Platform Scenario Seed Set + Harness Adapter Skeleton v0.1
- Platform Harness Execution Protocol + Fault Hook Contract v0.1
- Platform Harness Reference Runner + Protocol Test Vectors v0.1
- Platform Harness Implementation Plan + CI Job Wiring v0.1
- Implementation Backlog / Issue Pack v0.1
- Implementation Verification Design — Evidence-Gated Vertical Build + G0–G9 v0.1

## Verification principles

- Golden expected is read-only to implementation-under-test; blocking CI has no auto-bless path.
- Verification-only hooks do not enter Product public ABI and production targets must not depend on verification targets.
- Protocol seed must become trustworthy before platform seed is trusted.
- Unsupported platform scenarios are explicit SKIP/UNSUPPORTED, never false PASS.
- First divergence and deterministic repeat are machine-readable evidence requirements.
- Test harnesses prove runtime authority; they do not redefine it.

## 07 final-closure intake

Priority evidence includes semantic apply/idempotency, identity namespace separation, Incremental vs Full RuntimeScene equivalence, persist-first crash windows, LocalRecoveryClosure, external no-echo, missing resource closure, active-session conflict framework, PresentedFeedback/stale-generation/canonical-coverage behavior, surface/device/background late-event rejection and performance candidates.

PlatformQualified presentation proof must ensure PresentSubmitted, Approximate, stale SurfaceGeneration/MetricsGeneration or stale coverage cannot emit CanonicalVisible(token).
