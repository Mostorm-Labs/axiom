#pragma once

#include <cstdint>

namespace canvas::input {

enum class InputCaptureDecision : std::uint8_t { kPassThrough, kCapture, kRelease };

class PassThroughInputCaptureGate final {
 public:
  [[nodiscard]] constexpr InputCaptureDecision onBatchBoundary() const noexcept {
    return InputCaptureDecision::kPassThrough;
  }
  [[nodiscard]] constexpr InputCaptureDecision onTerminalBoundary() const noexcept {
    return InputCaptureDecision::kRelease;
  }
};

}  // namespace canvas::input
