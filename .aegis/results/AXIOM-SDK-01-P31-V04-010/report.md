# SDK-01 P31 v0.4 phase-order correction — 010

Source `a92c05734cd81f0f11e285692015d324239fbd7f`. This is a draft with `p32_authorized: false`, not an accepted implementation handoff. Exact package SHA-256s and preflight outputs are recorded alongside this report.

The revision removes the circular condition that the unimplemented SDK and future host drivers must already have passed before implementation. It proposes freezing fixture inputs, independent expectations and driver protocol before P32; real SDK/Windows/Chrome driver implementation and results remain blocking at P32 exit. No required product test was waived, marked passed or replaced by a synthetic result. The draft locks the independent fixtures, checker and runner from 008, and the actual lifecycle link RED from 009.

Integrity exit 0. PackageBinding, EvidenceContract, Design and all exit 2. Remaining precise issues: exact public ABI, owner and lifecycle proposal needs acceptance; independent expected/observed mapping, real provider and fault requirements, Windows and Chrome driver protocol and exact commands are incomplete; SDK-01 applicability and individual API01-API12 acceptance remain unresolved. The real SDK and drivers remain absent and their production execution is NOT_RUN. D07 OOM and physical status remain unchanged.
