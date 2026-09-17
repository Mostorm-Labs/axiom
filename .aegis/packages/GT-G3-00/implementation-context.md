# GT-G3-00 implementation context

Current flow: G2 canonical `RuntimeScene` and `SceneQueryPort` provide renderer-neutral derived scene state. G3 adds the first production `runtime/render/` vertical slice: per-view frame state, Render-owned visibility, a direct/non-tiled reference source, a minimal frame plan/backend seam, canonical surface lifecycle, and presentation feedback. Headless, Windows reference, and Web reference hosts must consume that same Render Core.

Preserved completed work: G2 canonical main is closed at `07654767eac7b021ca6e89004e36448ccebb727d`; G3 Entry Readiness and the P30 plan are complete. Do not reopen G2 or promote POC scene/render paths to production contracts.

First implementation action: run the G3-01 RED compile/contract test from the exact task anchor, then implement only the smallest per-view Render Core foundation. Continue through all 11 ordered work packages and the frozen verification closure in `execution-contract.json`.

Relevant owners/files are listed in the execution contract. This context is an execution aid, not a second architecture authority.
