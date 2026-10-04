# Application Undo/Redo integration

EditorHistory is already implemented in runtime/interaction and remains unchanged.
The common application host owns local intentions; Windows/ImGui are consumers.
The resumed seven-file WIP has RED observations in out/undo-redo-wip-20261004.
First pending action: test complete ObjectRecord/pixel restoration, identity collisions,
and Scene failure. Semantic acceptance owns history; derived projection recovers from
current post-state without rewind or duplicate compensation. Keep pointer ingress,
AutoIntent, BrushEngine and Arc algorithms unchanged.

Debug UI P32 review remains BLOCKED_EVIDENCE and is not superseded by this package.
