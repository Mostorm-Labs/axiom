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

Appearance values/patches add `cornerRadius` in object-local units [0,16384], independent of roughness. Radius0 clears the canonical property. Rectangle/triangle and Connector support it; `radiusEligible/radiusExcluded` report applicability, with mixed values only across eligible roots. The requested radius is clamped geometrically to prevent overlapping fillets. Existing preview/token/History semantics apply, and creation defaults materialize only on supported object kinds. Connector route/endpoints stay canonical; only its displayed interior bends round within obstacle constraints. Snapshot requires `rounded-corners-v1` (bit512).

The application font catalog adds Excalifont and Xiaolai v3.126 under OFL1.1. Font selection still uses owned resource IDs through existing `text.format({font})` and Table cell format. Excalifont carries explicit one-hop Xiaolai fallback in font metadata schema/layout version2; bytes/hashes/resource revisions remain in the existing resource owner. Fallback is shared C++ shaping/rendering, never browser/OS font selection or canonical run rewriting. Missing/corrupt bytes or unsupported metadata reject/diagnose explicitly. Snapshot requires `font-fallback-v2` (bit1024); full internal reader profile2047. Old v1 font resources and ordinary API lifetime/thread rules are unchanged.

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
Round and marker revision 3 select config v2; changing these controls on an old
round/marker preset explicitly promotes subsequent sessions. Other families reject
non-default vector tuning. Config version is a compatibility discriminator, not
a consumer-chosen engine/resource binding. Unsupported versions reject.

Controls freeze at session begin and affect the next session. Existing owner-thread
command receipts and lifetime rules apply; admission is not presentation or durability.
`readBrushConfig` returns copied values, including the supported tuning defaults for
legacy presets. The Windows JSON control transport uses the same fields, validation
and receipts without adding DLL exports or exposing C++ Runtime state.

Streamline filters the centerline; smoothing controls midpoint quadratic fitting.
It is not a promise that PF outline filtering has identical numeric semantics.
Pressure-to-width easing is separate from pressure generation and whole-stroke opacity.
Taper applies to world arc length and cap flags affect untapered endpoints.
Replay uses persisted effective knots and config, never pressure simulation again.
Historical config v1 bytes/interpretation remain unchanged; older readers reject v2.
