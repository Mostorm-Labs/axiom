# FC05 P20 verification freeze

Status: **FROZEN_FOR_P31**
Subject design revision: `c9ef5d1ac95879fc6bc1d35fd2a200982f77ad39`
Independent review: `docs/planning/fc05/evidence/architecture-p20/independent-review.json`

This freeze fixes the completion target for P31/P32. It does not claim product
implementation, DEV Gate PASS or physical validation.

## Blocking obligations

| ID | Frozen invariant | Blocking oracle |
| --- | --- | --- |
| CON-01 | Endpoint intent is connector-local FreePoint or target+AnchorRef; route is derived | transformed endpoint fixture and canonical-byte stability after route refresh |
| CON-02 | Shape/1, Shape/2 triangle, Shape/3 label, Image/1 and Sticky/1 target tuples validate atomically | insert/restore/reconnect and Shape label upgrade/Undo/Redo matrix; old tuple negatives remain |
| CON-03 | Auto hint, dynamic center and Triangle ports use exact analytical resolver | independent rectangle/ellipse/triangle rational oracle, all ports and coincident +X |
| CON-04 | Orthogonal route is axis-aligned, avoids admitted inflated obstacles, and reports no-route/budget truthfully | independent segment/interior checker and explicit failure receipts with zero canonical side effects |
| CON-05 | Route invalidation includes endpoint and unrelated corridor obstacle changes | third-obstacle move/insert/delete versus fresh reference solve and cache invalidation trace |
| CON-06 | One derived route feeds render, bounds, index, hit and arrow | integrated route observation and native/WASM topology/tolerance comparison |
| CON-07 | Rerouting is derived only; target mutation is the only canonical edit | ledger/generation/history interval oracle |
| CON-08 | Connector create/reconnect commit once; stale/loss/cancel leaves no partial edit | public RuntimeFacade owner sequence, failure injection and cascade restore |
| STK-01 | Empty Sticky is valid; first confirmed text creates exactly one direct RichText child | ordinary consumer create/cancel/retry graph/history oracle |
| STK-02 | Sticky text/layout/parent growth use fixed-size wrap, upward growth, ellipsis and one atomic parent/child publication | aggregate guard, fault injection, identity-preserving Undo/Redo and caret/render/hit parity |
| STK-03 | Debug UI and ordinary consumer use the same RuntimeFacade | public-header-only consumer versus Debug UI action/readback comparison |
| PLAT-01 | Windows/Web share semantics, lifecycle, receipt and route/layout rules | same serialized intent/object corpus, canonical equality and route topology/tolerance |

P20 also freezes the candidate routing work limits and native/WASM comparison
tolerances in `FC05_ROUTING_CONTRACT.md`; timing remains corroborative until
hardware-specific thresholds are measured. Physical Windows/Web/IME/touch
observation remains `PHYSICAL_PENDING` and is not a DEV completion substitute.
