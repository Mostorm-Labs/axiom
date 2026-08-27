#include "canvas/semantic/validator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace canvas::semantic {
namespace {

bool validId(const ObjectId& value) noexcept { return !value.isZero(); }

bool finiteVec(const Vec2& value) noexcept;
bool validVectorPath(const VectorPathGeometry& path) noexcept;

constexpr std::size_t kMaxKeyedBatchItems = 65535U;
constexpr std::size_t kMaxEraseMasksPerObject = 65535U;
constexpr std::size_t kMaxGenericStringBytes = 1024U * 1024U;
constexpr std::size_t kMaxRichTextInsertBytes = 8U * 1024U * 1024U;
constexpr std::size_t kMaxGeometryElements = 2000000U;

struct GeometryCount final {
    std::size_t units = 0U;
    ValidationIssue issue = ValidationIssue::kNone;
};

GeometryCount addGeometryUnits(GeometryCount total, GeometryCount next) noexcept {
    if (total.issue != ValidationIssue::kNone) return total;
    if (next.issue != ValidationIssue::kNone) return next;
    if (next.units > std::numeric_limits<std::size_t>::max() - total.units) {
        return {0U, ValidationIssue::kIntegerOverflow};
    }
    const auto sum = total.units + next.units;
    if (sum > kMaxGeometryElements) return {sum, ValidationIssue::kGeometryLimitExceeded};
    return {sum, ValidationIssue::kNone};
}

GeometryCount multiplyGeometryUnits(std::size_t count, std::size_t weight) noexcept {
    if (weight != 0U && count > std::numeric_limits<std::size_t>::max() / weight) {
        return {0U, ValidationIssue::kIntegerOverflow};
    }
    const auto units = count * weight;
    return units > kMaxGeometryElements
        ? GeometryCount{units, ValidationIssue::kGeometryLimitExceeded}
        : GeometryCount{units, ValidationIssue::kNone};
}

bool validUtf8(std::string_view text) noexcept {
    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t width = 0;
        if (lead <= 0x7fU) width = 1;
        else if (lead >= 0xc2U && lead <= 0xdfU) width = 2;
        else if (lead >= 0xe0U && lead <= 0xefU) width = 3;
        else if (lead >= 0xf0U && lead <= 0xf4U) width = 4;
        else return false;
        if (index + width > text.size()) return false;
        for (std::size_t offset = 1; offset < width; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xc0U) != 0x80U) return false;
        }
        if (width == 3U) {
            const auto second = static_cast<unsigned char>(text[index + 1U]);
            if ((lead == 0xe0U && second < 0xa0U) || (lead == 0xedU && second >= 0xa0U)) return false;
        } else if (width == 4U) {
            const auto second = static_cast<unsigned char>(text[index + 1U]);
            if ((lead == 0xf0U && second < 0x90U) || (lead == 0xf4U && second > 0x8fU)) return false;
        }
        index += width;
    }
    return true;
}

bool validColor(const ColorValue& value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) &&
           std::isfinite(value.a) && value.r >= 0.0F && value.r <= 1.0F &&
           value.g >= 0.0F && value.g <= 1.0F && value.b >= 0.0F && value.b <= 1.0F &&
           value.a >= 0.0F && value.a <= 1.0F;
}

bool validNormalizedRect(const NormalizedRect& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.width) &&
           std::isfinite(value.height) && value.x >= 0.0 && value.y >= 0.0 &&
           value.width > 0.0 && value.height > 0.0 && value.x + value.width <= 1.0 &&
           value.y + value.height <= 1.0;
}

bool validShape(const ShapeContent& value) noexcept {
    return (value.shape_kind == 1U || value.shape_kind == 2U) &&
           std::isfinite(value.width) && value.width > 0.0 &&
           std::isfinite(value.height) && value.height > 0.0;
}

bool validImage(const ImageContent& value) noexcept {
    if (!validId(value.resource_id.value) || !std::isfinite(value.intrinsic_width) ||
        value.intrinsic_width <= 0.0 || !std::isfinite(value.intrinsic_height) ||
        value.intrinsic_height <= 0.0 || !std::isfinite(value.width) || value.width <= 0.0 ||
        !std::isfinite(value.height) || value.height <= 0.0) return false;
    if (value.source_rect.has_value() && !validNormalizedRect(*value.source_rect)) return false;
    return value.content_mode == ImageContentMode::kStretch ||
           value.content_mode == ImageContentMode::kFit || value.content_mode == ImageContentMode::kFill;
}

