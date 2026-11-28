#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/connector_router.hpp"
#include "canvas/scene/corner_geometry.hpp"
#include "canvas/scene/sketch_geometry.hpp"

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

TEST(FC05Route, PointerPerimeterHintResolvesAtThePickedWorldPointUnderAffineTransforms) {
    const semantic::Transform2D pose{1.6,.55,-.3,1.3,210,105};
    const auto verify=[&](const semantic::ObjectRecord& object,semantic::Vec2 local,semantic::Vec2 tangent) {
        const auto& t=object.transform;
        const double x=t.a*local.x+t.c*local.y+t.tx,y=t.b*local.x+t.d*local.y+t.ty;
        const double dx=t.a*tangent.x+t.c*tangent.y,dy=t.b*tangent.x+t.d*tangent.y;
        for(double offset:{6.,-2.}) {
            const foundation::WorldPoint pointer{float(x+offset*dy/std::hypot(dx,dy)),float(y-offset*dx/std::hypot(dx,dy))};
            const auto hit=scene::nearestConnectorPerimeter(object,pointer);
            ASSERT_TRUE(hit && hit->anchor.hint);
            EXPECT_NEAR(hit->point.x,x,.025);EXPECT_NEAR(hit->point.y,y,.025);
            EXPECT_EQ(hit->inside,offset<0);
            const auto again=scene::connectorAnchorPoint(object,hit->anchor,{-800,900});
            ASSERT_TRUE(again);EXPECT_FLOAT_EQ(again->x,hit->point.x);EXPECT_FLOAT_EQ(again->y,hit->point.y);
        }
    };
    auto target=box(1,0,0,180,100);target.transform=pose;
    verify(target,{132,100},{-1,0});
    target.content=semantic::ShapeContent{2,180,100};
    const double angle=.73;
    verify(target,{90+90*std::cos(angle),50+50*std::sin(angle)},{-90*std::sin(angle),50*std::cos(angle)});
    target.kind_version=2;target.content=semantic::ShapeContent{3,180,100};
    verify(target,{144,60},{90,100});
    target.properties.entries={{0x103U,semantic::CornerStyleV1{1,12}}};
    verify(target,{129,100},{-1,0});
    target.kind_version=1;target.content=semantic::ShapeContent{1,180,100};
    const double diagonal=12/std::sqrt(2.);
    verify(target,{168+diagonal,88+diagonal},{-1,1});
    target.kind=semantic::ObjectKind::kSticky;target.content=semantic::StickyContent{180,100};
    verify(target,{150,100},{-1,0});
    target.kind=semantic::ObjectKind::kImage;
    semantic::ImageContent image;image.width=180;image.height=100;target.content=image;
    verify(target,{180,72},{0,1});
    target.transform={1,0,1,0,0,0};EXPECT_FALSE(scene::nearestConnectorPerimeter(target,{10,10}));
}

TEST(FC05Route, ClearParallelPortsUseTheMiddleLaneInOneQuantum) {
    for(bool vertical:{false,true}) {
        auto a=box(1,0,0,100,100),b=box(2,300,50,100,100);
        if(vertical)b.transform={1,0,0,1,50,300};
        auto c=wire(0,0,0,0);auto& cc=std::get<semantic::ConnectorContent>(c.content);
        cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{vertical?3U:2U}};
        cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{vertical?1U:4U}};
        const std::vector<semantic::ObjectRecord> objects{a,b};
        scene::ConnectorRouteJob job(c,objects);
        EXPECT_EQ(job.advance(),runtime::ConnectorRouteStatus::kReady);
        const auto route=scene::connectorGeometry(c,objects);ASSERT_TRUE(route);
        ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);
        ASSERT_EQ(route->route.size(),4U);
        if(vertical) {EXPECT_FLOAT_EQ(route->route[1].y,200);EXPECT_FLOAT_EQ(route->route[2].y,200);}
        else {EXPECT_FLOAT_EQ(route->route[1].x,200);EXPECT_FLOAT_EQ(route->route[2].x,200);}
        avoids(*route,objects);
    }
}

TEST(FC05Route, AlignedBranchesKeepTheirMidlineAcrossAnInterveningShape) {
    for(bool column:{false,true}) for(bool reflect:{false,true}) for(bool reverse:{false,true}) for(bool nearMiss:{false,true}) {
        SCOPED_TRACE(::testing::Message()<<"column="<<column<<" reflect="<<reflect<<" reverse="<<reverse<<" nearMiss="<<nearMiss);
        const auto rect=[=](unsigned id,double x,double y) {
            auto o=column ? box(id,y,x,80,120) : box(id,x,y,120,80);
            if(reflect){
                if(column){o.transform.a=-1;o.transform.tx=-o.transform.tx;}
                else{o.transform.d=-1;o.transform.ty=-o.transform.ty;}
            }
            return o;
        };
        const unsigned from=column?2:3, to=column?4:1;
        const auto hub=rect(1,0,0), center=rect(2,0,228), right=rect(3,180,228), far=rect(4,360,228);
        const auto upper=rect(5,194,nearMiss?69:103);
        const auto connect=[&](unsigned id,const semantic::ObjectRecord& target,bool flip){
            auto c=wire(0,0,0,0);c.id=foundation::ObjectId::fromUint64(id);
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{from}};
            cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{to}};
            if(flip)std::swap(cc.start,cc.end);
            c.properties.entries.push_back({0x201U,semantic::ConnectorDecorationValue::kArrow});
            return c;
        };
        auto reference=connect(10,center,!reverse);
        std::vector<semantic::ObjectRecord> objects{reference,upper,hub,center,right,far,connect(11,upper,false)};
        if(reverse)std::reverse(objects.begin(),objects.end());
        for(const auto& target:{right,far}){
            const auto c=connect(12,target,reverse);
            const auto g=scene::connectorGeometry(c,objects);
            ASSERT_TRUE(g);ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);ASSERT_FALSE(g->showsWarning());
            auto p=g->route;if(reverse)std::reverse(p.begin(),p.end());
            if(column)for(auto& point:p)std::swap(point.x,point.y);
            if(reflect)for(auto& point:p)point.y=-point.y;
            ASSERT_EQ(p.size(),4U);EXPECT_EQ(p[1].y,154);EXPECT_EQ(p[2].y,154);
            EXPECT_EQ(p[1].x,p[0].x);EXPECT_EQ(p[2].x,p.back().x);
            const auto head=scene::arrowHead(scene::connectorDisplayRoute(c,*g));
            EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
            bool bodyCrossing=false;
            for(std::size_t i=1;i<g->route.size();++i)
                bodyCrossing|=crosses(g->route[i-1],g->route[i],scene::computeBounds(upper).world);
            EXPECT_EQ(bodyCrossing,!nearMiss);
        }
        // Dropping a fixed port underneath another body continues to warn.
        auto occluded=objects;occluded.push_back(rect(16,174,205));
        const auto g=scene::connectorGeometry(connect(12,right,reverse),occluded);
        ASSERT_TRUE(g);EXPECT_EQ(g->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);
    }
}

TEST(FC05Route, ASharedPortAloneDoesNotEstablishAnAlignedBranchRow) {
    const auto a=box(1,0,0,120,80),b=box(2,180,228,120,80),blocker=box(3,194,103,120,80);
    auto c=wire(0,0,0,0);auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    const auto peer=box(4,-3600,228,120,80);
    auto other=c;other.id=foundation::ObjectId::fromUint64(15);
    std::get<semantic::ConnectorContent>(other.content).end.value=semantic::AttachedEndpoint{peer.id,semantic::StablePortAnchor{1}};
    const auto hasMidline=[&](const auto& graph){
        const auto g=scene::connectorGeometry(c,graph);
        return g && g->status==runtime::ConnectorRouteStatus::kReady && g->route.size()==4 && g->route[1].y==154;
    };
    std::vector<semantic::ObjectRecord> graph{b,other,a,blocker,peer};
    EXPECT_TRUE(hasMidline(graph))<<"a peer outside the search area still belongs to the same board row";
    graph.back().transform.ty+=5; EXPECT_FALSE(hasMidline(graph));
    graph.back()=peer;graph.back().properties.entries={{1U,false}};EXPECT_FALSE(hasMidline(graph));
    graph.back()=peer;graph[1].properties.entries={{1U,false}};EXPECT_FALSE(hasMidline(graph));
    graph[1]=other;std::get<semantic::ConnectorContent>(graph[1].content).routing=semantic::ConnectorRouting::kStraight;
    EXPECT_FALSE(hasMidline(graph));
    graph[1]=c;graph[1].id=other.id;EXPECT_FALSE(hasMidline(graph))<<"a duplicate wire has only one destination";
}

