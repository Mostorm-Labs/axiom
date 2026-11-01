# Vector tuning v3 local verification

Round/marker controls and release-tail fix; source manifest in summary.json.

- 23 InkV2/render-cache/navigation CTests passed (suite.log).
- Final pressure-position pairing, taper pixels, preview parity, SDK operation
  persistence passed (specialized.log).
- 6 baseline comparator Node tests passed (node-baseline.log).
- TypeScript strict consumer, module boundary check and git diff --check passed.
- Native and matching WASM builds used existing locked Semantic/Skia SDKs.
- Full configured CTest: 216/238 passed; 22 failure names already occur in
  out/g5-camera-reuse/regression-closure.log. No all-repository PASS claim.
- FC02Text was terminated after >8 minutes in full run; independent bounded
  rerun timed out at 240s (text-timeout.log).
- Browser JSON and captures are automated Chrome mouse evidence, not physical
  touch or Windows DLL qualification.

Local visual review URL: http://127.0.0.1:4198/example.html

Captured CTest log lines have trailing whitespace trimmed for repository hygiene.
