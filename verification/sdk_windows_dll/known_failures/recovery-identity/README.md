# Known failing recovery identity probe

This is a real consumer of the published SDK. A correct result is two distinct
OperationIds for two distinct edits separated by snapshot restore (exit 0).
The published clean `4fc3ebfe1325c9197024f1cd7137e54473a1c76d` DLL returns the same
ID and exits 1. This is not an expected-success/WILL_FAIL product test.

Build in an x64 MSVC 19.51 shell against an extracted SDK:

```powershell
cmake -S verification/sdk_windows_dll/known_failures/recovery-identity -B out/recovery-identity -G Ninja -DCMAKE_BUILD_TYPE=Release -DAxiomSDK_DIR=<SDK>/lib/cmake/AxiomSDK
cmake --build out/recovery-identity
ctest --test-dir out/recovery-identity --output-on-failure
```

The small wire-field reader independently extracts Operation.operation_id.value
using the published proto fields; it does not ask the production codec to decide
whether IDs should be unique. It also checks that payloads differ and that the
recovered Runtime accepts both objects. The failure belongs to Runtime identity
allocation / checkpoint recovery, not to subscription delivery or Axis retries.
Do not fix it by changing committed Operation bytes/IDs in an SDK callback.

This source is retained outside the regular green consumer sample because it is
an unresolved product failure, not a supported behavior. Recovery followed by
editing and release readiness remain blocked until its owner repairs it and this
same consumer passes. No expected values or old failure records were weakened.
