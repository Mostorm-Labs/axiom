# GT-G4-07

The interaction library already owns session orchestration but has no selection
state. G2 `Scene::hitTest` remains the query implementation; this WP adds only
a renderer/scene-neutral hit-query port and local selection policy.

First incomplete action: add a literal-fixture SelectionSession RED test, then
implement the smallest local state machine. Selection must never submit an
Operation or hold SemanticDocument/RuntimeScene pointers.
