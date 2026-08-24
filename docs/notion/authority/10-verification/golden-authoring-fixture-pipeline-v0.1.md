# Golden Corpus Authoring Rules + Fixture Generation Pipeline v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81b78a6cf4a73768cd19
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Golden Authoring / Trust Boundary Contract

## Core trust model

Golden authority flows Architecture/Semantic Authority → human-reviewed case intent/semantic projection/wire recipe → independent fixture compiler → generated binary/provenance → checked-in corpus → implementation observation → coordinator compare.

Production implementation output must never directly become authoritative expected. C++ and WASM agreement proves parity, not independent semantic authority.

## Artifact ownership classes

- `AUTHORITY_MANUAL`: case intent, authoring/expected semantic projection, wire recipe; corpus author + human review.
- `DERIVED_GENERATED`: protobuf/opstream/provenance mechanically generated from reviewed source; fixture compiler only.
- `CANDIDATE_CAPTURE`: current implementation/fuzzer/diagnostic capture awaiting authority review.
- `RUN_OUTPUT`: per-run observations/results/divergence.

Generated does not mean trusted by itself; trust derives from reviewed source and deterministic transformation.

## Human-controlled semantics

Human author/review is required for case ID/status/category/authority refs/entrypoint/capabilities/expected outcome, semantic input intent, expected projection, semantic error category where frozen, OPEN status, malformed-wire defect intent, OrderKey expected results, replay checkpoint meaning and golden update reason.

Tools may generate boilerplate but may not choose semantic answers.

## Allowed mechanical generation

Reviewed projection→protobuf; reviewed operations→Operation.pb; ordered bytes→opstream; reviewed wire recipe→malformed bytes; canonical reviewed projection→canonical protobuf; SHA/length/source relation→provenance; N-1/N/N+1 scaffolding only after limit/accounting unit freeze; derived indexes.

Generator must not apply production Axiom operations to derive expected state, choose OPEN policy/error category, silently repair semantic input, update expected from current behavior or promote parser exception text into semantic oracle.

## Independent fixture compiler

Fixture compiler may use published Reference IDL descriptors/tags, generic protobuf primitives, verification schemas, frozen declarative Common Wire metadata and utility code. It must not use production Semantic Core apply/validator/canonical encoder as the sole expected generator, production OrderKey allocator as expected generator, RuntimeScene/Renderer/Product Shell/Data Runtime.

Implementation language is not authority; dependency independence is the requirement.

## Canonical protobuf trust chain

Reviewed semantic projection → IDL-aware validation → verification-only canonical wire writer → candidate bytes → independent tag/hexdump review → checked-in generated artifact. Exact codec review should expose field tag/wire type/value/offset, not only opaque hex.

## Malformed wire

Use human-reviewed deterministic byte-surgery recipe (`SET_BYTE`, `REPLACE_RANGE`, `INSERT_HEX`, `APPEND_HEX`, `TRUNCATE`) rather than unexplained hand-edited binary. Recipe is authority-manual; resulting bytes are derived-generated.

## Expected projection policy

For seed-v0.1, authoritative expected projections are human written/reviewed by default. Production observations may assist comparison but cannot materialize authoritative expected. Agreement increases confidence; authority decides discrepancies.

## Promotion/update

Candidate capture may become golden only through human review, authority reference, legal update reason, independent regeneration and provenance. Legitimate reasons include AUTHORITY_CHANGE, BAD_GOLDEN_FIX, GENERATOR_BUGFIX, FORMAT_MIGRATION and FUZZ_PROMOTION. `CURRENT_IMPLEMENTATION_CHANGED` or `TEST_WAS_FAILING` is not sufficient.

Blocking CI must enforce read-only golden root and no bless/update path.