TEST(FC05Route, TheElbowInsideAPiercedBranchObjectKeepsItsRoundCorner) {
    const auto a=box(1,0,0,120,80),b=box(2,180,228,120,80),other=box(3,0,228,120,80);
    const auto crossed=box(4,194,103,120,80),untouched=box(5,400,20,60,30);
    auto c=wire(0,0,0,0);auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    auto sibling=c;sibling.id=foundation::ObjectId::fromUint64(20);
    std::get<semantic::ConnectorContent>(sibling.content).end.value=
        semantic::AttachedEndpoint{other.id,semantic::StablePortAnchor{1}};
    const auto g=scene::connectorGeometry(c,{a,b,other,crossed,untouched,sibling});
    ASSERT_TRUE(g);ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
    const auto visible=scene::connectorDisplayRoute(c,*g);
    std::vector<semantic::Vec2> points;
    for(auto p:visible.route)points.push_back({p.x,p.y});
    const auto path=scene::roundedPolyline(points,false,4,visible.sketchObstacles);
    EXPECT_EQ(std::count_if(path.commands.begin(),path.commands.end(),[](const auto& c){
        return std::holds_alternative<semantic::CubicTo>(c);
    }),2);
    EXPECT_EQ(g->work.obstacles,5U);
    EXPECT_EQ(visible.sketchObstacles.size(),4U)<<"both terminals and the untouched obstacles stay protected";
}

TEST(FC05Route, OppositePortsKeepFullArrowheadsInAnOffsetGutterAndAroundTouchingBodies) {
    auto c=wire(0,0,0,0);
    c.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}},
                          {0x200U,semantic::ConnectorDecorationValue::kArrow},
                          {0x201U,semantic::ConnectorDecorationValue::kArrow}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    for (bool transpose : {false,true}) for (bool touching : {false,true}) {
        for (double delta : {-.25,0.,.25,24.}) for (bool reverse : {false,true}) {
            SCOPED_TRACE(::testing::Message()<<"transpose="<<transpose<<" touching="<<touching
                         <<" offset="<<delta<<" reverse="<<reverse);
            // Use unequal widths so the open channel is centered on the free
            // space between the bodies, independently of their port centers.
            const double bx=touching ? 180 : 276, by=(touching ? 0 : 110)+delta;
            const auto rect=[&](unsigned id,double x,double y,double w,double h) {
                return transpose ? box(id,y,x,h,w) : box(id,x,y,w,h);
            };
            const auto a=rect(1,0,0,180,110), b=rect(2,bx,by,120,110);
            cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{transpose?2U:3U}};
            cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{transpose?4U:1U}};
            if (reverse) std::swap(cc.start,cc.end);
            std::vector<semantic::ObjectRecord> objects{b,a};
            const auto ready=scene::connectorGeometry(c,objects);
            ASSERT_TRUE(ready);ASSERT_EQ(ready->status,runtime::ConnectorRouteStatus::kReady);
            avoids(*ready,objects);
            auto p=ready->route;
            if (reverse) std::reverse(p.begin(),p.end());
            if (transpose) for(auto& point:p)std::swap(point.x,point.y);
            ASSERT_EQ(p.size(),6U);
            EXPECT_GE(p[1].y-p[0].y,32);
            EXPECT_GE(p[5].y-p[4].y,32);
            EXPECT_EQ(p[2].x,p[3].x);
            if(touching) EXPECT_TRUE(p[2].x<0 || p[2].x>bx+120);
            else EXPECT_EQ(p[2].x,(180+bx)/2);
            const auto visible=scene::connectorDisplayRoute(c,*ready);
            for(bool start:{true,false}) {
                const auto head=scene::arrowHead(visible,start);
                EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
            }
            for(unsigned n=0;n<1100;++n)objects.push_back(box(10+n,50000+n*20,50000,6,6));
            const auto pending=scene::connectorGeometry(c,objects,true);
            ASSERT_TRUE(pending);ASSERT_EQ(pending->status,runtime::ConnectorRouteStatus::kPending);
            ASSERT_EQ(pending->route.size(),ready->route.size());
            for(std::size_t i=0;i<p.size();++i) {
                EXPECT_FLOAT_EQ(pending->route[i].x,ready->route[i].x);
                EXPECT_FLOAT_EQ(pending->route[i].y,ready->route[i].y);
            }
        }
    }
}

TEST(FC05Route, BlockedOppositePortLaneKeepsBothLeadsOnTheAvailableSide) {
    const auto a=box(1,0,0,180,110),b=box(2,180,0,180,110);
    auto c=wire(0,0,0,0);
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    for(bool blockLeft : {false,true}) {
        const auto blocker=box(3,blockLeft ? -40 : 350,22,50,66);
        const std::vector<semantic::ObjectRecord> objects{a,b,blocker};
        const auto ready=scene::connectorGeometry(c,objects);
        ASSERT_TRUE(ready);ASSERT_EQ(ready->status,runtime::ConnectorRouteStatus::kReady);
        avoids(*ready,objects);
        const auto& p=ready->route;ASSERT_EQ(p.size(),6U);
        EXPECT_GE(p[1].y-p[0].y,32);
        EXPECT_GE(p[5].y-p[4].y,32);
        EXPECT_TRUE(blockLeft ? p[2].x>360 : p[2].x<0);
    }
}

TEST(FC05Route, DiagonallyAdjacentCornersKeepFullArrowsInTheLocalPassage) {
    auto c=wire(0,0,0,0);
    const auto stroke=semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}};
    c.properties.entries={{0x101U,stroke},{0x200U,semantic::ConnectorDecorationValue::kArrow},
                          {0x201U,semantic::ConnectorDecorationValue::kArrow}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    for(bool transpose:{false,true}) for(bool left:{false,true}) for(bool reverse:{false,true}) {
        for(double offset:{-12.,0.,1.,6.}) for(double gap:{12.,16.,24.,48.}) {
            SCOPED_TRACE(::testing::Message()<<"transpose="<<transpose<<" left="<<left
                         <<" reverse="<<reverse<<" offset="<<offset<<" gap="<<gap);
            const auto rect=[&](unsigned id,double x,double y) {
                auto r=transpose ? box(id,y,x,120,176) : box(id,x,y,176,120);
                r.properties.entries={{0x101U,stroke}};return r;
            };
            const auto a=rect(1,0,0), b=rect(2,(left?-1:1)*(176+offset),120+gap);
            cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{transpose?2U:3U}};
            cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{transpose?4U:1U}};
            if(reverse)std::swap(cc.start,cc.end);
            std::vector<semantic::ObjectRecord> objects{a,b};
            scene::ConnectorRouteJob job(c,objects);
            ASSERT_EQ(job.advance(),runtime::ConnectorRouteStatus::kReady);
            const auto& g=job.result();
            const auto visible=scene::connectorDisplayRoute(c,g);
            avoids(visible,objects);
            auto p=g.route;
            if(reverse)std::reverse(p.begin(),p.end());
            if(transpose)for(auto& point:p)std::swap(point.x,point.y);
            ASSERT_GE(p.size(),4U);ASSERT_LE(p.size(),8U);
            EXPECT_GE(p[1].y-p.front().y,32);
            EXPECT_GE(p.back().y-p[p.size()-2].y,32);
            for(const auto point:p) {
                EXPECT_GE(point.x,std::min(p.front().x,p.back().x));
                EXPECT_LE(point.x,std::max(p.front().x,p.back().x));
                EXPECT_GE(point.y,std::min(p.front().y,p.back().y-32));
                EXPECT_LE(point.y,std::max(p.back().y,p.front().y+32));
            }
            for(bool start:{true,false}) {
                const auto head=scene::arrowHead(visible,start);
                EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
            }
            for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            const auto pending=scene::connectorGeometry(c,objects,true);
            ASSERT_TRUE(pending);ASSERT_EQ(pending->status,runtime::ConnectorRouteStatus::kPending);
            ASSERT_EQ(pending->route.size(),g.route.size());
            for(std::size_t i=0;i<g.route.size();++i) {
                EXPECT_FLOAT_EQ(pending->route[i].x,g.route[i].x);
                EXPECT_FLOAT_EQ(pending->route[i].y,g.route[i].y);
            }
        }
    }
}

