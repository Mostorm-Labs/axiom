# Semantic corpus

`verification/corpus/semantic/v1/` is the GT-G1-02 candidate wire corpus. It is
not a product file format or collaboration protocol. The 60-case seed identity
is locked by `suites/seed-v0.1.json`; binary BG/BGX vectors are promoted only
after strict codec, descriptor and differential validation. Unknown fields,
non-finite values, duplicate canonical keys, truncation and hard-limit
violations must fail closed without mutating a Document.
