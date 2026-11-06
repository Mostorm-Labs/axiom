# G5 bounded material isolation experiment

Result: **BLOCKED / FROZEN_VERIFICATION_FAILURE**. The experiment is withdrawn from the default path. This is an implementation/performance finding,not an environment blocker or a Gate/P34 result.

- Tested corrected candidate: `1e6b5f5764c4585c61d658d6d628ba699a1e0d91`.
- Paired installed-SDK baseline: `7299f6874be4947af219354dfa06bc6729a7b380`.
- Earlier one-layer-perRAF regression: `4a09233437cd88b92bd5c22f121e6d36bf99aed6` (preserved separately).
- Active restored source: `5672f6aea1007f5bc654038353a6f5f36851a6e4`,production code exactly equal to `7299f6874be4947af219354dfa06bc6729a7b380` across the checked paths in retained-candidate.json.
- Stable restored Demo: http://127.0.0.1:4238/example.html. Experimental4236/4237 remain separate and are not recommended defaults.

## What was demonstrated

Ganesh-only material layers can replace the full isolated scratch for qualified v4 overflow ink. Explicit integer material clipping preserves the original coverage grid,global matrix,shader input and order. Independent actual-GPU Direct comparisons have zero changed bytes in all 11 cases. The constrained1024x768 positive fixtures complete within5MiB with no full-frame scratch;the conservative per-layer peak is532512 bytes. No mutable pooled-surface/COW cycle or partial canonical publication is used.

Per-layer leases are reserved before draw and remain charged until actual GPU completion. Failure keeps ready coverage and does not advance its cursor. The protocol tests cover real quota denial,resume,cancel and delayed completion without losing the qualified sharp source. Physical driver OOM was not injected. CPU raster retains its original path:the rejected broad prototype changed30/35 one-LSB channels in fractional shear/rotation. Pixel tolerance was never relaxed.

## Why the default was restored

The first isolated-layer candidate needed503 exact attempts and8.49s DPR4 cold completion. Correcting working headroom removed that regression (75 attempts in the pilot),but did not satisfy the paired baseline target:

| Frozen measurement | Baseline | Corrected experiment | Decision |
|---|---:|---:|---|
| DPR4 after-freeze exact wait | 1460.5ms | 1380.1ms | <15% improvement |
| DPR4 after-freeze time-weighted low-density fraction | 0.6427 | 0.7338 | >2 percentage-point regression |
| Maximum candidate input submission gap over matrix | — | 285.2ms | >150ms |

Strict final pixels,document state,selection,Operations and receipt checks remain consistent. The corrected candidate still spends roughly0.53s CPU-inclusive time in exact coverage on the representative DPR4 cold trace. Saving scratch capacity alone does not reduce that replay cost. Retaining a full sharp image while reclaiming detailed navigation cells also exposes a cache-value/lifetime tradeoff. This resource explanation is an inference from measured resource/coverage traces;the quality fraction is a source-density proxy,not optical blur.

## Verification boundary

The complete corrected comparison contains 22 bounded browser workers (11 pairs),DPR2/3/4,diagnostics ON/OFF,selected/unselected pan/zoom,60s foreground idle in the DPR2 selected-zoom cases,and5s CDP frozen lifecycle. DPR3/4 extra cases use1s idle. No build/test workload ran concurrently with performance sampling. Source and artifact stability,worker cleanup,physical framebuffer dimensions and final pixels are checked independently.

Native: 107/112 pass,exactly the same five baseline failures;see result.json. All 10 experimental GPU modes,31 Node tests,118 boundary contracts,normal installed root packages and actual root Demo smoke pass. The restored production code has separate fresh package,9 GPU-mode,8 focused native,Node,boundary and root Demo observations under retained-checks/ and retained-root/.

This is local Chrome/ANGLE synthetic SDK evidence. It does not establish physical input-to-photon,unique GPU memory,physical sleep recovery,Android/Windows parity or100K/1M qualification. The32/96MiB defaults,8MiB entry limit,CPU/GPU quotas,semantic/Skia locks,SDK public interface and previous G5/P34 results remain at the retained baseline.

Run `python3.12 verification/evidence/performance/g5-material-tile/verify_evidence.py` to recheck the archive. Exit0 verifies integrity and recomputed classification;it does not turn BLOCKED into a performance pass. Exact build/measurement recipes are in reproduce/. The tested experimental source is reachable through the branch history;the installed artifacts are bound by SHA256 in source-artifact-bindings.json and package checks.
