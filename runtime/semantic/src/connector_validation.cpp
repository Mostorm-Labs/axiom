#include "canvas/semantic/connector_validation.hpp"

#include "canvas/semantic/object_record.hpp"

#include <type_traits>
#include <variant>

namespace canvas::semantic {
namespace {

[[nodiscard]] StatefulResult invalid(StatefulIssue issue) noexcept {
    return StatefulResult{issue};
}

[[nodiscard]] bool connectableShapeTuple(const ObjectRecord& target) noexcept {
    const auto* shape = std::get_if<ShapeContent>(&target.content);
    if (shape == nullptr) return false;
    if (target.kind_version == 1U)
        return shape->shape_kind != 3U && !shape->label.has_value();
    if (target.kind_version == 2U)
        return shape->shape_kind == 3U && !shape->label.has_value();
    return target.kind_version == 3U &&
           (shape->shape_kind == 1U || shape->shape_kind == 2U ||
            shape->shape_kind == 3U) && shape->label.has_value();
}

[[nodiscard]] StatefulResult validateShapeTuple(const ObjectRecord& target) noexcept {
    const auto* shape = std::get_if<ShapeContent>(&target.content);
    if (shape == nullptr) return invalid(StatefulIssue::kInvalidKindVersion);
    if (target.kind_version == 1U && shape->shape_kind == 3U)
        return invalid(StatefulIssue::kInvalidKindVersion);
    if (target.kind_version == 2U && shape->shape_kind != 3U)
        return invalid(StatefulIssue::kInvalidKindVersion);
    if (target.kind_version == 3U &&
        (shape->shape_kind < 1U || shape->shape_kind > 3U || !shape->label.has_value()))
        return invalid(StatefulIssue::kInvalidKindVersion);
    return StatefulResult{};
}

[[nodiscard]] StatefulResult validateAttachedEndpoint(
    const StagedObjectView& staged, const AttachedEndpoint& endpoint) {
    const ObjectRecord* target = staged.find(endpoint.target_object_id);
    if (target == nullptr) return invalid(StatefulIssue::kInvalidReference);

    if (!isKnownObjectKind(target->kind) ||
        target->kind_version == 0U ||
        (target->kind != ObjectKind::kShape && target->kind_version != 1U) ||
        (target->kind == ObjectKind::kShape && target->kind_version > 3U)) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }

    if (target->kind != ObjectKind::kShape &&
        !isConnectableObjectKind(target->kind, target->kind_version)) {
        return invalid(StatefulIssue::kConnectorInvalid);
    }
    if (target->kind == ObjectKind::kShape &&
        (target->kind_version >= 2U ||
         (std::get_if<ShapeContent>(&target->content) != nullptr &&
          std::get<ShapeContent>(target->content).shape_kind == 3U))) {
        const auto tuple = validateShapeTuple(*target);
        if (!tuple.ok()) return tuple;
        if (!connectableShapeTuple(*target)) return invalid(StatefulIssue::kConnectorInvalid);
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
    if (kind == ObjectKind::kShape)
        return kind_version >= 1U && kind_version <= 3U;
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
        case ObjectKind::kSection:
            return false;
    }
    return false;
}

StatefulResult validateStablePortForTarget(
    const ObjectRecord& target, const StablePortAnchor& anchor) {
    if (!isKnownObjectKind(target.kind) ||
        target.kind_version == 0U ||
        (target.kind != ObjectKind::kShape && target.kind_version != 1U) ||
        (target.kind == ObjectKind::kShape && target.kind_version > 3U)) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }
    if (target.kind != ObjectKind::kShape &&
        !isConnectableObjectKind(target.kind, target.kind_version)) {
        return invalid(StatefulIssue::kConnectorInvalid);
    }
    if (target.kind == ObjectKind::kShape &&
        (target.kind_version >= 2U ||
         (std::get_if<ShapeContent>(&target.content) != nullptr &&
          std::get<ShapeContent>(target.content).shape_kind == 3U))) {
        const auto tuple = validateShapeTuple(target);
        if (!tuple.ok()) return tuple;
        if (!connectableShapeTuple(target)) return invalid(StatefulIssue::kConnectorInvalid);
    }

    if (target.kind == ObjectKind::kShape && target.kind_version >= 2U &&
        (anchor.port_id < 1U || anchor.port_id > 4U))
        return invalid(StatefulIssue::kConnectorInvalid);
    return StatefulResult{};
}

StatefulResult validateConnectorReferences(
    const StagedObjectView& staged, const ConnectorContent& content) {
    const StatefulResult start = validateEndpoint(staged, content.start);
    if (!start.ok()) return start;
    return validateEndpoint(staged, content.end);
}

} // namespace canvas::semantic
