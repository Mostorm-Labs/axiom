#pragma once

#if defined(_WIN32)

#include <cstdint>
#include <string>
#include <vector>

namespace canvas::ink_playground::windows_input {

struct PointerEvidenceSample final {
  std::uint32_t pointerId = 0;
  std::uint64_t timestampMs = 0;
  std::string inputType;
  std::string phase;
  float x = 0.0F;
  float y = 0.0F;
  float pressure = 0.0F;
  std::size_t batchSize = 0;
};

[[nodiscard]] std::string serializePointerTrace(
    const std::vector<PointerEvidenceSample>& samples, std::uint64_t deviceId);

}  // namespace canvas::ink_playground::windows_input

#endif
