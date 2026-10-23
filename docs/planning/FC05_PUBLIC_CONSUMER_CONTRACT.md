# FC05 ordinary consumer and platform contract candidate

Status: **P15/P17_REVIEW_CANDIDATE / NOT_IMPLEMENTED**.
Role: public source API contract. Authority: confirmed scope plus the linked
Sticky, routing and Shape successor candidates. Objective: the same usable
features through RuntimeFacade on Windows/Web. Non-goals: stable ABI, new host
editor/router, private runtime access. Output: methods, receipts and temporal
ownership below. Gate: ordinary-consumer versus Debug UI behavior observations.
Handoff: exact source surfaces/fixtures to P20/P31 after review.

## Source surfaces

Add typed values in `canvas/runtime/connector_input.hpp` and `sticky_input.hpp`
under foundation; neither exposes Semantic `src/`, ImGui, Skia, Win32, DOM or
container mutators. RuntimeFacade methods (source names proposed):

- `submitConnectorIntent(ConnectorIntent)` and `readConnectorState(ObjectId)`;
- `submitStickyIntent(StickyIntent)` and `readStickyState(ObjectId)`;
- `readFeatureSupport()` for local available/admitted capability facts;
- existing `submitTextIntent`, `submitTextInput`, `readTextEditor`, font catalog,
  text geometry and product Undo/Redo continue to serve text/History.

The intent header contains full CanvasTargetKey, clientId/requestId. Session
actions additionally contain an opaque Runtime-issued token (runtime/attachment/
view/session, source/pointer/generation). Platform requests supply view coordinate
facts; mapping and canonical endpoint construction are Runtime work. A programmatic
connector create/reconnect may name endpoint intent through renderer-neutral typed
ObjectId/anchor/free-point values, with coordinate space explicitly Connector-local.
It never passes routed vertices as canonical truth.

Connector intent actions: Create (one-shot typed endpoints), BeginDrag, UpdateDrag,
CommitDrag, Cancel, ReconnectBegin, SetRouting, RetryRoute. Drag origin may name an
object affordance or view point. Affordance/nearest eligible perimeter candidates
and stale validation belong to Interaction, not the host. Snap radius is a proposed
12 logical pixels converted once by the current view; ties use distance then full
ObjectId, then port ID. Stable port selection persists only when explicitly chosen;
otherwise AutoPerimeter intent is used. Tiny drag (<4 logical pixels) cancels.

Sticky actions: Create (point/size/fill/text preset), Resize, SetFill, FitToText.
Move/delete/selection/History use existing typed product controls. Enter text via
the existing `TextIntent::Edit` or EditAtPoint naming the parent; Runtime resolves
its unique child or creates a transient empty draft. Session readback retains
both parent format target and child editing identity. A nonexistent child does
not get a persistent ObjectId before confirmed nonempty input. Format whole
selected Sticky text through the same TextFormatPatch/color/font catalog; mixed
range effect readback retains the FC02 “any enabled” policy. Empty format changes
only the typing preset until text is confirmed.

## Receipts and state

Each intent returns its request key, status, reason, session/object/child IDs,
semantic generation and accepted OperationId when applicable. States are
Unsupported, Queued, Observed, Applied, Cancelled, Rejected, Stale, Collision,
Backpressure and Retryable; do not call Queued Applied. Reasons include Lifecycle,
WrongOwner, TargetMissing, UnsupportedTuple, GuardConflict, InvalidGeometry,
MissingCapability, ResourceUnavailable, NoRouteInDomain and BudgetExceeded.
An identical retry observes the existing outcome; a changed payload with the
same request key is Collision. Preserve terminal receipt retention/ack behavior
of the existing facade lane; do not invent an unbounded event log.

Readback contains route status/key/polyline and pending reason, parent dimensions,
child ID, overflow/expand availability, text format target and layout identity.
Readback is immutable and scoped to the currently mounted target; stale/lost
snapshots cannot be mistaken for live writable state. A read-only consumer can
display status but cannot obtain mutable runtime or store references.

## Temporal flows and platform obligations

Owner pump validates target/lifecycle/request identity, plans intent, computes
bounded transient geometry, prepares canonical mutation/History, commits and
sets canonical frame demand. Only canonical acceptance yields Applied; derived
Scene failure requests recovery and cannot retroactively return canonical reject.
Route completion and caret changes request frames without extra Semantic edits.
Idle ready/failed jobs do not self-schedule endless frames.

Windows and Web use the same typed actions from tool UI and Debug UI. Native
pointer cancellation clears both session and input identity; no late Up can commit.
Loss/close cancels routing and preedit preview and rejects further writes in the
shared owner seam, including exposed Operation/History ports. Preserve valid
native composition completion/cancel recovery channels. Recovery cannot publish
half-Ready Composition/Runtime state. Surface/ViewId/generation/frame identities
come from Runtime; actual presentation feedback is a fact, not a host counter.

Touch retains source-separated batches, generation identity, one coordinate map,
promoted-mouse suppression and existing AutoIntent timing/slop. Do not introduce
rotation gestures. Windows native IME and Web composition/beforeinput keep FC02
native-confirmed-only semantics. Hosts do not measure text or construct delta/Undo.

Ordinary test consumer includes only public foundation/runtime headers and links
the supported Composition library; it performs create/connect/reconnect/routing,
Sticky text/resize/fit/fill, loss/recovery and Undo/Redo. Compare canonical state,
receipt states, child IDs and layout/route facts with the same Debug UI actions.
A private host-only shortcut is a failure even if both demos draw correctly.
