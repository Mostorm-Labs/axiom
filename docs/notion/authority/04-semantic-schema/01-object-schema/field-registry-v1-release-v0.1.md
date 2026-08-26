# Field Registry V1 Release Table + Default Semantics Closure v0.1

> Source: Notion `Field Registry V1 Release Table + Default Semantics Closure v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81d9a77dc51bd00419e1
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — V1 Field Registry Release Table / RC-B03 Closure
> Repository status: frozen-by-v1-final-gate

## Release meaning

This page closes the V1 field/default-semantics blocker. The released registry artifact is part of the semantic contract, not merely generated convenience data.

## Required implementation behavior

- Explicit field value and absent field/default semantics must be deterministic and cross-language identical.
- Clear semantics must resolve according to the registry, not by language-specific zero/default construction.
- Field applicability is validated against ObjectKind.
- A semantically valid object may have no visible paint (for example a Shape with no fill and no stroke, or Connector with no stroke); visibility is not a schema-validity shortcut.

## Artifact expectation

The implementation should materialize a versioned registry artifact such as `schema/axiom/v1/registry/field_registry_v1.yaml` (exact repository placement may be adapted by implementation planning) and derive C++/TS validation tables from it.
