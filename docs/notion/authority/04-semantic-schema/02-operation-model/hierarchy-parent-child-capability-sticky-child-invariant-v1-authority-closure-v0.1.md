# Hierarchy Parent/Child Capability and Sticky Child Invariant v1

Status = Current Authority mirror

Notion source = `3cb4c57a-590c-815a-b8fa-cd785a837da7`

Approval token = `APPROVE_B_AUTH_02_HIERARCHY_CAPABILITY_V1_OPTION_B`

This repository document is a mirror of the Notion Current Authority. It is
not a second independent authority.

## V1 capability rule

- Root: any released V1 `ObjectKind` is allowed.
- Group parent: any released V1 `ObjectKind` is allowed, with zero or more
  direct children.
- Sticky parent: only `RichText` children are allowed, with at most one direct
  `RichText` child. An empty Sticky is valid.
- Every other object kind cannot be a non-root parent.

Consequently, `Shape`, `Connector`, `Image`, `RichText`, and `Stroke` cannot
parent children; `Group` may parent any released V1 kind; and a Sticky may
parent zero or one RichText child. This authority does not introduce
`primaryChildRole`, child-order heuristics, a protobuf/schema field, a second
hierarchy, or an exactly-one Sticky requirement.

## B5 revalidation boundary

The historical B5 fixture assumption that a Connector may have hierarchy
children, including a legal Connector-to-child multiwave graph, is superseded
by this Current Authority. B5 production delete-closure semantics remain
unchanged; only test fixtures and their source-bound evidence are revalidated
against the current hierarchy capability rule.
