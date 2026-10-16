# ResourceManifest ChangeSet v1 candidate

This is a separate semantic collection from existing ObjectSemanticChange. It is post-commit
data in the immutable ChangeSet and contains no renderer/cache/invalidation state.

```text
ResourceManifestSemanticChange {
  ResourceId resource_id;
  uint8 flags; // Created=1, Updated=2, Removed=4
  optional ResourceManifestEntry before_entry;
  optional ResourceManifestEntry after_entry;
  optional ResourceManifestTombstone tombstone;
}
```

Invariants:

- `resource_id` is nonzero and entries sort by complete 128-bit bytes.
- One ResourceId occurs once in a ChangeSet. Multiple staged mutations in one Operation are
  merged before publication: Created+Updated collapses to Created with the final after entry;
  Updated+Removed becomes Removed with the original before entry and final tombstone;
  Created+Removed is rejected as a duplicate contradictory target.
- Created requires `after_entry` and forbids `before_entry`/`tombstone`; Updated requires both
  before and after and forbids tombstone; Removed requires before and tombstone and forbids
  after. Flags and presence are validated together.
- `before_entry`/`after_entry` are immutable canonical values, not pointers into mutable
  Document state. Tombstone is the post-state retired record.
- Full, Delta and Recovery Scene consumers consume the same sorted collection. If the
  collection is absent or cannot be proven safe, the consumer uses a same-generation full
  compile. Equal-revision full compile is the correctness oracle.
- Resource availability changes produce no ChangeSet. A manifest Operation produces one
  SemanticGeneration successor and one ChangeSet, even when a batch contains multiple IDs.

The collection is not an Operation diff, Undo record, or persistence log. History uses the
Operation payload and preimage; Scene uses this post-commit view.
