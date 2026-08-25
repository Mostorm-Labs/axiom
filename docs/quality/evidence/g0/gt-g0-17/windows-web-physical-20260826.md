# GT-G0-17 Windows/Web physical release validation

- sourceCommit: `dc05153a8078b936e7b5867d802beac0e309558a`
- branch: `codex/gt-g0-17-windows-physical`
- fixture digest: `47826449b895ac4f4a57b4f386379775`
- Skia: `b6d106297ff9ef2ff8094033695d045e87775581`

## Native Windows

Win32 + Skia Ganesh + D3D12 ran on the Intel UHD Graphics 630 hardware adapter (`warp=false`, vendor `32902`, device `39880`). The fixed replay digest and 100 lifecycle iterations passed. The 1,000-node smoke ran for 60 seconds with 50,178 frames: p50 1.1022 ms, p95 1.6714 ms, p99 2.1589 ms, max 9.2187 ms. No frame exceeded 100 ms. The working-set series passed the sustained-growth gate.

Visual comparison passed: 99.9827% pixels within ±2; maximum channel delta 184.

## Chrome Stable WebGL2

System-installed headed Chrome Stable (user agent reports Chrome 151.0.0.0) ran through the CDP-only evidence runner; no Playwright, bundled Chromium, SwiftShader, or WARP was used. WebGL2 reported `Google Inc. (Intel)` and `ANGLE (Intel, Intel(R) UHD Graphics 630 (0x00009BC8) Direct3D11...)`. Digest and 100 lifecycle iterations passed. The 60-second 1,000-node smoke produced 3,597 frames: p50 1.2 ms, p95 1.8 ms, p99 2.2 ms, max 3.1 ms. WASM heap remained 17,170,432 bytes and the JS heap series passed.

Visual comparison passed: 99.9900% pixels within ±2; maximum channel delta 184. `chrome://gpu` text was unavailable through the internal CDP target and is recorded verbatim; the WebGL renderer is the authoritative hardware observation for this run.

All raw RGBA, logs, memory series, environment snapshots, visual artifacts and hashes are in the adjacent evidence directory. See the JSON report for retries (2 runner retries), unavailable fields and reproduction commands.
