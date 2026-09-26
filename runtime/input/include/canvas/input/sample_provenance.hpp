#pragma once

#include <cstdint>

namespace canvas::input {

enum class SampleProvenance : std::uint8_t {
  kConfirmedCurrent,
  kCoalescedHistory,
  kPredicted,
};

}  // namespace canvas::input
