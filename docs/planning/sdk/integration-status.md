# SDK integration status

The current Shell / Axis extension is authorized by
`.aegis/packages/AXIOM-SDK-SHELL-AXIS-20261031-v0.1/package.json`, based on the
published `4fc3ebfe1325c9197024f1cd7137e54473a1c76d` preview. It adds dynamic
subscriptions, precommit volatile packet custody/admission, real commit identity,
external status reports, mixed remote batches, resource discovery/byte retrieval
and a close publication barrier. See `shell-axis-integration.md`. The published
preview assets remain unchanged until a new matching release is built/published.

The preceding consumer SDK profile was authorized by
`.aegis/packages/AXIOM-SDK-CONSUMER-20261031-v0.1/package.json`. Historical SDK-00
and P31 candidates retain their original status; they are not the inventory of
this later explicit user-authorized consumer implementation.

- JS pending-frame handoff is closed by the real delayed-fence RED/GREEN
  regression. The SDK and shipped JS example cover controls, two erasers,
  orthogonal connectors, text/fonts, Image/Crop, multiselection, containers,
  snapshots, operation delivery/remote application and resources. See
  `frame-handoff-closure.md` and `consumer-delivery-20261031.md`.
- Windows now has a consumer `axiom.dll`, 16 explicit C/POD exports and a
  caller-side C++ facade. The /MD interactive external app uses no internal
  headers or component libraries. Native mouse/pointer/IMM32 collection reuses
  the current owners. Independent handles, contact generations, response-buffer
  ownership, callback reentry, format focus and controlled close are exercised.
- `sdk/common/` is a private shared transport decoder over the existing
  Composition/Facade; it adds no canonical, History or membership owner.
- The preview workflow produces four matching consumer packages; the old
  published 20261030-fc536ad0 assets remain immutable. A new preview exists only
  once that exact tag's build and publication have succeeded.
- Persistence/networking/resource blob storage remain application-owned.
  Snapshot restore does not reconstruct Undo history or embed resource bytes.
  In-memory operation delivery is not a persistence acknowledgement.
- Physical IME, touch and DPI/long-running use remain PHYSICAL_PENDING.
  The MSE-20/23/26 provider/fault gaps, historical qualification failures and
  D07 Protobuf/Abseil injected-OOM hang are not closed by this SDK work.

Automated tests and exact commands are recorded separately from physical and
Gate claims. Consumer API v1 is versioned; the preview does not promise an
unchanging cross-version C++ ABI or release readiness.
