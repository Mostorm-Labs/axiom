# ImageContent Canonical Geometry + Presence Policy V1 Release v0.1

> Source: Notion `ImageContent Canonical Geometry + Presence Policy V1 Release v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81948870c4f266a39861
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Released V1 ImageContent

V1 freezes ImageContent around:

- `resourceId`;
- intrinsic width/height;
- optional `sourceRect`;
- required `contentMode`;
- local semantic width/height.

The current release reconciliation assigns width/height wire tags `6/7` in the integrated schema.

## Presence / geometry rules

Image geometry and crop/fit are semantic state, not renderer-local metadata. Explicit presence must follow the generated Proto / canonical codec rules; absence and semantic defaults must not be conflated by language bindings.

Resource binary lifecycle remains outside the semantic object; ImageContent references a stable ResourceId.

## Implementation rule

Image decode texture size, GPU handles and cached raster bounds are derived. They must never overwrite intrinsic/local semantic geometry in the Document.
