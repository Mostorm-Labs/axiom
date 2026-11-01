#pragma once

#include "canvas/input/platform_input_contract.hpp"
#include "canvas/runtime/resource_binding.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/snapshot.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace canvas::runtime {

class RuntimePlatformAccess;

// Read-only physical and logical surface facts owned by the Composition.
struct RuntimeSurfaceMetrics final {
  std::uint64_t viewId = 0;
  std::uint64_t surfaceGeneration = 0;
  std::uint64_t metricsGeneration = 0;
  float logicalWidth = 0.0F;
  float logicalHeight = 0.0F;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float devicePixelRatio = 1.0F;
  float displayScale = 1.0F;
  bool available = false;
};

// Read-only owner-thread observations for platform diagnostics.
struct RuntimeHostObservation final {
  std::uint32_t lifecycleState = 0;
  std::uint64_t objectCount = 0;
  std::uint64_t lastCanonicalFrame = 0;
  std::uint64_t lastOwnerControlCount = 0;
  bool previewActive = false;
};

enum class DocumentSnapshotStatus : std::uint8_t {
  kCaptured, kWrongThread, kNotReady, kBusy, kStale, kInvalidCheckpoint, kFailed
};
// The data owner supplies metadata for the same semantic generation. The
// Runtime neither invents a recovery frontier nor asserts durable storage.
struct DocumentSnapshotRequest final {
  std::uint64_t expectedSemanticGeneration = 0;
  std::uint64_t documentRevision = 0;
  std::vector<std::uint8_t> recoveryFrontier;
  // Empty derives the requirements from the captured canonical objects.
  std::vector<std::uint8_t> capabilityRequirements;
};
struct DocumentSnapshotResult final {
  DocumentSnapshotStatus status = DocumentSnapshotStatus::kFailed;
  std::uint64_t semanticGeneration = 0;
  std::vector<std::uint8_t> bytes;
};
enum class DocumentRestoreStatus : std::uint8_t { kRestored, kWrongThread, kNotReady, kInvalidSnapshot, kSceneFailed, kFailed };
struct DocumentRestoreRequest final {
  std::vector<std::uint8_t> bytes;
  foundation::ObjectId expectedDocumentId{};
  std::uint64_t documentRevision = 0;
  std::vector<std::uint8_t> recoveryFrontier;
  std::vector<std::uint8_t> checkpointDigest;
  std::uint64_t resourceBinding = 0;
};

// Owner-thread entry. The borrowed facade belongs to this Composition and
// remains valid until its owner is destroyed. Close before destruction.
class RuntimeComposition final {
 public:
  // Owner-thread callback receives borrowed values only for the call duration.
  // It must not destroy the Runtime or perform reentrant mutation. Delivery
  // means canonical apply, not storage durability or network acknowledgment.
  using LocalOperationCallback = void (*)(void*, const semantic::Operation&,
                                          const semantic::CanonicalCommitRecord&) noexcept;
  using LocalOperationPrepare = bool (*)(void*, std::span<const semantic::Operation>) noexcept;
  using LocalOperationAbort = void (*)(void*) noexcept;
  struct OperationDataStatus {
      std::uint8_t localDurable = 0, cloudSynced = 0;
  };
  using LocalOperationStatus = OperationDataStatus (*)(
      void*, const semantic::OperationId&, const semantic::CanonicalCommitStamp&) noexcept;
  explicit RuntimeComposition(std::uint64_t viewId = 1);
  ~RuntimeComposition();
  RuntimeComposition(const RuntimeComposition&) = delete;
  RuntimeComposition& operator=(const RuntimeComposition&) = delete;
  RuntimeComposition(RuntimeComposition&&) = delete;
  RuntimeComposition& operator=(RuntimeComposition&&) = delete;

  [[nodiscard]] bool open() noexcept;
  // Opens one new document with the caller's full identity. This is not a
  // snapshot restore or a durable-storage acknowledgment.
  [[nodiscard]] bool openDocument(foundation::ObjectId documentId) noexcept;
  [[nodiscard]] std::optional<foundation::ObjectId> documentId() const noexcept;
  [[nodiscard]] std::optional<semantic::CanonicalCommitStamp> canonicalCommitFence() const noexcept;
  [[nodiscard]] bool setLocalOperationCallback(LocalOperationCallback callback, void* context) noexcept;
  [[nodiscard]] bool setLocalOperationCallbacks(LocalOperationCallback callback,
                                                void* context,
                                                LocalOperationPrepare prepare,
                                                LocalOperationAbort abort,
                                                LocalOperationStatus status) noexcept;
  // Remote apply is canonical-only; an Applied result does not assert that
  // a Scene has presented or that external storage has persisted the input.
  [[nodiscard]] semantic::ApplyResult applyRemoteOperation(const semantic::Operation& operation) noexcept;
  [[nodiscard]] semantic::ApplyResult applyReplayOperation(const semantic::Operation& operation) noexcept;
  [[nodiscard]] DocumentSnapshotResult captureDocumentSnapshot(const DocumentSnapshotRequest& request) noexcept;
  // Restores only into a fresh owner; checkpoint metadata is supplied by the
  // data layer. History and remote operations before the frontier are not replayed.
  // Must precede borrowing controlFacade(), whose lifetime is bound to this owner.
  [[nodiscard]] DocumentRestoreStatus openDocumentFromSnapshot(const DocumentRestoreRequest& request) noexcept;
  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool registerSource(input::InputSourceId source) noexcept;
  [[nodiscard]] bool unregisterSource(input::InputSourceId source, std::uint64_t sequence,
                                      std::uint64_t timestampNs) noexcept;
  // A Down starts one contact and returns the Runtime-assigned identity. Every
  // later observation must carry that identity, including delayed events.
  [[nodiscard]] std::optional<input::PointerKey> beginContact(
      const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] bool submitContact(const input::PointerKey& key,
                                   const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] bool cancelContact(const input::PointerKey& key, std::uint64_t sequence,
                                   std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool ownerPump() noexcept;
  // Bind the initial resource metadata for a new, empty document only. This is
  // not a document loader and cannot replace an existing manifest or objects.
  [[nodiscard]] bool bindInitialResourceManifest(const semantic::ResourceManifest& manifest,
                                                  std::uint64_t documentBinding) noexcept;
  // Retain the full token while obtaining bytes; stale completions are rejected.
  [[nodiscard]] std::optional<ResourceBindingToken> resourceToken(foundation::ObjectId id) const noexcept;
  [[nodiscard]] bool publishResourceBytes(const ResourceBindingToken& token,
                                          std::vector<std::uint8_t> bytes) noexcept;
  [[nodiscard]] std::optional<ResourcePortReceipt> resourceReceipt(foundation::ObjectId id) const noexcept;
  [[nodiscard]] std::optional<ResourceBindingSnapshot> resourceSnapshot() const noexcept;
  [[nodiscard]] std::optional<std::vector<std::uint8_t>>
  readResourceBytes(const ResourceBindingToken&) const noexcept;
  // Empty when queried from a thread other than the owner thread.
  [[nodiscard]] std::optional<RuntimeSurfaceMetrics> surfaceMetrics() const noexcept;
  [[nodiscard]] std::optional<RuntimeHostObservation> hostObservation() const noexcept;
  [[nodiscard]] bool hoverSelectionAtViewPoint(float x, float y) noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] RuntimeFacade& controlFacade() noexcept;
  [[nodiscard]] const RuntimeFacade& controlFacade() const noexcept;

 private:
  friend class RuntimePlatformAccess;
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace canvas::runtime
