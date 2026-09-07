# Semantic SDK Store: developer usage

## Delivery boundary

Task 7 supplies the Semantic v2 provider and entry points. A permanent v2
Release and the repository's accepted `semantic-sdk.lock.json` are promoted
later, after full implementation and consumer qualification. Until that lock
exists, default Linux consumers retain the historical v1 route. Windows/macOS
require a v2 lock; the Linux archive is not a portable substitute.

## Normal usage once v2 authority is accepted

```sh
python tools/setup_build_environment.py --semantic --status
python tools/setup_build_environment.py --semantic --target android-arm64-v8a
```

The host is detected automatically; `--target` names the final library ABI.
The provider selects one native host `protoc` and one target runtime. It uses
the generic Store/transport implementation, not source checkout or compilation.

Default persistent roots are `%LOCALAPPDATA%\Axiom\sdk` on Windows,
`~/Library/Application Support/Axiom/sdk` on macOS, and
`$XDG_DATA_HOME/axiom/sdk` (or `~/.local/share/axiom/sdk`) on Linux.
Set `AXIOM_SDK_STORE` to share a different resource folder across worktrees:

```powershell
$env:AXIOM_SDK_STORE = "D:\DevResources\AxiomSDK"
python tools/setup_build_environment.py --semantic --status
```

```sh
export AXIOM_SDK_STORE="$HOME/DevResources/AxiomSDK"
python tools/setup_build_environment.py --semantic --status
```

`--store PATH` takes precedence over that environment variable. Existing
arbitrary `.deps` folders are not automatically trusted as SDK packages.
A Store entry must carry the exact locked manifest, identity and payload.

## Mirror, warm Store, and offline operation

`--mirror VALUE` overrides `AXIOM_SDK_MIRROR`. A mirror may be a filesystem
folder, a file URI, or HTTPS; its layout is `MIRROR/RELEASE_TAG/ASSET_NAME`.
It contains the exact released index and ZIPs, not independently repacked files.
Only the required host/runtime pair is materialized. A valid materialized hit
requires no archive, mirror, download, source clone, or rebuild.

```sh
python tools/setup_build_environment.py --semantic --mirror /shared/axiom-releases
python tools/setup_build_environment.py --semantic --offline --status
```

Offline mode is strict: both the verified index and already-materialized
packages must exist. A cached ZIP alone does not authorize an offline install.
For first import from a local mirror, omit `--offline`; a complete filesystem
mirror uses no network even though transport is permitted. A missing mirror
file may fall back to the locked GitHub Release, but corrupt bytes fail closed.

`--core` remains the separate lightweight source bootstrap and cannot be
combined with `--offline`. Skia and lightweight `.deps` dependencies have not
been migrated by Task 7.

## Outputs and compatibility

Standard output ends in machine-readable JSON. `--facts-output FILE` writes
that build-environment object. `--status` also prints host, target, release,
package identities, paths, ABI, sources and network use to standard error.
Status resolves the requested environment; add `--offline` to forbid transport.

Stable environment fields are `AXIOM_PROTOC`, `AXIOM_SEMANTIC_HOST_ROOT` and
`AXIOM_SEMANTIC_RUNTIME_ROOT`. `CMAKE_PREFIX_PATH` and the transitional
`AXIOM_SEMANTIC_SDK_ROOT` alias both point to the runtime. Paths are real,
canonical paths; v2 never creates `.deps/protobuf` or requires a symlink.
`--github-env FILE` appends these fields for subsequent Actions steps. Running
Python does not modify the invoking shell's environment; local build tooling
must read the JSON or use the exported paths explicitly. Full Axiom CMake
host/target integration is the next task, not part of this checkpoint.

`semantic_fetch.py` delegates v2 to the same provider. Its old `--destination`
option is v1-only; v2 callers use `--store`. Explicit `--lock FILE` can select a
candidate v2 lock for controlled qualification or a historical v1 lock for
reproduction. It does not accept or commit that lock into the repository.
A present but invalid default v2 lock is always an error, never a reason to
fall back to v1 or source bootstrap.
