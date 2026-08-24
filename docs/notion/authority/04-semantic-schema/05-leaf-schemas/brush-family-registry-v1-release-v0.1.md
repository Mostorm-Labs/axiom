# BrushFamily Registry V1 Release v0.1

> Source: Notion `BrushFamily Registry V1 Release v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c8132ade5e6488e27a4df
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Release rule

Brush family/version is a versioned semantic namespace. V1 publishes only released `(family, version)` pairs; unsupported pairs must not be guessed or interpreted using a platform brush implementation.

The registry is expected to exist as a versioned semantic artifact alongside field and shape registries.

## Implementation rule

Brush family selection resolves to the released deterministic interpreter contract. Renderer-specific brush code consumes resolved canonical semantics rather than defining them.
