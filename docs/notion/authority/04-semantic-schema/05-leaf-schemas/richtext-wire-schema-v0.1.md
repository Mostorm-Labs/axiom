# RichTextDocument + RichTextDelta Wire Schema v0.1

> Source: Notion `RichTextDocument + RichTextDelta Wire Schema v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81639c2ed63028f984b4
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

Defines the canonical RichTextDocument and committed RichTextDelta wire schema integrated by the Reference IDL.

## Boundary

Caret, selection, focus, IME composition intermediate text and overlay/editor widget state are not canonical RichTextDocument state.

RichTextDelta may contain internal semantic mutation steps, but those steps are internal to RichText mutation and do not recreate a global transaction layer.

## Font dependency

Font semantics are further constrained by the V1 RichText Font Semantic Baseline. Implementations must apply that release authority rather than platform font fallback behavior.
