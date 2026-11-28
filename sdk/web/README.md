# Axiom JavaScript SDK

Versioning uses `x.y.z`: `z` for small improvements and fixes; `y` for small feature
iterations (reset `z`); `x` for large feature iterations (reset `y` and `z`). A
release with multiple change levels uses the highest one. API/ABI and data-format
compatibility are declared separately in the contracts and release notes.


Import `createRuntime` from `axiom.mjs`. The SDK loads its matching modular WASM
engine and creates two canvases inside the supplied empty mount element.
Every instance has its own WASM memory, Runtime and input listeners. Serve all
files over HTTP with `.mjs` as JavaScript and `.wasm` as WebAssembly.

```js
import {createRuntime} from './axiom.mjs';
const runtime = await createRuntime({
  mount: document.getElementById('canvas'),
  permission: 'localSingleUser',
  onError: error => console.error(error.code, error.details),
});
await runtime.selectShape('rectangle');
// Native canvas input now creates shapes using the existing Runtime.
await runtime.selectTool('select');
// When finished:
await runtime.close();
```

Give the empty mount a nonzero CSS width and height. The SDK handles drawing
buffers, DPR, pointer capture, resize and WebGL context restoration. Run
`python serve.py`, then open `/example.html` for a consumer using only this public
module. No Emscripten or C++ compiler is required to consume the built JS package.
The distribution can also be packed locally with `npm pack --ignore-scripts`.
The demo canvas fills the viewport; tools, properties and status float above it
and do not change its metrics. A mount resize reveals/clips the current pixels
at their existing logical scale and origin. The SDK coalesces continuous resize
observations until 120 ms of inactivity, then updates both surfaces to the latest
metrics. Starting a pointer contact flushes a pending resize before coordinate
collection. Resizing does not pan/zoom the Runtime camera or change the document.
The SDK clips its display layers to the mount and restores the mount's prior
inline overflow value on close. A retained frame bridges the buffer replacement
until qualified presentation; it does not emit a canonical completion receipt.

Its whitelist includes the internal loader; release verification exercises the
extracted npm package in Chrome as well as the ZIP. The package is named
`@mostorm/axiom`; `publishConfig` binds it to the private Nearhub registry and the
`latest` distribution tag. `private: false` enables publication; access control
is enforced by the private registry.
The three main artifacts are `axiom.mjs`, `axiom.wasm` and `axiom.data`.
The Emscripten loader lives in `internal/axiom.mjs`; applications import only
the public root `axiom.mjs`. Keep the `internal/` directory, both input modules,
declarations and the `licenses/` directory alongside them. For bundlers that relocate the
data/WASM, pass `assetBaseURL` pointing to the deployed engine assets.

The Web Demo groups primary tools in row one, active-tool subtypes (shape, brush,
selection or eraser mode) as flat buttons in row two, and board actions in row
three. Both tool rows scroll horizontally when their choices exceed the width. Object
properties/actions remain contextual. Creation-tool appearance controls update
new-object defaults even if a previous object remains selected. For new rectangles,
ellipses and triangles, the Demo pairs the fill with an outline of the same hue.
Choosing a stroke color in the creation controls keeps that choice for subsequent
shapes, including after switching tools. Object actions
delegate to the existing root SDK: text/cell Edit, sibling-level front/back,
Lock/Unlock, and Delete. Front/back currently requires a single selected object
and retains its parent/group. A locked selection exposes only Unlock in its
object actions; its editing properties are hidden. Global navigation/history
are separate board actions. Backspace/Delete invoke `deleteSelection` outside
text/IME, HTML inputs and modal dialogs, subject to Runtime availability.
Object Copy/Cut/Duplicate/Paste use shared Runtime clipboard v1. `copySelection()`
is a synchronous read returning owned `Uint8Array` bytes, object/root counts and
an owner-bound full selection `Stamp`; it creates no operation or History.
`cutSelection(capture.stamp)` validates the complete captured basis before one
atomic canonical deletion; write the OS clipboard successfully before calling it.
`duplicateSelection({offset:[24,24]})` and `pasteObjects(bytes,{offset:[24,24]})`
allocate all new identities in Runtime and atomically create/select new roots.
Offsets are world units; omitted offsets default to `[24,24]`. These queued
commands resolve after application, without claiming persistence or cloud sync.
`createController` exposes the same methods asynchronously.

The versioned opaque bundle includes selected subtrees, world root placement,
local child placement and exact resource descriptors. Internal connector targets
are remapped; attachments to objects outside the copied subtree become free
endpoints at their captured positions. Text paragraphs, Table axes and erase
masks receive fresh identities; sketch/ink seeds and appearance are preserved.
Paste uses the current group scope; Duplicate retains original parents. Cut uses
the canonical deletion closure (including dependent connectors); cutting a Section
removes its subtree, while the existing Delete action releases its boundary.
Locked/hidden source subtrees, active edits, stale/foreign cut stamps, unsupported
versions and invalid data reject. Clipboard v1 is bounded to 4096 objects and
16 MiB. The destination must already have matching active resource descriptors
through its resource owner. Missing image/font/brush resources reject the entire
paste with `CLIPBOARD_RESOURCE_UNAVAILABLE`; blobs are not silently substituted.
No internal object, store, codec or renderer handles cross this interface.

The Demo binds Ctrl/Cmd+C/D/X/V outside native text, HTML inputs, IME and dialogs.
Copy/Cut events use `application/x-axiom-objects` plus a version-prefixed plain-text
fallback; buttons use the browser Clipboard API. Clipboard write denial preserves
the source. Native text clipboard events retain their existing adapter path.
Cut/duplicate/paste each create one History item and follow existing operation
publication and backpressure admission. Buffers returned from Copy are independent
owned copies; Paste copies its input before queueing. All calls retain existing
owner-thread, callback non-reentrancy and close/lifecycle rules.

