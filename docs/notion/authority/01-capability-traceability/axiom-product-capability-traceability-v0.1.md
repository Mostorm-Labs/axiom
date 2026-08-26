# Axiom Product Capability Traceability v0.1

> Source: Notion `Axiom Product Capability Traceability v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c8178a397f11d9675a210
> Snapshot date: 2026-08-24
> Source status: Draft for Review — Product → Architecture Traceability Authority Candidate
> Repository status: proposed

## Purpose

This authority candidate traces product capabilities through:

`Product Capability → Semantic Object / Product Entity → Behavior / Session / Command → Canonical Operation → Runtime Capability → Owner Module → Platform Contract → Verification / SLO`.

It does not redefine product requirements and does not replace Semantic Schema or Module RFCs. Its job is to expose breaks where a product requirement exists but the object, operation, module, platform contract, or verification chain is incomplete.

## Authority rules

The current semantic model uses an **Operation-only canonical mutation model**. `Operation` is the canonical delta. Behavior, Session, and Command may span a continuous interaction, but canonical state changes only through Operations.

Trace states:

- `COVERED`: the main product-to-architecture chain has current authority coverage.
- `PARTIAL`: direction is clear, but payload/session/platform details remain.
- `GAP`: product requirement exists without a formal semantic/module representation.
- `LOCAL`: EditorSession/View/Shell transient state; no canonical Operation is required.
- `DATA/SHELL`: capability exists, but primary ownership is Shared TS Data Runtime or Product Shell.

## Core invariants

- Not every capability requires an Object; Pan/Zoom/Fit are examples.
- Not every behavior requires an Operation; hover, selection, drag-time snap, and viewport lock are local/transient.
- Operations express final semantic mutation, not pointer gestures or renderer commands.
- Platform differences belong at Input/Surface/IME/Storage/Overlay contracts and must not contaminate Semantic Objects or Operations.
- Transient behavior may feed Arc/Overlay/EditorSession; committed Operations may feed Shared TS Data Runtime for persistence/sync.

## High-value trace rows

### Canvas / Viewport / Page

- Infinite Canvas + Pan/Zoom: no canonical object; owned by Axiom Camera/Viewport/Bounds/SpatialIndex/Tile. `COVERED / LOCAL`.
- Fit / Zoom-to-content: derived from document world bounds; no Operation. `COVERED / LOCAL`.
- Lock Viewport: local interaction policy; no Semantic Schema entry. `COVERED / LOCAL`.
- Mini-map / outline navigation: requires multi-view/preview-render contract. `PARTIAL`.
- Product Canvas multi-page: **one Product Page = one Axiom Document**; page-list operations are Shell/Data Runtime concerns. `DATA/SHELL`.
- Duplicate Page: clone canonical semantic state, never RuntimeScene/Tile/GPU cache; ID remap/resource rules remain. `PARTIAL`.

### Ink / Brush / Low latency

- Pen/Brush: `PointerSampleBatch → StrokeSession → Preview + Canonical → AddStroke`; Axiom owns brush semantics, Arc owns input/preview presentation. `COVERED`.
- Highlighter: same canonical `AddStroke` path; blend/opacity semantics must remain registry-defined. `COVERED`.
- Texture-capable brush: canonical StrokeRecord/BrushDescriptor needs deterministic resource/spacing/seed semantics. `PARTIAL`.
- Low-latency ink: Active Stroke is transient; pointer-up commits `AddStroke`; Arc visible acknowledgement/fence ABI remains unresolved. `PARTIAL`.
- Pressure/Tilt: platform reports capabilities; Axiom interprets them. `COVERED`.
- Long stroke: incremental processing plus derived chunking/scheduler; requires bounded-growth benchmark. `PARTIAL`.
- Offline stroke completion: local canonical commit is independent of network. `COVERED`.

### Eraser

- Stroke erase: EraserSession + spatial hit-test resolves to `DeleteObjects`. `COVERED`.
- Partial erase: resolves to `SplitStrokes` or `AddEraseMasks` according to canonical brush strategy. `PARTIAL`.
- Eraser preview is transient; final pixels are never the semantic truth. `PARTIAL`.

### Basic objects

- RichText: canonical `RichText` object + Hybrid editing; committed changes use `EditRichText`; caret/IME stay transient.
- Shape/Image/VectorPath/VectorStroke/DabStroke are canonical semantic objects covered by V1 schema work.
- Sticky, Connector, Group, Frame, Section, Table, Rich Document, and Embed/WebView require the Product Object Model / schema closure appropriate to their category.

## Cross-layer interpretation

The traceability model is intentionally not a module inventory. It is a coverage matrix. When a row is `PARTIAL` or `GAP`, the missing contract must be closed in the downstream authority layer rather than patched locally into this document.

## Current downstream dependencies

- Product Object Model
- Interaction / Behavior Model
- Semantic Schema / Operation Model
- Runtime Capability Architecture
- Module Detailed Design
- Platform Contract
- Verification

## Migration note

This repository file is a versioned implementation-facing snapshot. Notion remains the living architecture authority until the snapshot is explicitly promoted to `frozen` in `docs/notion/manifest.yaml`.