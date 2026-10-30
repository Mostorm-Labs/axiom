#pragma once

#include "canvas/input/platform_input_contract.hpp"

namespace canvas::ink_playground::apple_input {

struct AppleExtractedPointer final {
  canvas::input::PlatformPointerSample sample{};
  bool predicted = false;
};

[[nodiscard]] canvas::input::PlatformPointerBatch makeBatch(
    const AppleExtractedPointer* samples, std::size_t count) noexcept;

}  // namespace canvas::ink_playground::apple_input