To install from the private registry, put the following in your application's
`.npmrc` and set `NODE_AUTH_TOKEN` in your environment. Do not commit credentials:

```ini
@mostorm:registry=http://npm.nearhub.cc/
//npm.nearhub.cc/:always-auth=true
//npm.nearhub.cc/:_authToken=${NODE_AUTH_TOKEN}
```

```sh
npm install @mostorm/axiom@0.0.1
```

```js
import {createRuntime} from '@mostorm/axiom';
```

Use a browser ESM bundler or deploy the package files with their relative layout
preserved; installing this browser SDK does not provide Node.js rendering. For
repeatable deployments, pin the exact version instead of the moving `latest` tag.
The npm package retains the Release `manifest.json` and its source/file identities.

API version 1 uses 32-character lowercase hexadecimal object IDs and decimal
strings for 64-bit revisions/tokens. Command promises resolve only after the
Runtime's result is available; this does not acknowledge durable storage.
Application callbacks are deferred and must not replace native IME results.

Local drawing tools are one-shot for shapes, lines and arrows: a successful
pointer creation switches to point selection and selects the new object. Sticky
creation opens its text editor; Table creation opens its first cell. Image import
selects the image in point mode even if a lasso was used previously. These local
interaction transitions do not run for restore, remote application or Undo/Redo.
The Demo chooses its round brush at startup; the SDK's initial brush is unchanged.

With the Text tool, a click opens an empty editor at the default 320 world-unit
wrap width. A horizontal drag shows a dashed, presentation-only box; release
opens the editor at the dragged width (minimum 24 world units). Text height grows
with its layout. Cancelling the drag creates no object or operation. An empty
editor remains a draft until actual text is inserted, including Sticky text.
Enter inserts a paragraph; Ctrl+Enter or Cmd+Enter finishes editing. Clicking
outside finishes and consumes that click. A successful finish switches to point
selection and selects the text object, Sticky parent or Table. Input-method Enter
remains owned by the IME, and pending or failed native completion does not count
as a successful finish. Text still uses the existing incremental History/Operation
path; finishing an editor does not itself acknowledge persistence.

In point selection mode, the first click selects a text-capable object. A later
single click on that selected object's body opens its existing text editor on
release: Shape labels, RichText, Sticky text and the clicked Table cell. Opening
an empty editor does not publish an operation. A selected Table also supports
clicking another cell to finish the current editor and select that cell without
editing it. Click the selected cell again to enter editing. This selection-first
rule also applies when changing cells while no editor is open. Pending native
composition must finish successfully before another cell can be selected.
Stationary clicks and up to 4 logical pixels of pointer jitter do not move or
snap the object. Dragging, resize handles, connector ports, Shift/multiple selection, locks,
Group scope and viewport navigation retain their existing priority. A browser
double-click does not restart an editor already opened by the pointer gesture.

Creation tools show a translucent default object under the mouse before pressing:
rectangles, ellipses and triangles use 120×80 world units, Sticky uses 200×200,
and Table uses 2×2 cells. Click creates that default; drag specifies custom size
or Table counts. The ghost is drawn at 25% of the object's opacity using the same
shared Skia geometry and style. Runtime reserves the prospective identity/style
once and reuses them on creation; hovering publishes no operation or History.
A selected Shape, Sticky or Image has four side ports and a rotation handle outside its upper-right corner.
Hovering a port previews the connected sibling. Clicking an Image port creates a connected rectangle. Each click first checks the footprint 148 board units straight out from the source port. An empty footprint with a reachable port is reused even if an earlier sibling was moved. An occupied footprint or blocked destination port sends the new object to the nearest free slot on the established row (top or bottom port) or column (left or right port). The row or column with the most visible targets connected from that same port to the opposite port takes precedence, including targets outside the viewport. A tie favors the row or column nearest the default placement. Hover shows the planned slot, and each node and its connector form one undo step.
Image crop mode shows the crop controls. Tables expose resize handles at the boundary. Leaving the canvas/point, changing
tools, editing text, navigation or stale context removes the preview. Ghosts are
presentation only and are excluded from snapshots and image export.

`selectShape('table')` and `selectShape('section')` arm shared Runtime creation
tools. Selecting a tool creates no object. A Table click creates 2×2 cells;
dragging adds rows and columns in 120×48 world-unit cells, with a 2×2 minimum.
Creation is bounded by the existing 64-axis/1024-cell limits. Programmatic
`table.create` defaults to 2×2 when counts are omitted and retains explicit sizes. A Section click creates the
existing 320×240 default; dragging specifies its range. Preview and cancellation
are Runtime-owned, and successful creation returns to point selection.

Confirmed Table text edits automatically grow the edited row when required by
its content and padding. Rows do not automatically shrink. Text and the matching
row resize use existing operations in one atomic local intent and one Undo entry;
the existing independent remote-operation protocol and conflict guards remain.
Ordinary byte image import centers the image on the current mounted viewport,
including pan and zoom. Explicit prepared source-profile image transforms and
image replacement retain their existing semantics.

Document backgrounds use complete value DTOs:

```js
await runtime.setBackground({
  mode: 'dots', // 'plain', 'dots', or 'grid'
  color: [1, 1, 1, 1],
  patternColor: [0.7, 0.75, 0.8, 0.55],
});
const background = runtime.readBackground();
```

