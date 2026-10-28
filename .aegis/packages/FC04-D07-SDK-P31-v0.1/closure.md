# P31 Closure — Route A SDK Exception Safety

This package accepts the Route A scope for a bounded P32 implementation and qualification task. It does not claim implementation correctness.

The incident and owner investigation justify a coordinated patch to Abseil once-state exception handling and Protobuf descriptor initialization state. P32 must implement versioned patch artifacts plus deterministic producer application/identity, then prove safe failure, waiter liveness, descriptor state integrity and byte-identical normal behavior.

No release is promoted and no accepted SDK lock changes until the P32 candidate is reviewed and the existing release/lock-only PR flow succeeds.

D07-A/B remain blocked until the patched SDK is formally released, consumed through the accepted lock, and the full Axiom verification matrix passes. D06-B remains independently OPEN.
