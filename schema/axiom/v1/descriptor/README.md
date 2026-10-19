# Descriptor baselines

`descriptor.lock.pb` and its SHA-256 retain the historical G1 reconciliation and
Golden inputs. Historical verifiers and evidence continue to consume those exact
bytes; this alignment does not supersede their records.

`descriptor.current.pb` and its SHA-256 lock the current complete `.proto` tree,
generated with the published Semantic SDK's host `protoc` 36.0. Current source
reproducibility and CI compare against this artifact, while validating both hashes
and the historical reconciliation contract separately.

The current artifact first records the resource-manifest schema already introduced
by commit `4bef9d6e` (FC-00X), without modifying any proto definition:

- `OperationPayload.update_resource_manifest` at tag 16 and its three carriers.
- `DocumentSnapshot` resource/recovery fields at tags 4–10 and eight new resource,
  metadata and recovery carrier messages.

All previous messages, fields and enums are preserved, including RichText. The
additive boundary is checked by `test_current_semantic_descriptor.py`. This is a
source baseline alignment, not a new wire release or an acceptance declaration.

To verify using resolver-emitted `AXIOM_PROTOC`:

```powershell
python verification/tools/validate_semantic_contract.py --root . --protoc $env:AXIOM_PROTOC
python -m unittest verification.tests.test_current_semantic_descriptor
```

Neither artifact is refreshed automatically by validation. Intentional future
schema changes require an explicit diff and baseline update in their own scope.
