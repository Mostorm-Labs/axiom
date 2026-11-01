# Rounded corners and handwriting font increment

Authority: 2026-11-01 user response annotations 1/2 and font-renaming question; AGENTS.md.
Repository Mostorm-Labs/axiom, anchor af5b1778284211c3834957084dd0a03e48308d69.
Preserve prior dirty Sketch/Table work: out/rounded-font-work/baseline.json and baseline-source.
This user-authorized additive contract does not rewrite frozen historical authority.

## Contract

- CornerStyleV1, field 0x103, version 1, requested radius [0,16384] in object local units. Zero canonicalizes to absence. Shape rectangle/triangle and Connector only; ellipse and all other kinds explicitly excluded in radius readback. Radius independent of sketch seed/roughness. Reuse appearance patch and preview sessions, per-field eligibility/mixed/defaults, atomic History/publication. No-op/cancel unchanged.
- Shared renderer-neutral fillet builder: true circular cubic fillets (cubic approximation), requested rectangle radius clamped to half shortest side; triangle radius to inradius; common edge budget prevents overlap. Stable authored dimensions/handles. Rounded boundary controls fill clip/hit/derived attachment; sketch decorations follow the rounded path without canonical coordinate rewrites. Absent/zero corner property retains prior v1 paths exactly.
- Connector: route first, fillet interior corners only, preserve first/last positions and terminal directions. Bound trim by half adjacent segments; use obstacle/control-envelope checks with a fixed bounded reduction count, then exact corner fallback. Never rewrite semantic route/endpoints for appearance. Arrow decorations retain existing terminal direction and seed.
- Closed semantic value append, IDL/codec/normalization/fingerprint/validation/current descriptor and Snapshot reader bit512 / rounded-corners-v1; current full profile2047. Unknown versions/unsupported kinds/readers reject. Runtime/Session/Web/types/Windows caller wrapper remain values/commands only.
- Import original Excalifont (lossless WOFF2 unpack to TTF for shared Skia) and Xiaolai v3.126 with hashes/provenance/OFL, preserve upstream names/copyright. User has not selected a derivative name or requested glyph changes. UI labels may be separate; any future derivative retains OFL and provenance and respects RFNs/trademarks.
- Deterministic single-hop font fallback stored in version2 font metadata/layout version2, explicit primary and fallback resource IDs. Fallback target must be an admitted version1 font; dangling/cyclic references reject. Older metadata readers reject v2; Snapshot bit1024 / font-fallback-v2 is also required. Only admitted bytes enter shared SkParagraph and glyph observations; no OS/browser fallback, guessed fonts, or canonical text-run rewriting. Resource bytes/revisions/hashes participate in layout invalidation. Existing v1 fonts unchanged.
- Demo four rows: tools / appearance properties / text-object properties / board-history-navigation tools. Public font catalog selector for text and Table cells; Excalifont primary uses admitted Xiaolai fallback. New text defaults remain separate from document Undo. No browser business renderer.

## Execution closure

Required: persisted corner lifecycle and common geometry including safe connector bends; font assets and controlled shared fallback; matching public DTOs/commands and actual four-row Demo.
Tests/oracles: focused red→green native geometry/semantic/Runtime/font shaping; matching real Chrome external SDK with mixed text, font chooser, slider/preview/history, Snapshot/remote/export, route/attachment safety and invalid cases; Native/WASM path comparison, prior sketch hashes, Table/Section/export regressions, boundary/type/descriptor/package checks and configured CTest with historical failures compared.
Blocking evidence: repo-local implementation report, reproducible logs/hashes, actual localhost Demo/samples. Hosted Windows ABI/device proof optional and explicitly unverified here.
Forbidden: dependency stack rebuild/lock edits, private owner exports, second document/history/selection owner, erasing prior work, automatic commit/push/release, proprietary relabeling of OFL fonts, formal Gate claims.
Terminal success: all required affected checks pass, prior failures classified, usable matching-assets Demo and report. Explicit blockers: authority conflict, missing required input, unavailable locked environment, new unresolved high-impact correctness failure. Continue through remaining executable items until success or explicit blocker.
SDK remains the unpublished numeric0.1.0 candidate; cornerVersion1/styleVersion1 and font metadata version2 are separate compatibility contracts.
