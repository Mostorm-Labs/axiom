# ResourceManifest ChangeSet v1 candidate (v0.4)

This is an immutable runtime event under ADR-0019 and 07-11, not a serialized Operation,
Undo preimage, persistence diff or new public ABI. Exact proposed C++ logical extension to
runtime/semantic/include/canvas/semantic/change_set.hpp (no implementation in this package):

```cpp
enum class ResourceManifestChangeFlags : std::uint8_t {
    kCreated = 1, kUpdated = 2, kRemoved = 4
};
struct ResourceManifestStateValue {
    std::optional<ResourceManifestEntry> entry;
    std::optional<ResourceManifestTombstone> tombstone;
};
struct ResourceManifestSemanticChange {
    ResourceId resource_id;
    ResourceManifestChangeFlags flags;
    ResourceManifestStateValue before;
    ResourceManifestStateValue after;
};
// Append private std::vector<ResourceManifestSemanticChange> resource_changes_ to ChangeSet.
// Add const accessor: std::span<const ResourceManifestSemanticChange> resourceChanges() const;
// Preserve existing fromChanges(before, after, objects) as empty resource_changes_.
// Add fromChanges(before, after, objects, resources); internal finalize uses same validation.
```

Each field is a full immutable owned value; no pointers into mutable state. Existing object
flags/names are untouched. Every record has exactly ONE resource flag; OR combinations and
unknown values reject in constructing/validating the event. Values follow operation states:

| action | flag | before | after |
|---|---|---|---|
| never-used create | kCreated=1 | neither field present | active entry, no tombstone |
| active update | kUpdated=2 | old entry, optional historic tombstone | new entry, same historic tombstone |
| remove | kRemoved=4 | old entry, optional historic tombstone | no entry, NEW tombstone |
| reactivate | kCreated=1 | no entry, exact tombstone | new entry, SAME tombstone |

All contained IDs must match resource_id and the operation state/counter rules. Sorted full
ResourceId bytes, one record per targeted ID. Action collections are disjoint; repeated or
contradictory same-ID actions REJECT the Operation, never merge flags. Internal helpers may
replace a staged value for that one action but cannot introduce additional semantic actions.
Thus no same-Operation Created+Updated/Removed merge rule exists.

One true Applied Operation yields one event Gbefore -> Gafter. AlreadyApplied/Rejected yield
none. Multiple Operations have DISTINCT events and generation boundaries; do not merge them
into one synthetic ChangeSet or assert same generation. Scene may consume an ordered span
of events only with contiguous before/after generations and a matching final read view.

SemanticReadView additionally exposes the immutable manifest at captured generation. The
shared Semantic->Scene adapter examines resourceChanges(): update all dependent Image/Text/
Brush records, bounds and hit data by full ResourceId, preserving captured expected hashes.
Missing/untrusted collection, generation gap, unavailable matching view or failed incremental
prepare triggers compileFull from the latest complete read view. Recovery also uses the same
full compiler. Publish Scene/bounds/spatial atomically via the existing coordinator gate;
on failure keep old complete Scene and dirty retry state. Query/render use the same published
Scene revision. Document generation is not reset by resource-only derived refresh.

Availability changes emit NO ChangeSet and use the separate runtime resource-refresh input.
The future shared adapter must compare Full/Delta/Recovery at the same semantic and resource
binding snapshot. The exact internal collection is now specified; no persistence serialization
is appropriate for this derived event. Operation/Snapshot bytes are the durable source.
