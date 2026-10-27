#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/connector_router.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::scene {
DerivedGeometry deriveGeometry(const semantic::ObjectRecord&r) noexcept { if(const auto* section=std::get_if<semantic::SectionContent>(&r.content)) { const auto& b=section->boundary; return SectionBoundaryGeometry{{static_cast<float>(b.x),static_cast<float>(b.y),static_cast<float>(b.x+b.width),static_cast<float>(b.y+b.height)}}; } if(const auto*s=std::get_if<semantic::ShapeContent>(&r.content)) if(r.kind==semantic::ObjectKind::kShape&&(r.kind_version==2U || r.kind_version==3U)) if(auto g=triangleGeometry(*s)) return *g; if(r.kind==semantic::ObjectKind::kConnector&&r.kind_version==1U) if(auto g=connectorGeometry(r)) return *g; return {}; }
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
ArrowHeadGeometry arrowHead(const ConnectorGeometry& g) noexcept {
 const auto from=g.route.size()>1U?g.route[g.route.size()-2U]:g.start;
 const float dx=g.end.x-from.x,dy=g.end.y-from.y,len=std::hypot(dx,dy);
 if(!g.arrow_end||g.status!=runtime::ConnectorRouteStatus::kReady||len<=0) return {g.end,g.end,g.end};
 const float ux=dx/len,uy=dy/len,px=-uy,py=ux;
 return {g.end,{g.end.x-ux*12+px*6,g.end.y-uy*12+py*6},
              {g.end.x-ux*12-px*6,g.end.y-uy*12-py*6}};
}
bool pointOnArrowHead(foundation::WorldPoint p,const ConnectorGeometry&g,float tolerance) noexcept { const auto h=arrowHead(g); if(!g.arrow_end||g.status!=runtime::ConnectorRouteStatus::kReady) return false; const float c1=(h.left.x-h.tip.x)*(p.y-h.tip.y)-(h.left.y-h.tip.y)*(p.x-h.tip.x),c2=(h.right.x-h.left.x)*(p.y-h.left.y)-(h.right.y-h.left.y)*(p.x-h.left.x),c3=(h.tip.x-h.right.x)*(p.y-h.right.y)-(h.tip.y-h.right.y)*(p.x-h.right.x); return (c1>=-tolerance&&c2>=-tolerance&&c3>=-tolerance)||(c1<=tolerance&&c2<=tolerance&&c3<=tolerance); }
}
