# Correctness test reverification r1 — READY_FOR_CONTROL_REVIEW

Only verification/tests/test_g5_ur0_remaining_runner.py changed relative to this
P36 package start. The historical reuse positive now builds an exact temporary
compiler fixture from pinned df214581 Git blobs and checks every file against
the original frozen packet SHA256. accepted_reuse, admit_record, canonical,
mode/source/receipt/artifact checks remain real. Only HERE is isolated, then
restored in finally. No result mock, skip, xfail or expectedFailure is used.

A separate test uses the actual current compiler and requires the original r4
packet to be rejected with the existing compiler-mismatch error. No reader or
packet was modified, and no current compiler reuse permission is issued.

The original focused RED is preserved. Focused tests 4/4, module 13/13, complete
Node 114/114, complete Python 61/61 and 120 boundary contracts pass. The Python
suite includes exactly 2 allowed malformed-snapshot native checks. No browser
worker, build, valid native qualification probe, commit/push or new window ran.
Historical sampling remains 33 starts / 1251 seconds.

All 188 r2 payload files and the exact r2 result/manifest were rehashed. Harness,
compiler and profiles are unchanged; all 12 oracle groups and 17 invariant
recording recompilations are referenced by their original precise hashes rather
than rerun. The r2 FAILED result stays unchanged. Independent control review and
current compiler/recording-reuse acceptance are still required before separately
authorizing 6 correctness plus 30 main workers. No performance/platform/P34 PASS.
