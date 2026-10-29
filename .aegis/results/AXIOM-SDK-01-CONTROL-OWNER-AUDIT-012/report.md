# SDK-01 control correlation and host owner audit — 012

Subject: `0ea22a88` (full revision recorded by Git). This is a source audit and existing-owner regression; it does not authorize or implement SDK-01. Exact input hashes are in `facts.json`.

The V1 command/setter declarations do not include full client/request IDs, target-generation token or generic final-receipt query. The existing 14-function additive candidate handles multi-selection only. The real Facade requires ordered requests and target identity, and queued admission is not terminal application. `verification/sdk01/control-correlation-gap-v1.md` records two concrete choices and their required error/order/identity tests; a complete accepted public control contract has not yet been produced.

Windows and Web currently use the host-owned composition. A distinct SDK-created composition would target a different document. `verification/sdk01/host-sdk-owner-bridge-v1.md` proposes an internal non-installed bridge with precise same-owner, lifetime and invalidation conditions; it is not implemented or accepted. Its distinction between SDK-owned external instances and host-borrowed integration needs a full design/verification binding.

Actual command: `ctest --test-dir out/sdk01-review/build -R '^(FC00XRuntimeComposition|FC00XRuntimeFacade|FC00XFrameCoordinator|FC00XRuntimeIsolation|FC02Text|FC03ImageFacade|FC05ConnectorFacade|FC08SectionConsumer|PX0ControlRouter|SelectionBatch3Summary|FC01ASelectionInputIntegration|SelectionBatch3Ui)$' --output-on-failure --no-tests=error --output-junit <absolute-output>`. Exit 0; independently parsed JUnit contains all 12 exact names, 0 failures, 0 skipped. These are existing internal Runtime/host regressions, not SDK public linkage, actual Chrome execution or physical tests.

SDK-01 still lacks accepted generic public control types/semantics and verified host-to-SDK same-owner integration. Existing multi-selection candidate, scope and P20 cases are draft. The v0.4 package remains `p32_authorized: false`; product SDK and platform consumer tests remain NOT_RUN. D07 OOM and MSE-20/23/26 findings are unchanged.
