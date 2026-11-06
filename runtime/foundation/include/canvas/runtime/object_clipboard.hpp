#pragma once
#include "canvas/runtime/multi_selection.hpp"
#include <cstdint>
#include <vector>
#include <optional>

namespace canvas::runtime {
enum class ClipboardAction : std::uint8_t { kCopy, kCut, kDuplicate, kPaste };
enum class ClipboardError : std::uint8_t { kNone, kInvalid, kBusy, kLocked, kStale, kUnsupported, kResourceUnavailable };
struct ClipboardRequest final {
    ClipboardAction action = ClipboardAction::kCopy;
    std::optional<MultiSelectionStamp> stamp;
    std::vector<std::uint8_t> bytes;
    double offsetX = 24, offsetY = 24;
};
// Owned serialized clipboard bytes and value projections only. No store/Scene
// handles. Mutations complete on the owner thread through existing History.
struct ClipboardResult final {
    ClipboardError error = ClipboardError::kBusy;
    bool applied = false, changed = false;
    MultiSelectionStamp stamp{};
    std::vector<std::uint8_t> bytes;
    std::vector<foundation::ObjectId> roots;
    std::uint32_t objectCount = 0;
    std::uint64_t generation = 0;
};
}
