# Ink v2 implementation checkpoint

Date: 2026-11-01. Branch: `codex/sdk-shell-axis-bridge`.
Base revision: `a0b20fbb0d31b6da0a536eb5f63fc7594468f415`.

This checkpoint contains the user-authorized implementation of INK-V2-01/02/03:
default simulated pressure with explicit device capability validation; compact
centerline/effective-pressure persistence; round, marker, chalk, crayon and
watercolor material rendering; transient laser; matching SDK controls and Web
Demo. Historical stroke readers remain supported. The compact carrier binding
is recorded in `compact-stroke-v1-contract.md`; dependency and historical
descriptor locks are unchanged.

The prepared package files retain their original hashes and preparation status
as the input record. They do not describe the later execution result. User
instructions in this session authorized implementation and this commit/push.

## Preview correction

The user clarified that new brushes showed live brush-colored lines but lacked
the previous amber preview cue. Persistent compact brushes now retain their
geometry and material while receiving an amber render-only paint override.
Effective preview opacity is applied once. Canonical records retain the selected
paint; the independent preview surface retires after canonical presentation.
Laser retains its selected color and fades without a canonical operation.
This user clarification supersedes the prepared task's captured-paint default;
it does not change persisted material interpretation.

## Verification

- Fresh native CTest selection: 25 passed, 0 failed (18 Ink v2 and 7
  preview/surface/handoff tests).
- Runtime boundary checker: 114 contract files passed.
- Current semantic descriptor unittest: all 7 passed using the resolver-emitted
  `AXIOM_PROTOC` from the existing consumer build configuration.
- JS public API error contract and verifier syntax checks passed.
- Matching WASM/Web build completed. Browser DPR 1 and 2 runs each passed
  91 checks, including amber held-pointer pixels, selected canonical paint,
  preview retirement, snapshot/remote replay, resize and context restoration.
  These browser artifacts are local under `out/ink-v2/preview-handoff-dpr*.json`;
  their WASM SHA-256 is
  `62f503ef12492cf69028232335949a09b060d3e9f5f0723d681819cf716228b2`.
- Implementation source whitespace checks passed. The exact prepared package
  documents contain existing extra blank lines at EOF; their hash-bound bytes
  are preserved rather than silently rewriting the preparation record.

## Remaining observations

The user reports that interaction feels slower than before. This checkpoint
does not claim a performance fix or physical-device qualification. Profile
input-to-preview latency and Skia coverage/material work at the actual canvas
size before changing rendering or caching.

The earlier broader regression run recorded 22 failures; the full FC02Text
CTest timed out at 600 seconds, and Python regression recorded 4 failures.
Those observations are not closed by the targeted checks above. Full repository
regression was not rerun for the preview paint correction. Windows DLL and
physical touch/pressure evidence remain separate. No Gate, P34 or release PASS
is issued by this checkpoint. SDK publication/versioning is outside this commit.
