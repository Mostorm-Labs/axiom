# GT-G4-09

Implement the authority-backed whole-stroke erase path only. A sweep produces
candidates with kind/visibility/lock metadata; the session filters to vector or
dab stroke and submits one existing `DeleteObjectsOp`. Do not invent partial
erase behavior where the upstream strategy is open.
