# 07-13 Blob / Resource Materialization State Machine v0.1

> Source page: https://app.notion.com/p/3c44c57a590c819390f7f7e6ea46c57d
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze

## Core separation

`CanonicalReferenced != LocalBytesPresent != IntegrityVerified != RuntimeMaterialized != CloudAvailable`.

ResourceId/BlobRef is canonical semantic identity. Missing local bytes, pending download, decode failure, GPU eviction or offline state do not change that identity or make an otherwise valid Operation semantically invalid.

Blob bytes are physical resource data; decoded image/SkImage/GPU texture/materialization are derived and disposable.

## Ownership

Axiom schema owns ResourceId/BlobRef meaning and canonical content metadata. Shared Data Runtime BlobManager owns byte custody, local/cloud availability, upload/download and GC. Axiom Resource/Render runtime owns decode and CPU/GPU materialization. Resource readiness never mutates SemanticDocument.

## Orthogonal axes

Resource state is not one enum. Separate semantic reference, local byte availability, integrity, decode, render materialization, cloud availability and retention/GC axes.

A resource may be canonically referenced, locally missing, integrity unknown, shown as placeholder and already CloudAvailable.

## Materialization flow

Canonical ResourceId → BlobManager lookup → bytes present or acquire/download → integrity verify → Scheduler decode/parse → runtime materialization → derived invalidation/frame request. Missing/failure uses canonical placeholder according to render policy. None of these steps emits Operation or advances SemanticGeneration.

## LOCAL READY

LOCAL READY does not wait for all BlobRefs to materialize. Canonical metadata must allow RuntimeScene/object records to exist while resources are placeholders/acquiring.

## Local import durability

Canonical image/resource commit does not synchronously wait for Blob fsync/upload. However, a locally introduced resource whose only necessary bytes are not yet crash-safe creates a LocalResourceRecoveryObligation.

Document-level Saved closure is `OperationLocalDurableThrough && LocalResourceRecoveryClosureThrough`; it does not require downloading every remote resource.

## Cloud closure

Operation CloudSynced does not prove referenced Blob CloudAvailable unless protocol explicitly defines that guarantee. Document-level Synced closure must include resource cloud obligations introduced by relevant local commits.

## Handoff

07-15 closes placeholder handoff ambiguity through token-specific ResourceCoveragePolicy: CanonicalFallbackAllowed or MaterializedResourceRequired.

## GC

Blob GC is physical custody logic and must respect canonical references, pending recovery/upload obligations and rehydratability. GPU/decoded eviction is independent of durable Blob GC.

## OPEN

Manifest/DB schema, ResourceId hash policy, integrity algorithm, BlobStore backend, blob transport/CDN protocol, placeholder visuals, decoder and quota/grace thresholds remain open.