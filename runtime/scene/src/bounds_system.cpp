#include "canvas/scene/bounds_system.hpp"

#include "canvas/scene/reference_geometry.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/text/text_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <variant>
#include <type_traits>

namespace canvas::scene {
namespace {
foundation::WorldRect shapeRect(const semantic::ObjectRecord& record,
                                const ConnectorGeometry* routeOverride) noexcept {
    return std::visit([&record, routeOverride](const auto& content) -> foundation::WorldRect {
        using T = std::decay_t<decltype(content)>;
        if constexpr (std::is_same_v<T, semantic::ShapeContent>) {
            return {0.0F, 0.0F, static_cast<float>(content.width), static_cast<float>(content.height)};
        } else if constexpr (std::is_same_v<T, semantic::ImageContent>) {
            return {0.0F, 0.0F, static_cast<float>(content.width), static_cast<float>(content.height)};
        } else if constexpr (std::is_same_v<T, semantic::StickyContent>) {
            return {0.0F, 0.0F, static_cast<float>(content.width), static_cast<float>(content.height)};
        } else if constexpr (std::is_same_v<T, semantic::SectionContent>) {
            const auto& b=content.boundary;
            return {static_cast<float>(b.x),static_cast<float>(b.y),
                    static_cast<float>(b.x+b.width),static_cast<float>(b.y+b.height)};
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
            // Without an admitted layout/resource context, text contributes no
            // fabricated geometry. Application/Scene supplies the derived snapshot.
            return {};
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
        } else if constexpr (std::is_same_v<T, semantic::DabBrushStrokeContent>) {
            const auto& dabs = content.stroke.dab_output.dabs;
            if (dabs.empty()) return {};
            foundation::WorldRect result{}; bool have = false;
            for (const auto& dab : dabs) {
                const float radius = static_cast<float>(std::max(0.0, dab.size) * 0.5);
                const foundation::WorldRect r{static_cast<float>(dab.center.x) - radius,
                    static_cast<float>(dab.center.y) - radius,
                    static_cast<float>(dab.center.x) + radius,
                    static_cast<float>(dab.center.y) + radius};
                result = have ? foundation::unionRects(result, r) : r; have = true;
            }
            return result;
        } else if constexpr (std::is_same_v<T, semantic::BrushStrokeContent>) {
            const auto& outline = content.stroke.vector_output.outline;
            if (outline.empty()) return {};
            foundation::WorldRect result{}; bool have = false;
            for (const auto& point : outline) {
                const foundation::WorldRect r{static_cast<float>(point.x), static_cast<float>(point.y),
                                              static_cast<float>(point.x), static_cast<float>(point.y)};
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
            }; add(content.start); add(content.end);
            const auto route = routeOverride != nullptr
                ? std::optional<ConnectorGeometry>(*routeOverride)
                : connectorGeometry(record);
            if (route.has_value() && route->route.size() > 1U) {
                result = {};
                have = false;
                for (const auto point : route->route) {
                    const foundation::WorldRect pointRect{point.x, point.y, point.x, point.y};
                    result = have ? foundation::unionRects(result, pointRect) : pointRect;
                    have = true;
                }
            }
            bool arrow=false; for(const auto& entry:record.properties.entries) if(entry.field_id==0x201U) if(const auto* v=std::get_if<semantic::ConnectorDecorationValue>(&entry.value)) arrow=*v==semantic::ConnectorDecorationValue::kArrow;
            if(arrow && have && route && route->status==runtime::ConnectorRouteStatus::kReady) {
                auto geometry=*route;geometry.arrow_end=true;
                const auto head=arrowHead(geometry);
                result=foundation::unionRects(result,{head.left.x,head.left.y,head.left.x,head.left.y});
                result=foundation::unionRects(result,{head.right.x,head.right.y,head.right.x,head.right.y});
            }

            return result;
        } else { return {}; }
    }, record.content);
}

bool finiteRect(const foundation::WorldRect& r) noexcept { return r.isFiniteAndOrdered(); }
} // namespace

BoundsResult computeBounds(const semantic::ObjectRecord& record,
                           const text::TextLayoutSnapshot* textLayout,
                           const ConnectorGeometry* connectorGeometryOverride) noexcept {
    BoundsResult result;
    if (record.kind == semantic::ObjectKind::kGroup) { result.geometry = {}; result.visual = {}; result.world = {}; return result; }
    result.geometry = record.kind == semantic::ObjectKind::kRichText && textLayout != nullptr
        ? textLayout->localBounds : shapeRect(record, connectorGeometryOverride);
    if(record.kind==semantic::ObjectKind::kRichText && textLayout) {
        const auto& content=std::get<semantic::RichTextContent>(record.content);
        if(content.layout)result.geometry=foundation::unionRects(result.geometry,{0,0,static_cast<float>(content.layout->wrap_width),textLayout->localBounds.bottom});
    }
    const float stroke = [&] {
        for (const auto& entry : record.properties.entries) {
            if (entry.field_id == 0x101U) {
                if (const auto* style = std::get_if<semantic::StrokeStyleValue>(&entry.value)) {
                    if (const auto* solid = std::get_if<semantic::SolidStroke>(style)) {
                        return static_cast<float>(std::max(0.0, solid->width) * 0.5);
                    }
                    return 0.0F;
                }
            }
        }
        return record.kind == semantic::ObjectKind::kConnector ? 2.0F : 0.0F;
    }();
    result.visual = {result.geometry.left - stroke, result.geometry.top - stroke,
                     result.geometry.right + stroke, result.geometry.bottom + stroke};
    // A triangle's acute miter can extend past the half-width expansion.
    // Include each admitted outer join before transforming/indexing the bounds.
    if(const auto* shape=std::get_if<semantic::ShapeContent>(&record.content);shape&&shape->shape_kind==3&&stroke>0) {
        for(const auto& entry:record.properties.entries)if(entry.field_id==0x101U)
            if(const auto* style=std::get_if<semantic::StrokeStyleValue>(&entry.value))
                if(const auto* solid=std::get_if<semantic::SolidStroke>(style))
                    if(const auto* join=std::get_if<semantic::MiterJoin>(&solid->join)) {
                        const std::array<std::array<double,2>,3> vertices{{{shape->width/2,0},{shape->width,shape->height},{0,shape->height}}};
                        for(std::size_t i=0;i<vertices.size();++i) {
                            const auto& p=vertices[(i+2)%3];const auto& v=vertices[i];const auto& n=vertices[(i+1)%3];
                            const double ax=v[0]-p[0],ay=v[1]-p[1],bx=n[0]-v[0],by=n[1]-v[1];
                            const double al=std::hypot(ax,ay),bl=std::hypot(bx,by);
                            if(al<=0||bl<=0)continue;
                            const double nx=ay/al+by/bl,ny=-ax/al-bx/bl,denominator=1+(ax*bx+ay*by)/(al*bl);
                            if(denominator<=0)continue;
                            const double dx=nx*stroke/denominator,dy=ny*stroke/denominator;
                            if(std::hypot(dx,dy)>stroke*join->limit)continue;
                            const float x=static_cast<float>(v[0]+dx),y=static_cast<float>(v[1]+dy);
                            result.visual=foundation::unionRects(result.visual,{x,y,x,y});
                        }
                    }
    }
    const double xs[] = {result.visual.left, result.visual.right};
    const double ys[] = {result.visual.top, result.visual.bottom};
    foundation::WorldRect transformed{};
    bool have = false;
    for (const double x : xs) {
        for (const double y : ys) {
            const double wx = record.transform.a * x + record.transform.c * y + record.transform.tx;
            const double wy = record.transform.b * x + record.transform.d * y + record.transform.ty;
            const foundation::WorldRect point{static_cast<float>(wx), static_cast<float>(wy),
                                              static_cast<float>(wx), static_cast<float>(wy)};
            transformed = have ? foundation::unionRects(transformed, point) : point;
            have = true;
        }
    }
    result.world = transformed;
    result.finite = finiteRect(result.geometry) && finiteRect(result.visual) && finiteRect(result.world);
    if (!result.finite) { const float nan = std::numeric_limits<float>::quiet_NaN(); result.geometry = {nan,nan,nan,nan}; result.visual = result.geometry; result.world = result.geometry; }
    return result;
}

} // namespace canvas::scene
