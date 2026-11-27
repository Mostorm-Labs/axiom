#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/connector_router.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::scene {
DerivedGeometry deriveGeometry(const semantic::ObjectRecord&r) noexcept { if(const auto* section=std::get_if<semantic::SectionContent>(&r.content)) { const auto& b=section->boundary; return SectionBoundaryGeometry{{static_cast<float>(b.x),static_cast<float>(b.y),static_cast<float>(b.x+b.width),static_cast<float>(b.y+b.height)},static_cast<float>(sectionTitleHeight(*section))}; } if(const auto*s=std::get_if<semantic::ShapeContent>(&r.content)) if(r.kind==semantic::ObjectKind::kShape&&(r.kind_version==2U || r.kind_version==3U)) if(auto g=triangleGeometry(*s)) return *g; if(r.kind==semantic::ObjectKind::kConnector&&r.kind_version==1U) if(auto g=connectorGeometry(r)) return *g; return {}; }
std::optional<TriangleGeometry> triangleGeometry(const semantic::ShapeContent& s) noexcept {
 if(!std::isfinite(s.width)||!std::isfinite(s.height)||s.width<=0||s.height<=0||s.shape_kind!=3U)return std::nullopt;
 return TriangleGeometry{{static_cast<float>(s.width/2),0},{static_cast<float>(s.width),static_cast<float>(s.height)},{0,static_cast<float>(s.height)},{0,0,static_cast<float>(s.width),static_cast<float>(s.height)}};
}
std::optional<ConnectorGeometry> connectorGeometry(const semantic::ObjectRecord& record,
    const std::vector<semantic::ObjectRecord>& obstacles, bool defer) noexcept {
 if (!std::holds_alternative<semantic::ConnectorContent>(record.content)) return std::nullopt;
 ConnectorRouteJob job(record, obstacles);
 if (defer && std::get<semantic::ConnectorContent>(record.content).routing == semantic::ConnectorRouting::kOrthogonal) {
     (void)job.advance();
     auto partial=job.result();
     if(partial.status==runtime::ConnectorRouteStatus::kPending&&partial.route.empty())
         partial.route={partial.start,partial.end};
     return partial;
 }
 while (job.advance() == runtime::ConnectorRouteStatus::kPending) {}
 return job.result();
}
std::optional<ConnectorGeometry> connectorGeometry(const semantic::ConnectorContent& content,
                                                   const semantic::PropertyBag& properties) noexcept {
 semantic::ObjectRecord record{}; record.kind=semantic::ObjectKind::kConnector;
 record.kind_version=1U; record.properties=properties; record.content=content;
 return connectorGeometry(record);
}
float distanceToSegment(foundation::WorldPoint p,foundation::WorldPoint a,foundation::WorldPoint b) noexcept {const float dx=b.x-a.x,dy=b.y-a.y;const float d=dx*dx+dy*dy;if(d<=0)return std::hypot(p.x-a.x,p.y-a.y);const float t=std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/d,0.0F,1.0F);return std::hypot(p.x-(a.x+t*dx),p.y-(a.y+t*dy));}
bool pointInTriangle(foundation::WorldPoint p,const TriangleGeometry&g,float tol) noexcept {auto cross=[](auto a,auto b,auto p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};const float c1=cross(g.top,g.right,p),c2=cross(g.right,g.left,p),c3=cross(g.left,g.top,p);const bool pos=c1>=-tol&&c2>=-tol&&c3>=-tol,neg=c1<=tol&&c2<=tol&&c3<=tol;return pos||neg;}
bool pointOnConnector(foundation::WorldPoint p,const ConnectorGeometry&g,float tolerance) noexcept { if (g.route.empty()) return distanceToSegment(p,g.start,g.end)<=tolerance; for (std::size_t i=1;i<g.route.size();++i) if (distanceToSegment(p,g.route[i-1],g.route[i])<=tolerance) return true; return false; }
std::optional<ConnectorGeometry> stagedConnectorRoute(const semantic::ObjectRecord& connector,
                                                      std::vector<semantic::ObjectRecord> graph,
                                                      bool defer) {
    const auto match = [&](const auto& object) { return object.id == connector.id; };
    const auto before = std::find_if(graph.begin(), graph.end(), match);
    if (before == graph.end())
        graph.push_back(connector);
    else
        *before = connector;
    auto world = projectWorldObjects(graph);
    const auto found = std::find_if(world.begin(), world.end(), match);
    return found == world.end() ? std::nullopt : connectorGeometry(*found, world, defer);
}
ConnectorGeometry connectorDisplayRoute(const semantic::ObjectRecord& object, ConnectorGeometry g) {
    const auto* content = std::get_if<semantic::ConnectorContent>(&object.content);
    if (!content || g.route.size() < 2)
        return g;
    const auto trim = [&](foundation::WorldPoint anchor,
                          foundation::WorldPoint next,
                          const semantic::ConnectorEndpoint& e) {
        if (!std::holds_alternative<semantic::AttachedEndpoint>(e.value))
            return anchor;
        const double dx = next.x - anchor.x, dy = next.y - anchor.y;
        const auto& t = object.transform;
        const double length = std::hypot(t.a * dx + t.c * dy, t.b * dx + t.d * dy);
        if (length <= 0)
            return anchor;
        // Eight board units at 100% zoom. Short terminal segments retain their direction and bends.
        const double fraction = std::min(8.0 / length, 0.25);
        return foundation::WorldPoint{float(anchor.x + dx * fraction),
                                      float(anchor.y + dy * fraction)};
    };
    const auto first = trim(g.start, g.route[1], content->start);
    const auto last = trim(g.end, g.route[g.route.size() - 2], content->end);
    g.route.front() = first;
    g.route.back() = last;
    return g;
}
double connectorArrowLength(double segmentLength, bool arrowsShareSegment) noexcept {
    return std::min(12.0, segmentLength / (arrowsShareSegment ? 3.0 : 2.0));
}
foundation::WorldPoint connectorShaftVertex(std::span<const foundation::WorldPoint> route,
    std::size_t index, bool startArrow, bool endArrow) noexcept {
    const auto vertex=route[index];
    if (route.size()<2 || !((index==0 && startArrow) || (index+1==route.size() && endArrow)))
        return vertex;
    const auto next=route[index==0 ? 1 : index-1];
    const double dx=double(next.x)-vertex.x, dy=double(next.y)-vertex.y;
    const double length=std::hypot(dx,dy);
    if (length==0) return vertex;
    const double inset=connectorArrowLength(length,route.size()==2 && startArrow && endArrow)/length;
    return {static_cast<float>(vertex.x+dx*inset),static_cast<float>(vertex.y+dy*inset)};
}
ArrowHeadGeometry arrowHead(const ConnectorGeometry& g, bool atStart) noexcept {
    const auto tip = g.route.size() > 1 ? (atStart ? g.route.front() : g.route.back())
                                        : (atStart ? g.start : g.end);
    const auto from = g.route.size() > 1 ? (atStart ? g.route[1] : g.route[g.route.size() - 2])
                                         : (atStart ? g.end : g.start);
    const float dx = tip.x - from.x, dy = tip.y - from.y, len = std::hypot(dx, dy);
    if (!(atStart ? g.arrow_start : g.arrow_end) ||
        g.status != runtime::ConnectorRouteStatus::kReady || len <= 0)
        return {tip, tip, tip};
    const float ux = dx / len, uy = dy / len, px = -uy, py = ux;
    const auto head = static_cast<float>(connectorArrowLength(len, g.route.size()<=2 && g.arrow_start && g.arrow_end));
    const auto halfWidth = head/2;
    return {tip,
            {tip.x - ux * head + px * halfWidth, tip.y - uy * head + py * halfWidth},
            {tip.x - ux * head - px * halfWidth, tip.y - uy * head - py * halfWidth}};
}
bool pointOnArrowHead(foundation::WorldPoint p,const ConnectorGeometry& g,float tolerance) noexcept {
    if(g.status!=runtime::ConnectorRouteStatus::kReady)return false;
    for(const bool atStart:{false,true}){
        if(!(atStart?g.arrow_start:g.arrow_end))continue;
        const auto h=arrowHead(g,atStart);
        if(pointInTriangle(p,{h.tip,h.left,h.right,{}},tolerance))return true;
    }
    return false;
}
}
