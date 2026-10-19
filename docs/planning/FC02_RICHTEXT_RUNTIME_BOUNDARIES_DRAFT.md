# FC-02 RichText editor: Runtime boundaries (design draft)

Status: draft for discussion, 2026-10-19. No implementation or new stable ABI.
Baseline main: `ea135122f8e3266cafa433d69903f7950a5e6300`, merged via
[PR #148](https://github.com/Mostorm-Labs/axiom/pull/148), containing integration
head `6d8bf236468c3dd2a29d7ba213125df8ba12b840`.
Design branch: `codex/fc02-runtime-boundary-design`.

The user authorized the merge after schema-check alignment and explicitly
deferred legacy Web/Playground test wiring and Android compilation repairs.
Those known integration failures remain unresolved on this baseline. They are
not an FC-02 implementation result or evidence of full CI/physical acceptance.

## Purpose and scope

Add bounded RichText creation, editing, caret/selection, IME composition and
committed undo/redo to the shared Runtime consumed by Minimal Host. The user's
requested sequence is FC-02 → FC-03 → FC-01A → FC-04. This document records that
direction without rewriting historical execution/Gate records.

FC-02 consumes the existing RichText semantic model and the G4.7 layout stack.
It includes the text producer's object bounds and common selection capability;
FC-01A/FC-04 can then operate on the produced objects. It does not implement
lasso, advanced multi-object editing, Sticky/ShapeText, clipboard, character-level
collaborative merge, G5 performance work or the later G6 recovery programme.

## Sources and present implementation

- [ADR-0006](../adr/0006-richtext-first-class.md): Runtime text model,
  interaction-owned editing, thin platform adapters, derived layout.
- [Current G6 v0.3](https://app.notion.com/p/3f64c57a590c81869d6ee408382a9137):
  bounded initial editing moves to FC-02; later G6 consumes the same owners.
- [Current FC policy v0.3](https://app.notion.com/p/3f74c57a590c812a8260dcf23b659ed0):
  object producers supply their selectable geometry; selection stays local.
- `runtime/semantic/include/canvas/semantic/object_content.hpp` already has
  paragraphs, runs, styles and scalar-indexed `RichTextDelta` steps;
  `operation_payload.hpp` has `EditRichTextOp`.
- `runtime/text/include/canvas/text/text_layout.hpp` already provides one
  `RichTextLayoutService` and immutable `TextLayoutSnapshot`.
- `RuntimeComposition` owns one `RuntimeFacade`. `EditorSession`,
  `InteractionSessionManager` and `OperationSubmitPort` exist. Production
  `TextEditSession`, `EditorProjection` and the text input facade are still gaps;
  qualification-only text mutation helpers are not an editor API.

## State ownership

| Owner | Owns | Boundary |
| --- | --- | --- |
| Semantic | Canonical RichText document, validation, atomic operations/replay | No native widget, caret, IME composition or layout snapshot in canonical state |
| Interaction / EditorSession | TextEditSession, caret/range, composition draft, editor action resolution and undo intent | Only existing operation submission/history path may commit |
| Interaction projection | Bounded editable projection and offset maps tied to a target and revision | Reconstructible; never a second canonical TextDocument |
| Text layout | Shared shaping, line/cluster geometry, fonts and layout caches | No editing session or focus authority; no DOM/native parallel layout engine |
| Composition / Facade | Single owner queue, target validation, action receipts and readback | Host and Debug UI borrow the existing facade; no alternate mutation owner |
| Platform host | Native text service/DOM input adapter, physical focus, candidate window, native lifecycle facts | No canonical Paragraph/TextRun construction, RichTextDelta generation or direct editor access |
| Render / Scene | Derived text/selection/caret presentation and text object geometry | Existing frame opportunity/completion and resource-generation contracts |
| External data owner | Persistence and committed operation transport | Host IME input does not use `applyExternalOperation` to bypass local editing/history |

`TextEditSession` belongs under `runtime/interaction`, orchestrated with existing
sessions. It does not become a standalone editor in `runtime/text` or an app-side
controller. Reconcile the current minimal `EditorSession` integration seam before
adding a second session owner. No new Registry, canonical object kind or protocol
version is proposed.

Runtime allocates object, paragraph and operation identities. Keep full canonical
IDs across the facade/projection boundary. The present generic `OperationRequest`
also carries a legacy 64-bit identity and its `CanvasEditingRuntime::submit` path
needs explicit history/admission reconciliation before reuse for text editing;
neither truncating IDs nor calling that helper from a host is an acceptable
shortcut. Existing write admission, read-only/lock rules and atomic rejection must
apply equally to text and Debug UI control intents.

## Input, projection and commit flow

```mermaid
flowchart LR
    H[Native IME / DOM input] --> A[Host adapter: platform facts + offsets]
    A --> F[Composition-owned facade ingress]
    F --> P[EditorProjection: validate + map offsets]
    P --> S[Interaction TextEditSession]
    S -->|transient composition / caret| L[Shared TextLayout + editing overlay]
    S -->|committed editor action| O[OperationSubmitPort + history]
    O --> C[Semantic atomic apply]
    C --> R[Scene / render / readback]
    R --> H
```

1. A text tool/entry intent enters the existing Facade lane. Runtime resolves the
   target, attachment and editing session; the host receives a bounded projection
   and physical focus request. Exact creation/entry gestures remain a product
   decision below.
2. The adapter reports composition begin/update/end/cancel, replacement text,
   selection and focus facts with the projection token. Runtime converts offsets
   to `ParagraphId + Unicode scalar index`, validates the session and resolves
   normalized actions. The adapter never supplies a canonical operation.
3. Composition updates only change transient state and derived presentation.
   Cancel-before-commit emits zero canonical operations and leaves the canonical
   digest unchanged. Cancel does not undo earlier committed edits.
4. A committed edit becomes a validated `EditRichTextOp` (or the existing object
   insertion operation for creation) through the Runtime-owned submission/history
   path. A rejected operation does not publish success or advance the projection
   as though applied. Repeated terminal callbacks must not commit twice.
5. Accepted semantic change rebuilds/reconciles the projection and derived layout;
   receipts and snapshots let both the product UI and Debug UI observe the result.
   Presentation uses existing Runtime frame identities and feedback, including
   retry; hosts do not retire text overlays on their own.

The text ingress and projection readback are proposed typed additions to the
existing public facade/composition boundary. Their exact C++ shape and transport
encoding require a separate interface design; no header or stable ABI is frozen
by this draft. High-frequency editor input must retain ordering and terminal
events without routing each IME update through an inspector-style control draft.

## Identity, positions, layout and focus

- Validate runtime/attachment identity, full object ID, text session generation,
  projection/content revision and ordered event identity. Geometry freshness also
  includes layout/font and view/metrics generations. These are distinct axes;
  resizing a surface does not itself replace a text document.
- Old callbacks after cancellation, detach, target replacement or projection
  rebuild cannot mutate the new session. A stale mutation is rejected and the
  adapter is resynchronised. Behaviour during a conflicting remote edit must
  consume the existing interaction-conflict authority, not invent text CRDT rules.
- Native UTF-16 offsets, canonical scalar positions, UTF-8 layout cluster offsets
  and user-perceived grapheme boundaries are different units. Map explicitly and
  version the mapping with the projection. `A😀B`, combining marks, ZWJ emoji and
  paragraph boundaries must be covered before a platform adapter is considered
  usable. Invalid offsets must not silently split a surrogate or UTF-8 sequence.
- Consume G4.7 geometry for caret, selection and hit testing. If the current
  snapshot lacks a query, extend that service's derived geometry seam. Do not
  introduce a DOM measurement engine. `LayoutContext.width/revision` is container
  policy, never inferred from camera zoom or persisted as a new semantic field.
- Host maps screen/client coordinates to logical view once; Runtime applies
  view/world/object transforms. The reverse caret anchor is returned with
  geometry/metrics identity so the host positions the native candidate UI once.
- Separate object selection from text-range selection. Pointer, keyboard and IME
  routing has one accepted interaction owner at a time; text input and Debug UI
  must not also draw on the canvas. Existing multi-contact intent policy remains
  unchanged; entering text editing must use its interruption/focus rules.
- Tool change, blur, Escape, host detach and document close terminate or suspend
  the session according to the applicable product policy. Late commits are
  fenced. Do not infer commit-versus-cancel from a native blur event alone.
- Native editing controls may carry a bounded disposable text projection, but
  their local undo stack cannot become document history. Runtime remains the
  committed Undo/Redo owner; composition-local undo needs explicit routing.

## Decisions to resolve before coding the affected slice

| Decision | Design question |
| --- | --- |
| Creation | Click vs drag, default insertion style, and when an empty draft becomes an object |
| Entry/exit | Editing gesture and explicit commit/cancel semantics for blur, Escape and tool switch |
| Sizing | Initial wrap/container width and whether resize changes layout policy or object transform |
| History | One composition commit's undo unit, ordinary typing grouping and composition-local Undo |
| Formatting | Initial enabled inline/paragraph controls and object-selection vs range-selection targets |
| Conflicts | Exact resync/cancel rule for remote changes to active text; no implied concurrent text merge |
| Platforms | First usable Windows/Web slices and Android adapter timing; no inferred physical acceptance |

These are bounded questions for the next design pass, not permission to infer new
semantic fields or to copy POC-04 `TextTransaction`/UTF-16 canonical positions.

## Proposed implementation slices and minimum proof

1. Reconcile existing operations, history inversion, conflict notices and layout
   geometry; settle creation/exit and undo rules. Keep product-visible behaviour
   separate from implementation API choices.
2. Implement a Runtime-owned session/projection with FakeIME first. Prove repeated
   updates preserve canonical state, one accepted commit applies once, cancellation
   produces no operation, rejection resyncs, and stale tokens cannot edit new text.
3. Add shared geometry/caret overlay and text producer selection readback. Verify
   Unicode mapping, fixed-font layout parity, pan/zoom and DPI/resize coordinates.
4. Connect thin Windows and Web adapters to the same ingress/readback. Test IME
   terminal-event differences, focus/tool changes, Debug UI isolation, history and
   render retry. Track automated and human observations separately.

This is the starting design, not an FC-02 implementation or acceptance claim.
