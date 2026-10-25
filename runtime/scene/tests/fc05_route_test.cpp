#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/connector_router.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <queue>

namespace {
using namespace canvas;
semantic::ObjectRecord box(std::uint64_t id, double x, double y, double w, double h) {
    semantic::ObjectRecord r{}; r.id = foundation::ObjectId::fromUint64(id);
    r.kind = semantic::ObjectKind::kShape; r.kind_version = 1;
    r.transform.tx = x; r.transform.ty = y; r.content = semantic::ShapeContent{1,w,h};
    return r;
}
semantic::ObjectRecord wire(double x, double y, double ex, double ey) {
    semantic::ObjectRecord r{}; r.id = foundation::ObjectId::fromUint64(900);
    r.kind = semantic::ObjectKind::kConnector; r.kind_version = 1;
    r.content = semantic::ConnectorContent{
        {semantic::FreePointEndpoint{{x,y}}}, {semantic::FreePointEndpoint{{ex,ey}}},
        semantic::ConnectorRouting::kOrthogonal};
    r.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::NoStroke{}}}};
    return r;
}
// Independent open-interior checker: no route implementation calls.
bool crosses(foundation::WorldPoint a, foundation::WorldPoint b, foundation::WorldRect r) {
    if (a.x == b.x) return a.x > r.left && a.x < r.right &&
        std::max(a.y,b.y) > r.top && std::min(a.y,b.y) < r.bottom;
    if (a.y == b.y) return a.y > r.top && a.y < r.bottom &&
        std::max(a.x,b.x) > r.left && std::min(a.x,b.x) < r.right;
    return true;
}
void avoids(const scene::ConnectorGeometry& g, const std::vector<semantic::ObjectRecord>& objects) {
    ASSERT_GE(g.route.size(),2U);
    for (std::size_t i=1; i<g.route.size(); ++i) {
        const auto a=g.route[i-1], b=g.route[i];
        EXPECT_TRUE(a.x==b.x || a.y==b.y);
        for (const auto& o:objects) EXPECT_FALSE(crosses(a,b,scene::computeBounds(o).world));
    }
}
}

TEST(FC05Route, CollinearEndpointsDetourAroundBlockingObject) {
    const auto c=wire(0,50,200,50);
    const std::vector<semantic::ObjectRecord> obstacles{box(1,80,20,40,60)};
    const auto route=scene::connectorGeometry(c,obstacles);
    ASSERT_TRUE(route); avoids(*route,obstacles);
}

TEST(FC05Route, BothSingleElbowsBlockedStillFindsMultipleBendRoute) {
    const auto c=wire(0,0,200,200);
    const std::vector<semantic::ObjectRecord> obstacles{
        box(1,40,-20,80,80), box(2,-20,80,80,80)};
    const auto route=scene::connectorGeometry(c,obstacles);
    ASSERT_TRUE(route); avoids(*route,obstacles);
}

TEST(FC05Route, CancellationIsTerminalAndEachQuantumIsBounded) {
    const auto c=wire(0,0,500,500);
    std::vector<semantic::ObjectRecord> objects;
    for(unsigned i=0;i<100;++i)objects.push_back(box(i+1,50+(i%10)*35,50+(i/10)*35,12,12));
    scene::ConnectorRouteJob cancelled(c,objects);
    ASSERT_EQ(cancelled.advance(),runtime::ConnectorRouteStatus::kPending);
    cancelled.cancel();
    const auto cancelledWork=cancelled.result().work;
    EXPECT_EQ(cancelled.advance(),runtime::ConnectorRouteStatus::kCancelled);
    EXPECT_EQ(cancelled.result().work,cancelledWork);
    scene::ConnectorRouteJob job(c,objects);
    for(unsigned q=0;q<64;++q) {
        const auto before=job.result().work;
        const auto status=job.advance();
        const auto after=job.result().work;
        EXPECT_LE(after.examined-before.examined,1024U);
        EXPECT_LE(after.expanded-before.expanded,1024U);
        EXPECT_LE(after.predicates-before.predicates,8192U);
        if(status!=runtime::ConnectorRouteStatus::kPending)break;
    }
    EXPECT_NE(job.result().status,runtime::ConnectorRouteStatus::kPending);
}

TEST(FC05Route, BudgetFailureIsExplicitAndNeverPublishesAnAvoidingRoute) {
    const auto c=wire(0,0,500,500);
    std::vector<semantic::ObjectRecord> objects;
    for(unsigned i=0;i<257;++i)objects.push_back(box(i+1,50+(i%17)*20,50+(i/17)*20,5,5));
    const auto route=scene::connectorGeometry(c,objects);
    ASSERT_TRUE(route); EXPECT_EQ(route->status,runtime::ConnectorRouteStatus::kBudgetExceeded);
    EXPECT_LE(route->work.obstacles,256U);EXPECT_LE(route->work.predicates,65536U);
    EXPECT_LE(route->work.expanded,8192U);EXPECT_LE(route->work.examined,8192U);
}

TEST(FC05Route, FreeEndpointInsideObstacleReportsNoRoute) {
    const auto c=wire(50,50,200,200);
    const auto route=scene::connectorGeometry(c,{box(1,40,40,40,40)});
    ASSERT_TRUE(route);EXPECT_EQ(route->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);
}

