#pragma once

#include "canvas/input/pointer_sample.hpp"

#include <cstdint>
#include <cstddef>
#include <unordered_map>

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
  [[nodiscard]] virtual bool selection(const input::PointerSample&,
                                       std::uint64_t sessionId) noexcept = 0;
  [[nodiscard]] virtual bool creation(const input::PointerSample&,
                                      std::uint64_t sessionId) noexcept = 0;
  [[nodiscard]] virtual bool pan(const input::PointerSample&,
                                 std::uint64_t sessionId) noexcept = 0;
  [[nodiscard]] virtual bool brush(const input::PointerSample&,
                                   std::uint64_t sessionId) noexcept = 0;
  [[nodiscard]] virtual bool eraser(const input::PointerSample&,
                                    std::uint64_t sessionId) noexcept = 0;
  [[nodiscard]] virtual bool cancel(DispatchMode, const input::PointerSample&,
                                    std::uint64_t sessionId) noexcept = 0;
};

class InteractionDispatchRuntime final {
 public:
  explicit InteractionDispatchRuntime(InteractionDispatchPort& port) noexcept : port_(port) {}

  [[nodiscard]] bool submit(const input::PointerSample& sample) noexcept;
  [[nodiscard]] bool cancel(const input::PointerSample& sample) noexcept;
  void cancelAll() noexcept;
  [[nodiscard]] std::size_t activeCount() const noexcept { return sessions_.size(); }

 private:
  struct Session final {
    DispatchMode mode = DispatchMode::kBrush;
    std::uint64_t id = 0;
  };
  InteractionDispatchPort& port_;
  std::uint64_t nextSessionId_ = 1;
  std::unordered_map<input::PointerKey, Session, input::PointerKeyHash> sessions_;
};

}  // namespace canvas::interaction
