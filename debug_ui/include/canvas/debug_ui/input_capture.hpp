#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace canvas::debug_ui {

enum class DebugInputOwner : std::uint8_t { kCanvas, kDebug };

struct DebugInputSequence final {
    std::uint64_t pointerId = 0;
    std::uint64_t sequence = 0;
    friend bool operator==(const DebugInputSequence&, const DebugInputSequence&) = default;
};

struct DebugInputSequenceHash final {
    std::size_t operator()(const DebugInputSequence& key) const noexcept {
        return static_cast<std::size_t>((key.pointerId * 0x9e3779b97f4a7c15ULL) ^ key.sequence);
    }
};

class InputCaptureGate final {
  public:
    [[nodiscard]] DebugInputOwner begin(DebugInputSequence sequence, DebugInputOwner requested);
    [[nodiscard]] std::optional<DebugInputOwner> owner(DebugInputSequence sequence) const;
    [[nodiscard]] std::optional<DebugInputOwner> route(DebugInputSequence sequence) const;
    [[nodiscard]] bool terminal(DebugInputSequence sequence);
    [[nodiscard]] std::size_t clear();

  private:
    mutable std::mutex mutex_;
    std::unordered_map<DebugInputSequence, DebugInputOwner, DebugInputSequenceHash> owners_;
};

}  // namespace canvas::debug_ui
