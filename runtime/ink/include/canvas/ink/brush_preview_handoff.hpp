#pragma once

#include "canvas/ink/brush_commit_intent.hpp"
#include "canvas/ink/brush_preview_delta.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace canvas::ink {

struct CanonicalVisibleReceipt final {
    std::uint64_t session = 0;
    std::uint64_t revision = 0;
    std::uint64_t targetGeneration = 0;
};

// Bounded latest-wins transport. Terminal messages are retained separately so
// a full preview queue cannot drop seal/cancel/commit intent.
class BrushPreviewHandoff final {
  public:
    explicit BrushPreviewHandoff(std::size_t capacity = 64U) : capacity_(capacity) {}
    bool publish(std::uint64_t session, BrushPreviewDelta delta) noexcept;
    bool seal(std::uint64_t session, BrushCommitIntent intent) noexcept;
    bool cancel(std::uint64_t session) noexcept;
    bool acknowledge(CanonicalVisibleReceipt receipt) noexcept;
    [[nodiscard]] std::size_t pendingPreviewCount() const noexcept { return previews_.size(); }
    [[nodiscard]] std::size_t pendingTerminalCount() const noexcept { return terminals_.size(); }

  private:
    struct Terminal final { BrushCommitIntent intent; bool cancelled = false; };
    std::size_t capacity_;
    std::unordered_map<std::uint64_t, BrushPreviewDelta> previews_;
    std::unordered_map<std::uint64_t, Terminal> terminals_;
};

} // namespace canvas::ink
