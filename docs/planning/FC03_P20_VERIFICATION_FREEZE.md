# FC-03 P20 Verification Design Freeze

状态：**P20_DESIGN_FROZEN_FOR_P31 / NOT_EXECUTED**。

## Blocking obligations

| ID | Invariant / failure mode | Oracle and fixture | Blocking evidence |
| --- | --- | --- | --- |
| IMG-01 | PNG/JPEG/WebP import creates one manifest entry and one ImageContent | Three pinned bytes fixtures; RuntimeFacade trace; manifest/hash/length projection | yes |
| IMG-02 | Unsupported/read/hash/decode failure has zero object, Operation, History and SemanticGeneration delta | Four negative fixtures; before/after snapshot and ledger | yes |
| IMG-03 | ImageContent source rect presence/canonicalization is exact | absent/full/partial and boundary vectors; canonical bytes | yes |
| IMG-04 | Crop-before-mode mapping is deterministic | 400×200→300×300 FIT/FILL/STRETCH geometry oracle | yes |
| IMG-05 | Crop preview never mutates canonical; Done is one atomic SetImageContent; Cancel is zero mutation | session trace with Done/Cancel/source-loss/surface-loss | yes |
| IMG-06 | Ordinary resize preserves aspect; modifier enables free stretch | pointer vectors with final ImageContent and transform readback | yes |
| IMG-07 | Missing→Ready/Failed changes derived pixels only | resource generation and semantic ledger negative oracle | yes |
| IMG-08 | Late/duplicate/stale provider result cannot overwrite current binding | old/new lifetime/resource revision tokens | yes |
| IMG-09 | Crop/mode/content changes invalidate Scene digest, bounds, hit and damage | independent Scene projection/hit/damage oracle | yes |
| IMG-10 | Image selection/move/resize/Undo/Redo follows existing object path | mixed Image+Shape replay and History receipt | yes |
| IMG-11 | Windows/Web mapping and failure categories agree | C++/WASM projection differential; host contract tests | yes |
| IMG-12 | Manifest remove with live Image reference rejects atomically | operation validator and no-partial-mutation oracle | yes |

## Exact execution surface

- Semantic vectors: `OperationSpecificStatefulValidation.IMG_B01_CompleteImageContentReplacementSucceeds`, `OperationSpecificStatefulValidation.SIZE_B02_ImageSucceedsAndPreservesNonSizeContent`, plus new FC03 `IMG-*` fixtures.
- Render/resource regression: `axiom_image_resource_tests` and the existing G4.7 image/resource fixture path.
- Host contracts: Windows Minimal Host and Web Minimal Host typed RuntimeFacade tests; no browser/real-GPU result is represented as automated PASS.
- Differential projection: canonical C++ and WASM ImageContent/source-rect/mode projection digest.

## Gate classification

IMG-01..IMG-12 are blocking for FC03 DEV because each covers a distinct correctness, data-integrity, lifecycle or cross-platform contract. Real file-picker, Web context-loss, DPI/GPU presentation and physical observation remain separate FC-13/platform evidence and cannot be closed by these deterministic tests.

## Terminal target

P31 may be generated only after this design is independently reviewed with the Authority bindings above. P32 terminal success is `READY_FOR_CONTROL_REVIEW`; only independent P34 may issue a DEV Gate verdict.
