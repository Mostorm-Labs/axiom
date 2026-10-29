# D07 Accepted Scope — Local Multi-Operation Atomicity & One Undo

The user accepted this bounded product requirement on 2026-10-27.

A user-visible intent spanning multiple canonical Operations must either:
1. commit as one coherent local Runtime/Scene result and create exactly one user-visible Undo unit; or
2. fail before canonical mutation and create no successful History entry.

Required:
- Preflight all forward Operations and targets before the first canonical mutation.
- Prepare exact inverse/preimage Operations and expectations before mutation.
- Reserve HistoryGroup/UndoIntent capacity and required result/publication memory before mutation.
- Serialize the local commit/publication segment so no partial local Scene/observer publication escapes between constituent Operations.
- Preserve ordinary typed Operations, identities, semantic validation, and per-operation records.
- Undo the exact committed intent, independent of current selection.
- Redo the accepted forward meaning with fresh Operation identities.
- Failure leaves Document, Scene/index publication, History cursor, and redo branch unchanged.
- No post-commit allocation failure may strand canonical state without its corresponding History unit.

Explicit exclusions: crash-atomic persistence, cross-client atomic visibility, distributed transactions, a canonical global Transaction, new wire semantics, new ObjectKind/FieldId/PropertyValue/OperationKind/public ABI/global registry. D06-B eligible-subset editing remains separate and OPEN.