TEST(FC05Route, DiagonalCornerPassageChecksObstaclesAndGenuinelyConfinedTerminals) {
    auto a=box(1,0,0,176,120), b=box(2,177,136,176,120), c=wire(0,0,0,0);
    const auto stroke=semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}};
    a.properties.entries=b.properties.entries=c.properties.entries={{0x101U,stroke}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    const auto blocker=box(3,125,128,10,8);
    const auto g=scene::connectorGeometry(c,{a,b,blocker});
    ASSERT_TRUE(g); ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
    const auto& p=g->route; ASSERT_GE(p.size(),4U);
    EXPECT_GE(p[1].y-p.front().y,32); EXPECT_GE(p.back().y-p[p.size()-2].y,32);
    for(std::size_t i=1;i<p.size();++i)
        EXPECT_FALSE(crosses(p[i-1],p[i],{116,119,144,145}));

    // Both port rays now face an actual painted body; the only nearby
    // horizontal passage is halfway through the 16-unit gap.
    b.transform.tx=12;
    const auto narrow=scene::connectorGeometry(c,{a,b});
    ASSERT_TRUE(narrow); ASSERT_EQ(narrow->status,runtime::ConnectorRouteStatus::kReady);
    ASSERT_EQ(narrow->route.size(),4U);
    EXPECT_FLOAT_EQ(narrow->route[1].y,128); EXPECT_FLOAT_EQ(narrow->route[2].y,128);
}

TEST(FC05Route, OppositePortsUseTheAvailableApproachBeforeShorteningAnArrow) {
    const auto a=box(1,0,0,100,100), b=box(2,200,100,100,100);
    auto c=wire(0,0,0,0);
    c.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}},
                          {0x201U,semantic::ConnectorDecorationValue::kArrow}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    cc.end.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    // One object caps the last approach at 29; the other covers the middle
    // of the gutter. There is room for the full arrow and a clear side lane.
    const std::vector<semantic::ObjectRecord> objects{a,b,box(3,16,138,65,14),box(4,148,88,4,24)};
    const auto ready=scene::connectorGeometry(c,objects);
    ASSERT_TRUE(ready);ASSERT_EQ(ready->status,runtime::ConnectorRouteStatus::kReady);
    const auto& p=ready->route;ASSERT_EQ(p.size(),6U);
    EXPECT_EQ(p[4].y-p[5].y,29);
    EXPECT_GE(p[0].y-p[1].y,32);
    EXPECT_TRUE(p[2].x==139 || p[2].x==161);
    avoids(*ready,objects);
    const auto head=scene::arrowHead(scene::connectorDisplayRoute(c,*ready));
    EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
}

TEST(FC05Route, PerpendicularPortsUseTheOpenGutterInEitherDirection) {
    const auto source = box(1, 50, 97, 150, 150);
    const auto target = box(2, 432, 147, 150, 150);
    const std::vector<semantic::ObjectRecord> objects{source, target};
    auto c = wire(0, 0, 0, 0);
    auto& content = std::get<semantic::ConnectorContent>(c.content);
    content.start.value = semantic::AttachedEndpoint{source.id, semantic::StablePortAnchor{1}};
    content.end.value = semantic::AttachedEndpoint{target.id, semantic::StablePortAnchor{4}};

    for (bool reverse : {false, true}) {
        if (reverse) std::swap(content.start, content.end);
        scene::ConnectorRouteJob job(c, objects);
        ASSERT_EQ(job.advance(), runtime::ConnectorRouteStatus::kReady);
        auto g = scene::connectorDisplayRoute(c, job.result());
        if (reverse) std::reverse(g.route.begin(), g.route.end());
        ASSERT_EQ(g.route.size(), 5U);
        EXPECT_GE(g.route[0].y - g.route[1].y, 16);
        EXPECT_NEAR(g.route[2].x, (200 + 432) / 2., 1);
        EXPECT_GE(g.route[4].x - g.route[3].x, 24);
        avoids(g, objects);

        // A third object covers the preferred trunk. The certified fallback must
        // keep clear of both the object and its eight-unit clearance envelope.
        const auto blocker = box(3, 300, 90, 40, 100);
        const std::vector<semantic::ObjectRecord> blocked{target, blocker, source};
        const auto fallback = scene::connectorGeometry(c, blocked);
        ASSERT_TRUE(fallback);
        ASSERT_EQ(fallback->status, runtime::ConnectorRouteStatus::kReady);
        for (std::size_t i = 1; i < fallback->route.size(); ++i)
            EXPECT_FALSE(crosses(fallback->route[i-1], fallback->route[i], {292,82,348,198}));

        // When the specified sides already face a single natural elbow, keep it.
        auto facing = objects;
        facing.front().transform.ty = 360;
        const auto simple = scene::connectorGeometry(c, facing);
        ASSERT_TRUE(simple);
        ASSERT_EQ(simple->status, runtime::ConnectorRouteStatus::kReady);
        EXPECT_EQ(simple->route.size(), 3U);
        avoids(*simple, facing);
    }
}

TEST(FC05Route, OutwardPerpendicularConnectionsUseSeparateOuterSides) {
    const std::array<semantic::Transform2D,4> poses{{
        {1,0,0,1,0,0},{-1,0,0,1,0,0},{0,1,1,0,0,0},{0,1,-1,0,0,0}}};
    for(const auto& t:poses) for(bool lower:{false,true}) for(bool reverse:{false,true}) {
        for(double height:{80.,224.}) {
            SCOPED_TRACE(::testing::Message()<<"lower="<<lower<<" reverse="<<reverse<<" height="<<height);
            auto a=box(1,0,0,120,height), b=box(2,349.5,61.5,120,height);
            a.transform=t; b.transform=t;
            b.transform.tx=t.a*349.5+t.c*61.5; b.transform.ty=t.b*349.5+t.d*61.5;
            auto c=wire(0,0,0,0);
            c.properties.entries={{0x200U,semantic::ConnectorDecorationValue::kArrow},
                                  {0x201U,semantic::ConnectorDecorationValue::kArrow}};
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{lower?3U:4U}};
            cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{lower?2U:1U}};
            if(reverse)std::swap(cc.start,cc.end);
            std::vector<semantic::ObjectRecord> objects{a,b};
            const auto g=scene::connectorGeometry(c,objects);
            ASSERT_TRUE(g); ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
            avoids(*g,objects);
            ASSERT_EQ(g->route.size(),5U)<<"three bends suffice to reach the outward port";
            auto p=g->route;
            if(reverse)std::reverse(p.begin(),p.end());
            for(auto& v:p) {
                EXPECT_GE(v.x,g->searchedDomain.left); EXPECT_LE(v.x,g->searchedDomain.right);
                EXPECT_GE(v.y,g->searchedDomain.top); EXPECT_LE(v.y,g->searchedDomain.bottom);
                v={float(t.a*v.x+t.b*v.y),float(t.c*v.x+t.d*v.y)};
            }
            if(lower) {
                EXPECT_EQ(p[1].x,p.front().x); EXPECT_EQ(p[1].y,p[2].y);
                EXPECT_GE(p[1].y,61.5+height+16);
                EXPECT_EQ(p[2].x,p[3].x); EXPECT_GE(p[3].x-p.back().x,32);
            } else {
                EXPECT_GE(p.front().x-p[1].x,32); EXPECT_EQ(p[1].x,p[2].x);
                EXPECT_LE(p[2].y,-16); EXPECT_EQ(p[2].y,p[3].y);
                EXPECT_EQ(p[3].x,p.back().x);
            }
            const auto visible=scene::connectorDisplayRoute(c,*g);
            for(bool start:{false,true}) {
                const auto head=scene::arrowHead(visible,start);
                EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
            }
            for(unsigned n=0;n<1100;++n)objects.push_back(box(10+n,50000+n*20,50000,6,6));
            const auto preview=scene::connectorGeometry(c,objects,true);
            ASSERT_TRUE(preview);ASSERT_EQ(preview->status,runtime::ConnectorRouteStatus::kPending);
            ASSERT_EQ(preview->route.size(),g->route.size());
            for(std::size_t i=0;i<p.size();++i){
                EXPECT_EQ(preview->route[i].x,g->route[i].x);EXPECT_EQ(preview->route[i].y,g->route[i].y);
            }
        }
    }
}

