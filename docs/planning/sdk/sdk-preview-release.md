# Consumer SDK releases

`sdk-preview-release.yml` builds Windows x64 and JavaScript/WebAssembly consumers
from one clean exact revision and `sdk/version.json`. Its historical filename and
the `axiom-sdk-preview-v1` manifest format identifier remain for compatibility;
version 0.0.1 is a regular GitHub Release and npm version without a prerelease suffix.

The current profile produces four assets:

- `axiom-sdk-windows.zip`: the DLL/import stub, public C/POD and caller-side C++
  headers, CMake consumer config, ICU/fonts/licenses and an external example.
- `axiom-demo-windows.zip`: the actual DLL-linked example and runtime assets.
- `axiom-sdk-js.zip`: the public ES module/types, private loader, WASM/data,
  DOM/IME adapters, licenses, server and three-row Web Demo.
- `axiom-demo-web.zip`: the same JS SDK and Demo with `index.html`.

Consumer packages do not contain Runtime, Semantic or Skia component archives.
Windows callers may use /MD; only versioned POD/opaque handles and owned response
buffers cross the DLL boundary. Source developer components remain opt-in.

Every ZIP's `manifest.json` binds the exact source revision, SDK version and file
hashes. `release.json` and `SHA256SUMS` bind all four ZIPs. Verification rejects
mismatched versions/revisions, corrupt or unexpected files and internal archives.
Earlier releases and their historical asset names remain immutable.

Both platform jobs execute real external consumers before publication. Native
verification includes out-of-tree /MD and pure C LoadLibrary/unload/reload callers,
packaged consumers and public identity/background/export regressions. Web
verification includes actual Chrome/WebGL2, real npm packing, outside-checkout
installation, positive/negative TypeScript consumers, and packed identity,
background and image-export APIs plus Demo downloads.

Only a new `v[0-9]*` tag publishes. The tag must exactly equal
`v<SDK version>`. After both jobs pass, CI creates a draft, uploads verified
assets and identities, and publishes a regular Release as latest. Manual/PR runs
build and verify without publishing. Existing releases/assets are never overwritten.
For 0.0.1 the tag is `v0.0.1` and title is `Axiom SDK 0.0.1`.

After GitHub publication, `publish-npm` extracts that run's verified Web asset using
`tools/release/npm_publish.py`, checks exact provenance, hashes, paths, required
assets, stable version, namespace and registry policy, then publishes the unchanged
payload as `@mostorm/axiom` to `http://npm.nearhub.cc/` with the `latest` tag. The npm
whitelist retains provenance, loader, WASM/data, types, Demo and licenses.

The repository Secret `AXIOM_NPM_TOKEN` supplies `NODE_AUTH_TOKEN` only to the
publication step. setup-node supplies temporary npm configuration. Credentials must
never enter Git, archives, tarballs, Actions artifacts or logs. PR and diagnostic
runs receive no publishing credential. Authentication or upload failure fails the
job; GitHub publication alone does not establish npm success.

Before another release, align `sdk/version.json`, `sdk/web/package.json`, and the
exported version literals in `axiom.mjs` and `axiom.d.mts` to the same unused stable
version. Push the source branch, then the matching version tag. Conflicts fail;
do not overwrite versions. If interrupted publication already reached npm, inspect
its exact identity before recovery instead of republishing over it.

Consumers configure `@mostorm:registry=http://npm.nearhub.cc/` and an environment
credential as shown in `sdk/web/README.md`, then use:

```sh
npm install @mostorm/axiom@0.0.1
```

The regular version does not imply production, Gate or physical-input acceptance.
Physical IME/touch/DPI, recorded legacy failures and the deferred D07 injected-OOM
work remain separate. Applications own persistence, synchronization, resource
storage and durable close. Snapshot does not include resource blobs or Undo history.
