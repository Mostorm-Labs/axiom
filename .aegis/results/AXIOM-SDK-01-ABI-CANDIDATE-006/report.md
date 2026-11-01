# SDK-01 ABI candidate and owner evidence — 006

Status: DRAFT; NOT P32 authorization or product SDK acceptance. Parent revision: `1444b3672d98ba2d9a9984b006e49c4ba216e4d2`. Source and raw evidence hashes are in `facts.json`. No production code, accepted V1 ABI, or SDK lock changed.

## What was materialized

A concrete multi-selection C ABI candidate, ownership and lifecycle design, compilation-only C/C++ external consumer scenarios, candidate layout probes, and a diagnostic using the real RuntimeComposition and MultiSelectionPort. The candidate supports explicit partial-subset acknowledgement, typed result and errors, complete IDs, snapshot tokens and per-root leaf preservation. It remains a proposal; the public functions are not implemented, linked or executed. The user accepted rejecting stale requests and requiring a refresh and retry; the owner test confirms this existing behavior.

The diagnostic selects two shapes with different stroke colors and calls the **existing whole-field replacement** with red stroke and width 19. Both colors become red. `--observe-replacement` exit 0 demonstrates the existing operation semantics; `--expect-preserved` exit 1 demonstrates why that existing entry cannot fulfill the proposed independent leaf edit. This intentionally calls replacement: it is not a product RED against an implemented leaf-patch API.

## Actual checks

- `raw/run-abi-candidate.ps1`: native MSVC 19.51 C/C++ `/W4 /WX`; Emscripten 6.0.6 C/C++ `-Wall -Wextra -Werror`; all compile or layout entries exit 0 in `raw/final-commands.json`. These compile consumers against declarations only. The layout probe ran natively and with Node/WASM, not in Chrome. Its proposed layout values are not approved goldens.
- Configured `out/sdk01-review/build` with `CMAKE_PROJECT_INCLUDE=verification/sdk01/inject-candidate-owner-tests.cmake` using the existing locked dependency build, then built `axiom_sdk01_owner_mixed_stroke_probe` and `axiom_sdk01_facade_stale_retry_test`; exit 0. `raw/reconfigure-build.log` retains output. CTest `ctest --test-dir out/sdk01-review/build -R '^(SDK01FacadeStaleRetry|SDK01WholeStrokeReplacementControl)$' --output-on-failure --no-tests=error --output-junit .aegis/results/AXIOM-SDK-01-ABI-CANDIDATE-006/raw/final-owner-ctest.xml` exit 0, 2 tests, 0 failures, 0 skipped. The relative XML path is relative to the build directory and the resulting bytes were copied into this evidence directory.
- `python tools/check_runtime_abi_manifest.py --root .` and `python tools/check_runtime_boundaries.py --root .` passed; raw logs retained.
- Existing v0.2 package preflights were run with `python .aegis/packages/AXIOM-SDK-01-v0.2/preflight.py --repo . --check <name>`. Integrity exit 0; PackageBinding, EvidenceContract and Design exit 2. See `raw/v02-preflights-final.json` for exact blocked reasons. This old package does not bind the new candidate and has `p32_authorized: false`.

Early build/probe failures remain recorded in `raw/`: an initial native wrapper command error, an initial WASM warning/Node path issue, and two selection-fixture attempts. The initial v0.2 preflight run detected our temporary edit to its locked verification file; the original file was restored byte-for-byte and new tests moved into a separate injection file. `v02-preflights.json` preserves that earlier result; the final preflight is separately recorded. Final results do not erase initial failures.

## Outstanding

No implemented public multi-selection SDK, installed library linking, actual Chrome/WASM driver, Windows public consumer, real external leaf-patch tests, physical testing or SDK Gate pass is claimed. The current Runtime lacks per-root property read and leaf-preserving batch mutation. A successor package needs to bind accepted ABI/lifecycle/owner scope and executable public Windows/Web evidence. D07 protobuf/Abseil OOM and MSE-20/23/26 remain independent, unchanged findings.
