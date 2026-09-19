# GT-G0-12 hosted validation repair context

The Android instrumentation adapter itself builds and emits facts-only evidence, but the hosted workflow currently runs the entire verification workspace. That imports historical G1 source-bound tests whose immutable revisions are unrelated to the Android adapter and whose C8 scope assertion is incompatible with the later G2/G3 repository baseline.

The first incomplete action is to make the G0-12 hosted boundary self-contained: fetch full history for immutable source refs, then run only the Android-owned workspace/schema/scenario/evidence checks and Android harness package tests. Do not modify semantic-conformance generators, G1 evidence, G2/G3 production behavior, or historical Gate identities.
