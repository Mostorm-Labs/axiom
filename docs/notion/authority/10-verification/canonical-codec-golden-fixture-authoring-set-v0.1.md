# Canonical Codec Golden Fixture Authoring Set v0.1

> Source: Notion `Canonical Codec Golden Fixture Authoring Set v0.1`
> Source page: https://app.notion.com/p/3c84c57a590c8152b536fc23cd564b73
> Source page id: `3c84c57a-590c-8152-b536-fc23cd564b73`
> Snapshot date: 2026-08-26
> Source status: Current Verification Authority — BG/BGX Authoring Set / Independent Fixture Contract
> Repository status: current

This page translates the current Canonical Codec Golden Authority Closure into a repository-materializable authoring contract for `BG-001..BG-010` and `BG-N01..BG-N08`. It does not expand the larger BGX Operation/Object/Snapshot corpus and does not authorize GT-G1-03.

## Repository logical layout

```text
verification/corpus/semantic/v1/
├── corpus.json
└── wire/
    └── bg/
        ├── BG-001-id128/
        │   ├── case.json
        │   ├── authoring/input.projection.json
        │   ├── expected/canonical.pb
        │   └── provenance.json
        ├── ...
        ├── BG-010-minimal-document-snapshot/
        ├── BG-N01-vec2-field-order/
        │   ├── case.json
        │   ├── authoring/wire.recipe.json
        │   ├── input/value.pb
        │   ├── expected/canonical.pb
        │   └── provenance.json
        └── BG-N08-order-key-empty/
```

The BGX root may remain empty until separate BGX authority is promoted. GT-G1-02 seed closure must not invent or count unpromoted BGX cases.

## Case metadata contract

Each `case.json` contains at least:

```json
{
  "formatVersion": 1,
  "id": "BG-001",
  "status": "SPEC_REQUIREMENT",
  "authorityRefs": [
    "Canonical Codec Golden Authority Closure v0.1#BG-001"
  ],
  "entrypoint": "ENCODE",
  "requiredCapabilities": [
    "CANONICAL_ENCODE"
  ],
  "input": {},
  "expected": {},
  "blockedByOpenPolicy": false
}
```

Rules:

- `SPEC_REQUIREMENT` is requirement status, not execution PASS.
- Case IDs are stable.
- `authorityRefs` point only to current authority.
- Binary `expected/canonical.pb` is the byte truth; hex is review annotation only.
- Production output must never auto-refresh `expected`.

## Positive authoring matrix

| ID | Human-authored source | Derived artifact | Expected |
|---|---|---|---|
| `BG-001` | Id128 projection `00112233445566778899aabbccddeeff` | `expected/canonical.pb` | exact byte equality |
| `BG-002` | OrderKey projection `0180ff` | `expected/canonical.pb` | exact byte equality |
| `BG-003` | Vec2 projection x=1,y=2 | `expected/canonical.pb` | exact byte equality |
| `BG-004` | Transform2D identity with six explicit scalars | `expected/canonical.pb` | presence + exact bytes |
| `BG-005` | PropertyValue f32=0.5 | `expected/canonical.pb` | oneof tag + fixed32 exact bytes |
| `BG-006` | ColorValue 1/0.5/0/1 with explicit zero channel | `expected/canonical.pb` | presence + exact bytes |
| `BG-007` | root Placement projection | `expected/canonical.pb` | parent absent + orderKey |
| `BG-008` | parented Placement projection | `expected/canonical.pb` | nested Id128 + orderKey |
| `BG-009` | DashPattern segments=[1,2], offset=.5 | `expected/canonical.pb` | packed repeated exact bytes |
| `BG-010` | minimal DocumentSnapshot projection | `expected/canonical.pb` | documentId/schemaVersion; empty objects omitted |

The verification-only fixture compiler must independently reproduce the canonical hex from the semantic authority. Production encoder output is not a valid source.

## Negative authoring matrix