bool validTransform2D(const Transform2D& value) noexcept {
    return std::isfinite(value.a) && std::isfinite(value.b) && std::isfinite(value.c) &&
           std::isfinite(value.d) && std::isfinite(value.tx) && std::isfinite(value.ty) &&
           (value.a * value.d - value.b * value.c) != 0.0;
}

bool validStrokeStyle(const SolidStroke& stroke) noexcept {
    if (!validColor(stroke.color) || !std::isfinite(stroke.width) || stroke.width <= 0.0) return false;
    if (stroke.cap != StrokeCap::kButt && stroke.cap != StrokeCap::kRound && stroke.cap != StrokeCap::kSquare) return false;
    if (const auto* miter = std::get_if<MiterJoin>(&stroke.join)) {
        if (!std::isfinite(miter->limit) || miter->limit < 1.0) return false;
    } else if (!std::holds_alternative<RoundJoin>(stroke.join) && !std::holds_alternative<BevelJoin>(stroke.join)) {
        return false;
    }
    if (const auto* pattern = std::get_if<DashPattern>(&stroke.dash)) {
        if (pattern->segments.size() < 2U || (pattern->segments.size() % 2U) != 0U || !std::isfinite(pattern->offset)) return false;
        for (const auto segment : pattern->segments) {
            if (!std::isfinite(segment) || segment <= 0.0) return false;
        }
    } else if (!std::holds_alternative<SolidDash>(stroke.dash)) {
        return false;
    }
    return true;
}

bool validParagraphStyle(const ParagraphStyle& style) noexcept {
    return style.alignment != ParagraphAlignment::kInvalid &&
           (style.alignment == ParagraphAlignment::kLeft || style.alignment == ParagraphAlignment::kCenter ||
            style.alignment == ParagraphAlignment::kRight || style.alignment == ParagraphAlignment::kJustify) &&
           std::isfinite(style.line_height) && style.line_height > 0.0 &&
           std::isfinite(style.spacing_before) && style.spacing_before >= 0.0 &&
           std::isfinite(style.spacing_after) && style.spacing_after >= 0.0;
}

bool validTextStyle(const TextStyle& style) noexcept {
    if (!style.font_resource_id.has_value() || !validId(style.font_resource_id->value) ||
        !std::isfinite(style.font_size) || style.font_size <= 0.0 || !validColor(style.color)) return false;
    return style.weight == 100U || style.weight == 200U || style.weight == 300U ||
           style.weight == 400U || style.weight == 500U || style.weight == 600U ||
           style.weight == 700U || style.weight == 800U || style.weight == 900U;
}

bool validRichTextDocument(const RichTextDocument& document) noexcept {
    for (const auto& paragraph : document.paragraphs) {
        if (!validId(paragraph.id) || !validParagraphStyle(paragraph.style)) return false;
        for (const auto& run : paragraph.runs) {
            if (run.text.size() > kMaxGenericStringBytes || !validUtf8(run.text) || !validTextStyle(run.style)) return false;
        }
    }
    return true;
}

bool validConnector(const ConnectorContent& value) noexcept {
    const auto endpoint_valid = [](const ConnectorEndpoint& endpoint) {
        return std::visit([](const auto& item) -> bool {
            using Endpoint = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Endpoint, FreePointEndpoint>) {
                return finiteVec(item.point);
            } else {
                if (!validId(item.target_object_id)) return false;
                return std::visit([](const auto& anchor) -> bool {
                    using Anchor = std::decay_t<decltype(anchor)>;
                    if constexpr (std::is_same_v<Anchor, AutoPerimeterAnchor>) {
                        if (!anchor.hint.has_value()) return true;
                        return finiteVec(*anchor.hint) && anchor.hint->x >= 0.0 && anchor.hint->x <= 1.0 &&
                               anchor.hint->y >= 0.0 && anchor.hint->y <= 1.0 &&
                               !(anchor.hint->x == 0.5 && anchor.hint->y == 0.5);
                    } else {
                        return anchor.port_id >= 1U && anchor.port_id <= 4U;
                    }
                }, item.anchor);
            }
        }, endpoint.value);
    };
    return endpoint_valid(value.start) && endpoint_valid(value.end) &&
           (value.routing == ConnectorRouting::kStraight || value.routing == ConnectorRouting::kOrthogonal);
}

