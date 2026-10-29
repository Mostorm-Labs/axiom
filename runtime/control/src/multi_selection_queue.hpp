#pragma once

#include "canvas/runtime/canvas_control_types.hpp"

#include <functional>

namespace canvas::runtime {
// Internal ordered-control work. The producer reserves the typed outcome and
// constructs both callbacks before queue admission. Callbacks run on the owner.
struct MultiSelectionControlWork final {
  CanvasControlReceiptKey key{};
  CanvasTargetKey target{};
  std::uint64_t deadlineSequence = 0;
  std::function<CanvasControlError()> apply;
  std::function<void(CanvasControlReceiptState, CanvasControlError)> finish;
};
}  // namespace canvas::runtime
