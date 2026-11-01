# Document lifecycle and external Web consumer — scoped result

Base: `d2318c731c8180561855a56210c59c46c6602119`. Source and raw result hashes are in `manifest.json`. All SDKs are consumed from locked published packages.

- Windows: `cmake --build out/sdk01-review/build --target axiom_sdk_public_document_snapshot_tests axiom_sdk_public_document_operations_tests axiom_sdk_public_document_identity_tests axiom_sdk_public_resources_tests --parallel 6` followed by `ctest --test-dir out/sdk01-review/build -R "^(SDKPublicDocumentSnapshot|SDKPublicDocumentOperations|SDKPublicDocumentIdentity|SDKPublicResources)$" --output-on-failure`: exit 0; 4/4 tests.
- Section codec: `out/sdk01-review/build/runtime/semantic/tests/axiom_fc08_section_codec_tests.exe`: exit 0; 49/49 tests, including Section snapshot compatibility.
- `python verification/sdk_cpp_windows/run_external.py ...`: exit 0, `PASS_SCOPED`; exact command and raw output are in `raw/windows-external.json`.
- `python out/sdk-web-adapter-build.py`: exit 0. `python verification/sdk_web_external/run_external.py ...`: exit 0, `PASS_SCOPED`; exact commands, expected negative checks and Chrome outputs are in `raw/web-external.json`. Chrome tests use synthetic input.
- Hover regression originally failed in `raw/hover-red.json`; after rebuilding the transient selection view, the scoped Chrome suite exercises the same selected shape and DOM hover path. The borrowed-facade restore regression failed before the guard in `raw/borrowed-facade-red.log` and is included in the passing public snapshot test.

This is a compatibility candidate, not a published wire authority. External data owners must coordinate and persist resources and checkpoint frontier, replay operations strictly after it, and handle durability; History is not restored. Close has no durable-acknowledgment barrier. Real IME, physical touch, browser coverage, advanced internal Debug UI parity, and full SDK acceptance remain unverified. D07 OOM is deferred, not passed.