TEST(FC05Route, OuterPerpendicularLanesCanClearAnotherObjectOnTheirPreferredSide) {
    const auto a=box(1,0,0,120,80), b=box(2,350,62,120,80);
    auto c=wire(0,0,0,0);
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    for(bool lower:{false,true}) {
        cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{lower?3U:4U}};
        cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{lower?2U:1U}};
        const auto blocker=box(3,170,lower?157:-42,64,22);
        const std::vector<semantic::ObjectRecord> objects{blocker,b,a};
        const auto g=scene::connectorGeometry(c,objects);
        ASSERT_TRUE(g);ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
        avoids(*g,objects);ASSERT_EQ(g->route.size(),5U);
        if(lower) EXPECT_GE(g->route[2].y,187); else EXPECT_LE(g->route[2].y,-50);
    }
}

TEST(FC05Route, PassingAboveAFaceLeavesItsOwnOutgoingPortLaneReadable) {
    const auto a=box(1,0,0,120,80), b=box(2,350,62,120,80);
    const std::vector<semantic::ObjectRecord> objects{a,b};
    auto c=wire(0,0,0,0);
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{4}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    const auto around=scene::connectorGeometry(c,objects);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{1}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{2}};
    const auto fromTop=scene::connectorGeometry(c,objects);
    ASSERT_TRUE(around && fromTop);
    ASSERT_EQ(around->route.size(),5U);ASSERT_EQ(fromTop->route.size(),5U);
    EXPECT_LE(around->route[2].y,-16);
    EXPECT_GE(around->route[2].y-fromTop->route[2].y,12);
}

TEST(FC05Route, CloseAttachedTargetsShareOnlyTheirClearFacingChannel) {
    const auto shapeStroke=semantic::StrokeStyleValue{semantic::SolidStroke{
        {0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}};
    auto a=box(1,0,0,100,100),b=box(2,108,0,100,100);
    a.properties.entries=b.properties.entries={{0x101U,shapeStroke}};
    auto c=wire(0,0,0,0);
    c.properties.entries={{0x101U,shapeStroke}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    for(bool transpose : {false,true}) {
        if(transpose) b.transform={1,0,0,1,0,108};
        cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{transpose?3U:2U}};
        cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{transpose?4U:1U}};
        for(bool reverse : {false,true}) {
            if(reverse)std::swap(cc.start,cc.end);
            const std::vector<semantic::ObjectRecord> source{a,b};
            const auto g=scene::connectorGeometry(c,source);
            ASSERT_TRUE(g);
            ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
            auto points=g->route;
            if(reverse)std::reverse(points.begin(),points.end());
            if(transpose)for(auto& p:points)std::swap(p.x,p.y);
            ASSERT_GE(points.size(),4U);
            EXPECT_FLOAT_EQ(points.front().x,100);
            EXPECT_FLOAT_EQ(points.back().x,158);
            EXPECT_FLOAT_EQ(points.back().y,0);
            EXPECT_TRUE(std::any_of(points.begin()+1,points.end(),[](auto p){return p.x==104 && p.y<0;}));
            for(std::size_t n=1;n<points.size();++n){
                EXPECT_FALSE(crosses(points[n-1],points[n],{0,0,100,100}));
                EXPECT_FALSE(crosses(points[n-1],points[n],{108,0,208,100}));
            }
            auto many=source;
            for(unsigned n=0;n<1500;++n)many.push_back(box(20+n,100000+n*20,100000,10,10));
            const auto preview=scene::connectorGeometry(c,many,true);
            ASSERT_TRUE(preview);ASSERT_EQ(preview->status,runtime::ConnectorRouteStatus::kPending);
            ASSERT_EQ(preview->route.size(),g->route.size());
            for(std::size_t n=0;n<g->route.size();++n){
                EXPECT_FLOAT_EQ(preview->route[n].x,g->route[n].x);
                EXPECT_FLOAT_EQ(preview->route[n].y,g->route[n].y);
            }
        }
    }
}

TEST(FC05Route, ClosedChannelRoutesOutsideOrReportsAnOccludedFixedPort) {
    auto a=box(1,0,0,100,100),b=box(2,100,0,100,100),c=wire(0,0,0,0);
    const auto stroke=semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}};
    a.properties.entries=b.properties.entries=c.properties.entries={{0x101U,stroke}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
    const auto around=scene::connectorGeometry(c,{a,b});
    ASSERT_TRUE(around);ASSERT_EQ(around->status,runtime::ConnectorRouteStatus::kReady);
    EXPECT_TRUE(std::any_of(around->route.begin(),around->route.end(),[](auto p){return p.x<0 || p.x>200;}));
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{2}};
    for(double x:{100.,98.}) {
        b.transform.tx=x;
        const auto blocked=scene::connectorGeometry(c,{a,b});
        ASSERT_TRUE(blocked);EXPECT_EQ(blocked->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);
    }
    b.transform.tx=108;
    // The two outlines leave six units, too little for a ten-unit turn.
    c.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},10}}}};
    const auto thick=scene::connectorGeometry(c,{a,b});
    ASSERT_TRUE(thick);EXPECT_EQ(thick->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);
}

TEST(FC05Route, ShortFacingBridgeStillChecksThirdObstaclesAndPaintedEdges) {
    auto a=box(1,0,0,100,100),b=box(2,103,0,100,100),c=wire(0,0,0,0);
    const auto stroke=semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}};
    a.properties.entries=b.properties.entries=c.properties.entries={{0x101U,stroke}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    for(bool automatic:{false,true}) {
        cc.start.value=semantic::AttachedEndpoint{a.id,automatic?semantic::AnchorRef{semantic::AutoPerimeterAnchor{}}:semantic::AnchorRef{semantic::StablePortAnchor{2}}};
        cc.end.value=semantic::AttachedEndpoint{b.id,automatic?semantic::AnchorRef{semantic::AutoPerimeterAnchor{}}:semantic::AnchorRef{semantic::StablePortAnchor{4}}};
        const auto straight=scene::connectorGeometry(c,{a,b});
        ASSERT_TRUE(straight);ASSERT_EQ(straight->status,runtime::ConnectorRouteStatus::kReady);
        ASSERT_EQ(straight->route.size(),2U);
        EXPECT_FLOAT_EQ(straight->start.x,100);EXPECT_FLOAT_EQ(straight->end.x,103);
        const auto blocked=scene::connectorGeometry(c,{b,box(3,102,55,2,5),a});
        ASSERT_TRUE(blocked);EXPECT_EQ(blocked->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);
        b.transform.tx=101;
        const auto paintedOverlap=scene::connectorGeometry(c,{b,a});
        ASSERT_TRUE(paintedOverlap);EXPECT_EQ(paintedOverlap->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);
        b.transform.tx=103;
    }
}