GeometryCount geometryUnits(const VectorPathGeometry& path) noexcept {
    GeometryCount total{};
    for (const auto& command : path.commands) {
        std::size_t units = 0U;
        if (std::holds_alternative<MoveTo>(command) || std::holds_alternative<LineTo>(command)) units = 1U;
        else if (std::holds_alternative<QuadTo>(command)) units = 2U;
        else if (std::holds_alternative<CubicTo>(command)) units = 3U;
        total = addGeometryUnits(total, {units, ValidationIssue::kNone});
        if (total.issue != ValidationIssue::kNone) return total;
    }
    return total;
}

GeometryCount geometryUnits(const ObjectContent& content) noexcept {
    return std::visit([](const auto& value) -> GeometryCount {
        using Item = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Item, VectorPathContent>) return geometryUnits(value.geometry);
        else if constexpr (std::is_same_v<Item, VectorStrokeContent>) {
            if (const auto* data = std::get_if<VectorStrokeData>(&value.stroke.data)) return multiplyGeometryUnits(data->samples.size(), 1U);
            return {0U, ValidationIssue::kIntegerOverflow};
        } else if constexpr (std::is_same_v<Item, DabStrokeContent>) {
            if (const auto* data = std::get_if<DabStrokeData>(&value.stroke.data)) return multiplyGeometryUnits(data->dabs.size(), 3U);
            return {0U, ValidationIssue::kIntegerOverflow};
        }
        else return {0U, ValidationIssue::kNone};
    }, content);
}

GeometryCount geometryUnits(const EraseMaskGeometry& geometry) noexcept {
    return std::visit([](const auto& value) -> GeometryCount {
        using Item = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Item, SweptCircleMask>) {
            return multiplyGeometryUnits(value.segments.size(), 6U);
        } else {
            return geometryUnits(value.path);
        }
    }, geometry);
}

GeometryCount geometryUnits(const ObjectRecord& object) noexcept {
    GeometryCount total = geometryUnits(object.content);
    for (const auto& mask : object.erase_masks) {
        total = addGeometryUnits(total, geometryUnits(mask.geometry));
        if (total.issue != ValidationIssue::kNone) return total;
    }
    return total;
}

GeometryCount geometryUnits(const OperationPayload& payload) noexcept {
    return std::visit([](const auto& value) -> GeometryCount {
        using Payload = std::decay_t<decltype(value)>;
        GeometryCount total{};
        auto addObject = [&total](const ObjectRecord& object) {
            total = addGeometryUnits(total, geometryUnits(object));
        };
        if constexpr (std::is_same_v<Payload, InsertObjectsOp> ||
                      std::is_same_v<Payload, RestoreObjectsOp>) {
            for (const auto& object : value.objects) addObject(object);
        } else if constexpr (std::is_same_v<Payload, AddStrokeOp>) {
            addObject(value.object);
        } else if constexpr (std::is_same_v<Payload, SplitStrokesOp>) {
            for (const auto& split : value.splits) {
                for (const auto& object : split.replacements) addObject(object);
            }
        } else if constexpr (std::is_same_v<Payload, AddEraseMasksOp>) {
            for (const auto& item : value.items) {
                for (const auto& mask : item.masks) {
                    total = addGeometryUnits(total, geometryUnits(mask.geometry));
                }
            }
        } else if constexpr (std::is_same_v<Payload, SetVectorPathGeometryOp>) {
            total = geometryUnits(value.geometry);
        }
        return total;
    }, payload);
}

bool validEraseGeometry(const EraseMaskGeometry& geometry) noexcept {
    return std::visit([](const auto& value) -> bool {
        using Item = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Item, SweptCircleMask>) {
            if (value.segments.empty() || value.segments.size() > kMaxGeometryElements) return false;
            for (const auto& segment : value.segments) {
                if (!finiteVec(segment.p0.position) || !finiteVec(segment.p1.position) ||
                    !finiteVec(segment.control1) || !finiteVec(segment.control2) ||
                    !std::isfinite(segment.p0.radius) || segment.p0.radius <= 0.0 ||
                    !std::isfinite(segment.p1.radius) || segment.p1.radius <= 0.0) return false;
            }
            return true;
        } else {
            return validVectorPath(value.path);
        }
    }, geometry);
}