Color components must be finite numbers in `[0,1]`; background alpha must be `1`.
`setBackground` replaces the complete value through the existing owner-thread
control/Operation path. A no-op creates no History entry or outbound Operation.
Background changes participate in Undo/Redo; compensation rejects if a remote
background edit conflicts with the captured intention. Snapshots and Operation
replay retain the settings; external Data/Axis still owns actual persistence and
network delivery. Non-default settings require `document-background-v1` reader
support; older snapshots retain their white background and legacy encoding.
The pattern is anchored at world origin with 24-world-unit spacing, adaptively
thinned at low zoom. It is not an object and does not affect content bounds,
selection or erasing. The demo applies draft controls only when Apply is pressed.

The Product Shell / Axis bridge adds `subscribeOperations`, `subscribeState` and
their returned disposers. Operation subscription attaches volatile custody;
`data.readNext`, `data.release`, `data.status` and `data.reportStatus` separate byte
handoff from actual external durability/sync. Callbacks copy/transfer promptly;
after the data owner accepts the complete prefix, release through that packet.
A full queue rejects local commits. Controlled close reports `PUBLICATION_PENDING`
until custody is released. `applyRemoteOperations` reports independent mixed
outcomes, never a generic transaction or local echo. `resources.list/readBytes`
allow an external data owner to discover and retain actual resource bytes.
See `docs/planning/sdk/shell-axis-integration.md` in the source repository.

## Connector editing

Choose the line tool for a free straight segment. The connector tool and quick-create ports initially use orthogonal routing with an end arrow. Each tool remembers its own settings. The Demo exposes route and arrow direction in the properties row.

A selected connector has two endpoint handles. Drag a handle to a port for a fixed attachment, to a target outline for automatic attachment, or to empty space for a free endpoint. The preview is local. Release commits one undoable change. Escape or pointer cancellation discards the preview. A plain click keeps the current binding.

`readConnector(id)` returns endpoint identities, arrow flags, routing status and two paths in connector-local coordinates. `route` connects the resolved anchors. `displayRoute` supplies the visible polyline used by the canvas and image export. Its attached ends are inset by up to four board units, limited to a quarter of the adjacent segment. Free endpoints use the supplied point. Apply the returned `worldTransform` once to map either path to board coordinates, including every parent transform. `sceneRevision` identifies the published projection. In a tight gap the two attached shapes can share a routing channel if it accommodates their painted bounds and the line width. Facing ports on the same axis can connect directly across a smaller visible gap. If another shape borders a fixed port, the bend can use the visible slot between their painted edges. An attached arrow first gives up some tip spacing to retain its normal size. An actually covered port still reports a routing warning. When an intervening obstruction shortens the last segment at an otherwise open port, the router tries a longer approach within the same avoidance budget. Two full-size arrows at the same port share their visible tip. Orthogonal connections between ports facing the same way use an outer lane with a preferred 32-board-unit lead from both ports, including when the objects align. Offset ports facing opposite ways also keep a preferred 32-unit approach when a middle lane would squeeze the arrow. The route uses the space between the shapes or goes around their outside edges when they touch. At diagonally adjacent corners it can turn on both sides of the narrow passage to preserve the terminal runs. Perpendicular ports use a single elbow when possible. When a bend would cut through an attached shape, the line can go around its far face with a three-bend route. Obstacles limit the available approach. The solver prefers a clear route. If the bounded search cannot find one, the middle of the line can cross intervening objects. It favors fewer obstacle entries, then less travel inside their clearance bounds. A valid crossing renders normally and reroutes when a clear passage opens. Orthogonal branches sharing a fixed port and facing distinct targets in the same row or column prefer a common halfway line. With enough room for both approaches, that line can pass through intervening shapes even when an individual branch could detour. Endpoint clearance still applies. Moving a shape over a fixed port preserves its binding and can produce a dashed route warning until a path opens.

`setConnectorEndpoint({objectId, side, endpoint, generation})` changes one end by ID. Pass the decimal generation from the last read and `'start'` or `'end'`. A free point uses `{point:[x,y]}` in connector-local coordinates. A fixed attachment uses `{objectId,port}` with ports 1–4 in top, right, bottom, left order. An automatic attachment uses `{objectId}` and may carry a normalized perimeter hint. Pointer drags near a fixed port use its port ID. A drop elsewhere on the outline stores a directional hint in the target's local space, so the preview and committed route land on the same edge. During a drag, small rings show the target's four ports and the highlighted ring marks the snap point. Programmatic attachments without a hint continue to face the other endpoint automatically. The receipt returns `changed` and the resulting generation. A stale generation rejects with `STALE`. A locked line or newly selected locked target rejects with `LOCKED`. The last pointer edit is reported in `endpointEdit` on the edited connector.

`setConnectorRouting` and `setConnectorDecoration({startArrow,endArrow})` with an object ID edit that line. Without an ID they update tool defaults, available through `readConnectorDefaults()`. Creating a new connection uses its common writable container. Reconnecting an existing line preserves its parent.

## Shell-owned canvas anchors

`createAnchorLayer({runtime})` returns the Web Runtime's anchor layer. Each
anchor has a nonzero Id128 and a rectangle `[x, y, width, height]` in world units.
The layer maps every rectangle into the canvas using the transform of the
submitted display, including navigation and editing feedback. `readTransform()`
returns a copy of the six CSS matrix coefficients in logical pixels, or `null`
while the layer has no display to align with.

```js
import {createAnchorLayer} from '@mostorm/axiom';

const anchors = createAnchorLayer({runtime});
const {id} = anchors.create({rect: [240, 120, 560, 315]});
const player = document.createElement('iframe');
player.title = 'Review video';
player.src = 'https://www.youtube.com/embed/M7lc1UVf-VE';
player.allowFullscreen = true;
Object.assign(player.style, {width: '100%', height: '100%', border: '0', pointerEvents: 'auto'});
anchors.getElement(id).append(player);
anchors.setRect(id, [200, 100, 640, 360]);
```

