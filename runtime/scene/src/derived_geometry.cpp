#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::scene {
namespace {
foundation::WorldPoint toWorld(const semantic::Transform2D& t, foundation::WorldPoint p) noexcept {
    return {static_cast<float>(t.a * p.x + t.c * p.y + t.tx),
            static_cast<float>(t.b * p.x + t.d * p.y + t.ty)};
}
bool inverseAabb(const semantic::Transform2D& t, const foundation::WorldRect& world,
                foundation::WorldRect& local) noexcept {
    const double det = t.a * t.d - t.b * t.c;
    if (!std::isfinite(det) || std::abs(det) < 1e-12) return false;
    const foundation::WorldPoint corners[] = {
        {world.left, world.top}, {world.right, world.top},
        {world.right, world.bottom}, {world.left, world.bottom}};
    bool have = false;
    for (const auto point : corners) {
        const double dx = point.x - t.tx, dy = point.y - t.ty;
        const foundation::WorldPoint localPoint{
            static_cast<float>((t.d * dx - t.c * dy) / det),
            static_cast<float>((t.a * dy - t.b * dx) / det)};
        const foundation::WorldRect item{localPoint.x, localPoint.y, localPoint.x, localPoint.y};
        local = have ? foundation::unionRects(local, item) : item;
        have = true;
    }
    return have && local.isFiniteAndOrdered();
}
}
DerivedGeometry deriveGeometry(const semantic::ObjectRecord&r) noexcept { if(const auto*s=std::get_if<semantic::ShapeContent>(&r.content)) if(r.kind==semantic::ObjectKind::kShape&&(r.kind_version==2U || r.kind_version==3U)) if(auto g=triangleGeometry(*s)) return *g; if(r.kind==semantic::ObjectKind::kConnector&&r.kind_version==1U) if(auto g=connectorGeometry(r)) return *g; return {}; }
std::optional<TriangleGeometry> triangleGeometry(const semantic::ShapeContent& s) noexcept {
 if(!std::isfinite(s.width)||!std::isfinite(s.height)||s.width<=0||s.height<=0||s.shape_kind!=3U)return std::nullopt;
 return TriangleGeometry{{static_cast<float>(s.width/2),0},{static_cast<float>(s.width),static_cast<float>(s.height)},{0,static_cast<float>(s.height)},{0,0,static_cast<float>(s.width),static_cast<float>(s.height)}};
}
std::optional<ConnectorGeometry> connectorGeometry(const semantic::ObjectRecord& record,const std::vector<semantic::ObjectRecord>& obstacles) noexcept {
 const auto* content = std::get_if<semantic::ConnectorContent>(&record.content); if (!content) return std::nullopt;
 const auto& c = *content;
 auto resolveEndpoint = [&](const semantic::ConnectorEndpoint& endpoint,
                            const semantic::ConnectorEndpoint* other) -> std::optional<foundation::WorldPoint> {
     if (const auto* free = std::get_if<semantic::FreePointEndpoint>(&endpoint.value))
         return foundation::WorldPoint{static_cast<float>(free->point.x), static_cast<float>(free->point.y)};
     const auto* attached = std::get_if<semantic::AttachedEndpoint>(&endpoint.value);
     if (!attached) return std::nullopt;
     const auto target = std::find_if(obstacles.begin(), obstacles.end(), [&](const auto& candidate) {
         return candidate.id == attached->target_object_id;
     });
     if (target == obstacles.end()) return std::nullopt;
     const auto* shape = std::get_if<semantic::ShapeContent>(&target->content);
     const auto* sticky = std::get_if<semantic::StickyContent>(&target->content);
     const auto* image = std::get_if<semantic::ImageContent>(&target->content);
     double width = 0.0, height = 0.0;
     if (shape) { width = shape->width; height = shape->height; }
     else if (sticky) { width = sticky->width; height = sticky->height; }
     else if (image) { width = image->width; height = image->height; }
     else return std::nullopt;
     if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0 || height <= 0.0) return std::nullopt;
     std::uint32_t port = 0;
     if (const auto* stable = std::get_if<semantic::StablePortAnchor>(&attached->anchor)) port = stable->port_id;
     double x = width * 0.5, y = height * 0.5;
     if (port == 1U) { x = width * 0.5; y = 0.0; }
     else if (port == 2U) { x = width; y = height * 0.5; }
     else if (port == 3U) { x = width * 0.5; y = height; }
     else if (port == 4U) { x = 0.0; y = height * 0.5; }
     else if (const auto* autoAnchor = std::get_if<semantic::AutoPerimeterAnchor>(&attached->anchor)) {
         if (autoAnchor->hint) { x = autoAnchor->hint->x * width; y = autoAnchor->hint->y * height; }
         else if (const auto* otherFree = other ? std::get_if<semantic::FreePointEndpoint>(&other->value) : nullptr) {
             const auto otherWorld = toWorld(record.transform,
                 {static_cast<float>(otherFree->point.x), static_cast<float>(otherFree->point.y)});
             const double dx = otherWorld.x - target->transform.tx;
             const double dy = otherWorld.y - target->transform.ty;
             if (std::abs(dx) >= std::abs(dy)) { x = dx >= 0.0 ? width : 0.0; y = height * 0.5; }
             else { x = width * 0.5; y = dy >= 0.0 ? height : 0.0; }
         }
     }
     const double wx = target->transform.a*x + target->transform.c*y + target->transform.tx;
     const double wy = target->transform.b*x + target->transform.d*y + target->transform.ty;
     const double det = record.transform.a * record.transform.d - record.transform.b * record.transform.c;
     if (!std::isfinite(wx) || !std::isfinite(wy) || !std::isfinite(det) || std::abs(det) < 1e-12) return std::nullopt;
     const double dx = wx - record.transform.tx;
     const double dy = wy - record.transform.ty;
     return foundation::WorldPoint{
         static_cast<float>((record.transform.d * dx - record.transform.c * dy) / det),
         static_cast<float>((record.transform.a * dy - record.transform.b * dx) / det)};
 };
 const auto startPoint = resolveEndpoint(c.start, &c.end);
 const auto endPoint = resolveEndpoint(c.end, &c.start);
 if (!startPoint || !endPoint) return std::nullopt;
 bool arrow=false; for(const auto&e:record.properties.entries)if(e.field_id==0x201U)if(const auto*v=std::get_if<semantic::ConnectorDecorationValue>(&e.value))arrow=*v==semantic::ConnectorDecorationValue::kArrow;
 const foundation::WorldPoint start = *startPoint;
 const foundation::WorldPoint end = *endPoint;
 std::vector<foundation::WorldPoint> route{start};
 if (c.routing == semantic::ConnectorRouting::kOrthogonal && start.x != end.x && start.y != end.y) {
     const foundation::WorldRect corridor{std::min(start.x,end.x), std::min(start.y,end.y), std::max(start.x,end.x), std::max(start.y,end.y)};
     const bool horizontalBlocked = std::any_of(obstacles.begin(), obstacles.end(), [&](const auto& obstacle) {
         if (obstacle.id == record.id) return false;
         const auto worldBounds = computeBounds(obstacle).world;
         foundation::WorldRect bounds{};
         return worldBounds.isFiniteAndOrdered() && inverseAabb(record.transform, worldBounds, bounds) &&
             bounds.intersects({corridor.left, start.y, corridor.right, start.y});
     });
     route.push_back(horizontalBlocked ? foundation::WorldPoint{start.x, end.y} : foundation::WorldPoint{end.x, start.y});
 }
 route.push_back(end);
 return ConnectorGeometry{start, end, std::move(route), arrow};
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
ArrowHeadGeometry arrowHead(const ConnectorGeometry&g) noexcept { const float dx=g.end.x-g.start.x,dy=g.end.y-g.start.y,len=std::hypot(dx,dy); if(!g.arrow_end||len<=0) return {g.end,g.end,g.end}; const float ux=dx/len,uy=dy/len,px=-uy,py=ux; return {g.end,{g.end.x-ux*12+px*6,g.end.y-uy*12+py*6},{g.end.x-ux*12-px*6,g.end.y-uy*12-py*6}}; }
bool pointOnArrowHead(foundation::WorldPoint p,const ConnectorGeometry&g,float tolerance) noexcept { const auto h=arrowHead(g); if(!g.arrow_end) return false; const float c1=(h.left.x-h.tip.x)*(p.y-h.tip.y)-(h.left.y-h.tip.y)*(p.x-h.tip.x),c2=(h.right.x-h.left.x)*(p.y-h.left.y)-(h.right.y-h.left.y)*(p.x-h.left.x),c3=(h.tip.x-h.right.x)*(p.y-h.right.y)-(h.tip.y-h.right.y)*(p.x-h.right.x); return (c1>=-tolerance&&c2>=-tolerance&&c3>=-tolerance)||(c1<=tolerance&&c2<=tolerance&&c3<=tolerance); }
}
