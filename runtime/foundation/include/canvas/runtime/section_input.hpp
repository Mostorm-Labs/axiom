#pragma once
#include "canvas/semantic/section_content.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/runtime/canvas_target.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace canvas::runtime {
enum class SectionAction : std::uint8_t {
    kCreate, kWrap, kRelease, kReparent, kResize, kRename, kSelect, kSelectChildren,
    kEnterGroupScope,kLeaveGroupScope, kGroup, kUngroup
};
// Programmatic geometry is Section-local (boundary) and parent-local
// (transform). Screen coordinates never enter these semantic intents.
struct SectionIntent final {
    SectionAction action=SectionAction::kCreate;
    foundation::ObjectId objectId{};
    std::vector<foundation::ObjectId> objects;
    std::optional<foundation::ObjectId> parentId;
    semantic::SectionContent content{{0,0,320,240},"Section",16,32};
    semantic::Transform2D transform{};
    std::uint64_t expectedSemanticGeneration=0;
    std::optional<std::uint64_t> selectionRevision;
    bool operator==(const SectionIntent&) const = default;
};
enum class SectionFailure : std::uint8_t {
    kNone,kUnsupported,kBusy,kStale,kInvalidTarget,kInvalidHierarchy,
    kNumericRange,kProjectionUnavailable,kCommandRejected,kUnsupportedMixedRemoval,kCollision
};
enum class SectionCanonicalOutcome : std::uint8_t { kNotCommitted,kNoChange,kCommitted };
enum class SectionProjectionOutcome : std::uint8_t { kNotRequired,kPending,kReady };
struct SectionOutcome final {
    SectionFailure failure=SectionFailure::kUnsupported;
    SectionCanonicalOutcome canonical=SectionCanonicalOutcome::kNotCommitted;
    SectionProjectionOutcome projection=SectionProjectionOutcome::kNotRequired;
    foundation::ObjectId objectId{}, operationId{};
    std::uint64_t semanticGeneration=0,sceneRevision=0,commitEpoch=0,commitOrdinal=0;
    bool operator==(const SectionOutcome&) const = default;
};
struct SectionSnapshot final {
    CanvasTargetKey target{};
    bool titleLayoutReady=false;
    std::uint64_t titleGlyphCount=0;
    std::vector<std::string> titleDiagnostics;
    bool available=false,projectionReady=false,busy=false;
    foundation::ObjectId objectId{};
    std::optional<foundation::ObjectId> parentId;
    std::optional<foundation::ObjectId> groupScope;
    semantic::SectionContent content{};
    semantic::Transform2D localTransform{},worldTransform{};
    std::array<foundation::WorldPoint,4> worldQuad{};
    std::vector<foundation::ObjectId> directChildren,selectedRoots;
    struct Member final {
        foundation::ObjectId objectId{};
        semantic::ObjectKind kind=semantic::ObjectKind::kShape;
        semantic::Transform2D localTransform{},worldTransform{};
    };
    std::vector<Member> members;
    std::uint64_t semanticGeneration=0,sceneRevision=0;
    std::uint64_t selectionRevision=0;
    SectionFailure unavailableReason=SectionFailure::kUnsupported;
    struct ActionAvailability final {
        bool builtSupported=true,sessionAdmitted=false,writable=false,contextEligible=false,temporarilyReady=false;
        SectionFailure reason=SectionFailure::kUnsupported;
    };
    std::array<ActionAvailability,10> actions{};
};
}
