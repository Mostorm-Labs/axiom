# MR-10-03 First-Divergence Result Lock Audit v0.1

> Audit date: 2026-08-24
> Branch: `docs/notion-bridge-bootstrap`
> Layer: `10-verification`
> Status: **AUTHORITY MIGRATION + MACHINE SCHEMA LOCK MATERIALIZED / CI EVIDENCE PENDING**

## 1. Purpose

MR-10-03 closes a verification-authority gap, not a runtime implementation feature.

The goal is that a clean Codex session can recover the exact first-divergence contract from GitHub without reconstructing it from Notion or chat history:

```text
observed evidence
    ↓
coordinator comparison
    ↓
deterministic first meaningful mismatch
    ↓
DivergenceRecord v1
```

This audit does **not** authorize a new semantic equality model, a new Operation model, or a new runtime owner.

## 2. Source authorities

MR-10-03 reconciles four existing Notion Freeze Candidate pages:

1. `Cross-language Conformance Runner + Golden Vector File Format v0.1`
   - page id `3c44c57a-590c-81a8-9ac2-d3dc36f16787`
   - owner: runner protocol, replay checkpoint narrowing, `--stop-after-operation`, bisection behavior.
2. `Golden Corpus Seed Set + Conformance CLI Skeleton v0.1`
   - page id `3c44c57a-590c-81be-adae-ec834eb54fbd`
   - owner: coordinator CLI, `diagnose --first-divergence`, deterministic bootstrap/self-test expectation.
3. `Semantic Projection Schema + Conformance Result JSON Schema v0.1`
   - page id `3c44c57a-590c-8165-a197-c6f50e1434d3`
   - owner: current `DivergenceRecord v1`, comparison basis, location fields, semantic path grammar and fixed comparison order.
4. `Conformance CI Gates + Corpus Governance v0.1`
   - page id `3c44c57a-590c-81e0-be88-edb90845e05a`
   - owner: first-divergence evidence retention and gate-evidence lifecycle.

All four remain **Freeze Candidate / proposed-freeze**. This migration does not promote them to Frozen/Accepted.

## 3. Source evolution / supersession reconciliation

The older runner authority contains an early `FirstDivergence` example using:

```text
expectedArtifact
actualArtifacts{implementationId → path}
```

The later machine-readable 10-04 authority explicitly replaces that diagnostic shape with:

```text
basis
reference?
observed[]
```

Reason: OPEN / Experimental cross-implementation comparison may have no golden semantic winner. Choosing one implementation as `expected` would fabricate authority.

Therefore the GitHub precedence rule is:

- 10-02 remains authoritative for **runner/replay localization behavior**;
- 10-03 remains authoritative for **CLI diagnosis behavior**;
- 10-04 is authoritative for the **current DivergenceRecord field shape and comparator ordering**;
- 10-06 remains authoritative for **CI evidence retention**.

This is source evolution, not an architecture conflict.

## 4. Repo-local authority migration completed in this closure

Expanded snapshots:

- `docs/notion/authority/10-verification/cross-language-runner-golden-vector-format-v0.1.md`
- `docs/notion/authority/10-verification/golden-corpus-seed-cli-v0.1.md`
- `docs/notion/authority/10-verification/semantic-projection-result-schema-v0.1.md`
- `docs/notion/authority/10-verification/conformance-ci-corpus-governance-v0.1.md`

The GitHub snapshots now preserve the implementation-relevant first-divergence details rather than only a high-level summary.

## 5. Frozen-candidate first-divergence order

For result localization, the source order is:

```text
0 Harness / corpus validity
1 Capability availability
2 Terminal stage
3 Accepted vs Rejected outcome
4 Semantic error category when upstream authority specifies it
5 Stage projection when captured
6 Final semantic projection
7 Canonical protobuf bytes when required
8 Replay checkpoints
```

For a single value / Operation, semantic stage order is:

```text
DECODE → NORMALIZE → VALIDATE → APPLY → PROJECTION → ENCODE
```

A later implementation must not reorder these checks merely because another ordering is easier to code.

## 6. Replay localization contract

Short replay with `EVERY_OPERATION` checkpoints uses the first differing checkpoint as the first differing Operation index.

Long replay uses:

```text
coarse checkpoint comparison
    ↓
first mismatching interval [L, R]
    ↓
re-run prefix with --stop-after-operation M
    ↓
binary search
    ↓
first mismatching Operation
    ↓
detailed stage / projection / byte evidence
```

The source pseudocode is equivalent to:

```text
lo = last_matching_checkpoint + 1
hi = first_mismatching_checkpoint
while lo < hi:
    mid = floor((lo + hi) / 2)
    replay prefix through mid
    if compared projection agrees with golden/reference at mid:
        lo = mid + 1
    else:
        hi = mid
first = lo
```

