# SDK preview integration status

This is a working integration note, not an acceptance or release record. The
published `axiom-sdk-preview-20261030-fc536ad0` release corresponds to commit
`fc536ad0586d58834d7abec0caeee1ea177ab9d2`. Later local changes are not part
of that release until separately built and published. Historical SDK-00 and
P31 drafts remain historical records; their proposed scope and status must
not be read as a current inventory of all subsequently implemented features.

The [2026-10-31 consumer readiness review](consumer-readiness-review-20261031.md)
records a successful single-DLL feasibility experiment, the repaired npm loader
whitelist, and an open rapid-input/frame handoff failure reproduced by the
external JS example. A passing 51-case core run is not full SDK acceptance.

- Windows: the installed C++ SDK and external window integration test exist.
  The external consumer is an automated integration test, not a full interactive
  SDK application. Physical touch, native IME and long-running use still need
  external-consumer validation.
- Web: the C++/WASM SDK and legacy external browser consumer remain available.
  Current JS artifacts are `axiom.mjs`, `axiom.wasm` and `axiom.data`; the
  generated loader is an implementation detail under `internal/axiom.mjs`.
  `sdk/web/` now implements the standalone JS API v1, modular engine, public
  types, input/IME adapters and a JS-only consumer. It covers the existing
  Facade controls, text/fonts, Image/Crop, containers, connectors/Sticky,
  multiselection and public document/resource channels. The earlier sample
  `control_api.mjs` remains a legacy helper, not the new SDK entry point.
  The new package is validated in headless Chrome with real Runtime execution;
  physical IME/touch and release acceptance remain separate.
- Persistence and networking remain application responsibilities. Snapshot
  restore does not restore Undo history; a local operation callback is not a
  persistence acknowledgement. Section snapshot compatibility remains a
  candidate as documented in `section-snapshot-compatibility.md`.
- The real per-object permission, provider-failure and reservation-fault
  variants for MSE-20/23/26 remain incomplete. Synthetic Chrome input is not
  physical IME or touch evidence. The known D07 Protobuf/Abseil OOM hang remains
  unresolved and is not qualified by the SDK preview.

The user prioritized the standalone JS SDK; the interactive out-of-tree Windows
SDK example is paused. Continue with cross-platform consumer/History regressions and physical
Windows and Web input/IME testing and review the unresolved fault and API
compatibility obligations before considering a stable release.
