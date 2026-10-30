# Axiom JavaScript SDK

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
extracted npm package in Chrome as well as the ZIP. `private: true` prevents
accidental registry publication and does not prevent local packing/installing.
The three main artifacts are `axiom.mjs`, `axiom.wasm` and `axiom.data`.
The Emscripten loader lives in `internal/axiom.mjs`; applications import only
the public root `axiom.mjs`. Keep the `internal/` directory, both input modules,
declarations and the `licenses/` directory alongside them. For bundlers that relocate the
data/WASM, pass `assetBaseURL` pointing to the deployed engine assets.

API version 1 uses 32-character lowercase hexadecimal object IDs and decimal
strings for 64-bit revisions/tokens. Command promises resolve only after the
Runtime's result is available; this does not acknowledge durable storage.
Application callbacks are deferred and must not replace native IME results.

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
