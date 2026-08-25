# Brush Deterministic Interpreter + PRNG + Highlighter Blend V1 Release v0.1

> Source: Notion `Brush Deterministic Interpreter + PRNG + Highlighter Blend V1 Release v0.1`
> Source page: https://app.notion.com/p/3c54c57a590c81ffa7c5c6080fe940d4
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Deterministic contract

V1 freezes the brush interpreter behavior needed for cross-language replay, including the released PRNG sequence and highlighter blend semantics.

The stochastic sequence is **AXIOM_SPLITMIX64_V1** under the released authority; implementations must not substitute `std::random`, JS `Math.random`, platform RNG or renderer RNG.

## Representation

Released brush families resolve to canonical stroke representation/interpreter behavior (including DAB where specified) before renderer compilation.

## Verification consequence

C++, TS and WASM/reference interpreters must produce equivalent canonical semantic projections and golden vectors from the same StrokeRecord/seed/input.
