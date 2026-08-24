# RichText Font Semantic Baseline V1 Release v0.1

> Source: Notion `RichText Font Semantic Baseline V1 Release v0.1`
> Source page: https://app.notion.com/p/3c54c57a590c81d8befacf2228f81fd6
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## V1 font lock

Current V1 authority freezes:

- `font_resource_id` as semantic-required;
- the released nine-level weight numeric domain;
- exact `(weight, italic)` face resolution;
- **no implicit platform font fallback**.

Historical optional/default-font wording is superseded.

## Implementation consequence

Headless replay/export, C++ native, Web/WASM and platform overlays must resolve the same semantic font identity. Platform font substitution may be a surfaced error/fallback policy outside canonical semantics, but it must not silently change canonical RichText meaning.
