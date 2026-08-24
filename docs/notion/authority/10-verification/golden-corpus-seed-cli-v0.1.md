# Golden Corpus Seed Set + Conformance CLI Skeleton v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81beadaeec834eb54fbd
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Seed Corpus + Tooling Scaffold

## Goal

Clean checkout → build C++ adapter + WASM → validate corpus metadata → run seed → collect ImplementationObservation → compare golden → PASS/FAIL/BLOCKED_OPEN → deterministic first-divergence report.

Logical responsibilities are golden, schemas, coordinator, adapters and output. TS/Node is an Experimental Target for v0.1 coordinator implementation, not semantic authority.

## Suite policy

`seed-v0.1` has stable ordered case membership and participant policy: C++ native REQUIRED; WASM REQUIRED; TS REQUIRED_WHEN_CAPABLE. Once TS declares a required capability, its divergence cannot be ignored. No `OPTIONAL_BUT_IGNORE_FAILURE` policy exists.

## Seed set — exactly 60 cases

- A Common Wire: 16
- B Geometry + integrated leaf schema: 8
- C OrderKey: 9
- D Canonical Codec: 5
- E 15 Operation Smoke: 15
- F Operation Contract Stress: 5
- G Replay: 2

Total = 60.

### Group A

Stable cases cover valid/short/long/zero/order Id128; finite/-0/NaN/+Inf/-Inf f64; valid/invalid UTF-8; absent-vs-default; clear-vs-set-default; PropertyBag canonical sort; duplicate canonical key rejection.

### Group B

Covers identity transform, transform composition (`T_world = T_parent × T_local`), singular transform reject, F32 PropertyValue opacity round-trip, NoFill vs transparent SolidFill inequality, EraseMask canonical-set ordering, RichText ordered delta steps and invalid scalar offset rejection.

### Group C

Covers OrderKey 1/32-byte valid boundaries, empty/33/trailing-zero invalidity, comparator relations, ObjectId tie-break, allocation boundaries and explicit rebalance requirement near exhaustion.

### Group D

Five exact canonical protobuf cases protect canonical field/tag/order/presence/numeric behavior against implementation drift.

### Group E

One valid canonical apply smoke path for each of the 15 Operation families.

### Group F

Five high-risk operation-contract cases cover atomic rejection, idempotent same-id/same-payload, same-id/different-payload collision, hierarchy cycle and an intentionally OPEN policy observation where applicable to the source snapshot. Later authority closure must update the OPEN case through governance rather than silently treating current implementation as truth.

### Group G

Two replay cases: mixed operation replay and duplicate replay/idempotency.

## CLI contract

Coordinator supports corpus/suite validation, listing, execution, comparison and deterministic divergence reporting. Blocking CLI has no `--bless`, `--update-golden` or accept-current-output path. Candidate capture, if present, writes outside stable golden root.

## CI bootstrap

First gate builds required adapters, validates schemas/corpus, runs exactly seed-v0.1, emits machine-readable observations/results and verifies deterministic repeat. C++ native + WASM must pass; TS participates according to declared capabilities.

## Important source evolution note

This seed document records the source page's 60 semantic seed cases. The later Platform Harness authority separately defines 56 protocol meta-vectors and 28 platform scenarios. These counts are distinct and must not be conflated.
