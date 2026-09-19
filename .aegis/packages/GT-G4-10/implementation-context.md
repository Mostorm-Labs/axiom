# GT-G4-10

Implement the Notion-authorized minimal Editor History inside
`runtime/interaction`. History is per EditorSession and stores only local
authored intention plus the semantic preimage needed to construct inverses.

Undo and redo submit new `semantic::Operation` values with fresh operation
identity and `ApplySource::kUndoRedo`. The supported V1 families are AddStroke,
SetTransforms, DeleteObjects, SplitStrokes, and AddEraseMasks. A rejected submit
must not advance history state. No snapshot, ObjectStore, RuntimeScene, or
canonical operation-vocabulary mutation is authorized.