template <typename Range, typename Key>
bool validCanonicalSet(const Range& values, Key key) {
    if (values.empty() || values.size() > kMaxKeyedBatchItems) return false;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto current = key(values[index]);
        if constexpr (std::is_same_v<std::decay_t<decltype(current)>, ObjectId>) {
            if (!validId(current)) return false;
        }
        if (index > 0U && !(key(values[index - 1U]) < current)) return false;
    }
    return true;
}

bool validPropertyValue(std::uint32_t field_id, const PropertyValue& value) noexcept {
    switch (field_id) {
        case 0x00000001U:
        case 0x00000002U:
            return std::holds_alternative<bool>(value);
        case 0x00000003U: {
            const auto* opacity = std::get_if<float>(&value);
            return opacity != nullptr && std::isfinite(*opacity) && *opacity >= 0.0F && *opacity <= 1.0F;
        }
        case 0x00000004U:
            return std::holds_alternative<BlendModeValue>(value) &&
                   std::get<BlendModeValue>(value) == BlendModeValue::kNormal;
        case 0x00000100U:
            if (!std::holds_alternative<FillStyleValue>(value)) return false;
            return std::visit([](const auto& fill) -> bool {
                using Fill = std::decay_t<decltype(fill)>;
                if constexpr (std::is_same_v<Fill, NoFill>) return true;
                else return validColor(fill.color);
            }, std::get<FillStyleValue>(value));
        case 0x00000101U:
            if (!std::holds_alternative<StrokeStyleValue>(value)) return false;
            return std::visit([](const auto& stroke) -> bool {
                using Stroke = std::decay_t<decltype(stroke)>;
                if constexpr (std::is_same_v<Stroke, NoStroke>) return true;
                else return validStrokeStyle(stroke);
            }, std::get<StrokeStyleValue>(value));
        case 0x00000200U:
        case 0x00000201U:
            if (const auto* decoration = std::get_if<ConnectorDecorationValue>(&value)) {
                return *decoration == ConnectorDecorationValue::kNone ||
                       *decoration == ConnectorDecorationValue::kArrow;
            }
            return false;
        default:
            return false;
    }
}

