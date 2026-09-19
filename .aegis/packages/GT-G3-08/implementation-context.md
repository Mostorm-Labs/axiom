# GT-G3-08 implementation context

G3-07 is closed and integrated. G3-08 is the next ordered reviewer-sized work
package in the Current G3 v0.2 execution authority. It adds only a demo harness:
camera pan/zoom, view/world coordinate conversion, delegation to the existing G2
`Scene::hitTest` pipeline, and an observation-only selected `ObjectId` snapshot.

The camera controller is per-view. Pan and zoom update `CameraState` and its
generation, never semantic or scene state. Zoom is anchored at a caller-supplied
view point, so the corresponding world point remains stable. Invalid dimensions,
coordinates, deltas, zoom values, and generation overflow fail without mutation.

Hit testing is not reimplemented. `SceneHitTestPort` delegates to the public G2
`Scene::hitTest` contract. The real-Scene oracle must observe nonzero spatial
candidate and precise-test diagnostics, front-to-back selection, and unchanged
Scene/RuntimeScene identities and records. Demo selection changes only
`DemoTransientSnapshot` and its deterministic overlay digest.

This package does not authorize canonical selection, EditorSession,
SelectionSession, tools, history, operations, platform hosts, G4 interaction
semantics, or modifications to existing G2/G3-01..07 production contracts.
