#pragma once

#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/resource_binding.hpp"
#include "canvas/runtime/scene_adapter.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <thread>

namespace canvas::runtime {

enum class RuntimeCompositionState : std::uint8_t {
  kCreated,
  kBound,
  kReady,
  kLost,
  kRecovering,
  kStopping,
  kQuiescent,
  kClosed,
};

struct RuntimeCompositionReceipt final {
  RuntimeCompositionState state = RuntimeCompositionState::kCreated;
  std::uint64_t lifetimeGeneration = 1;
  std::uint64_t ownerPumpCount = 0;
  std::uint64_t resourceRevision = 0;
  std::uint64_t sceneRevision = 0;
  std::uint64_t semanticGeneration = 0;
  std::size_t pendingNotifications = 0;
  bool canonicalSubmitted = false;
};

class RuntimeComposition final {
 public:
  RuntimeComposition();
  ~RuntimeComposition();
  RuntimeComposition(const RuntimeComposition&) = delete;
  RuntimeComposition& operator=(const RuntimeComposition&) = delete;

  [[nodiscard]] bool open() noexcept;
  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool bindManifest(const semantic::ResourceManifest& manifest,
                                  std::uint64_t documentBinding) noexcept;
  [[nodiscard]] bool registerSource(input::InputSourceId source) noexcept;
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] bool notifyResource(foundation::ObjectId resourceId) noexcept;
  [[nodiscard]] bool publishResource(foundation::ObjectId resourceId,
                                     std::vector<std::uint8_t> bytes) noexcept;
  [[nodiscard]] bool ownerPump() noexcept;
  [[nodiscard]] bool requestCanonicalFrame(std::uint64_t frameId) noexcept;
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] bool rebindSurface() noexcept;
  [[nodiscard]] bool receivePlatformPresented(
      const render::PresentedFeedback& feedback) noexcept;
  [[nodiscard]] bool stop() noexcept;
  [[nodiscard]] bool detach() noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] RuntimeCompositionReceipt readback() const noexcept;
  [[nodiscard]] RuntimeCompositionState state() const noexcept { return state_; }
  [[nodiscard]] interaction::CanvasEditingRuntime& runtime() noexcept { return *runtime_; }
  [[nodiscard]] const ResourceBindingSnapshot& resources() const noexcept {
    return resources_;
  }
  [[nodiscard]] std::optional<ResourcePortReceipt> resourceReceipt(
      foundation::ObjectId id) const noexcept;
  [[nodiscard]] ResourcePortError lastResourceError() const noexcept {
    return lastResourceError_;
  }

 private:
  struct Notification final {
    foundation::ObjectId resourceId{};
    ResourceBindingToken token{};
  };
  bool canMutate() const noexcept;
  bool transition(RuntimeCompositionState next) noexcept;

  std::unique_ptr<interaction::CanvasEditingRuntime> runtime_;
  RuntimeCompositionState state_ = RuntimeCompositionState::kCreated;
  std::uint64_t ownerPumpCount_ = 0;
  std::uint64_t providerEpoch_ = 0;
  std::uint64_t nextRequestId_ = 1;
  std::vector<Notification> notifications_;
  std::unordered_map<foundation::ObjectId, ResourcePortReceipt,
                     foundation::ObjectIdHash> resourceReceipts_;
  ResourceBindingSnapshot resources_;
  std::unique_ptr<MemoryResourceProvider> provider_;
  ResourcePortError lastResourceError_ = ResourcePortError::kNone;
  std::thread::id ownerThread_;
};

}  // namespace canvas::runtime
