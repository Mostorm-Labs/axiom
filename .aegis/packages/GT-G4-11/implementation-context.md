# GT-G4-11

Extend `InteractionSessionManager` with an explicit dependency footprint and
post-commit conflict policy. The policy returns Continue for unrelated changes,
ReResolve for a mutation of a depended-on live target, and Cancel for target
deletion. Generation mismatch alone is not a conflict. Document detach,
suspend, and Arc source loss cancel capture; finished sessions reject late
callbacks and cannot be resurrected.