| ID | Base | `AUTHORITY_MANUAL` recipe | Expected observation |
|---|---|---|---|
| `BG-N01` | BG-003 | `REPLACE_RANGE(0,18,11000000000000004009000000000000f03f)` | decode accepted; non-canonical; canonicalize -> BG-003 |
| `BG-N02` | BG-003 | `REPLACE_RANGE(1,8,0000000000000080)` | decode accepted; normalize -0 -> +0; non-canonical |
| `BG-N03` | BG-009 | `REPLACE_RANGE(0,18,09000000000000f03f090000000000000040)` | permissive decode may accept; strict packed profile rejects or canonicality fails; canonicalize -> BG-009 |
| `BG-N04` | BG-004 | `APPEND_HEX(09000000000000f03f)` | `WIRE_PREFLIGHT / DUPLICATE_SINGULAR_FIELD` |
| `BG-N05` | BG-005 | `INSERT_HEX(0,0801)` | `WIRE_PREFLIGHT / MULTIPLE_ONEOF_MEMBERS` |
| `BG-N06` | BG-001 | `APPEND_HEX(f80701)` | `WIRE_PREFLIGHT / UNKNOWN_WIRE_FIELD` |
| `BG-N07` | BG-001 | `SET_BYTE(1,0f)` then `TRUNCATE(17)` | `DTO_MAP / INVALID_ID` |
| `BG-N08` | BG-002 | `REPLACE_RANGE(0,5,0a00)` | `VALIDATE / INVARIANT_VIOLATION` |

## `wire.recipe.json` seed format

Only the Golden Authoring deterministic surgery primitives are allowed:

```json
{
  "formatVersion": 1,
  "baseCase": "BG-003",
  "baseArtifact": "expected/canonical.pb",
  "operations": [
    {
      "op": "REPLACE_RANGE",
      "offset": 0,
      "length": 18,
      "valueHex": "11000000000000004009000000000000f03f"
    }
  ]
}
```

V0.1 seed operations are exactly:

```text
SET_BYTE
REPLACE_RANGE
INSERT_HEX
APPEND_HEX
TRUNCATE
```

The authoring tool must not add semantic-smart mutation helpers that choose expected meaning.

## Independent fixture compiler contract

Recommended physical location:

```text
verification/fixture-author/
```

The compiler must:

1. consume human-reviewed projection / recipe plus current Reference IDL descriptor/tag metadata;
2. perform deterministic projection -> protobuf, canonical wire writing, wire surgery, hashing, and provenance only;
3. not link/import production `runtime/semantic` validator, apply path, or canonical writer as the expected generator;
4. not call production `SemanticCodec` to create `expected/canonical.pb`;
5. be allowed to use generic protobuf wire primitives, descriptor/tag tables, and verification JSON schemas;
6. regenerate byte-identically on repeated runs;
7. emit a tag/wireType/value/offset review surface.

## Provenance contract

Each generated case records at least:

```json
{
  "formatVersion": 1,
  "caseId": "BG-001",
  "authority": {
    "semantic": "Canonical Codec Golden Authority Closure v0.1",
    "verification": "Canonical Codec Golden Fixture Authoring Set v0.1"
  },
  "sources": [],
  "artifacts": [],
  "fixtureCompiler": {
    "identity": "verification-only",
    "sourceCommit": "<git-sha>"
  }
}
```

`sourceCommit` binds fixture-compiler / authoring-source code. Production observation hashes are not authority fingerprints.

## Corpus promotion contract

`verification/corpus/semantic/v1/corpus.json` may declare:

```json
{
  "status": "promoted",
  "differentialOracle": "authority_promoted"
}
```

only after all of the following are true:

- all 18 case authoring sources are complete;
- positive exact bytes match the Semantic Authority Closure;
- negative recipes regenerate deterministically;
- stable stage/category bindings match the Closure;
- independent fixture compiler dependency check passes;
- golden-root regeneration check passes;
- production codec did not participate in expected generation;
- provenance is complete.

## GT-G1-02 evidence binding

Final durable GT-G1-02 evidence binds at least:

- authority baseline commit;
- the Semantic Closure Notion page and GitHub mirror path;
- this Verification Authoring Set Notion page and GitHub mirror path;
- corpus inventory SHA-256;
- all 18 case IDs;
- fixture compiler source commit;
- regeneration result;
- hosted production-vs-golden differential result;
- first divergence = none;
- final GT-G1-02 status.

Only after those conditions are met and the hosted workflow passes can GT-G1-02 move from `BLOCKED_AUTHORITY` to `PASS`.
