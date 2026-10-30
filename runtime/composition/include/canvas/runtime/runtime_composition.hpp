#pragma once

#include "canvas/input/platform_input_contract.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <cstdint>
#include <memory>
#include <optional>

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

// Owner-thread entry. The borrowed facade belongs to this Composition and
// remains valid until its owner is destroyed. Close before destruction.
class RuntimeComposition final {
 public:
  explicit RuntimeComposition(std::uint64_t viewId = 1);
  ~RuntimeComposition();
  RuntimeComposition(const RuntimeComposition&) = delete;
  RuntimeComposition& operator=(const RuntimeComposition&) = delete;
  RuntimeComposition(RuntimeComposition&&) = delete;
  RuntimeComposition& operator=(RuntimeComposition&&) = delete;

  [[nodiscard]] bool open() noexcept;
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
  // Empty when queried from a thread other than the owner thread.
  [[nodiscard]] std::optional<RuntimeSurfaceMetrics> surfaceMetrics() const noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] RuntimeFacade& controlFacade() noexcept;
  [[nodiscard]] const RuntimeFacade& controlFacade() const noexcept;

 private:
  friend class RuntimePlatformAccess;
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace canvas::runtime
