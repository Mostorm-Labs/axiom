# ResourceManifest v0.3 candidate oracle matrix

| id | input | expected independent result |
|---|---|---|
| RM-DIGEST-LENGTH | domain prefix and empty manifest | 28-byte prefix, exact bytes, no character-count ambiguity |
| RM-DIGEST-PRESENCE | absent legacy manifest, explicit empty manifest, present-default metadata | distinct presence and expected digest relation |
| RM-DIGEST-REVISION | same entries, manifest_revision changed | digest differs; document/recovery revision alone does not |
| RM-METADATA-IMAGE | missing field, wrong schema, zero dimensions, >4096 bytes | reject before mutation |
| RM-METADATA-FONT | bad collection index/layout profile/unknown field | reject before mutation |
| RM-METADATA-BRUSH | bad asset role/zero dimensions/unknown field | reject before mutation |
| RM-REMOVE-REFERENCE | live image/font/brush reference | whole Operation rejected, no generation/ChangeSet |
| RM-REMOVE-REACTIVATE | remove, ordinary upsert, exact reactivation | upsert rejected; exact reactivation creates new revision and keeps tombstone |
| RM-UNDO-CONFLICT | remove, concurrent update, stale compensation | conflict/rejected, no partial mutation |
| RM-CHANGESET-MERGE | multiple IDs and same-ID staged changes | sorted unique output, before/after/tombstone presence rules |
| RM-LEGACY | schema1 absent field4 versus schema3 omitted field5..10 | only schema1 legacy accepted; schema3 fails closed |
