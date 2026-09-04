#include "canvas/semantic/normalizer.hpp"

#include "object_record_semantics_internal.hpp"

#include "canvas/semantic/canonical_numeric.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace canvas::semantic {
namespace {

bool normalizeDouble(double& value) noexcept {
    return normalizeFinite(value, value);
}

bool normalizeFloat(float& value) noexcept {
    if (!std::isfinite(value)) return false;
    if (value == 0.0F) value = 0.0F;
    return true;
}

bool normalizeVec(Vec2& value) noexcept {
    return normalizeDouble(value.x) && normalizeDouble(value.y);
}

bool normalizeSourceRect(std::optional<NormalizedRect>& source_rect) noexcept {
    if (!source_rect.has_value()) return true;
    auto& value = *source_rect;
    if (!normalizeDouble(value.x) || !normalizeDouble(value.y) ||
        !normalizeDouble(value.width) || !normalizeDouble(value.height)) {
        return false;
    }
    if (value.x == 0.0 && value.y == 0.0 && value.width == 1.0 && value.height == 1.0) {
        source_rect.reset();
    }
    return true;
}

bool normalizeColor(ColorValue& value) noexcept {
    return normalizeFloat(value.r) && normalizeFloat(value.g) &&
           normalizeFloat(value.b) && normalizeFloat(value.a);
}

bool normalizePropertyValue(PropertyValue& value) {
    return std::visit(
        [](auto& item) -> bool {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, bool> ||
                          std::is_same_v<Item, BlendModeValue> ||
                          std::is_same_v<Item, ConnectorDecorationValue> ||
                          std::is_same_v<Item, NoFill> ||
                          std::is_same_v<Item, NoStroke> ||
                          std::is_same_v<Item, RoundJoin> ||
                          std::is_same_v<Item, BevelJoin> ||
                          std::is_same_v<Item, SolidDash>) {
                return true;
            } else if constexpr (std::is_same_v<Item, float>) {
                return normalizeFloat(item);
            } else if constexpr (std::is_same_v<Item, ColorValue>) {
                return normalizeColor(item);
            } else if constexpr (std::is_same_v<Item, FillStyleValue>) {
                return std::visit(
                    [](auto& fill) -> bool {
                        using Fill = std::decay_t<decltype(fill)>;
                        if constexpr (std::is_same_v<Fill, NoFill>) return true;
                        else return normalizeColor(fill.color);
                    },
                    item);
            } else if constexpr (std::is_same_v<Item, StrokeStyleValue>) {
                return std::visit(
                    [](auto& stroke) -> bool {
                        using Stroke = std::decay_t<decltype(stroke)>;
                        if constexpr (std::is_same_v<Stroke, NoStroke>) return true;
                        else {
                            if (!normalizeColor(stroke.color) || !normalizeDouble(stroke.width)) return false;
                            if (auto* miter = std::get_if<MiterJoin>(&stroke.join)) {
                                if (!normalizeDouble(miter->limit)) return false;
                            }
                            if (auto* pattern = std::get_if<DashPattern>(&stroke.dash)) {
                                if (!normalizeDouble(pattern->offset)) return false;
                                for (auto& segment : pattern->segments) {
                                    if (!normalizeDouble(segment)) return false;
                                }
                            }
                            return true;
                        }
                    },
                    item);
            }
            return false;
        },
        value);
}

bool isExplicitDefaultProperty(const PropertyEntry& entry) noexcept {
    switch (entry.field_id) {
        case 0x00000001U:
            return std::holds_alternative<bool>(entry.value) && std::get<bool>(entry.value);
        case 0x00000002U:
            return std::holds_alternative<bool>(entry.value) && !std::get<bool>(entry.value);
        case 0x00000003U:
            return std::holds_alternative<float>(entry.value) && std::get<float>(entry.value) == 1.0F;
        case 0x00000004U:
            return std::holds_alternative<BlendModeValue>(entry.value) &&
                   std::get<BlendModeValue>(entry.value) == BlendModeValue::kNormal;
        case 0x00000100U:
            return std::holds_alternative<FillStyleValue>(entry.value) &&
                   std::holds_alternative<NoFill>(std::get<FillStyleValue>(entry.value));
        case 0x00000101U:
            return std::holds_alternative<StrokeStyleValue>(entry.value) &&
                   std::holds_alternative<NoStroke>(std::get<StrokeStyleValue>(entry.value));
        case 0x00000200U:
        case 0x00000201U:
            return std::holds_alternative<ConnectorDecorationValue>(entry.value) &&
                   std::get<ConnectorDecorationValue>(entry.value) == ConnectorDecorationValue::kNone;
        default:
            return false;
    }
}