`get`, `list`, and `setRect` return copied rectangle values. `getElement` lends
the layer element for the anchor's lifetime. Put Shell content inside this
element and use `setRect` to change its position or size. Layer methods run
synchronously on the browser's DOM thread. Content is pointer-transparent by
default, while interactive children can opt into input with `pointer-events: auto`.

Anchor IDs are unique within the layer. Supply a saved `id` when recreating an
anchor, or omit it to generate one in a secure context. `remove(id)` detaches the
corresponding content. `dispose()` and a successful `runtime.close()` detach the
entire layer. The Shell owns content, permissions, and persistence. Store IDs,
rectangles, and content references alongside your document ID when persisting.
Canvas erasers affect canvas objects only. Canvas History, Snapshots, Operation
packets, fitting, and image export use the canvas document.

In the Demo's 画板 toolbar, use `插入 YouTube` or `添加评论标记`. The `移动`
handle repositions the video, and the lower-right handle resizes it. Use
`缩小` or `放大` to zoom the canvas with its anchors. Turn off `视频交互` to
pass gestures and eraser input through the video to the canvas.

| Area | Public methods |
|---|---|
| Tools and style | `selectTool`, `selectShape`, `setSelectionMode`, `brushPresets`, `selectBrush`, `setInk`, `setEraser`, `setAppearance` |
| Document background | `readBackground`, `setBackground` |
| View and selection | `readState`, `readSelection`, `panBy`, `zoomAt`, `fitContent`, `fitSelection`, `fitObject`, `resizeSelection`, `transformSelection`, `deleteSelection`, `lockSelection`, `unlockSelection`, `orderSelection`, `setSelectionVisible` |
| Text and IME | `text.read`, `text.create`, `text.edit`, `text.editAtPoint`, `text.replace`, `text.select`, `text.selectAll`, caret/delete/paragraph/width methods, `text.format`, `text.fonts`, `text.fontStatus`, `text.completeComposition`, `text.finish`, `text.escape`, `text.resync`, `text.retry` |
| Image and crop | `importImage`, `replaceImage`, `resizeImage`, `readSelectedImage`, `beginCrop`, `readCrop`, `updateCrop`, `commitCrop`, `cancelCrop` |
| Containers | `createSection`, `wrapInSection`, `releaseSection`, `reparent`, `readSection`, `resizeSection`, `renameSection`, `setSectionTitleEditor`, `selectSection`, `selectSectionChildren`, `group`, `ungroup`, `enterGroup`, `leaveGroup` |
| Connectors and Sticky | `createConnector`, `setConnectorRouting`, `readConnector`, `createSticky`, `readSelectedSticky` |
| Multiple objects | `multi.read`, `multi.readValues`, `multi.setPermission`, `multi.setProperty`, `multi.setStrokeWidth` |
| Document and resources | `captureSnapshot`, `applyRemoteOperation`, `resources.token`, `resources.publish`, `resources.receipt`; startup `restore` and `initialResources` options |
| Lifetime and events | `close`; `onOperation`, `onError`, `onChange`, `onFrame` options |

Section creation, wrapping and renaming accept a single line of up to 50 Unicode code points.
Read the limit from `readSection(id).titleMaxCharacters`. Invalid SDK requests reject before publishing
an Operation. Existing Section/1 snapshots and history keep their original metadata. The Demo converts
line breaks in pasted text to spaces, completes with Enter and lets the IME confirm a candidate first.

A short caption fits its text. An overflowing caption spans the Section's width and ends in an ellipsis.
A selected Section keeps the fitted caption and exposes only boundary resize handles.
The inline editor expands to show the full text. Exports use the fitted caption.
For a drawable title, `readSection(id).titleFrame` provides the displayed frame in local units.
Its affine `toWorld` maps the frame's upper-left corner into document space, including ancestor transforms.
`width` and `height` bound the header, `insetX` and `insetY` locate the text, and
`textWidth`, `textHeight` and `ellipsized` describe the visible line. `fontId`, `fontSize`,
`baseline` and `lineHeight` describe its type. Position inline controls with the submitted camera transform.
The frame is null when the Section, layout or title is unavailable.
Call `setSectionTitleEditor(id, mode, readState().selectionRevision)` when an editor takes focus.
Both `inline` and `panel` hide the boundary resize handles for the single, selected, writable Section.
An inline editor requires point selection and supplies its own glyphs. The panel editor keeps the canvas caption visible.
Release focus with `closed` and the captured revision. Releases are idempotent and scoped to the selected ID and revision.
Invalid targets and stale opens reject with a Section command receipt. Focus changes are view-local and produce
a no-change receipt without publishing an Operation. Export and snapshots use the committed caption.
Draft text stays in the shell until `renameSection` commits the metadata change.

Single-object appearance/size/transform methods reject a multiselection. Use the
explicit `multi` path for supported batch properties. To write opacity, read
`const p = runtime.multi.read()`, inspect `p.properties['3']`, obtain the user's
confirmation of its eligible/excluded scope, and submit
`runtime.multi.setProperty(3, {type:'float', value:0.5}, p.stamp, p.properties['3'].eligible)`.
A changed stamp rejects the request; refresh and retry explicitly. Unconfigured
and unavailable permission contexts fail closed.
Stamp and resource token DTOs include an instance owner tag; preserve the full
value when retaining it for asynchronous work. Another Runtime rejects it even
when native engine counters or document contents happen to match.

