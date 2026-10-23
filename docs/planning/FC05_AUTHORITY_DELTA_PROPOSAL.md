# FC05 narrow Authority delta proposal

Status: **PROPOSED / NOT_CURRENT**. Source baseline:
`178173a03acd6f458991a69227ce3fdccaaffedd`.

## Source map and decisions

The human requested Connector + Sticky before FC01A/FC04 and confirmed:
straight + automatically avoided orthogonal routes; Sticky background + editable
RichText + move/resize + Undo/Redo + Connector target. Curves/manual bends are out.
This is the current product input. Its narrow execution/semantic effects are listed
here so a later acceptance cannot silently replace historical scope.

| Scope | Current source | Requested successor delta | Acceptance state |
| --- | --- | --- | --- |
| FC sequencing | `fc-feature-completion-execution-v0.3`; Notion `3f74c57a590c812a8260dcf23b659ed0` | Bring Sticky and only the named Connector slice before FC01A/FC04; do not cancel their later work or FC13 physical obligations | Human sequence/slice confirmed; successor record not yet materialized |
| Connectable tuples | Connector V1 Notion `3c44c57a590c817384dedc221b023bde`; repository mirror | Retain Shape/1, Image/1, Sticky/1; propose explicit Shape/2 triangle and released Shape/3 rectangle/ellipse/triangle-label tuples | Human decision pending; do not change validator |
| Triangle attachment deferral | `fc01-shape-triangle-semantic-v0.1`; Notion `3f74c57a590c8128b936d7491884ffb7` | Supersede only `attached_endpoint_triangle=DEFERRED_FC-06` for this first slice, with frozen perimeter/port math | Pending same compatibility decision and exact resolver design |
| Sticky hierarchy | Current zero-or-one direct RichText child authority | No change; use separate RichText child, not embedded Shape label or another text model | Existing Current rule consumed |

## Required successor content

If the compatibility extension is accepted, its versioned contract must publish the
exact tuple allowlist, Triangle auto-perimeter ray origin/intersection and all four
port coordinates. Shape/1 and Shape/3 rectangle/ellipse geometry remains the existing
analytical contract. Text label bounds do not become attachment geometry.

The successor must cover connected Shape label addition/removal/Undo, staged
Insert/Restore, whole-content SetConnectorContent and unsupported-reader rejection.
ObjectId, endpoint variants, port numeric IDs, hint presence/domain, routing enum,
wire tags, FreePoint connector-local meaning and cascade-delete/Restore do not change.
There is no implicit schema-version or generic reader capability upgrade.

Existing Shape/2 negative fixtures are historical truth for the old contract. They
must not be erased or silently regenerated. New accepted behavior receives separate
source/version-bound proof. Only applicable current regression expectations can move
after the explicit successor is accepted; old Gate outcomes remain untouched.

## Implementation hold and handoff

Do not edit `.aegis/authorities.json`, rewrite Notion Current, change historical
Goldens/Gates, or extend connectability in production from this proposal alone.
Prepare the successor after the human decision and completed P12/P13 geometry/
compatibility contract; validate its transition and independently review it before
P31. P14–P18 and P20 still need their own accepted completion targets. The existing
project-control manifests validate and their generated state matches, but they do
not yet track an FC05 package or Gate and therefore cannot certify FC05 readiness.
