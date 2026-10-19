#pragma once

// Compilable P14-P17 proposal only. Not exported by RuntimeFacade or installed.
#include "canvas/foundation/object_id.hpp"
#include "canvas/runtime/canvas_control_types.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace axiom::fc02::draft {
using Id = canvas::foundation::ObjectId;

struct SessionKey {
    std::uint64_t runtimeInstance = 0;
    Id document{};
    std::uint64_t documentAttachment = 0;
    std::uint64_t view = 0;
    std::uint64_t adapterBinding = 0;
    std::uint64_t session = 0;
    bool operator==(const SessionKey&) const = default;
};
struct ProjectionToken {
    SessionKey session{};
    Id object{}; // Full reserved ObjectId for a draft, never a truncated integer.
    std::uint64_t revision = 0;
    bool operator==(const ProjectionToken&) const = default;
};
struct Utf16Range {
    std::uint32_t start = 0, length = 0;
    bool operator==(const Utf16Range&) const = default;
};
struct GeometryToken {
    canvas::runtime::CanvasTargetKey surface{};
    std::uint64_t projection = 0, layout = 0, fonts = 0, viewport = 0;
    bool operator==(const GeometryToken&) const = default;
};
enum class InputKind { kBegin, kUpdate, kCommit, kCancel, kReplace, kSelection };
struct InputEvent {
    ProjectionToken target{};
    std::uint64_t sequence = 0, composition = 0, finishEffect = 0;
    InputKind kind = InputKind::kSelection;
    Utf16Range range{}; // In the confirmed projection; never canonical scalar offsets.
    std::u16string text;
    std::optional<Utf16Range> preeditSelection; // UTF-16 range in text, not in the document.
    bool operator==(const InputEvent&) const = default;
};
enum class InputStatus {
    kObserved, kPending, kApplied, kRejected, kStale, kOutOfOrder,
    kCollision, kInvalidOffset, kAlreadyTerminal, kBusy, kBackpressure, kExpired
};
struct InputReceipt {
    InputStatus status = InputStatus::kRejected;
    std::uint64_t sequence = 0;
    bool replay = false;
};
enum class Phase { kEditing, kComposing, kFinishing, kApplying, kCommitFailed, kClosed };
enum class ExitReason { kEnter, kToolSwitch, kPageExit, kDebugFocus };
struct HostEffect {
    SessionKey session{};
    std::uint64_t id = 0;
    bool operator==(const HostEffect&) const = default;
};
struct CompositionProjection {
    std::uint64_t id = 0;
    Utf16Range replacement{};
    std::u16string text;
    std::optional<Utf16Range> selection;
};
struct EditorSnapshot {
    ProjectionToken target{};
    Phase phase = Phase::kClosed;
    std::u16string projection; // Disposable bounded platform projection, not a TextDocument.
    std::optional<HostEffect> finishComposition;
    std::optional<CompositionProjection> preedit;
};

// Represents the proposed typed facade text lane. Production will add methods
// to the existing facade, not expose a second host-constructible Runtime owner.
class TextIngress {
public:
    virtual ~TextIngress() = default;
    [[nodiscard]] virtual InputReceipt submit(const InputEvent&) = 0;
    [[nodiscard]] virtual EditorSnapshot read() const = 0;
};
} // namespace axiom::fc02::draft