bool fieldAppliesToObjectKind(ObjectKind kind, std::uint32_t field_id) noexcept {
    switch (field_id) {
        case 0x00000001U:
        case 0x00000002U:
            return true;
        case 0x00000003U:
        case 0x00000004U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kVectorPath ||
                   kind == ObjectKind::kRichText || kind == ObjectKind::kImage ||
                   kind == ObjectKind::kConnector || kind == ObjectKind::kSticky;
        case 0x00000100U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kVectorPath || kind == ObjectKind::kSticky;
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

bool validPropertyBag(ObjectKind kind, const PropertyBag& bag) {
    if (bag.entries.empty()) return true;
    for (std::size_t index = 0; index < bag.entries.size(); ++index) {
        const auto& entry = bag.entries[index];
        if (!fieldAppliesToObjectKind(kind, entry.field_id) || !validPropertyValue(entry.field_id, entry.value)) return false;
        if (index > 0U && bag.entries[index - 1U].field_id >= entry.field_id) return false;
    }
    return true;
}

bool finiteVec(const Vec2& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool validVectorPath(const VectorPathGeometry& path) noexcept {
    if (path.commands.empty() ||
        (path.fill_rule != FillRule::kNonZero && path.fill_rule != FillRule::kEvenOdd)) {
        return false;
    }
    bool has_open_subpath = false;
    for (std::size_t index = 0; index < path.commands.size(); ++index) {
        const auto& command = path.commands[index];
        if (const auto* move = std::get_if<MoveTo>(&command)) {
            if (!finiteVec(move->point)) return false;
            has_open_subpath = true;
            continue;
        }
        if (const auto* line = std::get_if<LineTo>(&command)) {
            if (!has_open_subpath || !finiteVec(line->end)) return false;
            continue;
        }
        if (const auto* quad = std::get_if<QuadTo>(&command)) {
            if (!has_open_subpath || !finiteVec(quad->control) || !finiteVec(quad->end)) return false;
            continue;
        }
        if (const auto* cubic = std::get_if<CubicTo>(&command)) {
            if (!has_open_subpath || !finiteVec(cubic->control1) ||
                !finiteVec(cubic->control2) || !finiteVec(cubic->end)) {
                return false;
            }
            continue;
        }
        if (!has_open_subpath) return false;
        has_open_subpath = false;
    }
    return true;
}

bool validRichTextStep(const RichTextStep& step) noexcept {
    return std::visit(
        [](const auto& value) noexcept -> bool {
            using Step = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Step, InsertTextStep>) {
                return validId(value.paragraph_id) && value.text.size() <= kMaxGenericStringBytes &&
                       validUtf8(value.text) && validTextStyle(value.style);
            } else if constexpr (std::is_same_v<Step, DeleteTextStep> ||
                                 std::is_same_v<Step, SetInlineStyleStep> ||
                                 std::is_same_v<Step, SetParagraphStyleStep>) {
                if (!validId(value.paragraph_id)) return false;
                if constexpr (std::is_same_v<Step, SetInlineStyleStep>) return validTextStyle(value.style);
                if constexpr (std::is_same_v<Step, SetParagraphStyleStep>) return validParagraphStyle(value.style);
                return true;
            } else if constexpr (std::is_same_v<Step, SplitParagraphStep>) {
                return validId(value.paragraph_id) && validId(value.new_paragraph_id);
            } else {
                return validId(value.first_paragraph_id) && validId(value.second_paragraph_id);
            }
        },
        step);
}

bool validRichTextDelta(const RichTextDelta& delta) noexcept {
    if (delta.delta_version != 1U || delta.steps.empty()) return false;
    std::size_t inserted_bytes = 0U;
    for (const auto& step : delta.steps) {
        if (!validRichTextStep(step)) return false;
        if (const auto* insert = std::get_if<InsertTextStep>(&step)) {
            if (insert->text.size() > kMaxRichTextInsertBytes - inserted_bytes) return false;
            inserted_bytes += insert->text.size();
        }
    }
    return true;
}

bool validPressureCurve(const PiecewiseLinearCurve01& curve) noexcept {
    if (curve.points.size() < 2U) return false;
    if (curve.points.front().x != 0.0F || curve.points.back().x != 1.0F) return false;
    for (std::size_t index = 0; index < curve.points.size(); ++index) {
        const auto& point = curve.points[index];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0.0F || point.x > 1.0F || point.y < 0.0F || point.y > 1.0F) {
            return false;
        }
        if (index > 0U && !(curve.points[index - 1U].x < point.x)) return false;
    }
    return true;
}

bool validPressureMapping(const PressureMapping& pressure) noexcept {
    if (!pressure.enabled) {
        return !pressure.size_curve.has_value() && !pressure.opacity_curve.has_value();
    }
    return pressure.size_curve.has_value() && pressure.opacity_curve.has_value() &&
           validPressureCurve(*pressure.size_curve) && validPressureCurve(*pressure.opacity_curve);
}

bool validBrush(const BrushDescriptor& brush, bool dab_representation) noexcept {
    if (!validColor(brush.color) || !std::isfinite(brush.nominal_size) || brush.nominal_size <= 0.0 ||
        !std::isfinite(brush.opacity) || brush.opacity < 0.0F || brush.opacity > 1.0F ||
        !validPressureMapping(brush.pressure) || brush.tilt.enabled ||
        !std::isfinite(brush.tilt.size_influence) || !std::isfinite(brush.tilt.angle_influence) ||
        brush.tilt.size_influence != 0.0F || brush.tilt.angle_influence != 0.0F) {
        return false;
    }
    switch (brush.brush_family_id) {
        case 1U:
        case 2U:
            if (brush.brush_version != 1U || dab_representation || brush.texture_resource_id.has_value()) {
                return false;
            }
            return (brush.brush_family_id == 1U && brush.blend_mode == BrushBlendMode::kNormal) ||
                   (brush.brush_family_id == 2U && brush.blend_mode == BrushBlendMode::kHighlighter);
        case 3U:
            return brush.brush_version == 1U && dab_representation &&
                   brush.blend_mode == BrushBlendMode::kNormal &&
                   brush.texture_resource_id.has_value() &&
                   validId(brush.texture_resource_id->value);
        default:
            return false;
    }
}

bool validStrokeRecord(const StrokeRecord& stroke, bool dab_representation) noexcept {
    if (!validBrush(stroke.brush, dab_representation)) return false;
    if (dab_representation) {
        const auto* data = std::get_if<DabStrokeData>(&stroke.data);
        if (data == nullptr || data->dabs.empty()) return false;
        for (const auto& dab : data->dabs) {
            if (!finiteVec(dab.center) || !std::isfinite(dab.size) || dab.size <= 0.0 ||
                !std::isfinite(dab.rotation) || !std::isfinite(dab.opacity) ||
                dab.opacity < 0.0F || dab.opacity > 1.0F) {
                return false;
            }
        }
        return true;
    }
    const auto* data = std::get_if<VectorStrokeData>(&stroke.data);
    if (data == nullptr || data->samples.empty()) return false;
    for (const auto& sample : data->samples) {
        if (!finiteVec(sample.position) || !std::isfinite(sample.pressure) ||
            (stroke.brush.pressure.enabled
                ? (sample.pressure < 0.0F || sample.pressure > 1.0F)
                : sample.pressure != 1.0F) ||
            !finiteVec(sample.tilt) ||
            sample.tilt.x != 0.0 || sample.tilt.y != 0.0) {
            return false;
        }
    }
    return true;
}

bool validObjectKindTriple(const ObjectRecord& object) noexcept {
    if (!validId(object.id) || object.kind_version != 1U) return false;
    switch (object.kind) {
        case ObjectKind::kShape:
            return std::holds_alternative<ShapeContent>(object.content);
        case ObjectKind::kImage:
            return std::holds_alternative<ImageContent>(object.content);
        case ObjectKind::kVectorPath:
            return std::holds_alternative<VectorPathContent>(object.content);
        case ObjectKind::kRichText:
            return std::holds_alternative<RichTextContent>(object.content);
        case ObjectKind::kVectorStroke:
            return std::holds_alternative<VectorStrokeContent>(object.content);
        case ObjectKind::kDabStroke:
            return std::holds_alternative<DabStrokeContent>(object.content);
        case ObjectKind::kConnector:
            return std::holds_alternative<ConnectorContent>(object.content);
        case ObjectKind::kSticky:
            return std::holds_alternative<StickyContent>(object.content);
        case ObjectKind::kGroup:
            return std::holds_alternative<GroupContent>(object.content);
    }
    return false;
}

bool validObjectRecordStructure(const ObjectRecord& object) {
    if (!validObjectKindTriple(object) || !validPropertyBag(object.kind, object.properties)) return false;
    if (!validTransform2D(object.transform)) return false;
    if (geometryUnits(object.content).issue != ValidationIssue::kNone) return false;
    if (object.placement.parent_id.has_value() && !validId(*object.placement.parent_id)) return false;
    if (!object.placement.order_key.isValid()) return false;
    if (!object.erase_masks.empty()) {
        if (object.erase_masks.size() > kMaxEraseMasksPerObject) return false;
        if (!validCanonicalSet(object.erase_masks, [](const EraseMaskRecord& mask) { return mask.id; })) {
            return false;
        }
        for (const auto& mask : object.erase_masks) {
            if (!validEraseGeometry(mask.geometry)) return false;
        }
    }
    if (const auto* vector = std::get_if<VectorStrokeContent>(&object.content)) {
        if (!validStrokeRecord(vector->stroke, false)) return false;
    }
    if (const auto* dab = std::get_if<DabStrokeContent>(&object.content)) {
        if (!validStrokeRecord(dab->stroke, true)) return false;
    }
    if (const auto* path = std::get_if<VectorPathContent>(&object.content)) {
        if (!validVectorPath(path->geometry)) return false;
    }
    if (const auto* shape = std::get_if<ShapeContent>(&object.content)) {
        if (!validShape(*shape)) return false;
    }
    if (const auto* image = std::get_if<ImageContent>(&object.content)) {
        if (!validImage(*image)) return false;
    }
    if (const auto* rich = std::get_if<RichTextContent>(&object.content)) {
        if (!validRichTextDocument(rich->document)) return false;
    }
    if (const auto* connector = std::get_if<ConnectorContent>(&object.content)) {
        if (!validConnector(*connector)) return false;
    }
    if (const auto* sticky = std::get_if<StickyContent>(&object.content)) {
        if (!std::isfinite(sticky->width) || sticky->width <= 0.0 ||
            !std::isfinite(sticky->height) || sticky->height <= 0.0) return false;
    }
    if (!object.erase_masks.empty() && object.kind != ObjectKind::kVectorStroke &&
        object.kind != ObjectKind::kDabStroke) return false;
    return true;
}

bool validPatch(const PropertyPatch& patch) noexcept {
    if (!validId(patch.object_id)) return false;
    const auto* value = std::get_if<PropertyValue>(&patch.value);
    switch (patch.action) {
        case PropertyPatchAction::kSet:
            return value != nullptr && validPropertyValue(patch.field_id, *value);
        case PropertyPatchAction::kClear:
            return value == nullptr && patch.field_id != 0U &&
                   (patch.field_id == 0x00000001U || patch.field_id == 0x00000002U ||
                    patch.field_id == 0x00000003U || patch.field_id == 0x00000004U ||
                    patch.field_id == 0x00000100U || patch.field_id == 0x00000101U ||
                    patch.field_id == 0x00000200U || patch.field_id == 0x00000201U);
        case PropertyPatchAction::kInvalid:
            return false;
    }
    return false;
}

bool allUnique(std::vector<ObjectId> values) {
    std::sort(values.begin(), values.end());
    return std::adjacent_find(values.begin(), values.end()) == values.end();
}

ValidationResult invalidCollection() noexcept {
    return {ValidationIssue::kInvalidCollection};
}

ValidationResult invalidObjectKind() noexcept {
    return {ValidationIssue::kInvalidObjectKind};
}

ValidationResult invalidPropertyPatch() noexcept {
    return {ValidationIssue::kInvalidPropertyPatch};
}

} // namespace

