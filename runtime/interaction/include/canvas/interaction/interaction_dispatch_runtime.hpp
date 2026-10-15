#pragma once

#include "canvas/input/pointer_sample.hpp"

#include <cstdint>

namespace canvas::interaction {

enum class DispatchMode : std::uint8_t {
  kBrush,
  kObjectEraser,
  kPartialEraser,
  kCreation,
  kSelection,
  kPan,
};

enum class DispatchEvent : std::uint8_t {
  kBrush,
  kEraser,
  kCreation,
  kSelection,
  kPan,
};

class InteractionDispatchPort {
 public:
  virtual ~InteractionDispatchPort() = default;
  [[nodiscard]] virtual DispatchMode dispatchMode() const noexcept = 0;
  [[nodiscard]] virtual bool selection(const input::PointerSample&) noexcept = 0;
  [[nodiscard]] virtual bool creation(const input::PointerSample&) noexcept = 0;
  [[nodiscard]] virtual bool pan(const input::PointerSample&) noexcept = 0;
  [[nodiscard]] virtual bool brush(const input::PointerSample&) noexcept = 0;
  [[nodiscard]] virtual bool eraser(const input::PointerSample&) noexcept = 0;
};

class InteractionDispatchRuntime final {
 public:
  explicit InteractionDispatchRuntime(InteractionDispatchPort& port) noexcept : port_(port) {}

  [[nodiscard]] bool submit(const input::PointerSample& sample) noexcept;

 private:
  InteractionDispatchPort& port_;
};

}  // namespace canvas::interaction
