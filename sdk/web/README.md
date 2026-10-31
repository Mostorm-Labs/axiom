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

| Area | Public methods |
|---|---|
| Tools and style | `selectTool`, `selectShape`, `setSelectionMode`, `brushPresets`, `selectBrush`, `setInk`, `setEraser`, `setAppearance` |
| Document background | `readBackground`, `setBackground` |
| View and selection | `readState`, `readSelection`, `panBy`, `zoomAt`, `fitContent`, `fitSelection`, `fitObject`, `resizeSelection`, `transformSelection`, `deleteSelection`, `lockSelection`, `unlockSelection`, `orderSelection`, `setSelectionVisible` |
| Text and IME | `text.read`, `text.create`, `text.edit`, `text.editAtPoint`, `text.replace`, `text.select`, `text.selectAll`, caret/delete/paragraph/width methods, `text.format`, `text.fonts`, `text.fontStatus`, `text.completeComposition`, `text.finish`, `text.escape`, `text.resync`, `text.retry` |
| Image and crop | `importImage`, `replaceImage`, `resizeImage`, `readSelectedImage`, `beginCrop`, `readCrop`, `updateCrop`, `commitCrop`, `cancelCrop` |
| Containers | `createSection`, `wrapInSection`, `releaseSection`, `reparent`, `readSection`, `resizeSection`, `renameSection`, `selectSection`, `selectSectionChildren`, `group`, `ungroup`, `enterGroup`, `leaveGroup` |
| Connectors and Sticky | `createConnector`, `setConnectorRouting`, `readConnector`, `createSticky`, `readSelectedSticky` |
| Multiple objects | `multi.read`, `multi.readValues`, `multi.setPermission`, `multi.setProperty`, `multi.setStrokeWidth` |
| Document and resources | `captureSnapshot`, `applyRemoteOperation`, `resources.token`, `resources.publish`, `resources.receipt`; startup `restore` and `initialResources` options |
| Lifetime and events | `close`; `onOperation`, `onError`, `onChange`, `onFrame` options |

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

`zoomAt(x,y,scale)` takes an absolute target scale. `setAppearance` merges only
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
6-unit padding and clip at fixed row height. The last row/column cannot be removed.
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