template <typename Vector, typename Compare>
bool sortUnique(Vector& values, Compare compare) {
    if (values.empty()) return false;
    std::sort(values.begin(), values.end(), compare);
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (!compare(values[index - 1U], values[index]) &&
            !compare(values[index], values[index - 1U])) {
            return false;
        }
    }
    return true;
}

bool objectIdLess(const ObjectId& left, const ObjectId& right) noexcept {
    return left < right;
}

bool validObjectId(const ObjectId& value) noexcept { return !value.isZero(); }

template <typename Range, typename Extractor>
bool allIdsValid(const Range& values, Extractor extractor) {
    return std::all_of(values.begin(), values.end(), [&](const auto& value) {
        return validObjectId(extractor(value));
    });
}

bool normalizePath(VectorPathGeometry& path);

bool normalizeEraseGeometry(EraseMaskGeometry& geometry) {
    return std::visit(
        [](auto& value) -> bool {
            using Item = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Item, SweptCircleMask>) {
                for (auto& segment : value.segments) {
                    if (!normalizeVec(segment.p0.position) || !normalizeDouble(segment.p0.radius) ||
                        !normalizeVec(segment.p1.position) || !normalizeDouble(segment.p1.radius) ||
                        !normalizeVec(segment.control1) || !normalizeVec(segment.control2)) {
                        return false;
                    }
                }
                return true;
            } else {
                return normalizePath(value.path);
            }
        },
        geometry);
}

bool normalizeEraseMasks(std::vector<EraseMaskRecord>& masks) {
    if (!allIdsValid(masks, [](const auto& mask) { return mask.id; })) return false;
    if (!masks.empty()) {
        if (!sortUnique(masks, [](const auto& left, const auto& right) { return objectIdLess(left.id, right.id); })) {
            return false;
        }
    }
    for (auto& mask : masks) {
        if (!normalizeEraseGeometry(mask.geometry)) return false;
    }
    return true;
}

bool normalizePathCommand(PathCommand& command) {
    return std::visit(
        [](auto& value) -> bool {
            using Item = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Item, ClosePath>) {
                return true;
            } else if constexpr (std::is_same_v<Item, MoveTo>) {
                return normalizeVec(value.point);
            } else if constexpr (std::is_same_v<Item, LineTo>) {
                return normalizeVec(value.end);
            } else if constexpr (std::is_same_v<Item, QuadTo>) {
                return normalizeVec(value.control) && normalizeVec(value.end);
            } else {
                return normalizeVec(value.control1) && normalizeVec(value.control2) && normalizeVec(value.end);
            }
        },
        command);
}

bool normalizePath(VectorPathGeometry& path) {
    for (auto& command : path.commands) {
        if (!normalizePathCommand(command)) return false;
    }
    return true;
}

bool normalizeTextStyle(TextStyle& style) {
    if (!normalizeDouble(style.font_size)) return false;
    if (style.font_resource_id.has_value() && !validObjectId(style.font_resource_id->value)) return false;
    return normalizeColor(style.color);
}

bool normalizeParagraphStyle(ParagraphStyle& style) {
    if (!normalizeDouble(style.line_height) || !normalizeDouble(style.spacing_before) ||
        !normalizeDouble(style.spacing_after)) return false;
    return true;
}

bool normalizeRichTextDocument(RichTextDocument& document) {
    for (auto& paragraph : document.paragraphs) {
        if (!normalizeParagraphStyle(paragraph.style)) return false;
        for (auto& run : paragraph.runs) {
            if (!normalizeTextStyle(run.style)) return false;
        }
    }
    return true;
}

