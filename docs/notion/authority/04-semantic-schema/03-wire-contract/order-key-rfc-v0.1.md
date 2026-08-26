# OrderKey RFC v0.1

> Source: Notion `OrderKey RFC v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c813eb61fc24b0ddee194
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Semantic contract

`OrderKey` is an opaque byte sequence used inside atomic Placement (`parentId + orderKey`). Ordering uses unsigned-byte lexicographic comparison.

The rank-generation algorithm is not protocol truth and may evolve independently as long as generated keys obey the released validity/order constraints.

## Implementation rule

- Never compare OrderKey using locale/string collation.
- Never split parent and order mutation into independent canonical writes.
- Do not expose implementation-specific rank allocator state as semantic Document state.
- Enforce released hard limits before accepting wire values.
