#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>

namespace canvas::debug_ui {

inline constexpr std::size_t kDefaultDebugCommandQueueCapacity = 256;
inline constexpr std::size_t kDefaultDebugCommandReceiptCapacity = 1024;

enum class DebugCommandKind : std::uint8_t {
    kRequestSurfaceMode,
    kInvalidatePreview,
    kRequestInspection,
    kSetCadence,
};

enum class ReceiptState : std::uint8_t {
    kAccepted,
    kCompleted,
    kRejected,
    kStaleGeneration,
    kExpired,
    kDestroyed,
    kEvicted,
};

struct DebugCommand final {
    std::uint64_t id = 0;
    std::uint64_t generation = 0;
    std::uint64_t deadlineSequence = 0;
    DebugCommandKind kind = DebugCommandKind::kRequestSurfaceMode;
    std::string argument;
};

struct CommandReceipt final {
    std::uint64_t id = 0;
    ReceiptState state = ReceiptState::kRejected;
    std::uint64_t generation = 0;
};

class BoundedCommandQueue final {
  public:
    explicit BoundedCommandQueue(
        std::size_t capacity = kDefaultDebugCommandQueueCapacity,
        std::size_t receiptCapacity = kDefaultDebugCommandReceiptCapacity);
    [[nodiscard]] std::optional<CommandReceipt> admit(DebugCommand command);
    [[nodiscard]] std::optional<DebugCommand> take(std::uint64_t currentSequence,
                                                    std::uint64_t currentGeneration);
    [[nodiscard]] std::optional<CommandReceipt> complete(std::uint64_t id, ReceiptState state);
    [[nodiscard]] std::size_t destroyGeneration(std::uint64_t generation);
    [[nodiscard]] std::size_t expire(std::uint64_t currentSequence);
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::optional<CommandReceipt> receipt(std::uint64_t id) const;

  private:
    std::optional<CommandReceipt> terminalLocked(std::uint64_t id, ReceiptState state);
    mutable std::mutex mutex_;
    std::size_t capacity_;
    std::size_t receiptCapacity_;
    std::deque<DebugCommand> pending_;
    std::unordered_map<std::uint64_t, CommandReceipt> receipts_;
};

}  // namespace canvas::debug_ui
