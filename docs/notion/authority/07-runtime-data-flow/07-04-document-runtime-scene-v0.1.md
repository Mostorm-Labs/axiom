# 07-04 Document → RuntimeScene Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81e5b859fe6168157f26
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Canonical → Derived Scene Projection Contract

## Core invariant

For the same canonical generation, Incremental Compile RuntimeScene must be equivalent to Full Rebuild RuntimeScene.

SemanticDocument/ObjectStore is canonical. RuntimeScene, bounds, geometry chunks, SpatialIndex and dependency state are derived and rebuildable.

## Topology correction

ObjectStore/ObjectIndex are not downstream of RuntimeScene. SemanticDocument owns canonical ObjectStore and non-spatial ObjectIndex acceleration; SceneCompiler reads post-state through a semantic read boundary and projects derived runtime records.

## Input

Conceptual SceneCompileInput consists of source SemanticGeneration, post-commit ChangeSet for incremental mode, immutable SemanticReadPort/read view and runtime compile context. SceneCompiler does not re-execute Operations or mutate canonical state.

## Full compile

A stable SemanticDocument generation can build RuntimeScene from scratch: iterate canonical objects/hierarchy → kind-specific runtime records → bounds → geometry/chunks where needed → SpatialIndex → publish RuntimeScene at source generation. This is cold-open/recovery/corruption fallback and correctness oracle.

## Incremental compile

Existing RuntimeScene + ChangeSet + post-state SemanticDocument drives localized work: capture old derived contribution, classify impact, create/update/remove runtime records, expand dependency closure, recompute affected bounds/geometry, update spatial entries, then publish a self-consistent RuntimeScene for the new generation.

If incremental correctness cannot be proven, Full Compile is a valid recovery path.

## ChangeSet role

ChangeSet is a post-commit impact hint, not a second Operation or persistence format. It identifies actual inserted/deleted/updated/reordered objects and semantic component changes. Old visual contribution is obtained from existing RuntimeScene/bounds before-state rather than being mandatory Semantic ChangeSet payload.

07-11 freezes the logical ChangeSet shape and generation rules.

## Performance contract

Normal incremental cost should scale with changed objects + dependency closure + affected derived records, not total document size. Correctness takes priority over locality.

## OPEN

Physical RuntimeScene layout, Spatial backend, chunk size, exact queue/container/thread realization and benchmark-selected policies remain implementation/optimization decisions.