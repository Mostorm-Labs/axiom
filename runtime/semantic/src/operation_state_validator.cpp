#include "canvas/semantic/operation_state_validator.hpp"

#include "canvas/semantic/operation.hpp"

namespace canvas::semantic {
namespace {

[[nodiscard]] bool supportsRule(StateRule rule, ObjectKind kind) noexcept {
    switch (rule) {
        case StateRule::kCreateAbsent:
        case StateRule::kEditExisting:
        case StateRule::kPlacementTarget:
        case StateRule::kTransformTarget:
            return true;
        case StateRule::kSizeTarget:
            return kind == ObjectKind::kShape || kind == ObjectKind::kImage || kind == ObjectKind::kSticky;
        case StateRule::kVectorPathTarget:
            return kind == ObjectKind::kVectorPath;
        case StateRule::kImageTarget:
            return kind == ObjectKind::kImage;
        case StateRule::kStrokeTarget:
            return kind == ObjectKind::kVectorStroke || kind == ObjectKind::kDabStroke;
        case StateRule::kRichTextTarget:
            return kind == ObjectKind::kRichText;
        case StateRule::kConnectorTarget:
            return kind == ObjectKind::kConnector;
    }
    return false;
}

[[nodiscard]] bool supportsField(ObjectKind kind, std::uint32_t field_id) noexcept {
    switch (field_id) {
        case 0x00000001U:
        case 0x00000002U:
            return true;
        case 0x00000003U:
        case 0x00000004U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kImage ||
                   kind == ObjectKind::kVectorPath || kind == ObjectKind::kRichText ||
                   kind == ObjectKind::kConnector || kind == ObjectKind::kSticky;
        case 0x00000100U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kVectorPath ||
                   kind == ObjectKind::kSticky;
        case 0x00000101U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kVectorPath ||
                   kind == ObjectKind::kConnector || kind == ObjectKind::kSticky;
        case 0x00000200U:
        case 0x00000201U:
            return kind == ObjectKind::kConnector;
        default:
            return false;
    }
}

[[nodiscard]] bool valueMatchesField(std::uint32_t field_id, const PropertyValue& value) noexcept {
    switch (field_id) {
        case 0x00000001U:
        case 0x00000002U:
            return std::holds_alternative<bool>(value);
        case 0x00000003U:
            return std::holds_alternative<float>(value);
        case 0x00000004U:
            return std::holds_alternative<BlendModeValue>(value);
        case 0x00000100U:
            return std::holds_alternative<FillStyleValue>(value);
        case 0x00000101U:
            return std::holds_alternative<StrokeStyleValue>(value);
        case 0x00000200U:
        case 0x00000201U:
            return std::holds_alternative<ConnectorDecorationValue>(value);
        default:
            return false;
    }
}

[[nodiscard]] StatefulResult invalid(StatefulIssue issue) noexcept {
    return StatefulResult{issue};
}

} // namespace

StatefulResult requireExisting(const StagedObjectView& staged, ObjectId id) {
    return staged.find(id) == nullptr ? invalid(StatefulIssue::kObjectMissing) : StatefulResult{};
}

StatefulResult requireAbsent(const StagedObjectView& staged, ObjectId id) {
    return staged.find(id) == nullptr ? StatefulResult{} : invalid(StatefulIssue::kObjectAlreadyExists);
}

StatefulResult requireKindVersion(
    const ObjectRecord& record, ObjectKind kind, std::uint32_t kind_version) {
    if (!isKnownObjectKind(record.kind) || !isKnownObjectKind(kind)) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }
    if (record.kind != kind || record.kind_version != kind_version || kind_version != 1U ||
        record.kind_version != 1U) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }
    return StatefulResult{};
}

StatefulResult requirePropertyApplicability(
    const ObjectRecord& record, std::uint32_t field_id, const PropertyValue* value) {
    if (!isKnownObjectKind(record.kind) || record.kind_version != 1U) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }
    if (!supportsField(record.kind, field_id)) {
        return invalid(StatefulIssue::kInvalidApplicability);
    }
    return value == nullptr || valueMatchesField(field_id, *value)
               ? StatefulResult{}
               : invalid(StatefulIssue::kInvalidApplicability);
}

StatefulResult validateRecordStateForOperation(const ObjectRecord& record, StateRule rule) {
    if (!isKnownObjectKind(record.kind) || record.kind_version != 1U) {
        return invalid(StatefulIssue::kInvalidKindVersion);
    }
    return supportsRule(rule, record.kind) ? StatefulResult{}
                                           : invalid(StatefulIssue::kInvalidApplicability);
}

} // namespace canvas::semantic
