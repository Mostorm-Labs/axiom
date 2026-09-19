# GT-G4-01 implementation context

Canonical baseline: `Mostorm-Labs/axiom@f2a64ebc49ce842d583ee2f6990124ceaf0b310b`.

G3 is closed with durable Gate PASS. This package is limited to the first G4
vertical slice: orchestration and port boundaries. Do not implement pointer
ingress, ink algorithms, preview, handoff, selection, transform, eraser,
history, playground, or later WPs here. Do not invent unresolved conflict,
partial-erase, platform-topology, or Arc-token decisions.

The production interaction layer must not own canonical truth. Canonical writes
remain G1 Operation Engine submissions through `OperationSubmitPort`; interaction
may read SemanticDocument and query Scene but may not receive SemanticWritePort.
