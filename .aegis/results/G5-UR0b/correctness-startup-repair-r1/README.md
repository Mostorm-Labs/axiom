# Correctness startup repair r1 — READY_FOR_CONTROL_REVIEW

This narrow verification-host fix positively drains startup before collector
reset/enable and fixture work. A page-owned frontier registers onFrame before
createRuntime, retains early callbacks, waits for a positive actual accepted
presentation plus the existing SDK-owned RAF queue to drain, and checks fresh
document/target/error/deadline facts. It does not use generation0, disabled
pending_gpu0, a fixed sleep or ten RAFs as completion proof. OFF and reloaded ON
use the same frontier under the unchanged setup/correctness outer deadlines.

The default onFrame callback and measured bodies remain unchanged; only the
cold correctness init branch gains the helper. Both historical profile literals
remain intact. A third exact8-file correctness-only profile requires two real
host frontier records with exact P20 ref. JS and Python verify and reconstruct
them from bound inputs while preserving every native accepted-chain check.

The controlled17-turn delayed callback model reproduces the old orphan RED.
Full127 Node/96 Python/120 boundary and diff checks pass, including only the2
allowed malformed native unit checks. Three historical runner contexts retain
real validators and actual-current mismatch negatives.17+1 original records
are invariant except compiler provenance; actual r6 remains MISSING7=1+5+1,
with seq22 unresolved. No original raw, pixel, result or closed window changed.

No browser/native qualification, build, commit/push or r7 ran. Sampling stays
35starts/1276s. UR0b remains BLOCKED_EVIDENCE; this does not gate a separately
scoped production performance repair with its own local validation contract.
No performance/platform/P34 PASS or automatic new sampling authority is issued.
