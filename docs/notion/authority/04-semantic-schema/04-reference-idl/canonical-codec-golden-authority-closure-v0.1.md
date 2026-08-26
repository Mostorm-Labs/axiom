# Canonical Codec Golden Authority Closure v0.1

> Source: Notion `Canonical Codec Golden Authority Closure v0.1`
> Source page: https://app.notion.com/p/3c84c57a590c814aba1bd9435d0b451e
> Source page id: `3c84c57a-590c-814a-ba1b-d9435d0b451e`
> Snapshot date: 2026-08-26
> Source status: Current Authority — Canonical Codec Golden Case Closure / GT-G1-02 Authority Unblocker
> Repository status: current

This authority closes only the case-level semantic truth, canonical bytes, canonicality outcomes, and rejection stage/category needed by `BG-001..BG-010` and `BG-N01..BG-N08`. It does not redesign Reference IDL, Common Wire, Operation Model, Runtime ownership, or allow implementation output to become authority.

## Scope and non-changes

Direct source authority remains the current Common Wire, Reference IDL / Codec Mapping, Generated Proto / Canonical Codec freeze, Golden Authoring trust-boundary contract, and the prior codec skeleton/seed design.

The closure does not change:

- the 12-file Edition 2024 proto tags or field types;
- stable IDs as exactly 16 bytes;
- finite numeric rules or `-0 -> +0` normalization;
- root Placement as `parent_id` absent;
- ascending canonical field tags, packed profile, presence, or collection ordering;
- C++ / TS / WASM ownership;
- Product storage or sync wire formats.

## Golden outcome model

`BG-N01..BG-N03` are canonicality cases, not ordinary semantic-invalid inputs. Permissive semantic decode must not invent a semantic error solely because the bytes are non-canonical.

Stable codec stages:

```text
WIRE_PREFLIGHT
PROTO_DECODE
DTO_MAP
NORMALIZE
VALIDATE
CANONICAL_ENCODE
```

Stable error categories:

```text
MALFORMED_WIRE
TRUNCATED_INPUT
UNKNOWN_WIRE_FIELD
DUPLICATE_SINGULAR_FIELD
MULTIPLE_ONEOF_MEMBERS
NON_CANONICAL_PACKED_ENCODING
INVALID_UTF8
INVALID_ID
INVALID_NUMERIC
UNKNOWN_REGISTRY_VALUE
MISSING_REQUIRED_SEMANTIC_FIELD
INVALID_PRESENCE
INVARIANT_VIOLATION
LIMIT_EXCEEDED
INTERNAL
```

Canonicality-only outcomes:

```text
CANONICAL
NON_CANONICAL_INPUT
```

`NON_CANONICAL_INPUT` is an `assertCanonical` / strict-canonical outcome, not a synthetic semantic error for permissive decode.

## Positive binary authority — BG-001..BG-010

Hex is lowercase without spaces. Protobuf fixed32/fixed64 values use little-endian wire order.

| ID | Root type / human-reviewed semantic truth | Canonical hex |
|---|---|---|
| `BG-001` | `Id128.value = 00112233445566778899aabbccddeeff` | `0a1000112233445566778899aabbccddeeff` |
| `BG-002` | `OrderKey.value = 0180ff` | `0a030180ff` |
| `BG-003` | `Vec2{x=1.0,y=2.0}` | `09000000000000f03f110000000000000040` |
| `BG-004` | `Transform2D{a=1,b=0,c=0,d=1,tx=0,ty=0}` with all six semantic-required scalars explicitly present | `09000000000000f03f11000000000000000019000000000000000021000000000000f03f290000000000000000310000000000000000` |
| `BG-005` | `PropertyValue{f32_value=0.5}` | `150000003f` |
| `BG-006` | `ColorValue{r=1,g=0.5,b=0,a=1}` with `b=0` explicitly present | `0d0000803f150000003f1d00000000250000803f` |
| `BG-007` | root `Placement{parent_id absent, order_key=01}` | `12030a0101` |
| `BG-008` | parented `Placement{parent_id=BG-001, order_key=01}` | `0a120a1000112233445566778899aabbccddeeff12030a0101` |
| `BG-009` | `DashPattern{segments=[1.0,2.0], offset=0.5}`; segments are ordered and field 1 is canonical packed | `0a10000000000000f03f000000000000004011000000000000e03f` |
| `BG-010` | `DocumentSnapshot{document_id=BG-001, schema_version=1, objects=[]}` | `0a120a1000112233445566778899aabbccddeeff1001` |

Review implications:

- `BG-004` and `BG-006` prove required zero-valued scalars/channels cannot be silently removed by protobuf default elision.
- `BG-007` proves document root is represented only by absent `parent_id`, not all-zero ID.
- `BG-009` fixes the packed repeated-double canonical form.
- `BG-010` omits the empty objects field.

## Negative / canonicalization authority — BG-N01..BG-N08

All mutations are `AUTHORITY_MANUAL` intent and must be materialized by verification-only deterministic wire surgery.

