#pragma once
#include "canvas/runtime/canvas_control_types.hpp"

#include <optional>
#include <string>
#include <vector>

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
enum class TextInputKind : std::uint8_t {
    kBegin,
    kUpdate,
    kCommit,
    kCancel,
    kReplace,
    kSelection,
    kFinishFailed
};
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
    kResync,
    kBackspace,
    kDelete,
    kLeft,
    kRight,
    kSelectAll,
    kEditAtPoint,
    kCompleteComposition,
    kFormat
};
struct TextFormatPatch final {
    std::optional<std::array<float,4>> color;
    std::optional<double> fontSize;
    std::optional<foundation::ObjectId> font;
    std::optional<bool> bold, italic, underline, strikethrough, bulleted;
    std::optional<std::uint32_t> alignment;
    bool operator==(const TextFormatPatch&) const = default;
};
struct TextFontOption final { foundation::ObjectId id{}; std::string name; };
enum class FontResourceStatus : std::uint8_t {
    kMissing, kBytesValidated, kDecodeReady, kLayoutReady, kFailed,
    kEvicted, kRetryable, kProviderReplaced,
};
struct FontCatalogEntry final {
    foundation::ObjectId id{};
    std::string family;
    std::uint64_t revision = 0;
    std::string contentHash;
    std::size_t byteLength = 0;
    FontResourceStatus status = FontResourceStatus::kMissing;
    std::string diagnostic;
    bool operator==(const FontCatalogEntry&) const = default;
};
struct FontCatalogSnapshot final {
    std::uint64_t manifestRevision = 0;
    std::uint64_t bindingGeneration = 0;
    std::vector<FontCatalogEntry> entries;
    bool operator==(const FontCatalogSnapshot&) const = default;
};
struct TextIntent final {
    CanvasTargetKey target{};
    std::uint64_t clientId = 0, requestId = 0;
    TextIntentKind kind = TextIntentKind::kCreate;
    TextSessionToken token{};
    foundation::ObjectId object{};
    float viewX = 0, viewY = 0;
    double width = 320;
    TextFormatPatch format{};
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
    TextFormatPatch format{};
    // Explicit whole-label target when a Shape is selected, or its editor has
    // an insertion caret. A non-empty character selection retains range scope.
    foundation::ObjectId formatObject{};
    std::vector<TextFontOption> fonts;
    bool mixedFormat = false;
};
struct TextGeometryToken final {
    TextSessionToken session{};
    CanvasTargetKey target{};
    std::uint64_t camera = 0;
    std::string layout;
    bool operator==(const TextGeometryToken&) const = default;
};
struct TextGeometrySnapshot final {
    std::vector<std::string> diagnostics;
    bool available = false;
    TextGeometryToken token{};
    std::array<float, 8> caret{};
    std::vector<std::array<float, 8>> selection;
};
} // namespace canvas::runtime
