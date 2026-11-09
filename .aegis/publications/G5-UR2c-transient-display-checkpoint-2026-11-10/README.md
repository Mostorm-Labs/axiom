# UR2c transient display checkpoint

This is a requested WIP checkpoint, not a control-review or performance acceptance.
The source extends the existing SkiaRenderer/cache owners to protect successful
provider pixels during Retry and accumulate pending erase damage. It also contains
an unfinished attempt-to-staged resource handoff and independent trajectory oracle.
SDK contracts, Brush semantics and default 32/96 MiB budgets are unchanged.

Fresh focused verification: 3/5 native tests passed; the two new handoff tests
failed. Node 5/5 and Python 5/5 passed; diff whitespace check passed.
The warm DPR4 demand still fails to accept after 1024 bounded attempts, and the
in-flight test reports that the staged path retains the obsolete attempt.

The original exact kernel proved that two old test gestures were genuine no-ops.
Eight-positive-gesture proof, matching r2 SDKs, final runner binding and the two
authorized r2 Web windows remain unfinished. No r2 browser window has started.
Historical r1 results remain BLOCKED; no Gate, P34 or performance PASS is issued.

Only the explicit source, two package directories and selected compact evidence
records are published. Local raw buffers/builds and previous demos are preserved.
See checks.json, source-postimages.json and evidence-index.json for exact identities.
