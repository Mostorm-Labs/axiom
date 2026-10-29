# Batch 3 — Confirmed Scope & Behavior

## Goal
Expose a consistent, read-only selection summary from the existing Runtime selection owner so Debug UI and action-bar consumers can make accurate decisions. This is a consumer/projection layer, not a second selection state owner.

## Summary fields
Expose only facts derivable from the current selection snapshot and semantic/scene state:
- selection revision/freshness token, only if the existing contract provides one;
- selected root IDs or safe count-only view according to existing API boundaries;
- root count and counts by canonical object kind;
- primary root, validated as a member of the normalized root set;
- aggregate selection bounds when all required bounds are available from one consistent scene generation;
- typed capability states for actions with real command owners;
- property aggregate states for explicitly supported properties.

Do not add a canonical schema or persist selection. Selection is transient Runtime state.

## Root count versus descendants
Root count describes selected operation roots, not all nested objects below Group/Section roots. If descendant counts are available, expose them separately. Never fold them into root count. Normalize through the existing selection-root resolver; if normalization cannot be done safely, report unavailable/invalid rather than fabricate counts.

## Primary selection
The primary root must belong to the current normalized root set. Preserve the accepted stable-primary policy. Do not choose a different primary merely to make an action appear enabled.

## Capability resolution
Represent action capabilities with explicit state and reason, not an ambiguous boolean. Reuse existing status vocabulary if available; otherwise use a small typed state such as `available`, `unavailable(reason)`, `not_implemented`, `stale_selection`, `invalid_selection`.

Only advertise `available` when a real command path exists and preconditions can be checked. Group/Ungroup, Wrap in Section, bulk edit, Snap and Alignment remain disabled/not implemented until their commands exist. This batch does not add mutations.

## Property aggregation
For properties explicitly supported by existing read APIs, return:
- `uniform(value)` — all applicable roots agree;
- `mixed` — applicable roots disagree;
- `partial` — property applies to only some selected roots;
- `unavailable(reason)` — values cannot be safely resolved/read consistently.

An empty selection is not a uniform value. Do not infer defaults unless the semantic contract defines them. Start only with properties already surfaced by existing read APIs. Add no new PropertyId, schema field, mutation command or cross-object edit behavior.

## Bounds
Use existing Scene/BoundsSystem authority, not UI recomputation or guessed AABBs. Tie the summary to a consistent selection revision and scene generation. If a required bound is stale/unavailable, report that explicitly rather than present a partial union as complete.

## UI consumption
Debug UI/action bar consumes the summary read-only. It must not own selected IDs, calculate kind counts via ad-hoc scene traversal, infer command support from selection count, invoke multi-selection commands against only primary, or show future actions as executable. The same summary should be reusable by product UI without making Debug UI the semantic owner.

## Refresh and lifecycle
Refresh on selection revision and relevant semantic changes (delete, kind/content, placement, transform/bounds) that affect summary values. Use existing events/revisions; avoid polling and duplicate registries. Do not silently mix old selection with new scene state.

## Non-goals
No bulk property writes, Group/Ungroup implementation, Wrap-in-Section mutation, Snap/Alignment implementation, full property inspector redesign, new selection owner, selection persistence, new object/schema/operation, or requirement to finish future object types before supporting delivered ones.
