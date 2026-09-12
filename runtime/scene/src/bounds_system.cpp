#include "canvas/scene/bounds_system.hpp"

#include "canvas/scene/reference_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <variant>
#include <type_traits>

namespace canvas::scene {
namespace {
foundation::WorldRect shapeRect(const semantic::ObjectRecord& record) noexcept {
    return std::visit([](const auto& content) -> foundation::WorldRect {
        using T = std::decay_t<decltype(content)>;
        if constexpr (std::is_same_v<T, semantic::ShapeContent>) {
            return {0.0F, 0.0F, static_cast<float>(content.width), static_cast<float>(content.height)};
        } else if constexpr (std::is_same_v<T, semantic::ImageContent>) {
            return {0.0F, 0.0F, static_cast<float>(content.width), static_cast<float>(content.height)};
        } else if constexpr (std::is_same_v<T, semantic::StickyContent>) {
            return {0.0F, 0.0F, static_cast<float>(content.width), static_cast<float>(content.height)};
        } else if constexpr (std::is_same_v<T, semantic::VectorPathContent>) {
            ReferenceGeometry geometry;
            for (const auto& command : content.geometry.commands) {
                std::visit([&](const auto& c) {
                    using C = std::decay_t<decltype(c)>;
                    ReferencePathElement e{};
                    if constexpr (std::is_same_v<C, semantic::MoveTo>) { e.command = ReferencePathCommand::kMoveTo; e.x = c.point.x; e.y = c.point.y; }
                    else if constexpr (std::is_same_v<C, semantic::LineTo>) { e.command = ReferencePathCommand::kLineTo; e.x = c.end.x; e.y = c.end.y; }
                    else if constexpr (std::is_same_v<C, semantic::QuadTo>) { e.command = ReferencePathCommand::kQuadTo; e.x = c.end.x; e.y = c.end.y; e.control_x = c.control.x; e.control_y = c.control.y; }
                    else if constexpr (std::is_same_v<C, semantic::CubicTo>) { e.command = ReferencePathCommand::kCubicTo; e.x = c.end.x; e.y = c.end.y; e.control_x = c.control1.x; e.control_y = c.control1.y; e.control2_x = c.control2.x; e.control2_y = c.control2.y; }
                    else { e.command = ReferencePathCommand::kClose; }
                    geometry.elements.push_back(e);
                }, command);
            }
            geometry.kind = ReferenceGeometry::Kind::kPath;
            return geometryBounds(geometry);
        } else if constexpr (std::is_same_v<T, semantic::RichTextContent>) {
            double width = 0.0; double height = 0.0;
            for (const auto& paragraph : content.document.paragraphs) {
                height += paragraph.style.line_height > 0.0 ? paragraph.style.line_height : 1.0;
                for (const auto& run : paragraph.runs) width += run.text.size() * (run.style.font_size > 0.0 ? run.style.font_size * 0.5 : 0.5);
            }
            return {0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)};
        } else if constexpr (std::is_same_v<T, semantic::VectorStrokeContent>) {
            const auto* data = std::get_if<semantic::VectorStrokeData>(&content.stroke.data);
            if (data == nullptr || data->samples.empty()) return {};
            foundation::WorldRect result{}; bool have = false;
            const float radius = static_cast<float>(std::max(0.0, content.stroke.brush.nominal_size) * 0.5);
            for (const auto& sample : data->samples) {
                const foundation::WorldRect r{static_cast<float>(sample.position.x) - radius,
                    static_cast<float>(sample.position.y) - radius,
                    static_cast<float>(sample.position.x) + radius,
                    static_cast<float>(sample.position.y) + radius};
                result = have ? foundation::unionRects(result, r) : r; have = true;
            }
            return result;
        } else if constexpr (std::is_same_v<T, semantic::DabStrokeContent>) {
            const auto* data = std::get_if<semantic::DabStrokeData>(&content.stroke.data);
            if (data == nullptr || data->dabs.empty()) return {};
            foundation::WorldRect result{}; bool have = false;
            for (const auto& dab : data->dabs) {
                const float radius = static_cast<float>(std::max(0.0, dab.size) * 0.5);
                const foundation::WorldRect r{static_cast<float>(dab.center.x) - radius,
                    static_cast<float>(dab.center.y) - radius,
                    static_cast<float>(dab.center.x) + radius,
                    static_cast<float>(dab.center.y) + radius};
                result = have ? foundation::unionRects(result, r) : r; have = true;
            }
            return result;
        } else if constexpr (std::is_same_v<T, semantic::ConnectorContent>) {
            foundation::WorldRect result{};
            bool have = false;
            auto add = [&](const semantic::ConnectorEndpoint& endpoint) {
                if (const auto* point = std::get_if<semantic::FreePointEndpoint>(&endpoint.value)) {
                    foundation::WorldRect r{static_cast<float>(point->point.x), static_cast<float>(point->point.y), static_cast<float>(point->point.x), static_cast<float>(point->point.y)};
                    result = have ? foundation::unionRects(result, r) : r; have = true;
                } else if (const auto* attached = std::get_if<semantic::AttachedEndpoint>(&endpoint.value)) {
                    if (const auto* anchor = std::get_if<semantic::AutoPerimeterAnchor>(&attached->anchor)) {
                        if (anchor->hint) {
                            const auto r = foundation::WorldRect{static_cast<float>(anchor->hint->x),
                                static_cast<float>(anchor->hint->y), static_cast<float>(anchor->hint->x),
                                static_cast<float>(anchor->hint->y)};
                            result = have ? foundation::unionRects(result, r) : r; have = true;
                        }
                    }
                }
            }; add(content.start); add(content.end); return result;
        } else { return {}; }
    }, record.content);
}

bool finiteRect(const foundation::WorldRect& r) noexcept { return r.isFiniteAndOrdered(); }
} // namespace

BoundsResult computeBounds(const semantic::ObjectRecord& record) noexcept {
    BoundsResult result;
    if (record.kind == semantic::ObjectKind::kGroup) { result.geometry = {}; result.visual = {}; result.world = {}; return result; }
    result.geometry = shapeRect(record);
    const float stroke = [&] {
        for (const auto& entry : record.properties.entries) if (entry.field_id == 0x101U) if (const auto* style = std::get_if<semantic::StrokeStyleValue>(&entry.value)) if (const auto* solid = std::get_if<semantic::SolidStroke>(style)) return static_cast<float>(std::max(0.0, solid->width) * 0.5); return 0.0F;
    }();
    result.visual = {result.geometry.left - stroke, result.geometry.top - stroke,
                     result.geometry.right + stroke, result.geometry.bottom + stroke};
    const double xs[] = {result.visual.left, result.visual.right};
    const double ys[] = {result.visual.top, result.visual.bottom};
    foundation::WorldRect transformed{};
    bool have = false;
    for (const double x : xs) for (const double y : ys) {
        const double wx = record.transform.a * x + record.transform.c * y + record.transform.tx;
        const double wy = record.transform.b * x + record.transform.d * y + record.transform.ty;
        const foundation::WorldRect point{static_cast<float>(wx), static_cast<float>(wy),
                                          static_cast<float>(wx), static_cast<float>(wy)};
        transformed = have ? foundation::unionRects(transformed, point) : point; have = true;
    }
    result.world = transformed;
    result.finite = finiteRect(result.geometry) && finiteRect(result.visual) && finiteRect(result.world);
    if (!result.finite) { const float nan = std::numeric_limits<float>::quiet_NaN(); result.geometry = {nan,nan,nan,nan}; result.visual = result.geometry; result.world = result.geometry; }
    return result;
}

} // namespace canvas::scene
