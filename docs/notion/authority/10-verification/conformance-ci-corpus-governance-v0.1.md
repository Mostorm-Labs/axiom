# Conformance CI Gates + Corpus Governance v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81e0be88edb90845e05a
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Conformance CI / Corpus Governance Contract

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

Validate in order: corpus/schema/provenance; required implementation/capability availability; blocking Spec; blocking Freeze Candidate; OPEN observations; Experimental observations; benchmark references. Invalid evidence takes precedence over apparent pass. All implementations agreeing against golden is still golden mismatch failure.

## Golden Change Gate

Any change to golden, schemas, fixture-authoring, suite membership, status, gate policy or generator triggers special review. Report changed Case IDs, ownership class, old/new hashes/length, authority refs/status/suite membership and legal reason; independently regenerate derived artifacts; byte-diff checked-in output; prove runner did not write golden; run seed + affected full cases; emit human-reviewable report.

No blocking CI `--bless`/`--update-golden` path.

## Platform dependency

Platform evidence has its own protocol trusted-root requirement: the 56 platform-harness protocol vectors must pass before real platform scenario evidence can be considered trustworthy. This is separate from the 60 semantic seed corpus.
