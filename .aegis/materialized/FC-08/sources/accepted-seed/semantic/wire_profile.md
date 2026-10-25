# AP-02 proposed wire and collection profile

The .proto files only encode structure. The P12/P13 contracts remain the source
of field meaning, final-graph validation, transforms, history and rejection.
All new field/tag choices are proposed publication bindings, not existing facts.

## Presence and inherited leaves

All non-oneof singular fields in the new messages are semantically required and have
explicit presence, except ContainerChildrenExpectation.parent_id, whose absence
is Document root. Placement.parent_id keeps its existing root-absence rule.
A present empty Id128 or present all-zero identity is invalid, not root.

SectionContent.boundary requires every Rect2 component. width/height are finite
and positive; x/y may be negative; padding/header_height are finite and >= 0.
The explicitly present empty title is valid UTF-8; missing title is invalid.
Title maximum uses the inherited generic UTF-8 limit. No invented trim or NFC.
Negative zero normalization, numeric encoding and immutable defaults follow P12.

ContainerStructuralExpectation carries explicit object_id/kind_id/kind_version,
placement_before/transform_before. kind/version must be an accepted tuple, not
merely one of the historical version-1 entries. The before transform and all
inherited leaves retain their existing presence/domain rules.

EditContainerTopologyOp.guard must be present even if a particular internal
collection is empty. Exactly one effect is selected. RelocateObjectsEffect is
an intentionally empty message; a present empty message selects that effect,
while no effect is invalid. Unknown effect/semantic field is fail-closed under
the inherited wire preflight profile; never rely on protobuf last-one-wins.

All SectionContentEdit singular fields are present; expected_kind_version=1 for
this release. Both expected/replacement values must be valid SectionContent.

Repeated fields do not encode a distinct absent-vs-empty fact. Empty is accepted
only by the rows below; no presence wrapper is added to distinguish those cases.

## Keyed collection table

Every collection below uses canonical keyed-set ordering. Duplicate keys reject.
ObjectId comparison uses its unsigned 16 bytes, not the textual UUID spelling.
Each collection is bounded by the inherited 65,535 keyed-item ceiling; all
nested bytes also count toward the complete 32 MiB Operation envelope budget.

| Message.field | Empty | Key and canonical order |
|---|---|---|
| ContainerChildrenExpectation.complete_members | allowed (exact empty child set) | object_id |
| ContainerTopologyGuard.nodes | allowed only when required structural read-set is still complete | object_id |
| ContainerTopologyGuard.child_sets | allowed only when required affected-parent expectations are still complete | absent root first; then parent_id |
| EditContainerTopologyOp.relocations | allowed for introduce/release only if actual adoptions/frontier is empty; forbidden for relocate effect | object_id |
| EditContainerTopologyOp.order_only_edits | allowed | object_id |
| IntroduceWrappersEffect.containers | forbidden | ObjectRecord.id |
| ReleaseWrappersEffect.containers_before | forbidden | ObjectRecord.id |
| SetSectionContentOp.items | forbidden | object_id |
| SetSectionContentOp.geometry_basis | allowed for metadata-only edits; required coverage for changed boundary | object_id |

Each complete_members entry has present object_id and order_key. OrderKey is a
value, not the keyed-array sorting key. Guard and preimage overlap must agree.
Order-only IDs must not also occur in relocations. New wrappers are not duplicated
in existing-object relocations. Exact frontier, new-parent constraints, acyclicity,
references and all before/after guards follow P13 v0.2; array order cannot change
an operation's semantic result.

The 16 MiB ObjectRecord, 1 MiB generic UTF-8 and 1..32-byte OrderKey constraints
remain inherited. Arithmetic/length counters use checked accounting before
allocation. Do not split one command to evade limits. E01 checks forward and inverse
individually before a locally undoable command commits; it is not a wire field or
an extra constraint on ordinary headless replay.

## Additive scope

No old field is renumbered. No global Transaction, Frame, children member store,
HistoryId, local generation, selection or capability numeric bit is added.
Section opacity applies only to its own paint. Sticky remains 0..1 RichText child.
The new collection exceptions do not relax old operation nonempty rules.