bool normalizeRichTextDelta(RichTextDelta& delta) {
    for (auto& step : delta.steps) {
        const bool valid = std::visit(
            [](auto& value) -> bool {
                using Item = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Item, InsertTextStep>) {
                    return normalizeTextStyle(value.style);
                } else if constexpr (std::is_same_v<Item, DeleteTextStep> ||
                                     std::is_same_v<Item, SplitParagraphStep> ||
                                     std::is_same_v<Item, MergeParagraphStep>) {
                    return true;
                } else if constexpr (std::is_same_v<Item, SetInlineStyleStep>) {
                    return normalizeTextStyle(value.style);
                } else {
                    return normalizeParagraphStyle(value.style);
                }
            },
            step);
        if (!valid) return false;
    }
    return true;
}

bool normalizeStroke(StrokeRecord& stroke) {
    auto& brush = stroke.brush;
    if (!normalizeColor(brush.color) || !normalizeDouble(brush.nominal_size) ||
        !normalizeFloat(brush.opacity) || !normalizeFloat(brush.tilt.size_influence) ||
        !normalizeFloat(brush.tilt.angle_influence) || !normalizeFloat(brush.smoothing.amount) ||
        !normalizeFloat(brush.spacing.normalized_spacing)) return false;
    if (brush.pressure.size_curve.has_value()) {
        for (auto& point : brush.pressure.size_curve->points) {
            if (!normalizeFloat(point.x) || !normalizeFloat(point.y)) return false;
        }
    }
    if (brush.pressure.opacity_curve.has_value()) {
        for (auto& point : brush.pressure.opacity_curve->points) {
            if (!normalizeFloat(point.x) || !normalizeFloat(point.y)) return false;
        }
    }
    return std::visit(
        [](auto& data) -> bool {
            using Item = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<Item, VectorStrokeData>) {
                for (auto& sample : data.samples) {
                    if (!normalizeVec(sample.position) || !normalizeFloat(sample.pressure) ||
                        !normalizeVec(sample.tilt)) return false;
                }
            } else {
                for (auto& dab : data.dabs) {
                    if (!normalizeVec(dab.center) || !normalizeDouble(dab.size) ||
                        !normalizeFloat(dab.rotation) || !normalizeFloat(dab.opacity)) return false;
                }
            }
            return true;
        },
        stroke.data);
}

bool normalizeConnector(ConnectorContent& content) {
    const auto normalizeEndpoint = [](ConnectorEndpoint& endpoint) {
        return std::visit(
            [](auto& item) -> bool {
                using Endpoint = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<Endpoint, FreePointEndpoint>) {
                    return normalizeVec(item.point);
                } else {
                    return std::visit(
                        [](auto& anchor) -> bool {
                            using Anchor = std::decay_t<decltype(anchor)>;
                            if constexpr (std::is_same_v<Anchor, AutoPerimeterAnchor>) {
                                return !anchor.hint.has_value() || normalizeVec(*anchor.hint);
                            } else {
                                return true;
                            }
                        },
                        item.anchor);
                }
            },
            endpoint.value);
    };
    return normalizeEndpoint(content.start) && normalizeEndpoint(content.end);
}

bool normalizeContent(ObjectContent& content) {
    return std::visit(
        [](auto& value) -> bool {
            using Item = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Item, ShapeContent>) {
                return normalizeDouble(value.width) && normalizeDouble(value.height);
            } else if constexpr (std::is_same_v<Item, ImageContent>) {
                if (!validObjectId(value.resource_id.value)) return false;
                if (!normalizeDouble(value.intrinsic_width) || !normalizeDouble(value.intrinsic_height) ||
                    !normalizeDouble(value.width) || !normalizeDouble(value.height)) return false;
                return normalizeSourceRect(value.source_rect);
            } else if constexpr (std::is_same_v<Item, VectorPathContent>) {
                return normalizePath(value.geometry);
            } else if constexpr (std::is_same_v<Item, RichTextContent>) {
                return normalizeRichTextDocument(value.document);
            } else if constexpr (std::is_same_v<Item, VectorStrokeContent>) {
                return normalizeStroke(value.stroke);
            } else if constexpr (std::is_same_v<Item, DabStrokeContent>) {
                return normalizeStroke(value.stroke);
            } else if constexpr (std::is_same_v<Item, ConnectorContent>) {
                return normalizeConnector(value);
            } else if constexpr (std::is_same_v<Item, StickyContent>) {
                return normalizeDouble(value.width) && normalizeDouble(value.height);
            } else {
                return true;
            }
        },
        content);
}

