#include "canvas/scene/derived_geometry.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::scene {
DerivedGeometry deriveGeometry(const semantic::ObjectRecord&r) noexcept { if(const auto*s=std::get_if<semantic::ShapeContent>(&r.content)) if(r.kind==semantic::ObjectKind::kShape&&(r.kind_version==2U || r.kind_version==3U)) if(auto g=triangleGeometry(*s)) return *g; if(const auto*c=std::get_if<semantic::ConnectorContent>(&r.content)) if(r.kind==semantic::ObjectKind::kConnector&&r.kind_version==1U) if(auto g=connectorGeometry(*c,r.properties)) return *g; return {}; }
std::optional<TriangleGeometry> triangleGeometry(const semantic::ShapeContent& s) noexcept {
 if(!std::isfinite(s.width)||!std::isfinite(s.height)||s.width<=0||s.height<=0||s.shape_kind!=3U)return std::nullopt;
 return TriangleGeometry{{static_cast<float>(s.width/2),0},{static_cast<float>(s.width),static_cast<float>(s.height)},{0,static_cast<float>(s.height)},{0,0,static_cast<float>(s.width),static_cast<float>(s.height)}};
}
std::optional<ConnectorGeometry> connectorGeometry(const semantic::ConnectorContent& c,const semantic::PropertyBag& props) noexcept {
 auto a=std::get_if<semantic::FreePointEndpoint>(&c.start.value), b=std::get_if<semantic::FreePointEndpoint>(&c.end.value); if(!a||!b)return std::nullopt;
 if(!std::isfinite(a->point.x)||!std::isfinite(a->point.y)||!std::isfinite(b->point.x)||!std::isfinite(b->point.y))return std::nullopt;
 bool arrow=false; for(const auto&e:props.entries)if(e.field_id==0x201U)if(const auto*v=std::get_if<semantic::ConnectorDecorationValue>(&e.value))arrow=*v==semantic::ConnectorDecorationValue::kArrow;
 return ConnectorGeometry{{static_cast<float>(a->point.x),static_cast<float>(a->point.y)},{static_cast<float>(b->point.x),static_cast<float>(b->point.y)},arrow};
}
float distanceToSegment(foundation::WorldPoint p,foundation::WorldPoint a,foundation::WorldPoint b) noexcept {const float dx=b.x-a.x,dy=b.y-a.y;const float d=dx*dx+dy*dy;if(d<=0)return std::hypot(p.x-a.x,p.y-a.y);const float t=std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/d,0.0F,1.0F);return std::hypot(p.x-(a.x+t*dx),p.y-(a.y+t*dy));}
bool pointInTriangle(foundation::WorldPoint p,const TriangleGeometry&g,float tol) noexcept {auto cross=[](auto a,auto b,auto p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};const float c1=cross(g.top,g.right,p),c2=cross(g.right,g.left,p),c3=cross(g.left,g.top,p);const bool pos=c1>=-tol&&c2>=-tol&&c3>=-tol,neg=c1<=tol&&c2<=tol&&c3<=tol;return pos||neg;}
bool pointOnConnector(foundation::WorldPoint p,const ConnectorGeometry&g,float tolerance) noexcept{return distanceToSegment(p,g.start,g.end)<=tolerance;}
ArrowHeadGeometry arrowHead(const ConnectorGeometry&g) noexcept { const float dx=g.end.x-g.start.x,dy=g.end.y-g.start.y,len=std::hypot(dx,dy); if(!g.arrow_end||len<=0) return {g.end,g.end,g.end}; const float ux=dx/len,uy=dy/len,px=-uy,py=ux; return {g.end,{g.end.x-ux*12+px*6,g.end.y-uy*12+py*6},{g.end.x-ux*12-px*6,g.end.y-uy*12-py*6}}; }
bool pointOnArrowHead(foundation::WorldPoint p,const ConnectorGeometry&g,float tolerance) noexcept { const auto h=arrowHead(g); if(!g.arrow_end) return false; const float c1=(h.left.x-h.tip.x)*(p.y-h.tip.y)-(h.left.y-h.tip.y)*(p.x-h.tip.x),c2=(h.right.x-h.left.x)*(p.y-h.left.y)-(h.right.y-h.left.y)*(p.x-h.left.x),c3=(h.tip.x-h.right.x)*(p.y-h.right.y)-(h.tip.y-h.right.y)*(p.x-h.right.x); return (c1>=-tolerance&&c2>=-tolerance&&c3>=-tolerance)||(c1<=tolerance&&c2<=tolerance&&c3<=tolerance); }
}