`zoomAt(x,y,scale)` takes a positive absolute target scale. The camera limits zoom
to 10%–300% for commands, wheel and pinch gestures, and fitting content, selections or objects.
Zoom keeps the world point under the anchor in place. Fit centers the target at the resulting scale.
`setAppearance` merges only
the supplied fields into the current single-object/future-object appearance.
Connector routing defaults to orthogonal; a clear aligned route can contain one
straight segment. Programmatic Sticky/Connector creation accepts an optional
Id128, otherwise the JS application layer generates a UUID.

Snapshots require the data owner's `generation`, exact decimal `documentRevision`
and canonical `recoveryFrontier`. The returned bytes and `checkpointDigest` are
owned JS values. Pass the same metadata/digest and a nonzero resource binding to
`createRuntime({documentId, restore:{bytes, metadata}})` in a fresh mount. Resource
bytes are supplied separately through their full current binding tokens. Restore
does not recreate History or the pre-checkpoint applied-operation ledger: replay
only operations after the matching frontier. An `onOperation` byte delivery means
canonical local apply, with no persistence guarantee; remote apply does not echo
through that callback. `onFrame` describes completed GPU work, not physical scanout.

Chrome/WebGL2 is the exercised browser profile. The SDK includes synthetic native
input/composition tests; actual OS IME, physical touch and other browsers still
need device qualification. The known D07 Protobuf/Abseil allocation-failure hang
remains unresolved. These limitations prevent claiming production readiness.

The published `20261031-4fc3ebfe` preview contains the preceding JS SDK. The new
Shell/Axis extensions require a matching rebuilt wrapper/WASM and are not added
to previously published immutable assets. See the checked declarations for the
exact supported API. Unsupported operations reject with `AxiomError`.

Image export (`imageExport: 1`, introduced in `1.0.0-preview.4`; use matching wrapper/WASM versions):

```js
const image = await runtime.exportImage({format: 'png', scale: 2});
// Caller owns image.bytes; Shell chooses filename/storage/download behavior.
const blob = new Blob([image.bytes], {type: image.mimeType});
```

`exportImage()` captures committed visible content, including objects outside the
viewport and negative coordinates. It excludes hidden objects, selection handles,
caret, provisional creation, IME preedit and drag/crop previews, and does not finish
text or change the document, History, selection, camera or outbound queue. Empty
boards use the current viewport. The shared Runtime renders an independent raster
target through the existing Scene/Skia path; it does not capture the live canvas.

Options: `format` is `png` (default), `jpg` or `jpeg` (both JPEG); `scale` is finite
positive pixels per world unit (default 1); `padding` is integer output pixels
0..1024 (default 16); `quality` is 0..1 (default .92, JPEG only). `bounds` optionally
selects world `[left,top,right,bottom]`; otherwise complete visible content bounds
are used. Dimensions round up. Returned `bounds` include padding and rounding;
`generation` is the exact committed generation as a decimal string. Optional
`generation` rejects a stale query with `STALE_TARGET`.

`background` is `document` (default), `transparent` (PNG only), or an opaque RGBA
`Color` for a solid override. `includePattern: false` suppresses document dots/grid.
Transparency excludes all background pixels. Transparent JPEG requests reject.
Object erase masks reveal the selected background, without transparent holes or
black JPEG patches in opaque exports.

The result contains an owned `Uint8Array bytes`, `mimeType` (`image/png` or
`image/jpeg`), `width`, `height`, `bounds`, and `generation`; no heap views, rendering
objects or native owners escape. The query is serialized on the Runtime owner
thread and rejects closed, unavailable, reentrant or conflicting publication calls.
The Windows caller wrapper offers `export_image(options)` over the unchanged JSON
C ABI: `Response.bytes` is caller-owned and `Response.value["value"]` contains the
same metadata. No new DLL exports or internal headers are required.

Dimensions are limited to 8192 per axis and 16,777,216 total pixels, with at most
128 MiB encoded bytes. `EXPORT_TOO_LARGE` rejects before allocating the raster;
reduce scale or request smaller bounds. Missing/failed image or required font
resources reject with `RESOURCE_UNAVAILABLE`; export never saves placeholders.
Invalid options reject without partial bytes, and render/encoding failure
reports `EXPORT_FAILED`. This version performs synchronous raster/encoding work;
large exports can briefly occupy the owner thread. Web verification does not
establish Windows DLL/device qualification.

## Tables

`runtime.table` is the public Table/1 capability. Create an independently stored
leaf object; its cells do not become separate selectable document objects:

```js
const {objectId} = await runtime.table.create({rows: 3, columns: 3, x: 40, y: 40});
const table = runtime.table.read(objectId);
if (table.available) {
  const cell = {objectId, rowId: table.rows[0].id, columnId: table.columns[0].id};
  await runtime.table.edit(cell);
  await runtime.text.replace('Hello');
  await runtime.text.finish();
  await runtime.table.setCellStyle({...cell, fill: [0.9, 0.95, 1, 1], format: {bold: true}});
  await runtime.table.insertRow({objectId, index: 1});
  await runtime.table.setColumnWidth({objectId, columnId: cell.columnId, size: 160});
}
```

Rows/columns have Runtime-allocated stable IDs; indices describe insertion position
only. Cell readback is row-major and contains owned text/style values, not internal
RichText or scene objects. `read()` without an ID reads the single selected table.
Commands accept an optional exact decimal `generation`; an omitted generation is
captured at submission, before entering the command queue. Refresh after stale or
conflicting results. `TABLE_REJECTED` preserves canonical state and History.

