# AP-01: single canonical capability binding (acceptance proposal)

For Snapshot schema 3, derive the required set using the accepted content rules,
including resource-manifest-v1 and inherited feature requirements. Add section-v1
if and only if Section/1 is present. Encode the unique set in unsigned ASCII
lexicographic ascending order in the inherited CapabilityRequirementsV1.required_ids.

Set union removes repeated derivation of a requirement; it does not authorize a
reader to silently repair duplicate encoded IDs. A reader checks canonical bytes,
strict order, unique IDs, supported IDs and exact content-derived requirements.
Unknown, missing, extra or noncanonical encoded requirements fail closed, using
the inherited accepted profile. Case folding, locale sort and tail append are
not alternatives. Existing capability leaf size/framing/digest rules are inherited.

Example: resource-manifest-v1, section-v1, shape2-triangle-v1.
A connector requirement beginning with 'c' precedes the requirement beginning 'r'.

The existing semantic and checkpoint digest framing stays unchanged. Digests are
computed over the accepted canonical representation; do not silently sort and
rehash an imported inconsistent snapshot to make it appear historically valid.

Schema 1/2: preserve explicit legacy-no-manifest-v1 read/import-only admission;
no Section in those profiles. New FC-08 writer emits schema 3 with the explicit
manifest/recovery/checkpoint binding required by the accepted resource contract.

container-topology-v1 is an operation-log/writer requirement for opcode 18, even
for a Group-only state. It is not placed into state-derived Snapshot requirements
merely because a past operation used it. Opcode 17 requires section-v1. Opcode 18
also requires section-v1 where its records, targets or hierarchy use Section.
Legacy opcodes affecting Section similarly require the Section semantic support;
envelope 1/1 alone is never proof of client support.

Observed unsorted source output is implementation drift, not a second accepted
canonical profile. Existence and migration of actual persisted drifted snapshots
remain owned by explicit compatibility handling; this bundle neither asserts
that such data exists nor authorizes silent rewriting.
