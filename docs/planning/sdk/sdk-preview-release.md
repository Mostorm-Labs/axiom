# Consumer SDK preview releases

`sdk-preview-release.yml` builds locked Windows x64 and JavaScript/WebAssembly
consumers from one clean exact revision and SDK version (`sdk/version.json`).
The current profile produces four assets:

- `axiom-sdk-windows-preview.zip`: `bin/axiom.dll`, the small `lib/axiom.lib`
  import stub, public C/POD and caller-side C++ facade headers, CMake consumer
  config, ICU/fonts, licenses and a real external interactive example.
- `axiom-demo-windows-preview.zip`: that actual DLL-linked example executable,
  DLL and runtime assets. Tools/Create/Edit/Image/Text format/Font menus use
  the public SDK. It is not an internal Minimal Host executable.
- `axiom-sdk-js-preview.zip`: ready-to-import `axiom.mjs`, public TypeScript
  types, private generated loader, `axiom.wasm`, `axiom.data`, DOM/IME adapters,
  licenses and `example.html`. C++/Emscripten are not consumer prerequisites.
- `axiom-demo-web-preview.zip`: the same JS SDK and interactive example, with
  `index.html` and a loopback server. Run `python serve.py`.

Neither consumer SDK contains Semantic, Skia or Runtime component archives.
Windows callers may use /MD: all C++ containers compile in the application;
only versioned POD/opaque handles and SDK-owned buffers cross the DLL. Locked
MSVC 19.51 /MT remains an internal DLL production requirement. The C++ static
SDK profile is now explicitly opt-in (`AXIOM_INSTALL_DEVELOPER_COMPONENTS`)
and is not part of this release profile. Build the DLL with
`AXIOM_BUILD_WINDOWS_SDK=ON`.

Every archive's `manifest.json` binds source revision, SDK version and every
file hash. `release.json` and `SHA256SUMS` bind the four archives. The release
verifier rejects mismatched versions/revisions, extra archives, corrupt files
and internal archives in the consumer package. Original releases remain
unchanged; in particular the 20261030-fc536ad0 release contains the older
static-package layout and no standalone JS asset.

Both jobs build and execute real external consumers before publication.
Native tests run a fresh /MD app and pure C LoadLibrary/unload/reload consumer
outside the checkout, from an unrelated CWD. The packaged ZIP is exercised
again. JS is tested in actual Chrome/WebGL2, then real `npm pack`, offline
installation into a fresh application, TypeScript positive/negative consumers
and the packed engine in Chrome. Delayed fences, resize, native loss before
DOM notification, recovery and idle duplicate completion have explicit checks.

Only a new `axiom-sdk-preview-*` tag publishes. After both jobs pass, CI creates
a draft, uploads all verified assets/identities, then publishes it as a
prerelease (`latest=false`). Failed or existing assets are never overwritten.
PR/manual runs build and verify without publishing.

After the GitHub prerelease succeeds, `publish-npm` downloads that run's verified
Web asset and publishes `@axiom/axiom` to `http://npm.nearhub.cc/` using the
`preview` npm distribution tag. `tools/release/npm_publish.py` first checks the
exact commit, package/SDK version, manifest member hashes, required assets, safe
paths and private registry policy. It extracts the original bytes without
rewriting the package. The npm tarball retains `manifest.json`, loader, WASM/data,
types, input adapters, example and licenses.

The repository Secret `AXIOM_NPM_TOKEN` supplies `NODE_AUTH_TOKEN` only to this
tag-only job's publish step. `actions/setup-node` creates a temporary npm config;
no literal credential belongs in Git, a ZIP, a tarball, or an Actions artifact.
PRs and manual diagnostic/build runs never enter the publish job or receive this
Secret. Authentication and actual publish errors fail the job; success of the
GitHub release job alone is not proof that npm publication succeeded.

Before each new release, update **both** `sdk/version.json` and
`sdk/web/package.json`, plus the exported version strings in `sdk/web/axiom.mjs`
and `sdk/web/axiom.d.mts`, to the same unused prerelease version. The packaging
unit checks enforce these identities before the long build; the external DLL
verifier reads its expected version from the installed SDK version file. Push a new
`axiom-sdk-preview-*` tag containing the workflow changes. Registry versions are
immutable: conflicts must fail, not overwrite an older package. An interrupted
publish can be retried with Actions' rerun-failed-jobs facility; if the registry
already accepted the version, inspect its identity instead of forcing a rewrite.
The initial implementation configures the Secret and tests a local dry-run; it
does not create a new tag or publish an ad hoc test version.

Applications configure `@axiom:registry=http://npm.nearhub.cc/` and an environment
token as shown in `sdk/web/README.md`, then install `@axiom/axiom@preview` or pin an
exact version. This private registry currently accepts HTTP while its HTTPS
endpoint fails TLS negotiation. The registry operator can migrate it to HTTPS;
the package metadata, workflow and consumers must then change together. The
provided Feishu page returned only its JavaScript document shell, so its protected
document content was not used as a source of additional requirements.

This is a preview, not a Gate or stable release. Physical IME/touch/DPI coverage,
MSE-20/23/26 and the deferred D07 injected-OOM hang remain separate obligations.
Applications own persistence, networking, resource storage and durable close;
snapshots omit resource blobs and Undo history. See `sdk/windows/README.md`
and `sdk/web/README.md` for the consumer contracts.