Methods: `create`, `read`, `select`, `edit`, `navigate(1|-1)`, `insertRow`,
`insertColumn`, `removeRow`, `removeColumn`, `setRowHeight`, `setColumnWidth`,
`setCellStyle`, `setGridStyle`. Cell methods use `objectId,rowId,columnId`; axis
methods use `objectId` and the corresponding stable axis ID. Insertion takes
`objectId,index`. Size methods take `size`. Cell styles accept `fill` and `format`
(color, fontSize, font, bold, alignment). Grid style accepts `color,width`.

Limits: 1..64 rows/columns, at most 1024 cells, dimensions 24..16384 world units,
text at most 1 MiB per table; grid width 0..16 and font size 1..512. Cells wrap with
6-unit padding and clip at the stored row height; confirmed text editing grows
that height when necessary. The last row/column cannot be removed.
Select a cell, double-click or press Enter to edit; Tab/Shift+Tab navigates while
not editing. Drag internal row/column boundaries to preview size, release to
commit, or cancel the pointer to discard. IME uses the existing native fact lane.

Table edits, Snapshot restore, Operation transfer, conditional Undo/Redo and
PNG/JPEG export use shared Runtime owners. Version 1 conservatively rejects an
edit or compensation if its captured table content conflicts with remote changes;
it does not promise automatic merging of independent concurrent cell edits.
Table snapshots require `table-v1` capability; unsupported readers reject instead
of dropping table data. Grid paint version 1 is independent of boundary geometry;
future hand-drawn styles require an explicit version/capability extension. They
are not exposed by this version.


## Sketch appearance v1

Check `readBuildInfo().features.sketchStyle`, `patternFill` and `appearancePreview`
(all `1`) before offering these controls. `runtime.appearance` edits Shape
(rectangle/ellipse/triangle), Connector/line/arrow and Sticky plates. With
`features.sketchExtended === 1`, it also edits VectorPath, shared Table grid
roughness and Section document boundaries. Group itself is excluded and never
recursively edits its children. Images, text and freehand strokes keep their
existing owners. Table grid paint remains in `table.setGridStyle`, with explicit
per-parameter eligibility as documented below.

```js
await runtime.appearance.setDefaults({enabled: true, roughness: 1});
const observation = runtime.appearance.read();
await runtime.appearance.patch({fillKind: 'hachure', gap: 8, hatchWidth: 1}, observation);
const {token} = await runtime.appearance.begin();
await runtime.appearance.updatePreview(token, {roughness: 1.75}); // any number in [0,3]
await runtime.appearance.commit(token); // one atomic Operation / History entry
// Alternatively await runtime.appearance.cancel(token).
```

`read()` returns canonical values, per-parameter `mixed` names, eligible/excluded
roots, per-object owned values and a complete opaque `stamp`. Seed differences do
not make roughness mixed. `fillEligible/fillExcluded` separately describe plate
fill applicability, including Connector exclusions. Patches keep unmentioned
fields and each object's seed. The observation argument is captured at call time
when omitted. An explicit old observation rejects after selection, generation,
scene, permission or lifetime changes; it never retargets a different selection.

`begin()` captures exact targets/preimages. Updates accumulate in one local preview
and publish no canonical Operation. `commit()` publishes once; unchanged commits
and cancellation publish nothing. Export and `read()` use committed content during
preview. Do not keep a token after commit/cancel, resize/rebind, selection/document
change or close. Foreign, malformed or stale tokens reject and clear the preview.
SDK calls are owner-thread queued and reuse normal close/reentry/receipt rules.
`applied` is Runtime application, not external persistence or cloud acknowledgement.
Tokens and stamps are instance-scoped owned DTOs; retain every identity component.
All uint64 seed/stamp/serial values use decimal strings, never JS Number.

`enabled:false` keeps an allocated seed; enabling again and changing roughness,
color, opacity or fill reuse it. `roughness:0` keeps the seed and regular outline;
it does not disable pattern fill. `reseed:true` explicitly chooses another stroke,
with normal Undo/Redo. `styleVersion:1` is read-only, distinct from SDK version.
Copying canonical objects preserves appearance. Unsupported versions/readers reject.
Snapshot v3 requires bit128 plus `sketch-style-v1` / `pattern-fill-v1` requirements;
no historical descriptor is rewritten. Operations use the extended closed property
codec; consumers must reject unknown properties rather than discard them.

