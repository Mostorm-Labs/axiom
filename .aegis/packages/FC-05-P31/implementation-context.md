# FC05 P31 implementation context

P32 must start from the exact P31 materialization commit, not from main or the
architecture branch's earlier head. The package is intentionally stacked:
`origin/main` -> FC05 design -> FC05 architecture/P20 -> this package commit ->
`codex/fc05-p32`.

First action on P32: verify repository identity, package hashes and anchor
ancestry; run the old-baseline RED probes before production mutation. Existing
Semantic has Connector/1 validation, Sticky/1 content and a one-operation
OperationEngine. Existing RuntimeFacade has CanvasControl but no typed Connector/
Sticky source values. Preserve all existing FC02/FC03 text/image behavior.

Implementation order: semantic target/capability tests; Sticky aggregate guard and
History; derived route provider and Scene integration; public Facade/readback;
Windows/Web adapters; focused and regression verification; P32 result evidence.
If a requirement cannot be implemented without changing a frozen upstream contract,
stop and return the earliest-layer blocker rather than inventing a generic
transaction or host-owned shadow state.
