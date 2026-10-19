#pragma once
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/object_record.hpp"
#include <optional>
#include <variant>
namespace canvas::scene {
struct TriangleGeometry final { foundation::WorldPoint top{}, right{}, left{}; foundation::WorldRect bounds{}; bool operator==(const TriangleGeometry& o) const { return top.x==o.top.x&&top.y==o.top.y&&right.x==o.right.x&&right.y==o.right.y&&left.x==o.left.x&&left.y==o.left.y&&bounds==o.bounds; } };
struct ConnectorGeometry final { foundation::WorldPoint start{}, end{}; bool arrow_end=false; bool operator==(const ConnectorGeometry& o) const { return start.x==o.start.x&&start.y==o.start.y&&end.x==o.end.x&&end.y==o.end.y&&arrow_end==o.arrow_end; } };
struct ArrowHeadGeometry final { foundation::WorldPoint tip{}, left{}, right{}; };
using DerivedGeometry = std::variant<std::monostate, TriangleGeometry, ConnectorGeometry>;
[[nodiscard]] DerivedGeometry deriveGeometry(const semantic::ObjectRecord&) noexcept;
[[nodiscard]] std::optional<TriangleGeometry> triangleGeometry(const semantic::ShapeContent&) noexcept;
[[nodiscard]] std::optional<ConnectorGeometry> connectorGeometry(const semantic::ConnectorContent&, const semantic::PropertyBag&) noexcept;
[[nodiscard]] bool pointInTriangle(foundation::WorldPoint, const TriangleGeometry&, float tolerance=0.0F) noexcept;
[[nodiscard]] float distanceToSegment(foundation::WorldPoint, foundation::WorldPoint, foundation::WorldPoint) noexcept;
[[nodiscard]] bool pointOnConnector(foundation::WorldPoint, const ConnectorGeometry&, float tolerance) noexcept;
[[nodiscard]] bool pointOnArrowHead(foundation::WorldPoint, const ConnectorGeometry&, float tolerance) noexcept;
[[nodiscard]] ArrowHeadGeometry arrowHead(const ConnectorGeometry&) noexcept;
}