`fillKind` is `none`, `solid`, `hachure` or `crosshatch`; colors are RGBA `[0,1]`.
Pattern parameters: angle `[-180,180]` degrees, gap `[2,1024]`, hatchWidth `[.1,64]`;
resolved defaults are `-45`, `8`, `1`. Density/width do not track outline width.
On existing plates, pattern parameters require a pattern fill. `strokeWidth` is
`[.01,1024]`, opacity `[0,1]`; `dash:[]` clears dashing, otherwise provide an even
list of at most64 positive lengths. Dashed outlines use one pass. Closed paint
clear semantics remove their color; enabling a missing paint resolves the regular
palette (Sticky's absent plate remains yellow). Seedless future defaults retain
color/density/width preferences while disabled and do not enter document Undo.

`readDefaults()` and `setDefaults(patch)` address future creation separately.
Runtime reserves a transient seed for creation preview and materializes that same
seed at commit. Preview/committed rendering/export use shared C++ paths, never a
Shell drawing implementation. Generation is bounded:8192 hatch segments,32768
commands,4096 connector segments; over-dense hatching uses a deterministic,
origin-aligned integer stride. Cache budget is64 entries/131072 commands per thread;
paint colors, opacity, transform and camera do not resample paths. See the scoped
[design contract](../../docs/planning/sdk/sketch-v1-design.md) for algorithm identity.

Sticky's first changed appearance patch also materializes its v1 metadata/seed when
`enabled:false`. Explicit None without that record rejects, preventing old codecs
from silently eliding it into a yellow plate. No-op patches do not create metadata.
This is a compatibility requirement of the existing Sticky default, not another
selection or History owner.


### Sketch v1 extended object capability

`readBuildInfo().features.sketchExtended === 1` / shared Session feature `sketchExtended: 1` extends appearance to VectorPath, Table grids and Section document boundaries. `strokeEligible`/`strokeExcluded` and `fillEligible`/`fillExcluded` report per-parameter applicability. Table grid paint remains exclusively in `table.setGridStyle`; generic stroke/fill patches reject when all selected targets are inapplicable. Group is not recursive.

`createVectorPath({commands, fillRule?, x?, y?, objectId?})` returns `{objectId, generation}` after shared owner application; omitted identity is allocated by Runtime. Commands are owned value tuples (`move`/`line`: `point`, `quad`: `control`/`point`, `cubic`: `control1`/`control2`/`point`, `close`), at most 4096, finite coordinates with absolute value ≤1e7, exact two-number points. Fill rule is `nonzero` or `evenodd`. `readVectorPath(objectId?)` returns owned canonical commands, transform, appearance and generation, never a mutable path or private engine handle. Windows caller-side equivalents take/return owned Json through the unchanged C transport ABI.

New paths always retain versioned appearance metadata even with sketch disabled; canonical curve geometry/fill rule are preserved. Open outlines are not implicitly closed. Snapshot reader capability `extended-sketch-v1` uses the additive internal bit 256 (current full profile 511); older profile 255 rejects these object extensions, including disabled metadata. SDK version 0.1.0 and styleVersion 1 are distinct. Preview/patch/error/thread/lifetime rules remain those of the shared appearance owner. This local development verification does not establish Windows DLL ABI qualification.


### Rounded corners / handwriting resources

Appearance values/patches add `cornerRadius` in object-local units [0,16384], independent of roughness. Radius0 clears the canonical property. Rectangle/triangle and Connector support it; `radiusEligible/radiusExcluded` report applicability, with mixed values only across eligible roots. The requested radius is clamped geometrically to prevent overlapping fillets. Existing preview/token/History semantics apply, and creation defaults materialize only on supported object kinds. Connector route/endpoints stay canonical; only its displayed interior bends round within obstacle constraints. New orthogonal connectors start with a local radius of 4. An explicit creation default, including zero, takes precedence. Existing objects retain their saved radius. Snapshot requires `rounded-corners-v1` (bit512).

The application font catalog ships Noto Sans CJK SC and Excalifont under their respective licenses. Font selection still uses owned resource IDs through existing `text.format({font})` and Table cell format. Chinese coverage comes from Noto Sans CJK SC; Excalifont is the imported handwriting face. Missing/corrupt bytes or unsupported metadata reject/diagnose explicitly. Old v1 font resources and ordinary API lifetime/thread rules are unchanged.

The Demo has tools, appearance properties, text/object properties, then board tools. Create/edit/select text to choose the font; the selector is not an implicit mutation of unrelated objects or global settings. Assets keep original upstream names and copyright; UI display labels can be separate. Renaming/modifying a font does not remove its OFL obligations or transfer upstream design ownership. SDK0.1.0 remains an unpublished candidate; Windows DLL ABI requires separate Windows evidence.

Ink v2 adds `setBrushConfig` / `readBrushConfig` on the root facade. Families are
round=1, marker=2, chalk=3, crayon=4, watercolor-lite=5, transient laser=6.
`pressureSource:1` (simulated) is the default regardless of browser pressure;
`pressureSource:2` requires a pressure-capable source and valid pressure on every
sample. Invalid config/unknown fields reject without changing the document.
`roundMode:1|2`, size/opacity/color, markerAngle/markerFlatness, density,
textureScale/deposition/wetEdge and laserFadeMs/laserTailLength are owned values.
Readback returns a copy. Owner-thread queue, error and close semantics are unchanged.
Configuration freezes for each new session; changes affect subsequent sessions.
New persistent profiles save one compact spine plus effective pressure, config,
seed and versioned built-in resource binding. They contain no raw/dab/outline
copies. The internal compact-stroke-v1 reader bit is 2048 (full profile4095).
Old brush records remain readable. Laser creates no Operations or Undo entries and
is excluded from snapshot/export. The Demo provides seven choices across six
families. Persistent brushes preview their shape and material with an amber tint
in the independent Skia surface, then hand off to canonical ink in the selected
color and opacity. The transient laser retains its selected color while fading.

Persistent compact profiles also support package revision 2. The Demo uses this
revision for smoother midpoint-quadratic centerlines and distinct dry chalk,
directional wax and transparent watercolor materials. Revision 1 stays readable
with its original material. `selectBrush(profileId, 2)` selects the new version;
later `setBrushConfig()` edits preserve that preset's internal evaluator/resource
binding. No expanded dab/outline is stored. The Demo shows only controls relevant
to the active brush and exposes the next stroke's color.

### Shared round/marker tuning

`setBrushConfig` accepts owned numeric/boolean controls `thinning` [-1,1],
`streamline`/`smoothing` [0,1], `easing` (1 linear, 2 ease-in, 3 ease-out,
4 smoothstep), `pressureModel` (1 timed speed, 2 distance proxy),
`startTaper`/`endTaper` [0,4096] world units, and `startCap`/`endCap`.
Round and marker revisions 3/4 select config v2; changing these controls on an old
round/marker preset explicitly promotes subsequent sessions. Legacy chalk/watercolor and laser reject non-default vector tuning;
crayon/chalk/watercolor revision 3 admit their documented subset. Config version is a compatibility discriminator, not
a consumer-chosen engine/resource binding. Unsupported versions reject.

Controls freeze at session begin and affect the next session. Existing owner-thread
command receipts and lifetime rules apply; admission is not presentation or durability.
`readBrushConfig` returns copied values, including the supported tuning defaults for
legacy presets. The Windows JSON control transport uses the same fields, validation
and receipts without adding DLL exports or exposing C++ Runtime state.

Revision 4 uses PF-style bounded startup pressure initialization, distance pressure
recurrence, and default zero taper with round caps. Streamline normalizes the
centerline; smoothing filters paired outline anchors, then joins their midpoints
with quadratic curves. Revision 3 retains its previous centerline-fitting behavior.
These are explicit incremental adaptations, not all-input PF pixel parity.
Pressure-to-width easing is separate from pressure generation and whole-stroke opacity.
Taper applies to world arc length and cap flags affect untapered endpoints.
Replay uses persisted effective knots and config, never pressure simulation again.
Historical config v1 bytes/interpretation remain unchanged; older readers reject v2.


### Pressure crayon revision 3

`selectBrush('crayon-wax-v1', 3)` selects a pressure-responsive dry crayon. It uses
config v2 with a separately bound evaluator/material; old revisions keep their
appearance. The compact carrier still stores only centerline/effective pressure.
Pressure controls both width and pigment/texture contact. `deposition` controls
wax coverage and `textureScale` paper scale. Existing width/opacity, thinning,
streamline, easing, pressure model, taper and cap fields use the same owned control
transport and session-begin capture. There is no per-dab or image API/persistence.
Preview, canonical, image export and replay share the same Skia helper.


### Chalk and watercolor revision 3

`selectBrush("chalk-compact-v1", 3)` / `selectBrush("watercolor-lite-v1", 3)`
reuse config v2 controls and simulated-pressure defaults. Pressure changes width
and pigment concentration/contact. Chalk exposes density and textureScale;
watercolor exposes wetEdge and textureScale. Both support thinning, streamline,
easing, pressureModel, taper and caps. Smoothing remains fixed at .5 for these
sweep materials; outline smoothing is a round/marker capability. Revision 1/2
records replay unchanged; the new immutable evaluator/resource requires a matching
reader. No expanded dabs or image masks are persisted. Same-stroke max coverage
applies opacity once; separate strokes use SrcOver. Watercolor is a procedural
wash without diffusion, canvas color pickup or fluid simulation.


Live partial erasing uses transient per-stroke masks and the existing Skia full-scene
replacement presenter. Preview and commit share the same object-local swept mask;
non-ink objects and colored backgrounds remain visible through the cutout. Sparse
pointer samples are joined geometrically. Canonical state/History/publication change
only on Up. Unsupported opaque-coverage hit geometry for textured material strokes
uses conservative bounded candidates; the renderer still clips against actual ink.


Version readback: `readBuildInfo()` reports `sdkVersion` (`X.Y.Z.Build`),
`sdkSemver` (`X.Y.Z`) and numeric `buildNumber`. npm `version` stays X.Y.Z with
separate buildNumber metadata. Local builds use Build=0; release artifacts share
one pipeline build number. See sdk/VERSIONING.md.

Private CPU compute assets are shipped under `internal/cpu-worker-*` with a matching build identity. They contain the shared C++ exact-path kernel compiled as an isolated, CPU-only WASM module. Current root editing remains synchronous; these assets do not activate pending Up or change commit/presentation semantics. Consumers continue importing the package root. Partial results are never document data.


Private CW2 development profile: `AXIOM_CPU_PENDING_ERASE` defaults OFF. The
controlled candidate build connects partial-erase DOM Up to the existing Runtime
pending owner and same-build CPU Worker; readState and operation events remain
committed-state facts. Candidate pending erase admits subsequent erasing and
viewport navigation; competing canonical mutations explicitly reject Busy until
that queue drains. This is an intermediate controlled profile, not general
mixed-editing activation. Worker construction uses the already charged fixed
64 MiB heap; the resident8 MiB path cache and normal public exports remain
unchanged. No private Worker/pending modules are consumer business APIs.


0.5.0 release profile: published Web SDK/Demo builds explicitly enable
AXIOM_CPU_PENDING_ERASE. The preceding OFF/CW2 paragraphs describe development
checkpoints. The existing Runtime admits ordered pending erase/create/transform/
History work while the same-build isolated CPU Worker computes exact paths.
The controlled erase domain remains algorithm-v4 compact round/marker with
translation/rotation/uniform scale; unsupported document brush mixes use the
existing legacy path. One fixed 64 MiB Worker and at most eight intents remain.
Pending state is a read-only DTO; mutation promises resolve at terminal Runtime
commit/failure, never at admission. Worker tail latency and physical-platform
qualification remain open. Native package build profiles do not enable this Web
executor. All assets must come from the same installed package.

An ordered local intent that fails after admission rejects its promise and reports
CPU_PENDING_FAILED via onError. For example, publication capacity denial returns
a terminal LOCAL_INTENT_FAILED event without advancing canonical generation.

0.5.1 retains the enabled Web Worker release profile and integrates erase/navigation
correctness and rendering reuse improvements. The settled-navigation diagnostic
checkpoint remains BLOCKED / ROOT_CAUSE_NOT_ESTABLISHED; original EFF4 performance
acceptance remains unmet. Private timing/progress observers remain off in releases.

0.5.2 fixes Android NDK aggregate initialization in the shared navigation path and
keeps the 0.5.1 runtime behavior and enabled Web Worker release profile.
