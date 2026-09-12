#include "canvas/scene/bounds_system.hpp"

#include "canvas/scene/reference_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <variant>

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
                    else if constexpr (std::is_same_v<C, semantic::CubicTo>) { e.command = ReferencePathCommand::kCubicTo; e.x = c.end.x; e.y = c.end.y; e.control_x = (c.control1.x + c.control2.x) / 2.0; e.control_y = (c.control1.y + c.control2.y) / 2.0; }
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
        } else if constexpr (std::is_same_v<T, semantic::VectorStrokeContent> || std::is_same_v<T, semantic::DabStrokeContent>) {
            return {};
        } else if constexpr (std::is_same_v<T, semantic::ConnectorContent>) {
            foundation::WorldRect result{};
            bool have = false;
            auto add = [&](const semantic::ConnectorEndpoint& endpoint) {
                if (const auto* point = std::get_if<semantic::FreePointEndpoint>(&endpoint.value)) {
                    foundation::WorldRect r{static_cast<float>(point->point.x), static_cast<float>(point->point.y), static_cast<float>(point->point.x), static_cast<float>(point->point.y)};
                    result = have ? foundation::unionRects(result, r) : r; have = true;
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
    result.world = {result.visual.left + static_cast<float>(record.transform.tx), result.visual.top + static_cast<float>(record.transform.ty),
                    result.visual.right + static_cast<float>(record.transform.tx), result.visual.bottom + static_cast<float>(record.transform.ty)};
    result.finite = finiteRect(result.geometry) && finiteRect(result.visual) && finiteRect(result.world);
    if (!result.finite) { const float nan = std::numeric_limits<float>::quiet_NaN(); result.geometry = {nan,nan,nan,nan}; result.visual = result.geometry; result.world = result.geometry; }
    return result;
}

} // namespace canvas::scene