ValidationResult validateEnvelope(
    const Operation& operation,
    const OperationFieldPresence& presence) noexcept {
    if (operation.id.isZero() || operation.document_id.isZero()) {
        return {ValidationIssue::kInvalidId};
    }
    if (!presence.schema_version || !presence.payload_version ||
        operation.schema_version != 1U || operation.payload_version != 1U) {
        return {ValidationIssue::kUnsupportedVersion};
    }
    return {};
}

ValidationResult validatePayloadStructure(const Operation& operation) noexcept {
    const auto geometry = geometryUnits(operation.payload);
    if (geometry.issue != ValidationIssue::kNone) return {geometry.issue};
    return std::visit(
        [](const auto& payload) -> ValidationResult {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, InsertObjectsOp> ||
                          std::is_same_v<Payload, RestoreObjectsOp>) {
                if (!validCanonicalSet(payload.objects, [](const ObjectRecord& object) { return object.id; })) {
                    return invalidCollection();
                }
                for (const auto& object : payload.objects) {
                    if (!validObjectRecordStructure(object)) return invalidObjectKind();
                }
            } else if constexpr (std::is_same_v<Payload, DeleteObjectsOp>) {
                if (!validCanonicalSet(payload.object_ids, [](const ObjectId& value) { return value; })) {
                    return invalidCollection();
                }
            } else if constexpr (std::is_same_v<Payload, SetPlacementsOp>) {
                if (!validCanonicalSet(payload.items, [](const PlacementItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                for (const auto& item : payload.items) {
                    if (!item.placement.order_key.isValid() ||
                        (item.placement.parent_id.has_value() && !validId(*item.placement.parent_id))) {
                        return invalidCollection();
                    }
                }
            } else if constexpr (std::is_same_v<Payload, SetTransformsOp>) {
                if (!validCanonicalSet(payload.items, [](const TransformItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                for (const auto& item : payload.items) {
                    if (!validTransform2D(item.transform)) return {ValidationIssue::kInvalidLeaf};
                }
            } else if constexpr (std::is_same_v<Payload, PatchPropertiesOp>) {
                if (payload.patches.empty() || payload.patches.size() > kMaxKeyedBatchItems) return invalidCollection();
                for (std::size_t index = 0; index < payload.patches.size(); ++index) {
                    const auto& patch = payload.patches[index];
                    if (!validPatch(patch)) return invalidPropertyPatch();
                    if (index > 0U) {
                        const auto& previous = payload.patches[index - 1U];
                        const bool ordered = previous.object_id < patch.object_id ||
                                             (previous.object_id == patch.object_id &&
                                              previous.field_id < patch.field_id);
                        if (!ordered) return invalidCollection();
                    }
                }
            } else if constexpr (std::is_same_v<Payload, SetObjectSizeOp>) {
                if (!validCanonicalSet(payload.items, [](const ObjectSizeItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                for (const auto& item : payload.items) {
                    if (!std::isfinite(item.width) || item.width <= 0.0 ||
                        !std::isfinite(item.height) || item.height <= 0.0) return {ValidationIssue::kInvalidLeaf};
                }
            } else if constexpr (std::is_same_v<Payload, SetVectorPathGeometryOp> ||
                                 std::is_same_v<Payload, SetImageContentOp> ||
                                 std::is_same_v<Payload, EditRichTextOp> ||
                                 std::is_same_v<Payload, SetConnectorContentOp>) {
                if (!validId(payload.object_id)) return invalidCollection();
                if constexpr (std::is_same_v<Payload, SetVectorPathGeometryOp>) {
                    if (payload.geometry.commands.size() > kMaxGeometryElements ||
                        !validVectorPath(payload.geometry)) return {ValidationIssue::kInvalidLeaf};
                }
                if constexpr (std::is_same_v<Payload, SetImageContentOp>) {
                    if (!validImage(payload.content)) return {ValidationIssue::kInvalidLeaf};
                }
                if constexpr (std::is_same_v<Payload, EditRichTextOp>) {
                    if (!validRichTextDelta(payload.delta)) return {ValidationIssue::kInvalidLeaf};
                }
                if constexpr (std::is_same_v<Payload, SetConnectorContentOp>) {
                    if (!validConnector(payload.content)) return {ValidationIssue::kInvalidLeaf};
                }
            } else if constexpr (std::is_same_v<Payload, AddStrokeOp>) {
                if (!validObjectRecordStructure(payload.object)) return invalidObjectKind();
            } else if constexpr (std::is_same_v<Payload, SplitStrokesOp>) {
                if (!validCanonicalSet(payload.splits, [](const StrokeSplit& split) { return split.source_stroke_id; })) {
                    return invalidCollection();
                }
                std::vector<ObjectId> replacement_ids;
                for (const auto& split : payload.splits) {
                    if (!validCanonicalSet(split.replacements, [](const ObjectRecord& object) { return object.id; })) {
                        return invalidCollection();
                    }
                    for (const auto& replacement : split.replacements) {
                        if (!validObjectRecordStructure(replacement)) return invalidObjectKind();
                        replacement_ids.push_back(replacement.id);
                    }
                }
                if (!allUnique(std::move(replacement_ids))) return invalidCollection();
            } else if constexpr (std::is_same_v<Payload, AddEraseMasksOp>) {
                if (!validCanonicalSet(payload.items, [](const EraseMaskAddItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                std::vector<ObjectId> mask_ids;
                for (const auto& item : payload.items) {
                    if (!validCanonicalSet(item.masks, [](const EraseMaskRecord& mask) { return mask.id; }) ||
                        item.masks.size() > kMaxEraseMasksPerObject) {
                        return invalidCollection();
                    }
                    for (const auto& mask : item.masks) {
                        if (!validEraseGeometry(mask.geometry)) return {ValidationIssue::kInvalidLeaf};
                        mask_ids.push_back(mask.id);
                    }
                }
                if (!allUnique(std::move(mask_ids))) return invalidCollection();
            } else if constexpr (std::is_same_v<Payload, RemoveEraseMasksOp>) {
                if (!validCanonicalSet(payload.items, [](const EraseMaskRemoveItem& item) { return item.object_id; })) {
                    return invalidCollection();
                }
                for (const auto& item : payload.items) {
                    if (!validCanonicalSet(item.mask_ids, [](const ObjectId& value) { return value; })) {
                        return invalidCollection();
                    }
                }
            }
            return {};
        },
        operation.payload);
}

} // namespace canvas::semantic
