# CPUW6b: bounded local exact preparation

Base `c5ba55996319e96c801f4e275814473137d7e85c` on `codex/g5-cpu-worker`, worktree `/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`. Read package, this context and execution-contract first; locks progressively.

CPUW6a measured 38.377s native AUp→drain; kernel35.757s(93.17%), newA16×7200 sequential unions35.699s; initial historical32×32 only57.9ms. NativeB is one segment; WebB is eight. Scene stamp1.685s nested in commit2.229s is the next slice.

Current flow: CanvasEditingRuntime freezes raw trace → all full local masks prepared by CpuExactPathJob → Render strict full query/history pins → exact Hit → unchanged AddEraseMasks/OperationEngine/History/Scene → matching display retirement. Full union is not canonical. Existing Renderer clears original segments. PreparedNavigationGeometry::visitExact already supports local body traversal. Main consumers requiring full paths today: pending current/history, future erase, selectionQuery, active prefix and cache handoff.

Implement the coherent narrow alternative agreed in read-only review: conservative local piece preparation and exact hit decision through existing owners. Keep full original mask for commit; qualify pieces distinctly, cover history and selection, bound fallback, stop eager full active-prefix work. Worker stays geometry-only C++/WASM. Do not falsely publish a subset as a full path or silently change exact semantics. Do not compute union once in world coordinates and transform: measured16 local paths have different numeric topology.

Use original independent evaluator as decision oracle, not new caches. Preserve original full-kernel/parity tests. First add RED+semantic oracle; record design and continue if CLEAR. Ordinary implementation decisions within frozen contracts are executor discretion; notify control rather than wait at intermediate green.

Native reuse: out/G5-CPUW-checkpoint-20261113/build-native and emitted environment. Use locked Semantic offline and CanvasSkia_DIR under /Users/qing/Desktop/sources/git/deps/axiom/darwin-arm64/skia-sdk/macos-arm64-metal/release/lib/cmake/CanvasSkia. No dependency rebuild. Existing cpu_tail_attribution harness is accepted measurement and can inspire a new scoped runner supporting new task kinds; do not reinterpret old64 jobs as64 complete long rebuilds.

Keep historical raw-pruning limits; originalFixed40 and independent referencebinary/source remain. Existing4330 assets are a potential Web baseline only after verifying production source hashes; otherwise make one matching baseline SDK. Never touch4330 user document. Fresh Demo is new path/port. Primary build diagnosticsOFF, same0.4.3.0 identity, candidate private Worker capabilityON.

Return result hash, source delta, U1..U4 facts, measured remaining tail, DemoURL. No tag/commit/push in this package. Proactively callback control at CLEAR, Web window and terminal. Current request authorizes implementation, not another read-only-only return.
