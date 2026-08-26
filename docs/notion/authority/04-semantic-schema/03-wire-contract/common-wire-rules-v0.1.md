# Common Wire Rules v0.1

> Source: Notion `Common Wire Rules v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81f9a6d0e1ec2a9bcab7
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Cross-cutting Canonical Wire Contract
> Repository status: frozen-by-v1-final-gate

## Purpose

Defines cross-cutting canonical wire behavior shared by Object, Operation and leaf schemas.

## Required classes of rules

- stable ID encoding and validation;
- numeric canonicalization and rejection rules;
- explicit presence/default semantics;
- collection ordering / canonical ordering;
- enum and registry compatibility;
- unknown field/version behavior;
- deterministic serialization requirements;
- protocol safety/preflight limits.

## Numeric rules

Semantic numeric values reject NaN/Infinity and canonicalize `-0` to `+0`. Geometry/transform semantics and normalized appearance values follow their released semantic precision/range rules.

## Enum / registry rule

If an enum/registry value changes semantic behavior, unknown values cannot be treated as an arbitrary platform fallback. Compatibility must follow the released version/unknown-value policy.

## Determinism

Canonical bytes must be stable across C++, TS and WASM/reference implementations for the same canonical semantic value. The V1 stochastic brush sequence is defined by the released deterministic interpreter authority, not host-language RNG.

## Implementation rule

Wire preflight happens before expensive decode/apply work. Canonical codec and semantic validator must share the same released limits and presence rules.
