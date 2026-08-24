# Golden Corpus Seed Set + Conformance CLI Skeleton v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81beadaeec834eb54fbd
> Source page id: `3c44c57a-590c-81be-adae-ec834eb54fbd`
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Seed Corpus + Tooling Scaffold
> MR-10-03 note: CLI/diagnostic first-divergence details expanded from source; no source status promotion.

## Goal

```text
clean checkout
  ↓
build C++ adapter + WASM
  ↓
validate corpus metadata
  ↓
run seed-v0.1
  ↓
collect ImplementationObservation
  ↓
compare with golden
  ↓
PASS / FAIL / BLOCKED_OPEN
  ↓
if fail: deterministic first-divergence report
```

The seed is a bootstrapping verification suite, not a replacement for the full semantic corpus. Coordinator implementation language is not semantic authority.

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

## Coordinator CLI contract

The source contract separates validation, execution, comparison and diagnosis. Logical commands include:

```text
axiom-conformance validate-corpus ...
axiom-conformance list ...
axiom-conformance run ...
axiom-conformance compare --run <run-dir>
axiom-conformance diagnose \
  --case <case-id> \
  --impl cpp \
  --impl wasm \
  --first-divergence
```

Recommended selection/diagnostic flags include:

```text
--category <prefix>
--status SPEC_REQUIREMENT|FREEZE_CANDIDATE|...
--capability <id>
--stop-after-operation <N>
--output <path>
--fail-fast
```

### `validate-corpus`

Validates corpus artifacts only: JSON Schema, case-ID uniqueness, path safety, artifact existence, suite references, OPEN expected restrictions, status vocabulary and required-capability syntax. It does not launch Axiom.

### `run`

Discovers cases, queries adapter capabilities, dispatches work, collects observations, compares and writes results. Default behavior is not fail-fast so CI can retain a complete divergence set.

### `compare`

Re-compares already produced observations. This permits comparator/reporting corrections without forcing expensive replay to execute again, provided the original observations remain valid evidence.

### `diagnose`

Produces detailed evidence for one divergent case. Replay diagnosis uses checkpoint narrowing plus first-divergence bisection rather than reporting only a final mismatch.

For replay localization:

```text
coarse checkpoints mismatch
        ↓
locate first mismatching interval [L, R]
        ↓
run adapter --stop-after-operation M
        ↓
compare projection
        ↓
binary search first mismatching Operation
        ↓
produce detailed stage/projection/byte evidence
```

`--stop-after-operation N` preserves normal replay semantics for `[0..N]`; it is a verification diagnostic boundary, not a new semantic operation or transaction concept.

## First-divergence self-test requirement

The coordinator/tooling bootstrap is not considered trustworthy merely because it can emit PASS/FAIL. It must prove that known divergent evidence is localized deterministically. At minimum the tooling contract expects:

- stable first mismatch stage for a single-Operation/vector case;
- stable first differing Operation index for replay;
- deterministic repeat gives the same divergence location;
- projection mismatch can reference semantic path when available;
- canonical-byte mismatch can reference byte offset when available;
- OPEN cross-implementation divergence remains observation and never selects a winner.

The detailed current field shape of `DivergenceRecord` is owned by `semantic-projection-result-schema-v0.1.md`; this page owns CLI/diagnostic behavior.

## CI bootstrap

The intended first semantic gate builds required adapters, validates schemas/corpus, runs exactly `seed-v0.1`, emits machine-readable observations/results and verifies deterministic repeat. C++ native + WASM are REQUIRED; TS participates according to declared capabilities.

MR-10-03 does **not** require authority migration itself to finish all 60 executable cases or materialize every adapter. The migration requirement is that the GitHub-local CLI/diagnostic contract is complete enough that a later Codex implementation package does not have to reconstruct it from Notion or chat history.

## Golden protection

Blocking CLI has no `--bless`, `--update-golden` or accept-current-output path. Candidate capture, if implemented, writes outside the stable golden root and remains non-authoritative until governed review/promotion.

## Important source evolution note

This seed document records the source page's 60 semantic seed cases. The later Platform Harness authority separately defines 56 protocol meta-vectors and 28 platform scenarios. These counts are distinct and must not be conflated.
