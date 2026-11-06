# SDK 0.4.0 release

The user authorized merging codex/uiux-selection-handles through
`d79bb7fdde43ca99598efe7583dccec0d5665f03` and publishing a new SDK. Main
fast-forwards from published 0.3.14 to that cutoff. Both feature worktrees and
subsequent/uncommitted work remain available outside this release boundary.

This is a minor feature iteration: object clipboard, live partial erasing, creation
hover and interaction refinements. CanvasEditingRuntime owns clipboard capture,
identity remapping, placement, editing sessions and previews. Mutations follow
existing OperationEngine/History/publication admission; Render/Skia owns derived
feedback. Shell owns clipboard permissions, native focus and UI. The public JS
and caller-side Windows wrappers expose owned bytes and complete stamps only.

Object clipboard v1 adds Copy/Cut/Duplicate/Paste with versioned bounded bundles,
new identities, hierarchy/paint order/resource validation, atomic History and
stale/foreign/locked rejection. Table/Section creation presets extend existing
controls. API/ABI/transport versions remain 1; existing C/POD exports and canonical
Operation/Snapshot schema/conflict protocol remain unchanged. Clipboard bundle
version is independent of SDK version. Normal packages keep private owners hidden.

Refined handles, rotation affordance, default creation/placement, selected text
editing and hover ghosts reuse shared owners. Fading laser/eraser feedback and
live partial erase preserve cancellation and canonical commit boundaries. Erased,
transformed strokes retain navigation sources without replacing canonical data.

Release inspection found that the shared asynchronous controller declared the new
clipboard methods but omitted their implementation, affecting RN consumers. A
focused release fix delegates all four methods to the existing Session, copies
bytes/DTOs, validates offsets/size/foreign tokens and preserves captured full
stamps and stale error receipts. Three added transport regressions fail before
that fix (undefined methods) and pass afterward; no new editing owner is added.

All platforms/package versions are 0.4.0. Accepted Semantic/Skia SDK locks and
32/96MiB production budgets remain unchanged. No dependency stack rebuild.

Fresh local release checks pass: 41 selected native CTest groups (including
FC02Text, clipboard/live erase, preview/overlay, laser, navigation, Table, image
export and public object-identity isolation); 39 G5 Node tests; 15 controller
Node tests including the three new RED/GREEN regressions; 37 package tests;
15 workflow/server tests; strict Web/shared-controller/DOM-free consumer types;
119 Runtime boundary contracts; offline Semantic Store resolution; and strict
shadow compilation of 25 affected production C++ translation units.

The controller also passes real native Session/Runtime calls for owned clipboard
bytes, complete eight-field stamps, duplicate/paste/cut with atomic Undo, stale
and foreign rejection, pointer tools, snapshots, text, Sticky and Connector.
That fixture explicitly reacquires selection after Undo/Redo, which correctly
clears selection; the initial empty-selection rejection is a fixture correction.
It finishes the newly automatic Table text editor before testing table Undo.

Matching rebuilt JS/WASM/resources pass real Chrome external-consumer, selected
navigation and feedback checks; bounded dense DPR2/DPR4 pan/idle/freeze/recovery
workers keep source/artifact/document/selection/operation identities stable and
clean up their owned process families. Fourteen additional Chrome checks pass:
default interactions, placement, selected text click, creation hover, Demo actions,
object clipboard, live partial erase, eraser preview, transient feedback, masked
navigation, multi-stroke erase, affine erased-stroke navigation at DPR1/DPR2, and
Table. These are host-qualified release/regression checks, not universal latency
or physical-input acceptance.

Before reporting publication complete, hosted gates must bind the exact release
source, verify Web/Windows SDK and Demo archives, all six RN native targets, and
publication of all six numeric npm versions with latest. Tags/assets/versions
are immutable after any publication.
Historical full-suite failures and BLOCKED performance evidence remain retained;
this release does not declare full-suite, physical-input/IME, all-device latency,
100K/1M or formal G5/P34/Gate acceptance. Web checks do not establish Windows ABI;
Windows hosted consumers and each RN native target remain separate obligations.