TEST(FC05Route, VisibleAttachedEndsLeaveFourBoardUnitsAndShortSegmentsKeepTheirDirection) {
    auto c=wire(0,0,0,0);
    c.transform={2,0,0,.5,17,28};
    auto& content=std::get<semantic::ConnectorContent>(c.content);
    content.start.value=semantic::AttachedEndpoint{foundation::ObjectId::fromUint64(1),semantic::StablePortAnchor{2}};
    content.end.value=semantic::AttachedEndpoint{foundation::ObjectId::fromUint64(2),semantic::StablePortAnchor{1}};
    scene::ConnectorGeometry route;
    route.start={0,0};route.end={20,60};route.route={route.start,{20,0},route.end};
    const auto visible=scene::connectorDisplayRoute(c,route);
    EXPECT_FLOAT_EQ(2*visible.route.front().x,4);
    EXPECT_FLOAT_EQ(.5F*(route.end.y-visible.route.back().y),4);
    route.route[1].x=2;
    EXPECT_FLOAT_EQ(scene::connectorDisplayRoute(c,route).route.front().x,.5F);
    content.start.value=semantic::FreePointEndpoint{{0,0}};
    EXPECT_FLOAT_EQ(scene::connectorDisplayRoute(c,route).route.front().x,0);
}

TEST(FC05Route, SameSidePortsKeepBothElbowsClearAndPendingFollowsThePortDirections) {
    const auto a = box(1, 50, 97, 150, 150), b = box(2, 432, 147, 150, 150);
    auto c = wire(0,0,0,0);
    auto& content = std::get<semantic::ConnectorContent>(c.content);
    content.start.value = semantic::AttachedEndpoint{a.id, semantic::StablePortAnchor{1}};
    for (std::uint32_t targetPort : {1U,4U}) {
        content.end.value = semantic::AttachedEndpoint{b.id, semantic::StablePortAnchor{targetPort}};
        std::vector<semantic::ObjectRecord> objects{a,b};
        const auto ready = scene::connectorGeometry(c, objects);
        ASSERT_TRUE(ready);
        ASSERT_EQ(ready->status, runtime::ConnectorRouteStatus::kReady);
        const auto visible = scene::connectorDisplayRoute(c, *ready);
        ASSERT_GE(visible.route.size(), 4U);
        EXPECT_GE(visible.route[0].y - visible.route[1].y, 16);
        if (targetPort == 1U) {
            EXPECT_EQ(visible.route.size(), 4U);
            EXPECT_EQ(visible.route[1].y, visible.route[2].y);
        }
        for (unsigned n=0; n<1500; ++n) objects.push_back(box(10+n,100000+n*20,100000,10,10));
        const auto pending = scene::connectorGeometry(c, objects, true);
        ASSERT_TRUE(pending);
        EXPECT_EQ(pending->status, runtime::ConnectorRouteStatus::kPending);
        ASSERT_EQ(pending->route.size(), ready->route.size());
        for (std::size_t i=0; i<ready->route.size(); ++i) {
            EXPECT_EQ(pending->route[i].x, ready->route[i].x);
            EXPECT_EQ(pending->route[i].y, ready->route[i].y);
        }
    }
}

TEST(FC05Route, AligningSameFacingPortsPreservesBothOutwardLegsAndArrowheads) {
    const auto a=box(1,0,0,120,80);
    auto c=wire(0,0,0,0);
    c.properties.entries={{0x200U,semantic::ConnectorDecorationValue::kArrow},
                          {0x201U,semantic::ConnectorDecorationValue::kArrow}};
    for (unsigned port : {1U,2U,3U,4U}) {
        const bool vertical=port==1 || port==3;
        const float outward=port==1 || port==4 ? -1.f : 1.f;
        const auto axis=[vertical](auto point){return vertical ? point.y : point.x;};
        for (double offset : {-8.,-.25,-1./1024,0.,1./1024,.25,8.}) {
            SCOPED_TRACE(::testing::Message()<<"port "<<port<<", offset "<<offset);
            auto b=box(2,vertical ? 300. : offset,vertical ? offset : 300.,120,80);
            std::vector<semantic::ObjectRecord> objects{a,b};
            // Also cover the bounded preview before the obstacle scan finishes.
            for(unsigned i=0;i<1200;++i)objects.push_back(box(10+i,50000+i*50,50000,6,6));
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{port}};
            cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{port}};
            if(offset<0)std::swap(cc.start,cc.end);
            const auto pending=scene::connectorGeometry(c,objects,true);
            const auto ready=scene::connectorGeometry(c,objects);
            ASSERT_TRUE(pending && ready);
            ASSERT_EQ(pending->status,runtime::ConnectorRouteStatus::kPending);
            ASSERT_EQ(ready->status,runtime::ConnectorRouteStatus::kReady);
            ASSERT_EQ(ready->route.size(),4U);
            ASSERT_EQ(pending->route.size(),ready->route.size());
            for(std::size_t i=0;i<ready->route.size();++i) {
                EXPECT_FLOAT_EQ(pending->route[i].x,ready->route[i].x);
                EXPECT_FLOAT_EQ(pending->route[i].y,ready->route[i].y);
            }
            const auto& path=ready->route;
            EXPECT_GE(outward*(axis(path[1])-axis(path[0])),32.f);
            EXPECT_GE(outward*(axis(path[2])-axis(path[3])),32.f);
            EXPECT_FLOAT_EQ(axis(path[1]),axis(path[2]));
            const auto visible=scene::connectorDisplayRoute(c,*ready);
            for(bool atStart:{true,false}) {
                const auto head=scene::arrowHead(visible,atStart);
                EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12.f);
            }
        }
    }
}

TEST(FC05Route, SameFacingPortsUseAvailableSpaceWhenAnObstacleBlocksThePreferredLane) {
    const auto a=box(1,0,0,100,100), b=box(2,300,0,100,100);
    auto c=wire(0,0,0,0);
    c.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}},
                          {0x200U,semantic::ConnectorDecorationValue::kArrow},
                          {0x201U,semantic::ConnectorDecorationValue::kArrow}};
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{3}};
    for(bool roof : {false,true}) {
        // A central blocker leaves the outer lane open. A roof across both
        // stems leaves 29 units of clearance, still enough for full arrowheads.
        const auto obstacle=roof ? box(3,-35,138,470,15) : box(3,160,124,40,12);
        const foundation::WorldRect clearance=roof ? foundation::WorldRect{-44,129,444,162}
                                                   : foundation::WorldRect{151,115,209,145};
        // Exact quarter turns place the obstacle on each of the four sides.
        const std::array<semantic::Transform2D,4> turns{{
            {1,0,0,1,0,0},{0,1,-1,0,0,0},{-1,0,0,-1,0,0},{0,-1,1,0,0,0}}};
        for(const auto& t : turns) {
            const auto rotate=[&](semantic::ObjectRecord object){
                const auto x=object.transform.tx,y=object.transform.ty;
                object.transform={t.a,t.b,t.c,t.d,t.a*x+t.c*y,t.b*x+t.d*y};
                return object;
            };
            const auto local=[&](foundation::WorldPoint p)->foundation::WorldPoint {
                return {float(t.a*p.x+t.b*p.y),float(t.c*p.x+t.d*p.y)};
            };
            const auto g=scene::connectorGeometry(c,{rotate(obstacle),rotate(b),rotate(a)});
            ASSERT_TRUE(g);ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
            ASSERT_EQ(g->route.size(),4U);
            EXPECT_FLOAT_EQ(local(g->route[1]).y,roof ? 129.f : 145.f);
            EXPECT_FLOAT_EQ(local(g->route[1]).y,local(g->route[2]).y);
            for(std::size_t i=1;i<g->route.size();++i)
                EXPECT_FALSE(crosses(local(g->route[i-1]),local(g->route[i]),clearance));
            const auto visible=scene::connectorDisplayRoute(c,*g);
            for(bool start : {false,true}) {
                const auto head=scene::arrowHead(visible,start);
                EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12.f);
            }
        }
    }
}

