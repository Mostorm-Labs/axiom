#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/property_value.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::runtime {

enum class SelectionSummaryStatus : std::uint8_t { kUnavailable, kAvailable, kStale, kInvalid };
enum class SelectionReason : std::uint8_t {
    kNone,
    kNoSelection,
    kUnsupportedOwner,
    kInvalidHierarchy,
    kSceneUnavailable,
    kStaleScene,
    kBoundsUnavailable,
    kClosed,
    kWrongThread,
    kBusy,
    kLocked,
    kMultipleRoots,
    kUnsupportedKind,
    kNotImplemented,
    kOutsideBatch,
    kMissingProperty,
    kNotApplicable,
    kNonNormalizedSelection,
    kCommandRejected,
    kResourceUnavailable,
    kStaleSnapshot
};
enum class SelectionAction : std::uint8_t {
    kFit,
    kDelete,
    kLock,
    kUnlock,
    kOrder,
    kTransform,
    kSize,
    kAppearance,
    kFitPrimary,
    kGroup,
    kUngroup,
    kWrapInSection,
    kBulkProperties,
    kSnap,
    kAlignment,
    kCount
};
enum class SelectionCapabilityState : std::uint8_t {
    kUnavailable,
    kAvailable,
    kNotImplemented,
    kStaleSelection,
    kInvalidSelection
};
struct SelectionCapability final {
    SelectionCapabilityState state = SelectionCapabilityState::kUnavailable;
    SelectionReason reason = SelectionReason::kUnsupportedOwner;
    [[nodiscard]] bool available() const noexcept {
        return state == SelectionCapabilityState::kAvailable;
    }
    bool operator==(const SelectionCapability&) const = default;
};
enum class SelectionPropertyState : std::uint8_t { kUnavailable, kUniform, kMixed, kPartial };
struct SelectionPropertyAggregate final {
    std::uint32_t fieldId = 0;
    SelectionPropertyState state = SelectionPropertyState::kUnavailable;
    SelectionReason reason = SelectionReason::kNoSelection;
    std::size_t applicableRoots = 0, knownRoots = 0;
    bool applicableValuesMixed = false;
    // Only a complete uniform aggregate exposes a value. Partial never borrows
    // the primary's value for roots to which this property does not apply.
    std::optional<semantic::PropertyValue> value{};
    bool operator==(const SelectionPropertyAggregate&) const = default;
};
struct SelectionKindCount final {
    std::uint32_t kind = 0;
    std::size_t count = 0;
    bool operator==(const SelectionKindCount&) const = default;
};
struct SelectionBoundsSummary final {
    SelectionSummaryStatus status = SelectionSummaryStatus::kUnavailable;
    SelectionReason reason = SelectionReason::kSceneUnavailable;
    std::optional<foundation::WorldRect> world{};
    bool operator==(const SelectionBoundsSummary&) const = default;
};
// A read-only projection of SelectionSession, never another selection store.
// Scene fields describe one published generation; no preview geometry is mixed in.
struct SelectionSummary final {
    std::size_t count = 0;
    foundation::ObjectId primary{};
    SelectionSummaryStatus status = SelectionSummaryStatus::kUnavailable;
    SelectionReason reason = SelectionReason::kUnsupportedOwner;
    std::vector<foundation::ObjectId> roots{};
    std::vector<SelectionKindCount> kindCounts{};
    std::optional<foundation::ObjectId> groupScope{};
    std::uint64_t selectionRevision = 0, semanticGeneration = 0, sceneGeneration = 0,
                  sceneRevision = 0;
    SelectionBoundsSummary bounds{};
    std::array<SelectionCapability, static_cast<std::size_t>(SelectionAction::kCount)> actions{};
    std::array<SelectionPropertyAggregate, 3> properties{{{1U}, {2U}, {3U}}};
    [[nodiscard]] SelectionCapability capability(SelectionAction action) const noexcept {
        const auto i = static_cast<std::size_t>(action);
        return i < actions.size() ? actions[i] : SelectionCapability{};
    }
    bool operator==(const SelectionSummary&) const = default;
};

[[nodiscard]] inline const char* selectionReasonName(SelectionReason reason) noexcept {
    switch (reason) {
    case SelectionReason::kNone:
        return "Available";
    case SelectionReason::kNoSelection:
        return "No selection";
    case SelectionReason::kUnsupportedOwner:
        return "Owner unavailable";
    case SelectionReason::kInvalidHierarchy:
        return "Invalid or missing hierarchy";
    case SelectionReason::kSceneUnavailable:
        return "Scene unavailable";
    case SelectionReason::kStaleScene:
        return "Scene generation is stale";
    case SelectionReason::kBoundsUnavailable:
        return "Bounds unavailable";
    case SelectionReason::kClosed:
        return "Runtime closed";
    case SelectionReason::kWrongThread:
        return "Runtime owner thread required";
    case SelectionReason::kBusy:
        return "Editing session active";
    case SelectionReason::kLocked:
        return "Selection is locked";
    case SelectionReason::kMultipleRoots:
        return "Multiple roots unsupported by this action";
    case SelectionReason::kUnsupportedKind:
        return "Object kind unsupported by this action";
    case SelectionReason::kNotImplemented:
        return "Not implemented";
    case SelectionReason::kOutsideBatch:
        return "Outside this selection action surface";
    case SelectionReason::kMissingProperty:
        return "Property has no authoritative value";
    case SelectionReason::kNotApplicable:
        return "No applicable roots";
    case SelectionReason::kNonNormalizedSelection:
        return "Selection changed; acquire roots again";
    case SelectionReason::kCommandRejected:
        return "Command preconditions rejected";
    case SelectionReason::kResourceUnavailable:
        return "Resources unavailable";
    case SelectionReason::kStaleSnapshot:
        return "Snapshot changed during capture";
    }
    return "Unknown";
}
} // namespace canvas::runtime
