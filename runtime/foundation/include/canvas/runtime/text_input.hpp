#pragma once
#include "canvas/runtime/canvas_control_types.hpp"

#include <optional>
#include <string>

namespace canvas::runtime {
// Source API for the initial editor. No native handles or canonical mutations.
struct TextSessionToken final {
    std::uint64_t runtimeInstance = 0, attachment = 0, view = 0, adapter = 0, session = 0,
                  projection = 0;
    foundation::ObjectId document{}, object{};
    bool operator==(const TextSessionToken&) const = default;
};
struct TextRange final {
    std::uint32_t start = 0, length = 0;
    bool operator==(const TextRange&) const = default;
};
enum class TextInputKind : std::uint8_t { kBegin, kUpdate, kCommit, kCancel, kReplace, kSelection };
enum class TextStatus : std::uint8_t {
    kUnsupported,
    kQueued,
    kObserved,
    kApplied,
    kRejected,
    kStale,
    kOutOfOrder,
    kCollision,
    kBusy,
    kBackpressure,
    kExpired
};
enum class TextPhase : std::uint8_t { kClosed, kEditing, kComposing, kFinishing, kCommitFailed };
struct TextInputEvent final {
    TextSessionToken token{};
    std::uint64_t sequence = 0, composition = 0, finishEffect = 0;
    TextInputKind kind = TextInputKind::kSelection;
    TextRange range{};
    std::u16string text;
    std::optional<TextRange> preeditSelection = std::nullopt;
    bool operator==(const TextInputEvent&) const = default;
};
enum class TextIntentKind : std::uint8_t {
    kCreate,
    kEdit,
    kFinish,
    kInsertParagraph,
    kEscape,
    kSetWidth,
    kRetry,
    kResync
};
struct TextIntent final {
    CanvasTargetKey target{};
    std::uint64_t clientId = 0, requestId = 0;
    TextIntentKind kind = TextIntentKind::kCreate;
    TextSessionToken token{};
    foundation::ObjectId object{};
    float viewX = 0, viewY = 0;
    double width = 320;
    bool operator==(const TextIntent&) const = default;
};
struct TextReceipt final {
    TextStatus status = TextStatus::kUnsupported;
    TextSessionToken token{};
    std::uint64_t request = 0, semanticGeneration = 0;
    foundation::ObjectId operation{};
    bool replay = false;
};
struct TextEditorSnapshot final {
    TextSessionToken token{};
    TextPhase phase = TextPhase::kClosed;
    std::u16string projection, preedit;
    TextRange selection{}, replacement{};
    std::optional<TextRange> preeditSelection = std::nullopt;
    std::uint64_t composition = 0, finishEffect = 0, nextInputSequence = 1;
    std::size_t projectionLimit = 65536;
    double width = 320;
    TextStatus exitStatus = TextStatus::kObserved;
};
} // namespace canvas::runtime