TEST(FC05Route, AnObstacleBesideTheMiddleOfARouteDoesNotShrinkAnExposedPortArrow) {
    for (bool vertical:{false,true}) for (bool mirror:{false,true}) for (bool reverse:{false,true})
        for (double offset:{0.,5.}) {
            SCOPED_TRACE(::testing::Message()<<"vertical="<<vertical<<" mirror="<<mirror
                <<" reverse="<<reverse<<" obstacle offset="<<offset);
            const auto make=[=](unsigned id,double x,double y) {
                auto r=vertical ? box(id,y,x,80,120) : box(id,x,y,120,80);
                if(mirror) {
                    if(vertical) {r.transform.d=-1;r.transform.ty=-r.transform.ty;}
                    else {r.transform.a=-1;r.transform.tx=-r.transform.tx;}
                }
                r.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{
                    {0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}}}};
                return r;
            };
            const auto hub=make(1,0,0), upper=make(2,382.5+offset,50.5), lower=make(3,238.5,177);
            auto c=wire(0,0,0,0);c.properties=hub.properties;
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            const unsigned port=vertical?3U:2U;
            cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{port}};
            cc.end.value=semantic::AttachedEndpoint{lower.id,semantic::StablePortAnchor{port}};
            if(reverse)std::swap(cc.start,cc.end);
            c.properties.entries.push_back({reverse?0x200U:0x201U,semantic::ConnectorDecorationValue::kArrow});
            const std::vector<semantic::ObjectRecord> objects{hub,upper,lower};
            const auto g=scene::connectorGeometry(c,objects);
            ASSERT_TRUE(g);ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
            const auto display=scene::connectorDisplayRoute(c,*g);
            avoids(display,objects);
            const auto head=scene::arrowHead(display,reverse);
            EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
            const auto tip=reverse?g->route.front():g->route.back();
            const auto bend=reverse?g->route[1]:g->route[g->route.size()-2];
            const double outward=vertical ? bend.y-tip.y : bend.x-tip.x;
            EXPECT_GE((mirror?-1.:1.)*outward,20)<<"open space for tip, bend and anchor gap";
            EXPECT_EQ(vertical?bend.x:bend.y,vertical?tip.x:tip.y);
        }
}

TEST(FC05Route, TwoArrowsAtOneSidePortShareATipAndUseTheClearHorizontalRun) {
    for(bool reverse : {false,true}) for(double top : {50.5,47.}) {
        const auto rect=[](unsigned id,double x,double y) {
            auto r=box(id,x,y,120,80);
            r.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{
                {0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}}}};
            return r;
        };
        const auto hub=rect(1,0,0), upper=rect(2,382.5,top), lower=rect(3,238.5,177);
        const auto link=[&](unsigned id,unsigned origin) {
            auto c=wire(0,0,0,0);c.id=foundation::ObjectId::fromUint64(id);c.properties=hub.properties;
            c.properties.entries.push_back({reverse?0x200U:0x201U,semantic::ConnectorDecorationValue::kArrow});
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{origin}};
            cc.end.value=semantic::AttachedEndpoint{upper.id,semantic::StablePortAnchor{2}};
            if(reverse)std::swap(cc.start,cc.end);
            return c;
        };
        const auto outer=link(4,1), middle=link(5,2);
        const std::vector<semantic::ObjectRecord> objects{hub,upper,lower,outer,middle};
        const auto reference=scene::connectorGeometry(outer,objects),g=scene::connectorGeometry(middle,objects);
        ASSERT_TRUE(reference && g);
        ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
        auto visible=scene::connectorDisplayRoute(middle,*g);
        const auto other=scene::arrowHead(scene::connectorDisplayRoute(outer,*reference),reverse);
        const auto head=scene::arrowHead(visible,reverse);
        EXPECT_FLOAT_EQ(std::abs(head.left.y-head.right.y),12);
        EXPECT_FLOAT_EQ(head.tip.x,other.tip.x);EXPECT_FLOAT_EQ(head.tip.y,other.tip.y);
        avoids(visible,{hub,upper,lower});
        if(reverse)std::reverse(visible.route.begin(),visible.route.end());
        if(top==50.5) {
            ASSERT_EQ(visible.route.size(),4U)<<"eight units of painted clearance allow a single horizontal run";
            EXPECT_EQ(visible.route[1].y,40);EXPECT_GE(visible.route[1].x,522.5f);
        } else {
            // A line at y=40 would pass only five units above the painted
            // target and the wire's own half width. Keep an actual detour.
            EXPECT_FALSE(visible.route[1].x>502.5 && visible.route[1].y==40);
        }
    }
}

TEST(FC05Route, AnArrowKeepsItsCertifiedConnectionWhenALongerLandingIsADeadEnd) {
    const auto hub=box(1,0,0,120,80),target=box(2,240,177,120,80);
    auto c=wire(0,0,0,0);
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{2}};
    cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{2}};
    c.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}},
                          {0x201U,semantic::ConnectorDecorationValue::kArrow}};
    // The ray from the right port is open for 46 units, but the last 32
    // cannot be reached from the side. The ordinary 14.5-unit turn is clear.
    const std::vector<semantic::ObjectRecord> walls{box(3,383.5,190,100,18),box(4,383.5,226,100,18),
                                                   box(5,415,210,10,14)};
    auto world=walls;world.push_back(hub);world.push_back(target);
    const auto route=scene::connectorGeometry(c,world);
    ASSERT_TRUE(route);ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);
    const auto tip=route->route.back(),turn=route->route[route->route.size()-2];
    EXPECT_GT(turn.x-tip.x,0);EXPECT_LT(turn.x-tip.x,17);
    avoids(*route,walls);
    const auto open=scene::connectorGeometry(c,{hub,target});
    ASSERT_TRUE(open);ASSERT_EQ(open->status,runtime::ConnectorRouteStatus::kReady);
    const auto head=scene::arrowHead(scene::connectorDisplayRoute(c,*open));
    EXPECT_FLOAT_EQ(std::abs(head.left.y-head.right.y),12);
}

TEST(FC05Route, ArrowsFitTheOpenSlotBetweenATargetAndItsNeighbour) {
    for (const double gap : {28.5, 19.5}) for (bool vertical : {false,true})
        for (bool fartherTarget : {false,true}) for (bool reverse : {false,true}) {
            SCOPED_TRACE(::testing::Message()<<"gap="<<gap<<" vertical="<<vertical
                <<" fartherTarget="<<fartherTarget<<" reverse="<<reverse);
            const auto rect=[&](unsigned id,double x,double y) {
                auto o=vertical ? box(id,y,x,80,120) : box(id,x,y,120,80);
                o.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}}};
                return o;
            };
            const auto source=rect(1,0,0), left=rect(2,200,98), right=rect(3,320+gap,98);
            const auto& target=fartherTarget ? right : left;
            auto c=wire(0,0,0,0);
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::AttachedEndpoint{source.id,semantic::StablePortAnchor{vertical?3U:2U}};
            cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{
                fartherTarget ? (vertical?1U:4U) : (vertical?3U:2U)}};
            if(reverse)std::swap(cc.start,cc.end);
            c.properties=source.properties;
            c.properties.entries.push_back({reverse?0x200U:0x201U,semantic::ConnectorDecorationValue::kArrow});
            const std::vector<semantic::ObjectRecord> objects{left,source,right};
            const auto route=scene::connectorGeometry(c,objects);
            ASSERT_TRUE(route);
            ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);
            const auto display=scene::connectorDisplayRoute(c,*route);
            avoids(display,objects);
            const auto head=scene::arrowHead(display,reverse);
            EXPECT_FLOAT_EQ(std::hypot(head.left.x-head.right.x,head.left.y-head.right.y),12);
            const auto bend=display.route[reverse?1:display.route.size()-2];
            auto rounded=c.properties;
            rounded.entries.insert(rounded.entries.begin()+1,{0x103U,semantic::CornerStyleV1{1,4}});
            const auto geometry=scene::sketchGeometry(c.content,rounded,&display);
            ASSERT_TRUE(geometry);
            EXPECT_TRUE(std::any_of(geometry->outline.commands.begin(),geometry->outline.commands.end(),
                [&](const auto& command) {
                    const auto* curve=std::get_if<semantic::CubicTo>(&command);
                    return curve && std::hypot(curve->end.x-bend.x,curve->end.y-bend.y)<=4.01;
                }))<<"the clear bend behind the arrow can still round inside its landing";
            if(gap>25) {
                const double lane=vertical?bend.y:bend.x;
                EXPECT_GE(fartherTarget ? lane-320 : 320+gap-lane,8)
                    <<"keep clearance from the neighbouring outline when the arrow already fits";
            }
            const auto nearEdge=vertical ? head.tip.y : head.tip.x;
            const auto farEdge=vertical ? head.left.y : head.left.x;
            EXPECT_GT(std::min(nearEdge,farEdge),321);
            EXPECT_LT(std::max(nearEdge,farEdge),319+gap);
        }
}

