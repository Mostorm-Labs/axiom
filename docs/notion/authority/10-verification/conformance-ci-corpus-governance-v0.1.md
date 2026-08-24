# Conformance CI Gates + Corpus Governance v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81e0be88edb90845e05a
> Source page id: `3c44c57a-590c-81e0-be88-edb90845e05a`
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Conformance CI / Corpus Governance Contract
> MR-10-03 note: evidence-retention rules expanded from source; no source status promotion.

## Three-layer decision model

Requirement status → ConformanceResult per case → Gate Aggregator → GateDecision → PR/Nightly/Release workflow. Requirement status, observation result and CI gate decision are distinct.

GateDecision v0.1: `PASS`, `PASS_WITH_OBSERVATIONS`, `FAIL`, `INVALID_EVIDENCE`, `BLOCKED_AUTHORITY`. PASS/PASS_WITH_OBSERVATIONS may map to CI success; the others block the relevant gate.

OPEN/Experimental can be non-blocking observations, but broken corpus/schema/provenance/harness is never excused by non-blocking requirement status.

## CI tiers

- PR `conformance/pr-seed`: exactly semantic `seed-v0.1` 60 cases.
- Nightly `conformance/nightly-full`: complete active semantic conformance corpus.
- Release `conformance/release`: release-scope G0/G1 correctness evidence for a fixed revision.

Any PR affecting semantic core, IDL/codec, Common Wire, Operations, OrderKey, verification tooling/schema or corpus runs the full seed; path skip is only for clearly unrelated monorepo paths and must be explicit.

PR order: schema validation → corpus metadata → provenance/reproducibility → build C++ → build WASM → discover TS capability → run seed → compare → aggregate → assert golden unmodified → retain required evidence.

## Requirement status behavior

Spec Requirement selected = blocking. Freeze Candidate in seed/nightly/release scope = blocking correctness evidence. Benchmark Target routes to measurement/regression, not semantic gate. Experimental is observation-only by default. OPEN has no semantic winner; release becomes BLOCKED_AUTHORITY only when release scope requires that policy closed.

## Nightly

Runs all active Spec/Freeze Candidate/OPEN/semantic Experimental cases, full Operation families, malformed/limits, long replay/checkpoints, stable fuzz regression, C++/WASM parity and TS declared capabilities. Random fuzz discoveries become lasting regression only after candidate→review→stable vector promotion.

## Release

Clean checkout + checked-in corpus. Re-runs all release-scope Spec/Freeze Candidate cases. OPEN/Experimental remain observations unless release explicitly requires closure. Release job cannot choose missing policy.

## Aggregation

Validate in order:

```text
A. corpus / schema / provenance validity
B. required implementation / capability availability
C. blocking Spec Requirement results
D. blocking Freeze Candidate results
E. OPEN observations
F. Experimental observations
G. Benchmark / measurement references
```

Invalid evidence takes precedence over apparent pass. All implementations agreeing against golden remains a golden mismatch failure.

## Failure / observation evidence retention

A conformance failure is not sufficiently evidenced by a red CI badge or a textual stack trace. The source authority requires retaining the evidence needed to reproduce and localize the result. An evidence bundle may contain:

```text
run.json
execution-plan.json
summary / GateDecision
ConformanceResult for failing cases
ImplementationObservation for participating implementations
DivergenceRecord / first-divergence output
referenced projection / canonical bytes / checkpoint artifacts
adapter stdout/stderr or structured diagnostics
build + compiler/runtime + git commit metadata
corpusId / corpus version / case ID / provenance reference
```

The stable machine-readable `DivergenceRecord` is therefore part of gate evidence, not optional reporter decoration.

For long replay, retaining only final projection mismatch is insufficient when first-divergence localization has run. The evidence set should retain the relevant coarse checkpoint interval and/or rerun artifacts required to explain the reported first differing Operation.

OPEN/Experimental behavior drift that is important enough to surface must retain enough evidence to reproduce its first divergence. It remains non-blocking according to requirement status, but evidence infrastructure failure is still `INVALID_EVIDENCE`.

## Deterministic repeat

Where a gate claims deterministic first divergence, repeated execution against the same corpus/input/revision must produce the same stable divergence location fields. Reporter prose, stack traces or timing metadata may vary and are not part of semantic authority.

A mismatch in stable first-divergence location for identical deterministic replay evidence is a verification-tooling defect until explained; it must not be silently accepted as implementation nondeterminism.

## Golden Change Gate

Any change to golden, schemas, fixture-authoring, suite membership, status, gate policy or generator triggers special review. Report changed Case IDs, ownership class, old/new hashes/length, authority refs/status/suite membership and legal reason; independently regenerate derived artifacts; byte-diff checked-in output; prove runner did not write golden; run seed + affected full cases; emit human-reviewable report.

No blocking CI `--bless`/`--update-golden` path.

## Platform dependency

Platform evidence has its own protocol trusted-root requirement: the 56 platform-harness protocol vectors must pass before real platform scenario evidence can be considered trustworthy. This is separate from the 60 semantic seed corpus.

## MR-10-03 ownership note

This page owns evidence lifecycle and CI retention policy. It does not own the current `DivergenceRecord` field schema or replay bisection algorithm; those remain in 10-04 and 10-02/10-03 respectively. MR-10-03 closes those sources together rather than duplicating them into a new semantic authority.
