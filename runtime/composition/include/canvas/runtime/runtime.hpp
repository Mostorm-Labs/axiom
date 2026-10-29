#pragma once

#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/input/platform_input_contract.hpp"

#include <cstdint>
#include <memory>

namespace canvas::runtime {

// Owner-thread C++ entry point. The borrowed facade is valid for this object's
// lifetime; after close its commands fail through the existing owner contract.
// Runtime binaries and clients must use the same compiler and CRT configuration.
class Runtime final {
 public:
  Runtime();
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
  Runtime(Runtime&&) = delete;
  Runtime& operator=(Runtime&&) = delete;

  [[nodiscard]] bool open() noexcept;
  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool registerSource(input::InputSourceId source) noexcept;
  [[nodiscard]] bool unregisterSource(input::InputSourceId source, std::uint64_t sequence,
                                      std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] bool pump() noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] RuntimeFacade& facade() noexcept;
  [[nodiscard]] const RuntimeFacade& facade() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace canvas::runtime