| ID | Defect / input intent | Expected semantic / canonical outcome | Stable rejection binding |
|---|---|---|---|
| `BG-N01` | `Vec2` field 2 precedes field 1; semantic value remains `{x=1,y=2}` | permissive decode accepted; canonicality=`NON_CANONICAL_INPUT`; canonicalize -> `BG-003` | not a semantic error; no `CodecErrorCategory` |
| `BG-N02` | `Vec2.x` wire bits are f64 `-0.0`, `y=2.0` | permissive decode accepted; normalize `-0 -> +0`; canonicality=`NON_CANONICAL_INPUT`; canonical hex=`090000000000000000110000000000000040` | not a semantic error; no `CodecErrorCategory` |
| `BG-N03` | `DashPattern.segments=[1,2]` uses two unpacked fixed64 field-1 occurrences | permissive decode may accept; semantic value unchanged; canonicality=`NON_CANONICAL_INPUT`; canonicalize -> `BG-009` | strict profile may reject at `WIRE_PREFLIGHT / NON_CANONICAL_PACKED_ENCODING`; permissive path has no semantic error |
| `BG-N04` | duplicate singular `Transform2D.a` | reject before generated parser can erase duplicate evidence | `WIRE_PREFLIGHT / DUPLICATE_SINGULAR_FIELD` |
| `BG-N05` | `PropertyValue` wire contains bool member followed by f32 member | reject before oneof last-wins semantics hides the first member | `WIRE_PREFLIGHT / MULTIPLE_ONEOF_MEMBERS` |
| `BG-N06` | unknown field 127 varint appended to `BG-001` Id128 | V1 editor profile fails closed | `WIRE_PREFLIGHT / UNKNOWN_WIRE_FIELD` |
| `BG-N07` | `Id128.value` length is 15 bytes | protobuf structural decode succeeds; valid fixed-width domain ID cannot be materialized | `DTO_MAP / INVALID_ID` |
| `BG-N08` | `OrderKey.value` is present but empty | structural decode succeeds; 1..32-byte semantic invariant is violated | `VALIDATE / INVARIANT_VIOLATION` |

The stage binding for `BG-N07` closes the earlier ambiguity between mapping and validation: exact-16-byte width is a domain materialization prerequisite, so failure is `DTO_MAP / INVALID_ID`. `BG-N08` is structurally valid protobuf but violates the OrderKey semantic invariant, so it is `VALIDATE / INVARIANT_VIOLATION`.

## Exact wire recipe authority

Offsets are zero-based byte offsets.

```text
BG-N01
base = BG-003 canonical.pb
REPLACE_RANGE(offset=0, length=18,
  hex=11000000000000004009000000000000f03f)

BG-N02
base = BG-003 canonical.pb
REPLACE_RANGE(offset=1, length=8,
  hex=0000000000000080)
expected canonical = 090000000000000000110000000000000040

BG-N03
base = BG-009 canonical.pb
REPLACE_RANGE(offset=0, length=18,
  hex=09000000000000f03f090000000000000040)
# field2 offset=0.5 remains unchanged after the replaced range

BG-N04
base = BG-004 canonical.pb
APPEND_HEX(09000000000000f03f)

BG-N05
base = BG-005 canonical.pb
INSERT_HEX(offset=0, hex=0801)

BG-N06
base = BG-001 canonical.pb
APPEND_HEX(f80701)
# field_number=127, wire_type=varint, value=1

BG-N07
base = BG-001 canonical.pb
SET_BYTE(offset=1, valueHex=0f)
TRUNCATE(length=17)

BG-N08
base = BG-002 canonical.pb
REPLACE_RANGE(offset=0, length=5, hex=0a00)
```

For `BG-N03`, only the packed field-1 region is replaced; field 2 (`offset=0.5`) remains unchanged. The fixture compiler must expose the final tag/wire-type/value/offset review surface.

## Trust boundary

- semantic values, defect intent, stage/category, and wire recipes in this authority are `AUTHORITY_MANUAL`;
- generated `.pb`, patched input bytes, SHA-256 values, and provenance are `DERIVED_GENERATED`;
- production `SemanticCodec` and the runtime canonical writer must not generate authoritative expected values;
- C++ == WASM proves same-core parity, not an independent oracle;
- the fixture compiler may consume current descriptor/tag metadata and declarative Common Wire rules, but must be verification-only and must not link production Semantic Core validator/apply/canonical writer as its sole expected generator.

## GT-G1-02 unblock contract

Once this page and the matching Verification Authoring Set are listed as current in `docs/notion/manifest.yaml`, GT-G1-02 may resume only in this order:

```text
Current Authority Closure
-> Verification Authoring Set
-> Independent Fixture Compiler
-> BG seed materialization
-> production-vs-golden differential
-> durable commit-bound evidence
-> GT-G1-02 PASS
```

Until the repository manifest points to this current authority, GT-G1-02 remains `BLOCKED_AUTHORITY`.
