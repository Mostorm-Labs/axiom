#pragma once

#include "canvas/input/platform_input_contract.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <cstdint>
#include <memory>

namespace canvas::runtime {

class RuntimePlatformAccess;

// Owner-thread entry. The borrowed facade belongs to this Composition and
// remains valid until its owner is destroyed. Close before destruction.
class RuntimeComposition final {
 public:
  RuntimeComposition();
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
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] bool ownerPump() noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] RuntimeFacade& controlFacade() noexcept;
  [[nodiscard]] const RuntimeFacade& controlFacade() const noexcept;

 private:
  friend class RuntimePlatformAccess;
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace canvas::runtime
