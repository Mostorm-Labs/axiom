#include "canvas/semantic/connector_validation.hpp"

#include "canvas/semantic/object_record.hpp"

#include <type_traits>
#include <variant>

namespace canvas::semantic {
namespace {

[[nodiscard]] StatefulResult invalid(StatefulIssue issue) noexcept {
    return StatefulResult{issue};
}

[[nodiscard]] StatefulResult validateAttachedEndpoint(
    const StagedObjectView& staged, const AttachedEndpoint& endpoint) {
    const ObjectRecord* target = staged.find(endpoint.target_object_id);
    if (target == nullptr) return invalid(StatefulIssue::kInvalidReference);

    if (!isKnownObjectKind(target->kind) || target->kind_version != 1U) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }

    if (!isConnectableObjectKind(target->kind, target->kind_version)) {
        return invalid(StatefulIssue::kConnectorInvalid);
    }

    return std::visit(
        [&](const auto& anchor) -> StatefulResult {
            using Anchor = std::decay_t<decltype(anchor)>;
            if constexpr (std::is_same_v<Anchor, AutoPerimeterAnchor>) {
                // A owns AutoPerimeter structural validation. Once that
                // precondition holds, every released connectable V1 target
                // accepts the anchor without renderer or geometry lookup.
                return StatefulResult{};
            } else {
                return validateStablePortForTarget(*target, anchor);
            }
        },
        endpoint.anchor);
}

[[nodiscard]] StatefulResult validateEndpoint(
    const StagedObjectView& staged, const ConnectorEndpoint& endpoint) {
    return std::visit(
        [&](const auto& value) -> StatefulResult {
            using Endpoint = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Endpoint, FreePointEndpoint>) {
                // A owns point finiteness/shape validation.
                return StatefulResult{};
            } else {
                return validateAttachedEndpoint(staged, value);
            }
        },
        endpoint.value);
}

} // namespace

bool isConnectableObjectKind(ObjectKind kind, std::uint32_t kind_version) noexcept {
    if (kind_version != 1U) return false;
    switch (kind) {
        case ObjectKind::kShape:
        case ObjectKind::kImage:
        case ObjectKind::kSticky:
            return true;
        case ObjectKind::kVectorPath:
        case ObjectKind::kRichText:
        case ObjectKind::kVectorStroke:
        case ObjectKind::kDabStroke:
        case ObjectKind::kConnector:
        case ObjectKind::kGroup:
            return false;
    }
    return false;
}

StatefulResult validateStablePortForTarget(
    const ObjectRecord& target, const StablePortAnchor& anchor) {
    if (!isKnownObjectKind(target.kind) || target.kind_version != 1U) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }
    if (!isConnectableObjectKind(target.kind, target.kind_version)) {
        return invalid(StatefulIssue::kConnectorInvalid);
    }

    // The caller's A structural-validation precondition normally guarantees
    // 1..4. Keep a defensive result for direct misuse without introducing a
    // new issue or any port normalization/fallback behavior.
    if (anchor.port_id < 1U || anchor.port_id > 4U) {
        return invalid(StatefulIssue::kConnectorInvalid);
    }
    return StatefulResult{};
}

StatefulResult validateConnectorReferences(
    const StagedObjectView& staged, const ConnectorContent& content) {
    const StatefulResult start = validateEndpoint(staged, content.start);
    if (!start.ok()) return start;
    return validateEndpoint(staged, content.end);
}

} // namespace canvas::semantic