Deterministic replay is a prerequisite. The diagnostic path may not reorder, merge, skip or reinterpret canonical Operations.

If final projection differs but coarse checkpoints are insufficient to locate the concrete first operation, `diagnose --first-divergence` must re-run localization. A report containing only `final mismatch` does not satisfy the source contract.

## 7. DivergenceRecord v1 lock

Current kinds:

- `TERMINAL_STAGE`
- `OUTCOME`
- `SEMANTIC_ERROR_CATEGORY`
- `STAGE_PROJECTION`
- `SEMANTIC_PROJECTION`
- `CANONICAL_BYTES`
- `REPLAY_CHECKPOINT`
- `CAPABILITY`
- `IMPLEMENTATION_ERROR`

Comparison basis:

- `GOLDEN`
- `CROSS_IMPLEMENTATION`

Rules:

- GOLDEN requires a golden `reference`;
- CROSS_IMPLEMENTATION forbids synthetic reference and requires at least two observed implementations;
- `operationIndex` and `operationId` are an atomic location pair;
- semantic projection location uses verification semantic path;
- canonical-byte location may include zero-based `byteOffset`;
- diagnostics text does not replace stable machine fields.

Semantic path grammar is repo-local authority:

```text
$                         root projection value
.field                    struct field
[N]                       OrderedSequence index
[key=value]               CanonicalSet / CanonicalMap selector
```

It is verification-only and must never expose host-language pointer/address/generated-field-index details.

## 8. Machine-readable materialization

`verification/schemas/result.schema.json` now locks the structural portion of the current source contract:

- GOLDEN reference requirement;
- CROSS_IMPLEMENTATION reference prohibition and `observed[] >= 2`;
- paired `operationIndex + operationId`;
- verification semantic-path structural grammar;
- `byteOffset` only on `CANONICAL_BYTES` divergence;
- semanticPath only on projection/checkpoint divergence kinds;
- existing PASS / OPEN / golden-result invariants remain in force.

`verification/conformance/coordinator/test_semantic_artifact_contracts.py` contains MR-10-03 schema meta-tests. These tests validate the machine contract; they are not the full future first-divergence comparator engine.

## 9. MR-10-01 implementation erratum discovered during MR-10-03

Re-reading the full 10-04 source authority exposed a pre-existing implementation drift in the MR-10-01 projection validator. The authority already required:

```text
u64 / fixed64       u64: + exactly 16 lowercase hex digits
arbitrary bytes     hex: + even-count lowercase hex
OrderKey            hex: + even-count lowercase hex
Id128               id128: + 32 lowercase hex digits
```

The earlier Python implementation instead accepted decimal `u64:` payloads and used `bytes:` / `orderkey:` prefixes. This was an implementation defect, not an authority ambiguity and not a 04 Semantic Schema change.

The drift was verified locally before correction:

```text
authority_u64_hex=False
legacy_u64_decimal=True
authority_orderkey_hex=False
legacy_orderkey_prefix=True
authority_bytes_hex=False
```

Regression tests were added in commit `af4b3045bccf9fcbcc76065bbe27a4bd6d02f3de`. The validator was corrected in commit `66d0ee150ed2074d39cccafdba92f2fc0bf207ff` to use the source-owned forms. The same exact representation checks then produced:

```text
authority_u64_hex=True
legacy_u64_decimal=False
authority_orderkey_hex=True
legacy_orderkey_prefix=False
authority_bytes_hex=True
```

The 10-04 Notion authority page also records this erratum. MR-10-01 remains closed as a closure phase, with this correction appended to its implementation evidence history; the source authority status remains Freeze Candidate.

## 10. Explicit non-goals / deferred implementation

MR-10-03 authority migration does **not** require, at this stage:

- completing all 60 semantic seed fixtures;
- implementing C++ / WASM / TS semantic adapters;
- implementing production-quality replay bisection execution;
- implementing recursive semantic diff over every Axiom type;
- executing G0–G9 implementation gates;
- redefining semantic equality to fit verification tooling.

Those are implementation-package concerns after authority closure.

## 11. Exit criteria

Authority-side criteria are met:

- source owner pages identified;
- source statuses preserved;
- old/new FirstDivergence shape supersession is explicit;
- comparator order is repo-local;
- replay bisection contract is repo-local;
- CLI diagnostic contract is repo-local;
- CI evidence-retention contract is repo-local;
- result schema reflects the source field constraints;
- verification-tooling self-test IDs are repo-local;
- the projection-tag implementation drift found during reconciliation is corrected and documented.

The only remaining MR-10-03 exit evidence is a successful CI run covering the current result schema, meta-tests and projection-tag erratum. Until that evidence is available the status remains:

**AUTHORITY MIGRATION + MACHINE SCHEMA LOCK MATERIALIZED / CI EVIDENCE PENDING**.
