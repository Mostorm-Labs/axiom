# Operation Payload + Validation Rules v0.1

> Source: Notion `Operation Payload + Validation Rules v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81b78f2ce18ff60bb6c0
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Canonical Operation Payload / Validation Contract
> Repository status: frozen-by-v1-final-gate

## Canonical role

Operation is the only canonical mutation unit. The released V1 protocol uses **15 Operation payload kinds**, superseding the historical 12-operation overview. Exact numeric IDs/oneof tags are locked by Reference IDL / Generated Proto; operation oneof tags are `1..15`.

## Validation pipeline

Implementation must preserve the staged contract:

1. Decode / wire preflight.
2. Envelope validation.
3. Payload structural validation.
4. Registry / type / version validation.
5. Reference / ObjectKind / invariant validation.
6. Prepare apply plan.
7. Atomic apply.
8. Revision advance + ChangeSet publication only after success.

No partially-applied semantic mutation is allowed when a later validation step fails.

## Cross-object validation

Operations that modify placement, connector endpoints, object geometry/size, rich text, strokes or erase masks must validate the whole payload and affected references before mutation. Connector validation uses the released connectability and anchor contract.

## Idempotence

Same OperationId + same semantic payload is idempotent. Same OperationId + different payload is protocol corruption.

## Codex implementation rule

Use one semantic validator/apply-plan path for native C++, WASM and TS-facing replay behavior. Binding-specific decoding must converge before semantic validation rather than duplicating semantic rules per platform.
