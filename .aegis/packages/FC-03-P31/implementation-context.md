# FC-03 P31 Implementation Context

- Repository: Mostorm-Labs/axiom
- Task anchor: `462d80868af2f2e6fb09b235f283c1cb7b221ff3`
- Authority: `fc-feature-completion-execution-v0.3`, repo-local current
- Package scope: existing ImageContent/ResourceManifest/SetImageContent/SetObjectSize plus typed RuntimeFacade and Crop session.

## Required ownership

RuntimeFacade is the only consumer command surface. RuntimeComposition owns resource binding, owner safe-point ordering and derived invalidation. CanvasEditingRuntime owns canonical operation/history. Host adapters provide immutable file bytes and platform facts only.

## Forbidden

No new Image semantic fields, ObjectKind, OperationKind, protobuf tags, second manifest, direct SemanticDocument host mutation, FC02/FC04/FC05 expansion, G5 optimization, or physical PASS claim.

## First step

Run FC03-RED-01 against this anchor: compile/test probe must demonstrate no typed import/crop API exists before the implementation change. Then implement one narrow vertical slice at a time and run the corresponding IMG oracle.
