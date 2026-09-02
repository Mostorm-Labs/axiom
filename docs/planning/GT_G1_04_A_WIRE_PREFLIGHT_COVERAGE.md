# GT-G1-04-A WirePreflight Coverage (P36)

This table binds each V1 hard-limit carrier to the earliest stage that can
enforce it. Raw checks are schema-aware span inspections performed before
`ParseFromArray`; typed checks remain the semantic backstop.

| Authority rule | Schema path | Limit | Raw-wire knowable? | Current preflight | Typed validator | Required stage | Status |
| --- | --- | ---: | --- | --- | --- | --- | --- |
| WIRE-V1-01 | `Operation` encoded bytes | 33,554,432 bytes | yes | `preflightOperationBytes` | n/a | A2 | COVERED |
| WIRE-V1-02 | `Operation.payload.*.ObjectRecord` | 16,777,216 bytes | yes | `rawOperationLimitViolation` / `inspectObjectCollection` | object structure | A2 then A3 | COVERED |
| WIRE-V1-03 | `TextRun.text`, `InsertTextStep.text` | 1,048,576 bytes each | yes | `inspectRichTextDocument` / `inspectRichTextDelta` | UTF-8/domain | A2 then A3 | COVERED |
| WIRE-V1-04 | `EditRichTextOp.delta` inserted text aggregate | 8,388,608 bytes aggregate | yes (schema-aware InsertText branch traversal) | `inspectRichTextDelta` | rich-text validator | A2 then A3 | COVERED |
| WIRE-V1-05 | keyed repeated operation collections | 65,535 occurrences | yes | schema-aware collection walkers | canonical-set validator | A2 then A1/A3 | COVERED |
| WIRE-V1-05 | `ObjectRecord.erase_masks` | 65,535 occurrences | yes | `inspectObjectRecord` | object validator | A2 then A3 | COVERED |
| ORDER-V1-01 | `Placement.order_key.value` | 1..32 bytes | yes for max length | `inspectOrderKey` | `OrderKey::isValid` | A2 then A3 | COVERED |
| GAA-V1-01 | geometry atom aggregate | 2,000,000 units | not generally (exact branch semantics) | no approximation | `geometryUnits` | A3 | COVERED |

Raw preflight is fail-closed on malformed/truncated wire, computes the EditRichText
InsertText aggregate before `ParseFromArray`, and does not replace the canonical
typed geometry counter.
