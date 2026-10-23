#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"

#include <gtest/gtest.h>

TEST(FC05Route, OrthogonalConnectorKeepsStableEndpoints) {
    canvas::semantic::ConnectorContent content{};
    content.start.value = canvas::semantic::FreePointEndpoint{{10.0, 20.0}};
    content.end.value = canvas::semantic::FreePointEndpoint{{90.0, 70.0}};
    content.routing = canvas::semantic::ConnectorRouting::kOrthogonal;
    const auto route = canvas::scene::connectorGeometry(content, {});
    ASSERT_TRUE(route.has_value());
    EXPECT_FLOAT_EQ(route->start.x, 10.0F);
    EXPECT_FLOAT_EQ(route->start.y, 20.0F);
    EXPECT_FLOAT_EQ(route->end.x, 90.0F);
    EXPECT_FLOAT_EQ(route->end.y, 70.0F);
}

TEST(FC05Route, OrthogonalRouteUsesObstacleAndBoundsShareDerivedPolyline) {
    canvas::semantic::ObjectRecord connector{};
    connector.id = canvas::foundation::ObjectId::fromUint64(100);
    connector.kind = canvas::semantic::ObjectKind::kConnector;
    connector.kind_version = 1;
    connector.content = canvas::semantic::ConnectorContent{};
    auto& content = std::get<canvas::semantic::ConnectorContent>(connector.content);
    content.start.value = canvas::semantic::FreePointEndpoint{{10.0, 20.0}};
    content.end.value = canvas::semantic::FreePointEndpoint{{90.0, 70.0}};
    content.routing = canvas::semantic::ConnectorRouting::kOrthogonal;
    canvas::semantic::ObjectRecord obstacle{};
    obstacle.id = canvas::foundation::ObjectId::fromUint64(101);
    obstacle.kind = canvas::semantic::ObjectKind::kShape;
    obstacle.kind_version = 1;
    obstacle.content = canvas::semantic::ShapeContent{1, 20.0, 20.0};
    obstacle.transform.tx = 40.0;
    obstacle.transform.ty = 10.0;
    const std::vector<canvas::semantic::ObjectRecord> objects{connector, obstacle};
    const auto route = canvas::scene::connectorGeometry(connector, objects);
    ASSERT_TRUE(route.has_value());
    ASSERT_EQ(route->route.size(), 3U);
    EXPECT_FLOAT_EQ(route->route[1].x, 10.0F);
    EXPECT_FLOAT_EQ(route->route[1].y, 70.0F);
    const auto bounds = canvas::scene::computeBounds(connector, nullptr, &*route);
    EXPECT_FLOAT_EQ(bounds.geometry.left, 10.0F);
    EXPECT_FLOAT_EQ(bounds.geometry.right, 90.0F);
    EXPECT_FLOAT_EQ(bounds.geometry.bottom, 70.0F);
}

TEST(FC05Route, AttachedEndpointResolvesTargetPortInConnectorLocalSpace) {
    canvas::semantic::ObjectRecord target{};
    target.id = canvas::foundation::ObjectId::fromUint64(200);
    target.kind = canvas::semantic::ObjectKind::kShape;
    target.kind_version = 1;
    target.content = canvas::semantic::ShapeContent{1, 100.0, 40.0};
    target.transform.tx = 100.0;
    canvas::semantic::ObjectRecord connector{};
    connector.id = canvas::foundation::ObjectId::fromUint64(201);
    connector.kind = canvas::semantic::ObjectKind::kConnector;
    connector.kind_version = 1;
    connector.transform.tx = 50.0;
    connector.content = canvas::semantic::ConnectorContent{};
    auto& content = std::get<canvas::semantic::ConnectorContent>(connector.content);
    content.start.value = canvas::semantic::AttachedEndpoint{
        target.id, canvas::semantic::StablePortAnchor{2U}};
    content.end.value = canvas::semantic::FreePointEndpoint{{0.0, 20.0}};
    const auto route = canvas::scene::connectorGeometry(connector, {connector, target});
    ASSERT_TRUE(route.has_value());
    EXPECT_FLOAT_EQ(route->start.x, 150.0F);
    EXPECT_FLOAT_EQ(route->start.y, 20.0F);
}
