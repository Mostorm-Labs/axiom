# Private npm publication — 2026-10-31

Base: `d5d8f795e4774df49bd3b390ed7b8fa9c068cd84` on
`codex/sdk-shell-axis-bridge`. The user's explicit CI/publication request is bound
in `.aegis/packages/AXIOM-SDK-NPM-PUBLISH-v0.1/package.json`.

The existing tag-only Windows/Web build and GitHub prerelease pipeline now adds
`publish-npm`, dependent on the successful Release job. It publishes the verified
Web artifact as `@axiom/axiom` to `http://npm.nearhub.cc/`, under `preview`. Source
identity, SDK/package version, required assets and manifest hashes are verified
before extraction. Unsafe paths, symlinks, duplicate/unbound entries, credential
configuration and lifecycle scripts reject before any publication step.

The user-provided credential authenticated successfully at the specified private
registry (`GET /-/whoami`, HTTP 200). GitHub repository Secret `AXIOM_NPM_TOKEN`
was created successfully and its name verified with `gh secret list`. Its value
was provided to `gh` over stdin, not command arguments or tracked files. It is
referenced only as `NODE_AUTH_TOKEN` on the tag-only publish step. No credential,
Feishu password, `.npmrc` or environment file is included in the SDK archive or
committed evidence. Registry write permission and GitHub runner connectivity will
be established by the first real publish; an authentication read is not write
qualification.

The supplied Feishu URL could be fetched only as a JavaScript document shell;
its protected body was not read. The directly provided registry/scope/example
configuration supplies this task's executable requirements. The configured HTTP
registry is reachable; the corresponding HTTPS endpoint currently fails TLS
negotiation. No alternate host or TLS bypass is configured.

## Actual verification

- A witnessed RED exposed the old `@mostorm/axiom`, `private: true` metadata and
  missing verified npm preparation entry.
- Python 3.12: **17 tests pass**, zero skipped, including hash/identity/version,
  unsafe-path/symlink/credential guards and preservation of existing output work.
- Official actionlint **1.7.12**: workflow validation exits **0**.
- Real `npm publish --dry-run --ignore-scripts --tag=preview`: exits **0**;
  tarball includes `manifest.json`, loader, WASM/data, declarations and licenses,
  with no old `axiom-runtime` filenames or credential configuration.
- Real npm packing, fresh external offline installation, and the positive/negative
  TypeScript 5.9.3 consumer now import `@axiom/axiom`; all exit **0**. The pack
  verifier additionally compares manifest/package/README bytes with the ZIP.
- Actual Chrome/WebGL2 qualification passes for both the ZIP and extracted npm
  payload: each runs 52 general assertions, 20 Shell/Axis assertions, frame-handoff
  cases and the example's CDP mouse/pixel checks, with no uncaught exceptions.

The local candidate reuses previously qualified compiled Runtime bytes and
applies current npm metadata. Its engine build stamp remains the earlier
`1a8bc6a0`/dirty stamp; source/file hashes are retained in browser evidence. It is
**local packaging/consumer evidence**, not a clean new Release artifact. Actual
tag CI still rebuilds both platforms from its exact clean commit and enforces
that revision. An initial local run used Python 3.11.3 and stopped at the existing
Python 3.12 tar extraction API; rerunning with the same Python 3.12 as CI resolved
the environment mismatch without removing the safe extraction filter.

See [the result](evidence/20261031-private-npm/result.json) and its raw evidence.
No actual npm version or GitHub tag was created. Before a real SDK release, bump
both version files to the same unused prerelease version and push a new
`axiom-sdk-preview-*` tag containing this change. Existing registry versions must
never be overwritten. This work does not alter Runtime, descriptor/SDK locks,
D07's deferred OOM findings, or physical/Gate acceptance.