TEST(FC05Route, ObstacleOrderDoesNotChangeRouteAndThirdObstacleRecomputes) {
    const auto c=wire(0,50,200,50);
    std::vector<semantic::ObjectRecord> obstacles{box(1,80,20,40,60),box(2,60,90,80,40)};
    const auto first=scene::connectorGeometry(c,obstacles);ASSERT_TRUE(first);
    EXPECT_EQ(first->status,runtime::ConnectorRouteStatus::kReady);avoids(*first,obstacles);
    std::reverse(obstacles.begin(),obstacles.end());
    const auto reverse=scene::connectorGeometry(c,obstacles);ASSERT_TRUE(reverse);EXPECT_EQ(*first,*reverse);
    obstacles.push_back(box(3,20,-20,20,40));
    const auto changed=scene::connectorGeometry(c,obstacles);ASSERT_TRUE(changed);
    EXPECT_EQ(changed->status,runtime::ConnectorRouteStatus::kReady);avoids(*changed,obstacles);
}

TEST(FC05Route, TriangleCardinalPortsAndEllipseAutoAnchorUseActualPerimeter) {
    auto t=box(1,0,0,200,120);t.kind_version=2;
    std::get<semantic::ShapeContent>(t.content).shape_kind=3;
    auto c=wire(0,0,300,60);auto& content=std::get<semantic::ConnectorContent>(c.content);
    content.routing=semantic::ConnectorRouting::kStraight;
    content.start.value=semantic::AttachedEndpoint{t.id,semantic::StablePortAnchor{2}};
    auto route=scene::connectorGeometry(c,{t});ASSERT_TRUE(route);
    EXPECT_FLOAT_EQ(route->start.x,150);EXPECT_FLOAT_EQ(route->start.y,60);
    t.kind_version=1;std::get<semantic::ShapeContent>(t.content).shape_kind=2;
    content.start.value=semantic::AttachedEndpoint{t.id,semantic::AutoPerimeterAnchor{semantic::Vec2{1,1}}};
    route=scene::connectorGeometry(c,{t});ASSERT_TRUE(route);
    const double dx=(route->start.x-100)/100,dy=(route->start.y-60)/60;
    EXPECT_NEAR(dx*dx+dy*dy,1.0,1e-6);
}

TEST(FC05Route, ArrowFollowsFinalSegmentAndBoundsIncludeHead) {
    scene::ConnectorGeometry g{{0,0},{100,100},{{0,0},{0,100},{100,100}},true};
    const auto head=scene::arrowHead(g);
    EXPECT_FLOAT_EQ(head.left.x,88);EXPECT_FLOAT_EQ(head.right.x,88);
    EXPECT_FLOAT_EQ(head.left.y,106);EXPECT_FLOAT_EQ(head.right.y,94);
    auto c=wire(0,0,100,100);
    c.properties.entries={{0x201U,semantic::ConnectorDecorationValue::kArrow}};
    const auto bounds=scene::computeBounds(c,nullptr,&g);
    EXPECT_GE(bounds.geometry.bottom,106);
}

TEST(FC05Route, ShortestLengthMatchesIndependentUnitGridOracle) {
    // Independent exhaustive integer grid, deliberately unlike the production
    // coordinate-compressed directional A* search. All fixture edges and the
    // eight-unit clearance are integral, so this is an exact length oracle.
    for(int offset=0;offset<8;++offset) {
        const int width=225,height=185,left=-64,top=-64;
        const auto c=wire(0,20,100,20);
        const std::vector<semantic::ObjectRecord> objects{box(1,40,offset-8,20,48)};
        const auto route=scene::connectorGeometry(c,objects);ASSERT_TRUE(route);
        ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);
        const foundation::WorldRect blocked{32.0F,static_cast<float>(offset-16),68.0F,static_cast<float>(offset+48)};
        const auto index=[&](int x,int y){return (y-top)*width+x-left;};
        std::vector<int> distances(static_cast<std::size_t>(width*height),-1);
        std::queue<std::pair<int,int>> queue;queue.push({0,20});distances[static_cast<std::size_t>(index(0,20))]=0;
        while(!queue.empty()) {
            const auto [x,y]=queue.front();queue.pop();
            if(x==100&&y==20)break;
            for(const auto [dx,dy]:std::array<std::pair<int,int>,4>{{{1,0},{0,1},{-1,0},{0,-1}}}) {
                const int nx=x+dx,ny=y+dy;
                if(nx<left||ny<top||nx>=left+width||ny>=top+height)continue;
                auto& d=distances[static_cast<std::size_t>(index(nx,ny))];
                if(d>=0||crosses({static_cast<float>(x),static_cast<float>(y)},
                    {static_cast<float>(nx),static_cast<float>(ny)},blocked))continue;
                d=distances[static_cast<std::size_t>(index(x,y))]+1;queue.push({nx,ny});
            }
        }
        double length=0;
        for(std::size_t i=1;i<route->route.size();++i)length+=
            std::abs(route->route[i].x-route->route[i-1].x)+std::abs(route->route[i].y-route->route[i-1].y);
        EXPECT_EQ(length,distances[static_cast<std::size_t>(index(100,20))]);
    }
}

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
