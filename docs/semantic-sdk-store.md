# Semantic SDK Store: developer usage

## Delivery boundary

Tasks 7-8 supply the Semantic v2 provider, entry points and CMake host/target
separation. A permanent v2 Release and the accepted `semantic-sdk.lock.json`
are promoted later, after full implementation and consumer qualification.
Until that lock exists, default Linux consumers retain the historical v1 route.
Windows/macOS require a v2 lock; the Linux archive is not a portable substitute.

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
must read the JSON or use the exported paths explicitly.

`semantic_fetch.py` delegates v2 to the same provider. Its old `--destination`
option is v1-only; v2 callers use `--store`. Explicit `--lock FILE` can select a
candidate v2 lock for controlled qualification or a historical v1 lock for
reproduction. It does not accept or commit that lock into the repository.
A present but invalid default v2 lock is always an error, never a reason to
fall back to v1 or source bootstrap.

## Axiom CMake integration

When `CANVAS_SEMANTIC_ENABLE_PROTOBUF=ON`, always pass both the target prefix
and the explicit host tool. CMake requires `protobuf::libprotobuf` from the
CONFIG package but neither requires nor executes `protobuf::protoc` from it.
A missing host path (or a directory) fails configuration. A non-runnable file
fails when code generation is executed. No PATH search or target-tool fallback
is performed. The host executable and proto files are generation dependencies.

For Linux native development, after resolving to `out/build-environment.json`
and preparing the lightweight core sources, configure from its exact paths:

```sh
python tools/setup_build_environment.py --core --semantic --facts-output out/build-environment.json
python - <<'PY'
import json
import subprocess
from pathlib import Path
from tools.semantic.cmake_consumer import consumer_cmake_arguments
env = json.loads(Path('out/build-environment.json').read_text())['environment']
subprocess.run(['cmake', '-S', '.', '-B', 'out/semantic', '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=Debug', '-DCANVAS_BUILD_POC01=OFF',
                '-DCANVAS_BUILD_SEMANTIC=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
                *consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),
                                          Path(env['AXIOM_PROTOC']))], check=True)
PY
cmake --build out/semantic --parallel 2
ctest --test-dir out/semantic --output-on-failure
```

The same explicit prefix/protoc interface works during the Linux v1 transition:
the resolver exports the historical combined SDK as both host and runtime roots.
For v2 they are distinct Store packages. The helper restricts CMake package
search to the selected SDK and disables user/system package registries.
Cross builds additionally need their target toolchain arguments and compatible
ABI/configuration; SDK resolution does not install an OS SDK or select a CMake
toolchain. Full platform-matrix Axiom qualification remains a later checkpoint.
With Protobuf OFF, neither the SDK package nor `AXIOM_PROTOC` is required.