TEST(FC05Route, AClosedSideSlotStillWarnsAndReopensWhenTheNeighbourMoves) {
    const auto source=box(1,0,0,120,80), target=box(2,340,98,120,80);
    auto beside=box(3,220,98,120,80), c=wire(0,0,0,0);
    beside.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}}};
    c.properties=beside.properties;c.properties.entries.push_back({0x201U,semantic::ConnectorDecorationValue::kArrow});
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{source.id,semantic::StablePortAnchor{2}};
    cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{4}};
    for (const double gap : {0., -3., 8., 19.5}) {
        beside.transform.tx=220-gap;
        const auto g=scene::connectorGeometry(c,{source,target,beside});
        ASSERT_TRUE(g);
        if(gap<=0) {
            EXPECT_EQ(g->status,runtime::ConnectorRouteStatus::kNoRouteInDomain);EXPECT_TRUE(g->showsWarning());
        } else {
            ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
            const auto display=scene::connectorDisplayRoute(c,*g);
            const auto head=scene::arrowHead(display);
            EXPECT_EQ(g->end.x,340);EXPECT_EQ(head.tip.y,138);
            EXPECT_GT(head.left.x,340-gap+1);EXPECT_LT(head.left.x,head.tip.x);
            if(gap<12)EXPECT_LT(head.left.y-head.right.y,12);
            else EXPECT_FLOAT_EQ(head.left.y-head.right.y,12);
        }
    }
}

TEST(FC05Route, AScaledConnectorBudgetsItsArrowInLocalUnitsInsideAWorldSpaceSlot) {
    auto c=wire(0,0,0,0);
    c.transform={2,0,0,.75,17,-9};
    const auto hub=box(1,0,0,120,80), neighbour=box(2,200,98,120,80), target=box(3,358,98,120,80);
    auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{2}};
    cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{4}};
    c.properties.entries.push_back({0x201U,semantic::ConnectorDecorationValue::kArrow});
    const auto g=scene::connectorGeometry(c,{hub,neighbour,target});
    ASSERT_TRUE(g);ASSERT_EQ(g->status,runtime::ConnectorRouteStatus::kReady);
    const auto head=scene::arrowHead(scene::connectorDisplayRoute(c,*g));
    EXPECT_FLOAT_EQ(head.left.y-head.right.y,12);
    EXPECT_GT(2*head.left.x+17,320);
}

TEST(FC05Route, ShortTerminalSegmentsKeepTheArrowheadClearOfTheBend) {
    scene::ConnectorGeometry g;
    g.route = {{0, 0}, {0, 40}, {8, 40}};
    g.start = g.route.front(); g.end = g.route.back(); g.arrow_end = true;
    const auto head = scene::arrowHead(g);
    EXPECT_EQ(head.tip.x, 8);
    EXPECT_GE(head.left.x, 4);
    EXPECT_GE(head.right.x, 4);
    EXPECT_GT(head.left.y, head.right.y);

    const auto connector = wire(0, 0, 8, 40);
    for (const double radius : {0., 8.}) {
        semantic::PropertyBag style;
        style.entries = {{0x102U, semantic::SketchStyleV1{1, 123, 1, true}},
                         {0x201U, semantic::ConnectorDecorationValue::kArrow}};
        if (radius > 0) style.entries.push_back({0x103U, semantic::CornerStyleV1{1, radius}});
        const auto painted = scene::sketchGeometry(connector.content, style, &g);
        ASSERT_TRUE(painted);
        ASSERT_GE(painted->arrowFill.commands.size(), 3U);
        for (const auto& command : painted->arrowFill.commands)
            if (const auto* edge = std::get_if<semantic::LineTo>(&command))
                EXPECT_GE(edge->end.x, 4);
            else if (const auto* edge = std::get_if<semantic::CubicTo>(&command))
                EXPECT_GE(edge->end.x, 4);
    }

    g.route = {{0, 0}, {8, 0}}; g.arrow_start = true;
    EXPECT_LT(scene::arrowHead(g, true).left.x, scene::arrowHead(g).left.x);
}

TEST(FC05Route, AnAttachedArrowFitsBesideALargeFilletWithoutTheShaftDoublingBack) {
    auto c=wire(0,0,18,80);
    std::get<semantic::ConnectorContent>(c.content).end.value=
        semantic::AttachedEndpoint{foundation::ObjectId::fromUint64(2),semantic::StablePortAnchor{4}};
    scene::ConnectorGeometry g;
    g.route={{0,0},{0,80},{18,80}};g.start=g.route.front();g.end=g.route.back();g.arrow_end=true;
    const auto display=scene::connectorDisplayRoute(c,g);
    const auto head=scene::arrowHead(display);
    EXPECT_FLOAT_EQ(head.left.y-head.right.y,12);
    EXPECT_GE(g.end.x-head.tip.x,1);EXPECT_LE(g.end.x-head.tip.x,4);
    for(const double radius:{4.,32.}) {
        semantic::PropertyBag style;
        style.entries={{0x103U,semantic::CornerStyleV1{1,radius}},{0x201U,semantic::ConnectorDecorationValue::kArrow}};
        const auto geometry=scene::sketchGeometry(c.content,style,&display);
        ASSERT_TRUE(geometry);
        bool sawCurve=false;
        for(const auto& command:geometry->outline.commands) {
            if(const auto* edge=std::get_if<semantic::CubicTo>(&command)) {
                sawCurve=true;
                EXPECT_LE(edge->control1.x,head.left.x+.001);
                EXPECT_LE(edge->control2.x,head.left.x+.001);
                EXPECT_LE(edge->end.x,head.left.x+.001);
            } else if(const auto* edge=std::get_if<semantic::LineTo>(&command)) {
                EXPECT_LE(edge->end.x,head.left.x+.001);
            }
        }
        EXPECT_TRUE(sawCurve);
    }
}

