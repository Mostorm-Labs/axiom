# Semantic corpus

`verification/corpus/semantic/v1/` is the GT-G1-02 candidate-pending-authority
wire corpus. It is not a product file format or collaboration protocol. The
60-case seed identity
is locked by `suites/seed-v0.1.json`; binary BG/BGX vectors are promoted only
after strict codec, descriptor and differential validation. Unknown fields,
non-finite values, duplicate canonical keys, truncation and hard-limit
violations must fail closed without mutating a Document.

`verification/tools/run_g1_semantic_differential.py` inventories BG/BGX files
and refuses to call inventory-only data a differential pass. The manifest must
be authority-promoted and declare `differentialOracle: "authority_promoted"`;
otherwise the result is `BLOCKED_AUTHORITY`. The repository currently contains
no authority-promoted BG/BGX binaries, so this is an intentional gate blocker,
not a generated placeholder corpus.