bool normalizeObject(ObjectRecord& object) {
    if (!validObjectId(object.id)) return false;
    if (!normalizeDouble(object.transform.a) || !normalizeDouble(object.transform.b) ||
        !normalizeDouble(object.transform.c) || !normalizeDouble(object.transform.d) ||
        !normalizeDouble(object.transform.tx) || !normalizeDouble(object.transform.ty)) return false;
    if (object.placement.parent_id.has_value() && object.placement.parent_id->isZero()) return false;
    if (!object.properties.entries.empty() &&
        !sortUnique(object.properties.entries, [](const auto& left, const auto& right) {
            return left.field_id < right.field_id;
        })) return false;
    for (auto& entry : object.properties.entries) {
        if (entry.field_id == 0U || !normalizePropertyValue(entry.value)) return false;
    }
    object.properties.entries.erase(
        std::remove_if(object.properties.entries.begin(), object.properties.entries.end(),
                       isExplicitDefaultProperty), object.properties.entries.end());
    if (!normalizeContent(object.content) || !normalizeEraseMasks(object.erase_masks)) return false;
    return true;
}

bool normalizeOperationPayload(OperationPayload& payload) {
    return std::visit(
        [](auto& value) -> bool {
            using Item = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Item, InsertObjectsOp> || std::is_same_v<Item, RestoreObjectsOp>) {
                if (!allIdsValid(value.objects, [](const auto& object) { return object.id; })) return false;
                if (!sortUnique(value.objects, [](const auto& left, const auto& right) { return objectIdLess(left.id, right.id); })) return false;
                for (auto& object : value.objects) if (!internal::normalizeObjectRecord(object)) return false;
                return true;
            } else if constexpr (std::is_same_v<Item, DeleteObjectsOp>) {
                if (!allIdsValid(value.object_ids, [](const auto& object_id) { return object_id; })) return false;
                return sortUnique(value.object_ids, objectIdLess);
            } else if constexpr (std::is_same_v<Item, SetPlacementsOp>) {
                if (!allIdsValid(value.items, [](const auto& item) { return item.object_id; })) return false;
                if (!sortUnique(value.items, [](const auto& left, const auto& right) { return objectIdLess(left.object_id, right.object_id); })) return false;
                return std::all_of(value.items.begin(), value.items.end(), [](auto& item) {
                    return item.placement.order_key.isValid() &&
                           (!item.placement.parent_id.has_value() || !item.placement.parent_id->isZero());
                });
            } else if constexpr (std::is_same_v<Item, SetTransformsOp>) {
                if (!allIdsValid(value.items, [](const auto& item) { return item.object_id; })) return false;
                if (!sortUnique(value.items, [](const auto& left, const auto& right) { return objectIdLess(left.object_id, right.object_id); })) return false;
                for (auto& item : value.items) {
                    if (!normalizeDouble(item.transform.a) || !normalizeDouble(item.transform.b) ||
                        !normalizeDouble(item.transform.c) || !normalizeDouble(item.transform.d) ||
                        !normalizeDouble(item.transform.tx) || !normalizeDouble(item.transform.ty)) return false;
                }
                return true;
            } else if constexpr (std::is_same_v<Item, PatchPropertiesOp>) {
                if (!allIdsValid(value.patches, [](const auto& patch) { return patch.object_id; })) return false;
                if (!sortUnique(value.patches, [](const auto& left, const auto& right) {
                        if (left.object_id != right.object_id) return objectIdLess(left.object_id, right.object_id);
                        return left.field_id < right.field_id;
                    })) return false;
                for (auto& patch : value.patches) {
                    if (patch.field_id == 0U) return false;
                    if (std::holds_alternative<PropertyValue>(patch.value) &&
                        !normalizePropertyValue(std::get<PropertyValue>(patch.value))) return false;
                }
                return true;
            } else if constexpr (std::is_same_v<Item, SetObjectSizeOp>) {
                if (!allIdsValid(value.items, [](const auto& item) { return item.object_id; })) return false;
                if (!sortUnique(value.items, [](const auto& left, const auto& right) { return objectIdLess(left.object_id, right.object_id); })) return false;
                for (auto& item : value.items) if (!normalizeDouble(item.width) || !normalizeDouble(item.height)) return false;
                return true;
            } else if constexpr (std::is_same_v<Item, SetVectorPathGeometryOp>) {
                if (!validObjectId(value.object_id)) return false;
                return normalizePath(value.geometry);
            } else if constexpr (std::is_same_v<Item, SetImageContentOp>) {
                if (!validObjectId(value.object_id) || !validObjectId(value.content.resource_id.value)) return false;
                if (!normalizeDouble(value.content.intrinsic_width) ||
                    !normalizeDouble(value.content.intrinsic_height) ||
                    !normalizeDouble(value.content.width) || !normalizeDouble(value.content.height)) return false;
                return normalizeSourceRect(value.content.source_rect);
            } else if constexpr (std::is_same_v<Item, AddStrokeOp>) {
                return internal::normalizeObjectRecord(value.object);
            } else if constexpr (std::is_same_v<Item, SplitStrokesOp>) {
                if (!allIdsValid(value.splits, [](const auto& split) { return split.source_stroke_id; })) return false;
                if (!sortUnique(value.splits, [](const auto& left, const auto& right) { return objectIdLess(left.source_stroke_id, right.source_stroke_id); })) return false;
                std::vector<ObjectId> replacement_ids;
                for (auto& split : value.splits) {
                    if (!sortUnique(split.replacements, [](const auto& left, const auto& right) { return objectIdLess(left.id, right.id); })) return false;
                    for (auto& replacement : split.replacements) {
                        if (!internal::normalizeObjectRecord(replacement)) return false;
                        replacement_ids.push_back(replacement.id);
                    }
                }
                return sortUnique(replacement_ids, objectIdLess);
            } else if constexpr (std::is_same_v<Item, AddEraseMasksOp>) {
                if (!allIdsValid(value.items, [](const auto& item) { return item.object_id; })) return false;
                if (!sortUnique(value.items, [](const auto& left, const auto& right) { return objectIdLess(left.object_id, right.object_id); })) return false;
                std::vector<ObjectId> mask_ids;
                for (auto& item : value.items) {
                    if (!normalizeEraseMasks(item.masks)) return false;
                    for (const auto& mask : item.masks) mask_ids.push_back(mask.id);
                }
                return sortUnique(mask_ids, objectIdLess);
            } else if constexpr (std::is_same_v<Item, RemoveEraseMasksOp>) {
                if (!allIdsValid(value.items, [](const auto& item) { return item.object_id; })) return false;
                if (!sortUnique(value.items, [](const auto& left, const auto& right) { return objectIdLess(left.object_id, right.object_id); })) return false;
                for (auto& item : value.items) if (!sortUnique(item.mask_ids, objectIdLess)) return false;
                return true;
            } else if constexpr (std::is_same_v<Item, EditRichTextOp>) {
                if (!validObjectId(value.object_id)) return false;
                return normalizeRichTextDelta(value.delta);
            } else {
                if (!validObjectId(value.object_id)) return false;
                return normalizeConnector(value.content);
            }
        },
        payload);
}

} // namespace

bool internal::normalizeObjectRecord(ObjectRecord& object) {
    return normalizeObject(object);
}

NormalizeResult normalizeOperation(const Operation& input) {
    NormalizeResult result;
    result.value = input;
    if (!normalizeOperationPayload(result.value.payload)) {
        result.error = SemanticError::kInvalidSemanticValue;
    }
    return result;
}

} // namespace canvas::semantic