TEST(FC05Route, DecoratedShaftStopsAtArrowBasesForBothSharpAndRoundedSketchPaths) {
    scene::ConnectorGeometry route;
    route.route = {{10,20}, {150,20}, {150,140}};
    route.start=route.route.front(); route.end=route.route.back();
    route.arrow_start=route.arrow_end=true;
    const auto c=wire(10,20,150,140);
    for (const double radius : {0.,8.}) {
        for (const double roughness : {0.,1.5}) {
            semantic::PropertyBag style;
            style.entries={{0x102U,semantic::SketchStyleV1{1,123,roughness,true}},
                           {0x200U,semantic::ConnectorDecorationValue::kArrow},
                           {0x201U,semantic::ConnectorDecorationValue::kArrow}};
            if (radius) style.entries.push_back({0x103U,semantic::CornerStyleV1{1,radius}});
            const auto geometry=scene::sketchGeometry(c.content,style,&route);
            ASSERT_TRUE(geometry);
            const auto& shaft=geometry->outline.commands;
            ASSERT_GT(shaft.size(),1U);
            const auto start=std::get<semantic::MoveTo>(shaft.front()).point;
            EXPECT_EQ(start.x,22); EXPECT_EQ(start.y,20);
            const auto nextMove=std::find_if(std::next(shaft.begin()),shaft.end(),[](const auto& command){
                return std::holds_alternative<semantic::MoveTo>(command);
            });
            const auto& last=*std::prev(nextMove);
            const auto* line=std::get_if<semantic::LineTo>(&last);
            const auto* curve=std::get_if<semantic::CubicTo>(&last);
            ASSERT_TRUE(line||curve);
            const auto end=line?line->end:curve->end;
            EXPECT_EQ(end.x,150); EXPECT_EQ(end.y,128);
            EXPECT_EQ(scene::arrowHead(route,true).tip.x,10);
            EXPECT_EQ(scene::arrowHead(route).tip.y,140);
        }
    }
}

TEST(FC05Route, DeferredPreviewKeepsReadyPolylineAndPendingOrthogonalShape) {
    for(bool pending:{false,true}) {
        auto a=box(1,0,0,100,100),b=box(2,50,300,100,100);
        auto c=wire(0,0,0,0);auto& cc=std::get<semantic::ConnectorContent>(c.content);
        cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
        cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
        std::vector<semantic::ObjectRecord> objects{a,b};
        if(pending)for(unsigned n=0;n<1500;++n)objects.push_back(box(n+10,100000+n*20,100000,10,10));
        const auto route=scene::connectorGeometry(c,objects,true);ASSERT_TRUE(route);
        EXPECT_EQ(route->status,pending?runtime::ConnectorRouteStatus::kPending:runtime::ConnectorRouteStatus::kReady);
        ASSERT_EQ(route->route.size(),4U);
        EXPECT_FLOAT_EQ(route->route[0].x,50);EXPECT_FLOAT_EQ(route->route[0].y,100);
        EXPECT_FLOAT_EQ(route->route[1].x,50);EXPECT_FLOAT_EQ(route->route[1].y,200);
        EXPECT_FLOAT_EQ(route->route[2].x,100);EXPECT_FLOAT_EQ(route->route[2].y,200);
        EXPECT_FLOAT_EQ(route->route[3].x,100);EXPECT_FLOAT_EQ(route->route[3].y,300);
        for(std::size_t n=1;n<route->route.size();++n)
            EXPECT_TRUE(route->route[n-1].x==route->route[n].x||route->route[n-1].y==route->route[n].y);
    }
}

TEST(FC05Route, AlignedPortsStayStraightAndCenterLaneNeverCrossesAnObstacle) {
    auto a=box(1,0,0,100,100),b=box(2,300,0,100,100);
    auto c=wire(0,0,0,0);auto& cc=std::get<semantic::ConnectorContent>(c.content);
    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{2}};
    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{4}};
    auto route=scene::connectorGeometry(c,{a,b});ASSERT_TRUE(route);
    ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);
    EXPECT_EQ(route->route.size(),2U);
    b.transform.ty=50;
    const std::vector<semantic::ObjectRecord> objects{a,b,box(3,180,40,40,80)};
    route=scene::connectorGeometry(c,objects);ASSERT_TRUE(route);
    ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);avoids(*route,objects);
}

TEST(FC05Route, AlignedTriangleVerticalPortsPreferStraightAcrossEscapeAlternatives) {
    auto a=box(1,0,0,100,100),b=box(2,0,300,100,100);
    a.kind_version=b.kind_version=2;
    std::get<semantic::ShapeContent>(a.content).shape_kind=3;
    std::get<semantic::ShapeContent>(b.content).shape_kind=3;
    for(bool reverse:{false,true}) {
        auto c=wire(0,0,0,0);auto& cc=std::get<semantic::ConnectorContent>(c.content);
        cc.start.value=semantic::AttachedEndpoint{reverse?b.id:a.id,semantic::StablePortAnchor{reverse?1U:3U}};
        cc.end.value=semantic::AttachedEndpoint{reverse?a.id:b.id,semantic::StablePortAnchor{reverse?3U:1U}};
        const auto route=scene::connectorGeometry(c,{a,b});ASSERT_TRUE(route);
        ASSERT_EQ(route->status,runtime::ConnectorRouteStatus::kReady);
        EXPECT_EQ(route->route.size(),2U);
        EXPECT_FLOAT_EQ(route->start.x,50);EXPECT_FLOAT_EQ(route->end.x,50);
        EXPECT_FLOAT_EQ(route->start.y,reverse?300.0F:100.0F);
        EXPECT_FLOAT_EQ(route->end.y,reverse?100.0F:300.0F);
        const auto preview=scene::connectorGeometry(c,{a,b},true);ASSERT_TRUE(preview);
        EXPECT_EQ(preview->status,runtime::ConnectorRouteStatus::kReady);
        EXPECT_EQ(preview->route.size(),2U);
        const std::vector<semantic::ObjectRecord> blocked{a,b,box(3,35,180,30,40)};
        const auto detour=scene::connectorGeometry(c,blocked);ASSERT_TRUE(detour);
        ASSERT_EQ(detour->status,runtime::ConnectorRouteStatus::kReady);
        EXPECT_GT(detour->route.size(),2U);avoids(*detour,blocked);
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
    // Force an actual input-scan yield, rather than depending on a small
    // clear route artificially yielding before it can be certified.
    for(unsigned i=0;i<1500;++i)objects.push_back(box(i+1,100000+i*20,100000,12,12));
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

TEST(FC05Route, CrossOnlyTheUnavoidableInterveningObjectAndRecoverWhenAPassageOpens) {
    const auto source=box(1,0,0,80,60), target=box(2,0,208,80,60);
    auto wall=box(3,-2048,108,4096,12);
    const auto other=box(4,20,147,40,18);
    auto wireObject=wire(40,60,40,208);
    auto& cc=std::get<semantic::ConnectorContent>(wireObject.content);
    const auto touches=[](const auto& path,foundation::WorldRect rect) {
        for(std::size_t i=1;i<path.size();++i)if(crosses(path[i-1],path[i],rect))return true;
        return false;
    };
    for(bool attached : {false,true}) {
        std::vector<semantic::ObjectRecord> objects{wall,other};
        if(attached) {
            cc.start.value=semantic::AttachedEndpoint{source.id,semantic::StablePortAnchor{3}};
            cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{1}};
            objects.push_back(source);objects.push_back(target);
        }
        const auto solve=[&](){return scene::connectorGeometry(wireObject,objects).value();};
        const auto through=solve();
        ASSERT_EQ(through.status,runtime::ConnectorRouteStatus::kReady);
        EXPECT_FALSE(through.showsWarning());
        EXPECT_TRUE(touches(through.route,scene::computeBounds(wall).world));
        EXPECT_FALSE(touches(through.route,{12,139,68,173}));
        if(attached)avoids(through,{source,target});
        std::reverse(objects.begin(),objects.end());
        const auto same=solve();
        ASSERT_EQ(same.route.size(),through.route.size());
        for(std::size_t i=0;i<same.route.size();++i) {
            EXPECT_EQ(same.route[i].x,through.route[i].x);EXPECT_EQ(same.route[i].y,through.route[i].y);
        }
        const auto it=std::find_if(objects.begin(),objects.end(),[&](const auto& o){return o.id==wall.id;});
        *it=box(3,-30,108,140,12);
        const auto clear=solve();
        ASSERT_EQ(clear.status,runtime::ConnectorRouteStatus::kReady);avoids(clear,objects);
    }
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
