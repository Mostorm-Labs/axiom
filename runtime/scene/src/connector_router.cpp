#include "canvas/scene/corner_geometry.hpp"
#include "canvas/scene/connector_router.hpp"
#include "canvas/scene/bounds_system.hpp"
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <compare>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace canvas::scene {
namespace {
using Status = runtime::ConnectorRouteStatus;
struct P { double x=0,y=0; bool operator==(const P&) const = default; };
struct R { double l=0,t=0,r=0,b=0; };
struct Obstacle { semantic::ObjectId id; R rect; bool terminal=false; };
constexpr std::array<double,3> searchPadding{64,256,1024};
constexpr std::size_t obstacleLimit=256;
constexpr double obstacleClearance=8;
P world(const semantic::Transform2D& t,P p) { return {t.a*p.x+t.c*p.y+t.tx,t.b*p.x+t.d*p.y+t.ty}; }
bool inverse(const semantic::Transform2D& t,P p,P& v) {
    const double det=t.a*t.d-t.b*t.c;
    if (!std::isfinite(det)||std::abs(det)<1e-12) return false;
    const double x=p.x-t.tx,y=p.y-t.ty;
    v={(t.d*x-t.c*y)/det,(-t.b*x+t.a*y)/det};
    return std::isfinite(v.x)&&std::isfinite(v.y);
}
bool finite(P p) { return std::isfinite(p.x)&&std::isfinite(p.y); }
double scale(const semantic::Transform2D& t) {
    const double s=t.a*t.a+t.b*t.b+t.c*t.c+t.d*t.d;
    const double det=t.a*t.d-t.b*t.c;
    return std::sqrt((s+std::sqrt(std::max(0.0,s*s-4*det*det)))*0.5);
}
double stroke(const semantic::ObjectRecord& o) {
    for (const auto& e:o.properties.entries) if (e.field_id==0x101U)
        if (const auto* s=std::get_if<semantic::StrokeStyleValue>(&e.value)) {
            if (const auto* v=std::get_if<semantic::SolidStroke>(s)) {
                double outset=v->width*scale(o.transform)*0.5;
                if(o.kind!=semantic::ObjectKind::kConnector)
                    if(const auto* miter=std::get_if<semantic::MiterJoin>(&v->join))outset*=std::max(1.0,miter->limit);
                return outset;
            }
            return 0;
        }
    // The legacy unstyled Connector renderer uses a four-unit stroke.
    return o.kind==semantic::ObjectKind::kConnector?2*scale(o.transform):0;
}
bool admitted(const semantic::ObjectRecord& o) {
    if (o.kind==semantic::ObjectKind::kShape) return o.kind_version>=1&&o.kind_version<=3;
    return (o.kind==semantic::ObjectKind::kSticky||o.kind==semantic::ObjectKind::kImage)&&o.kind_version==1;
}
bool visible(const semantic::ObjectRecord& o) {
    for (const auto& e:o.properties.entries) if (e.field_id==1U)
        if (const auto* v=std::get_if<bool>(&e.value)) return *v;
    return true;
}
bool dimensions(const semantic::ObjectRecord& o,double& w,double& h) {
    if (!admitted(o)) return false;
    if (const auto* s=std::get_if<semantic::ShapeContent>(&o.content)) { w=s->width;h=s->height; }
    else if (const auto* sticky=std::get_if<semantic::StickyContent>(&o.content)) { w=sticky->width;h=sticky->height; }
    else if (const auto* image=std::get_if<semantic::ImageContent>(&o.content)) { w=image->width;h=image->height; }
    else return false;
    return std::isfinite(w)&&std::isfinite(h)&&w>0&&h>0;
}
bool aabb(const semantic::ObjectRecord& o,R& r) {
    double w=0,h=0; if (!dimensions(o,w,h)) return false;
    r={INFINITY,INFINITY,-INFINITY,-INFINITY};
    for (P p:std::array<P,4>{{{0,0},{w,0},{w,h},{0,h}}}) {
        p=world(o.transform,p); if (!finite(p)) return false;
        r.l=std::min(r.l,p.x);r.r=std::max(r.r,p.x);r.t=std::min(r.t,p.y);r.b=std::max(r.b,p.y);
    }
    return true;
}
bool obstacleBounds(const semantic::ObjectRecord& o, double wireOutset, R& r) {
    if (!aabb(o,r)) return false;
    const double margin=obstacleClearance+wireOutset+stroke(o);
    if (!std::isfinite(margin)) return false;
    r={std::floor((r.l-margin)*64)/64,std::floor((r.t-margin)*64)/64,
       std::ceil((r.r+margin)*64)/64,std::ceil((r.b+margin)*64)/64};
    return true;
}
bool intersects(R a,R b) { return a.l<=b.r&&a.r>=b.l&&a.t<=b.b&&a.b>=b.t; }
bool inside(P p,R r) { return p.x>r.l&&p.x<r.r&&p.y>r.t&&p.y<r.b; }
bool crossing(P a,P b,R r) {
    if (a.x==b.x) return a.x>r.l&&a.x<r.r&&std::max(a.y,b.y)>r.t&&std::min(a.y,b.y)<r.b;
    if (a.y==b.y) return a.y>r.t&&a.y<r.b&&std::max(a.x,b.x)>r.l&&std::min(a.x,b.x)<r.r;
    return true;
}
double distance(P a,P b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y); }
bool pLess(P a,P b) { return std::tie(a.x,a.y)<std::tie(b.x,b.y); }
bool pathLess(const std::vector<P>& a,const std::vector<P>& b) {
    return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),pLess);
}
std::vector<P> simplified(std::vector<P> path) {
    std::vector<P> out;
    for (const auto p:path) {
        if (!out.empty()&&out.back()==p) continue;
        while (out.size()>1) {
            const auto a=out[out.size()-2],b=out.back();
            if (!((a.x==b.x&&b.x==p.x)||(a.y==b.y&&b.y==p.y))) break;
            out.pop_back();
        }
        out.push_back(p);
    }
    return out;
}
struct Endpoint {
    P p{},normal{},cornerNormal{};
    const semantic::ObjectRecord* target=nullptr;
    R obstacle{};
    double arrowApproach=0;
    std::optional<double> slotReach;
};
P primaryDirection(const Endpoint& e) {
    const P n{e.normal.x+e.cornerNormal.x,e.normal.y+e.cornerNormal.y};
    if (std::abs(n.x)>std::abs(n.y)) return {n.x>0 ? 1. : -1.,0};
    return {0,n.y>0 ? 1. : -1.};
}
const semantic::AttachedEndpoint* fixedAttachment(const semantic::ConnectorEndpoint& e) {
    const auto* a=std::get_if<semantic::AttachedEndpoint>(&e.value);
    if (!a) return nullptr;
    const auto* port=std::get_if<semantic::StablePortAnchor>(&a->anchor);
    return port && port->port_id>=1 && port->port_id<=4 ? a : nullptr;
}
double branchCoordinate(double axis) { return std::round(axis*64)/64; }
bool pointsOut(const Endpoint& e,P dir) {
    return dir.x*e.normal.x+dir.y*e.normal.y>0 ||
           dir.x*e.cornerNormal.x+dir.y*e.cornerNormal.y>0;
}
void fitFacingEdges(double lowPaint, double highPaint, double wireOutset,
                    double& lowPaddingEdge, double& highPaddingEdge) {
    if (lowPaint>=highPaint || lowPaddingEdge<=highPaddingEdge) return;
    // A turning shaft needs its half-width on both sides. Quantize inward
    // on the free space, leaving both painted bodies outside the channel.
    const double first=std::ceil((lowPaint+wireOutset)*64)/64;
    const double last=std::floor((highPaint-wireOutset)*64)/64;
    if (first>last) return;
    const double middle=std::round((first+(last-first)*.5)*64)/64;
    lowPaddingEdge=std::min(lowPaddingEdge,middle);
    highPaddingEdge=std::max(highPaddingEdge,middle);
}
P exitOnSide(P anchor, P dir, R bounds) {
    if (dir.x != 0) return {dir.x > 0 ? bounds.r : bounds.l, anchor.y};
    return {anchor.x, dir.y > 0 ? bounds.b : bounds.t};
}

// Same-facing ports can use one straight arm past the far target. A
// conservative miter bound should not introduce a tiny perpendicular step
// when the actual paint still leaves the normal eight-unit clearance.
void fitPassingArm(const Endpoint& from,Endpoint& past,P dir,double wireOutset) {
    const bool horizontal=dir.x!=0;
    const double across=horizontal?from.p.y:from.p.x;
    double& low=horizontal?past.obstacle.t:past.obstacle.l;
    double& high=horizontal?past.obstacle.b:past.obstacle.r;
    if(across<=low || across>=high) return;
    const auto ink=computeBounds(*past.target);
    if(!ink.finite) return;
    const double margin=obstacleClearance+wireOutset;
    if(across+margin<=(horizontal?ink.world.top:ink.world.left)) low=across;
    else if(across-margin>=(horizontal?ink.world.bottom:ink.world.right)) high=across;
}

// Leave enough room to read an outward-facing leg. When it points into the
// gutter between the two bodies, place the turn halfway across that gutter.
constexpr double portLead = 32;
double& face(R& rect,P normal) {
    if (normal.x!=0) return normal.x>0 ? rect.r : rect.l;
    return normal.y>0 ? rect.b : rect.t;
}

// An unrelated object's avoidance padding can close a visibly open side port.
// Use only the facing strip, and stop the landing before either painted body.
void fitPortSlot(Endpoint& port,const Endpoint& peer,const semantic::ObjectRecord& neighbour,R padded,
                 double wireOutset,R& obstacle) {
    const P dir=primaryDirection(port);
    const double preferred=std::max(portLead,port.arrowApproach);
    const P wanted{port.p.x+dir.x*preferred,port.p.y+dir.y*preferred};
    if (!crossing(port.p,wanted,padded) || (peer.target && crossing(port.p,wanted,peer.obstacle))) return;
    const auto other=computeBounds(neighbour);
    if (!other.finite) return;
    const bool horizontal=dir.x!=0;
    const double across=horizontal ? port.p.y : port.p.x;
    const auto& ink=other.world;
    if (across<(horizontal?ink.top:ink.left) || across>(horizontal?ink.bottom:ink.right)) return;
    const double sign=horizontal?dir.x:dir.y, origin=horizontal?port.p.x:port.p.y;
    const double near=horizontal ? (sign>0?ink.left:ink.right) : (sign>0?ink.top:ink.bottom);
    const double safe=std::floor((sign*near-wireOutset)*64)/64;
    const double paddedReach=sign*(face(padded,{-dir.x,-dir.y})-origin);
    // Keep the usual obstacle clearance whenever it already fits a complete
    // tip, its inset and a bend. Borrow padding only in tighter slots.
    const double reach=paddedReach>=port.arrowApproach ? std::min(preferred,paddedReach)
                                                       : std::min(port.arrowApproach,safe-sign*origin);
    if (reach<=0) return;
    const auto owner=computeBounds(*port.target);
    if (!owner.finite) return;
    const auto& body=owner.world;
    const double outer=horizontal ? (sign>0?body.right:body.left) : (sign>0?body.bottom:body.top);
    if (sign*origin+reach < sign*outer+wireOutset) return;
    auto& entry=face(obstacle,{-dir.x,-dir.y});
    entry=sign*std::max(sign*entry,sign*origin+reach);
    port.slotReach=std::min(port.slotReach.value_or(reach),reach);
}

P gutterExit(P anchor, P escape, R peer) {
    const bool horizontal = escape.y == anchor.y;
    const double delta = horizontal ? escape.x-anchor.x : escape.y-anchor.y;
    const double sign = delta > 0 ? 1 : -1;
    const double nearFace = horizontal ? (sign > 0 ? peer.l : peer.r)
                                       : (sign > 0 ? peer.t : peer.b);
    const double gap = (nearFace - (horizontal ? anchor.x : anchor.y))*sign;
    const double lead = std::max(std::abs(delta), gap > 0 ? gap/2 : portLead);
    return horizontal ? P{anchor.x+sign*lead,anchor.y} : P{anchor.x,anchor.y+sign*lead};
}

// Open obstacle interiors exclude a lane; their boundaries are usable.
std::pair<double,double> adjacentFreeLanes(double wanted, std::vector<std::pair<double,double>>& blocked) {
    std::sort(blocked.begin(),blocked.end());
    double lower=wanted, upper=wanted;
    for (const auto& [lo,hi]:blocked)
        if (lo<upper && upper<hi) upper=hi;
    for (auto it=blocked.rbegin();it!=blocked.rend();++it)
        if (it->first<lower && lower<it->second) lower=it->first;
    return {lower,upper};
}

// Join two parallel arms with a clear crossbar. Preferred and limit are
// signed coordinates, increasing in the arms' outward direction.
std::optional<double> outerLane(P start, P end, bool horizontal, double sign,
                                double preferred, double limit, std::span<const Obstacle> obstacles) {
    const double sideA = horizontal ? start.y : start.x;
    const double sideB = horizontal ? end.y : end.x;
    if (sideA == sideB) return {}; // A crossbar needs a nonzero span.
    const double startA = sign * (horizontal ? start.x : start.y);
    const double startB = sign * (horizontal ? end.x : end.y);
    const double minimum = std::max(startA, startB);
    double maximum = std::max(minimum,limit);
    const auto [left, right] = std::minmax(sideA, sideB);
    std::vector<std::pair<double,double>> blocked;
    for (const auto& obstacle : obstacles) {
        const auto& r = obstacle.rect;
        const double low = horizontal ? r.t : r.l, high = horizontal ? r.b : r.r;
        if (right <= low || left >= high) continue;
        const double first = sign * (horizontal ? r.l : r.t);
        const double last = sign * (horizontal ? r.r : r.b);
        const auto [entry, exit] = std::minmax(first, last);
        // An obstacle over either leg limits how far that leg can extend.
        if (low < sideA && sideA < high && exit > startA) maximum = std::min(maximum, std::max(startA, entry));
        if (low < sideB && sideB < high && exit > startB) maximum = std::min(maximum, std::max(startB, entry));
        if (exit > minimum && entry < maximum) blocked.emplace_back(entry, exit);
    }
    if (maximum < minimum) return {};
    const double wanted = std::clamp(preferred,minimum,maximum);
    // The open interiors are blocked; their inflated boundaries can carry the
    // crossbar. Prefer clearance outside the bend before trying a shorter leg.
    const auto [lower,upper]=adjacentFreeLanes(wanted,blocked);
    if (upper<=maximum) return sign*upper;
    if (lower>=minimum) return sign*lower;
    return {};
}

// If the crossbar would cut through the peer, carry it past the peer's far
// face. The other end stays on its chosen port and retains its approach.
std::optional<P> perpendicularLead(const Endpoint& owner, P exit, P lead, P bend,
                                   const Endpoint& peer, P peerLead, R peerBody,
                                   std::span<const Obstacle> obstacles, double pad) {
    if (!crossing(lead,bend,peer.obstacle)) return lead;
    const bool horizontal=exit.y==owner.p.y;
    const auto axis=[horizontal](P p){return horizontal ? p.x : p.y;};
    const double sign=axis(exit)>axis(owner.p) ? 1 : -1;
    const auto farSide=[horizontal,sign](R r) {
        return horizontal ? (sign>0 ? r.r : -r.l) : (sign>0 ? r.b : -r.t);
    };
    const double far=farSide(peer.obstacle);
    // Keep a passing line separate from the 32-unit lead of a wire
    // originating on that same face.
    constexpr double passingGap=16;
    const double limit=std::max(sign*axis(owner.p),sign*axis(peer.p))+searchPadding.back();
    if (far>limit) return {};
    const double preferred=std::min(limit,std::max({sign*axis(lead),far,farSide(peerBody)+passingGap}));
    const auto lane=outerLane(exit,peerLead,horizontal,sign,preferred,std::min(limit,preferred+pad),obstacles);
    if (!lane || sign*(*lane)<far || sign*(*lane)>limit) return {};
    return horizontal ? P{*lane,lead.y} : P{lead.x,*lane};
}

// Two lateral arms meet a common lane between their folds. The folded
// coordinate system is transposed for horizontal ports so the lane is vertical.
std::optional<double> crossingLane(P a, P b, double wanted, double pad, std::span<const R> obstacles) {
    double low=std::min(a.x,b.x)-pad, high=std::max(a.x,b.x)+pad;
    std::vector<std::pair<double,double>> blocked;
    for (const R r:obstacles) {
        for (const P arm:{a,b}) {
            if (arm.y<=r.t || arm.y>=r.b) continue;
            if (arm.x<=r.l) high=std::min(high,r.l);
            else if (arm.x>=r.r) low=std::max(low,r.r);
            else return {};
        }
        if (low>high) return {};
        if (std::min(a.y,b.y)<r.b && std::max(a.y,b.y)>r.t) blocked.emplace_back(r.l,r.r);
    }
    wanted=std::clamp(wanted,low,high);
    const auto [lower,upper]=adjacentFreeLanes(wanted,blocked);
    if (lower<low && upper>high) return {};
    return lower>=low && (upper>high || wanted-lower<=upper-wanted) ? lower : upper;
}

std::vector<P> opposingPortPath(const Endpoint& a, P start, const Endpoint& b, P end,
                                bool horizontal, double sign, double pad,
                                std::span<const Obstacle> obstacles) {
    const auto axes=[horizontal](P p){ return horizontal ? P{p.y,p.x} : p; };
    const auto bounds=[horizontal](R r){ return horizontal ? R{r.t,r.l,r.b,r.r} : r; };
    const P from=axes(a.p), to=axes(b.p), exitA=axes(start), exitB=axes(end);
    const R bodyA=bounds(a.obstacle), bodyB=bounds(b.obstacle);
    const double mid=from.y+(to.y-from.y)/2;
    const double railA=from.y+sign*std::max(portLead,std::abs(exitA.y-from.y));
    const double railB=to.y-sign*std::max(portLead,std::abs(exitB.y-to.y));
    const bool gutter=bodyA.r<=bodyB.l || bodyB.r<=bodyA.l;
    // Prefer a direct line, or a middle lane with room for both approaches.
    if (from.x==to.x && sign*(to.y-from.y)>0) return {};
    if (sign*(mid-railA)>=0 && sign*(railB-mid)>=0) return {};

    std::vector<R> blockers{bodyA,bodyB};
    for (const auto& o:obstacles)
        if (o.id!=a.target->id && o.id!=b.target->id) blockers.push_back(bounds(o.rect));
    const double middle=gutter ? (std::max(bodyA.l,bodyB.l)+std::min(bodyA.r,bodyB.r))/2
                               : (from.x+to.x)/2;
    const auto availableEnd=[&](P exit,double desired,double direction) {
        for (const R r:blockers) {
            if (exit.x<=r.l || exit.x>=r.r) continue;
            const double face=direction>0 ? r.t : r.b;
            if (direction*(face-exit.y)>=0 && direction*(desired-face)>0) desired=face;
        }
        return desired;
    };
    const auto pathAt=[&](double first,double last) -> std::vector<P> {
        first=availableEnd(exitA,first,sign);
        last=availableEnd(exitB,last,-sign);
        const P armA{from.x,first}, armB{to.x,last};
        const auto lane=crossingLane(armA,armB,middle,pad,blockers);
        const auto [left,right]=std::minmax(from.x,to.x);
        if (sign*(exitB.y-exitA.y)>=0 && (!lane || *lane<=left || *lane>=right)) {
            // Diagonally adjacent corners can leave a passage across the
            // two faces without a single clear lane through both folds.
            const double corner=(std::max(bodyA.l,bodyB.l)+std::min(bodyA.r,bodyB.r))/2;
            const P bridge{std::clamp(corner,left,right),exitA.y+(exitB.y-exitA.y)/2};
            const auto in=crossingLane(armA,bridge,(armA.x+bridge.x)/2,0,blockers);
            const auto out=crossingLane(bridge,armB,(bridge.x+armB.x)/2,0,blockers);
            if (in && out)
                return {start,axes(armA),axes({*in,first}),axes({*in,bridge.y}),
                        axes({*out,bridge.y}),axes({*out,last}),axes(armB),end};
        }
        if (lane) return {start,axes(armA),axes({*lane,first}),axes({*lane,last}),axes(armB),end};
        return {};
    };
    auto path=pathAt(railA,railB);
    if (!path.empty() || gutter) return path;

    // With adjacent bodies each nominal arm can be boxed in by the other's
    // edge. Allow the crossing arm to clear that edge to reach an outer flank.
    const double outerA=sign>0 ? std::max(railA,bodyB.b) : std::min(railA,bodyB.t);
    const double outerB=sign>0 ? std::min(railB,bodyA.t) : std::max(railB,bodyA.b);
    const double domainLow=std::min({from.y-pad,to.y-pad,exitA.y,exitB.y});
    const double domainHigh=std::max({from.y+pad,to.y+pad,exitA.y,exitB.y});
    if (outerA<domainLow || outerA>domainHigh || outerB<domainLow || outerB>domainHigh ||
        (outerA==railA && outerB==railB)) return {};
    return pathAt(outerA,outerB);
}

// The returned interior path starts and ends on the inflated target bounds.
// Ready paths must still certify every segment against all obstacles.
std::vector<P> portPath(const Endpoint& a, P start, const Endpoint& b, P end,
                        std::span<const Obstacle> obstacles, double domainPadding) {
    if (!a.target || !b.target || start==a.p || end==b.p) return {};
    const bool horizontal = start.y==a.p.y;
    const double da = horizontal ? start.x-a.p.x : start.y-a.p.y;
    if (horizontal == (end.y==b.p.y)) {
        const double db = horizontal ? end.x-b.p.x : end.y-b.p.y;
        const double sign=da>0 ? 1 : -1;
        if ((da>0)!=(db>0)) return opposingPortPath(a,start,b,end,horizontal,sign,domainPadding,obstacles);
        const double aa=(horizontal?a.p.x:a.p.y)*sign, bb=(horizontal?b.p.x:b.p.y)*sign;
        const double outer=std::max(aa,bb);
        const auto lane=outerLane(start,end,horizontal,sign,outer+portLead,outer+domainPadding,obstacles);
        if (!lane) return {};
        return horizontal ? std::vector<P>{start,{*lane,start.y},{*lane,end.y},end}
                          : std::vector<P>{start,{start.x,*lane},{end.x,*lane},end};
    }
    const P elbow=horizontal ? P{end.x,start.y} : P{start.x,end.y};
    const auto ahead=[](P anchor,P escape,P bend) {
        return (bend.x-escape.x)*(escape.x-anchor.x)+(bend.y-escape.y)*(escape.y-anchor.y)>=0;
    };
    if (ahead(a.p,start,elbow) && ahead(b.p,end,elbow)) return {start,elbow,end};
    R bodyA{},bodyB{};
    if (!aabb(*a.target,bodyA) || !aabb(*b.target,bodyB)) return {};
    const P leadA=gutterExit(a.p,start,bodyB), leadB=gutterExit(b.p,end,bodyA);
    const auto corner=[horizontal](P first,P last){return horizontal ? P{first.x,last.y} : P{last.x,first.y};};
    const P turn=corner(leadA,leadB);
    const auto first=perpendicularLead(a,start,leadA,turn,b,leadB,bodyB,obstacles,domainPadding);
    const auto last=perpendicularLead(b,end,leadB,turn,a,leadA,bodyA,obstacles,domainPadding);
    if (!first || !last) return {};
    return {start,*first,corner(*first,*last),*last,end};
}
// Resolve once in target-local f64; transform once to world. No recursive
// perimeter dependency: an attached opposite endpoint contributes its center.
double targetCornerRadius(const semantic::ObjectRecord& object) {
    for (const auto& entry : object.properties.entries)
        if (const auto* corner=std::get_if<semantic::CornerStyleV1>(&entry.value)) return corner->radius;
    return 0;
}
bool endpoint(const semantic::ObjectRecord& c,const semantic::ConnectorEndpoint& e,
              const semantic::ConnectorEndpoint& other,const semantic::ObjectRecord* target,
              const semantic::ObjectRecord* otherTarget,Endpoint& out) {
    if (const auto* f=std::get_if<semantic::FreePointEndpoint>(&e.value)) {
        out.p=world(c.transform,{f->point.x,f->point.y}); return finite(out.p);
    }
    if (!target) return false;
    double w=0,h=0; if (!dimensions(*target,w,h)) return false;
    const auto& a=std::get<semantic::AttachedEndpoint>(e.value);
    P d{};
    if (const auto* port=std::get_if<semantic::StablePortAnchor>(&a.anchor)) {
        if (port->port_id<1||port->port_id>4) return false;
        d=std::array<P,4>{{{0,-1},{1,0},{0,1},{-1,0}}}[port->port_id-1];
    } else {
        const auto& automatic=std::get<semantic::AutoPerimeterAnchor>(a.anchor);
        if (automatic.hint) d={(automatic.hint->x-0.5)*w,(automatic.hint->y-0.5)*h};
        else {
            P reference{};
            if (const auto* f=std::get_if<semantic::FreePointEndpoint>(&other.value))
                reference=world(c.transform,{f->point.x,f->point.y});
            else {
                double ow=0,oh=0; if (!otherTarget||!dimensions(*otherTarget,ow,oh)) return false;
                reference=world(otherTarget->transform,{ow*0.5,oh*0.5});
            }
            if (!inverse(target->transform,reference,d)) return false;
            d.x-=w*0.5;d.y-=h*0.5;
        }
    }
    if (!finite(d)) return false;
    if (d.x==0&&d.y==0) d={1,0};
    const double maximum=std::max(std::abs(d.x),std::abs(d.y)); d.x/=maximum;d.y/=maximum;
    const auto* shape=std::get_if<semantic::ShapeContent>(&target->content);
    P p{},n{},cornerNormal{};
    const double radius=targetCornerRadius(*target);
    semantic::Vec2 roundedPosition,roundedNormal;
    if(shape&&radius>0&&roundedShapePerimeter(*shape,radius,{d.x,d.y},roundedPosition,roundedNormal)){p={roundedPosition.x,roundedPosition.y};n={roundedNormal.x,roundedNormal.y};}
    else if (shape&&shape->shape_kind==2U) {
        const double denom=std::hypot(d.x/(w*0.5),d.y/(h*0.5));
        p={w*0.5+d.x/denom,h*0.5+d.y/denom};
        n={(p.x-w*0.5)/(w*w),(p.y-h*0.5)/(h*h)};
    } else if (shape&&shape->shape_kind==3U) {
        const double qx=d.x/w,qy=d.y/h;
        const std::array<double,3> times{2*qx+qy<0?-0.5/(2*qx+qy):INFINITY,
            -2*qx+qy<0?-0.5/(-2*qx+qy):INFINITY,qy>0?0.5/qy:INFINITY};
        const double t=*std::min_element(times.begin(),times.end());
        const std::array<P,3> normals{{{-2/w,-1/h},{2/w,-1/h},{0,1}}};
        bool haveNormal=false;
        for(std::size_t i=0;i<times.size();++i)if(times[i]==t) {
            if(!haveNormal){n=normals[i];haveNormal=true;}else cornerNormal=normals[i];
        }
        p={w*0.5+t*d.x,h*0.5+t*d.y};
    } else {
        const double tx=d.x==0?INFINITY:w*0.5/std::abs(d.x);
        const double ty=d.y==0?INFINITY:h*0.5/std::abs(d.y);
        const double t=std::min(tx,ty);
        p={w*0.5+t*d.x,h*0.5+t*d.y};
        n=tx<=ty?P{d.x>0?1.0:-1.0,0}:P{0,d.y>0?1.0:-1.0};
        if(tx==ty)cornerNormal={0,d.y>0?1.0:-1.0};
    }
    out.p=world(target->transform,p);out.target=target;
    const auto& t=target->transform;const double det=t.a*t.d-t.b*t.c;
    if (!std::isfinite(det)||std::abs(det)<1e-12) return false;
    out.normal={(t.d*n.x-t.b*n.y)/det,(-t.c*n.x+t.a*n.y)/det};
    out.cornerNormal={(t.d*cornerNormal.x-t.b*cornerNormal.y)/det,(-t.c*cornerNormal.x+t.a*cornerNormal.y)/det};
    return finite(out.p)&&finite(out.normal);
}
std::optional<Endpoint> resolvedAnchor(const semantic::ObjectRecord& target,const semantic::AnchorRef& anchor,
                                       foundation::WorldPoint towards) {
    semantic::ObjectRecord line;
    const semantic::ConnectorEndpoint attached{semantic::AttachedEndpoint{target.id,anchor}};
    const semantic::ConnectorEndpoint other{semantic::FreePointEndpoint{{towards.x,towards.y}}};
    Endpoint out;
    if(!endpoint(line,attached,other,&target,nullptr,out))return {};
    return out;
}
}

std::optional<foundation::WorldPoint> connectorAnchorPoint(const semantic::ObjectRecord& target,
    const semantic::AnchorRef& anchor, foundation::WorldPoint towards) {
    const auto out=resolvedAnchor(target,anchor,towards);
    if(!out)return {};
    return foundation::WorldPoint{float(out->p.x),float(out->p.y)};
}

std::optional<ConnectorPerimeterHit> nearestConnectorPerimeter(const semantic::ObjectRecord& target,
                                                               foundation::WorldPoint pointer) {
    double w=0,h=0;
    if(!dimensions(target,w,h))return {};
    const auto* shape=std::get_if<semantic::ShapeContent>(&target.content);
    const semantic::ShapeContent rectangle{1,w,h};
    const double radius=shape ? targetCornerRadius(target) : 0;
    const auto hint=nearestShapePerimeterHint(shape?*shape:rectangle,radius,target.transform,pointer);
    if(!hint)return {};
    const semantic::AutoPerimeterAnchor anchor{*hint};
    const auto resolved=resolvedAnchor(target,anchor,pointer);
    if(!resolved)return {};
    const P offset{pointer.x-resolved->p.x,pointer.y-resolved->p.y};
    const auto behind=[&](P normal){return offset.x*normal.x+offset.y*normal.y<=0;};
    return ConnectorPerimeterHit{anchor,{float(resolved->p.x),float(resolved->p.y)},
                                 behind(resolved->normal) && behind(resolved->cornerNormal)};
}

struct ConnectorRouteJob::Impl {
    semantic::ObjectRecord connector;
    std::span<const semantic::ObjectRecord> source;
    ConnectorGeometry result{};
    const semantic::ObjectRecord *targetA=nullptr,*targetB=nullptr;
    Endpoint a{},b{};
    // Match visible orthogonal wires and their target records as they arrive
    // in the existing bounded world scan. Either record can arrive first.
    struct BranchPeer { const semantic::ObjectRecord* object=nullptr; std::array<unsigned,2> ports{}; };
    std::map<semantic::ObjectId,BranchPeer> branchPeers;
    P branchDirection{};
    bool sharedMidline=false;
    std::optional<std::size_t> sharedCandidate;
    std::bitset<obstacleLimit> branchCrossings;
    double wireOutset=0;
    bool closeFacingBridge=false;
    std::vector<Obstacle> obstacles;
    std::vector<P> exitsA,exitsB;
    std::vector<double> xs,ys;
    std::size_t scan=0,phase=0,domainIndex=0,pairIndex=0;
    R finalDomain{},domain{};
    P start{},end{};
    // At the widest search domain, a crossing is legal only between clear
    // terminal stubs. Lexicographic cost keeps every clear path ahead of it.
    struct Cost {
        unsigned entries=0;
        double overlap=0, length=0;
        auto operator<=>(const Cost&) const = default;
        Cost extended(double extra) const { return {entries,overlap,length+extra}; }
    };
    static constexpr Cost unreached{std::numeric_limits<unsigned>::max(),INFINITY,INFINITY};
    struct Node { Cost g=unreached; unsigned bends=0; std::vector<P> path; bool closed=false; };
    using Key=std::pair<std::size_t,unsigned>;
    using Entry=std::tuple<Cost,unsigned,double,std::size_t,unsigned>;
    std::map<Key,Node> nodes;
    std::set<Entry> open;
    std::map<std::pair<unsigned,double>,std::vector<Obstacle>> axisObstacles;
    std::pair<unsigned,double> axisKey{};
    std::size_t axisScan=0;
    bool axisBuilding=false;
    Key current{}; unsigned edge=4; std::size_t predicateIndex=0; P candidate{}; bool candidateValid=false;
    Cost candidateCost{};
    std::vector<P> best;
    Cost bestCost=unreached; unsigned bestBends=0;
    // An optional second search starts only after a legal route is available.
    // Keep that route in result until an equally clear, readable one wins.
    struct CertifiedLanding { Cost cost; foundation::WorldRect domain; };
    std::optional<CertifiedLanding> shorterLanding;
    bool searching=false;
    std::vector<std::vector<P>> preferred;
    std::size_t preferredIndex=0, preferredSegment=1, preferredObstacle=0;

    Impl(const semantic::ObjectRecord& c,std::span<const semantic::ObjectRecord> s):connector(c),source(s) {
        result.status=Status::kPending;
        if (const auto* cc=std::get_if<semantic::ConnectorContent>(&c.content)) {
            for(const auto& property:c.properties.entries) {
                const auto* value=std::get_if<semantic::ConnectorDecorationValue>(&property.value);
                if(!value)continue;
                if(property.field_id==0x200U)result.arrow_start=*value==semantic::ConnectorDecorationValue::kArrow;
                if(property.field_id==0x201U)result.arrow_end=*value==semantic::ConnectorDecorationValue::kArrow;
            }
            (void)cc;
        } else fail(Status::kInvalidEndpoint);
    }
    void fail(Status s) {
        if(shorterLanding && (s==Status::kBudgetExceeded || s==Status::kNoRouteInDomain)) {
            result.status=Status::kReady;
            result.searchedDomain=foundation::unionRects(shorterLanding->domain,result.searchedDomain);
            return;
        }
        result.status=s; result.work.obstacles=static_cast<std::uint32_t>(obstacles.size());
        nodes.clear();open.clear();
        // Diagnostic chord is deliberately non-avoiding and carries its status.
        result.route={result.start,result.end};
    }
    bool prepareTargetBounds() {
        wireOutset=stroke(connector);
        if (!std::isfinite(wireOutset)) return false;
        const auto& t=connector.transform;
        const double area=std::abs(t.a*t.d-t.b*t.c);
        for (auto* e : {&a,&b}) if(e->target) {
            if (!obstacleBounds(*e->target,wireOutset,e->obstacle)) return false;
            // Convert the arrow and bend to a run along this world axis. The
            // anchor gap is already in board units, independent of the camera.
            const double axis=primaryDirection(*e).x!=0 ? std::hypot(t.d,t.b) : std::hypot(t.c,t.a);
            e->arrowApproach=std::min(searchPadding.back(),connectorTipGap+
                (connectorNominalArrowLength+connectorBendClearance)*(area/axis));
        }
        if (!a.target || !b.target || a.target==b.target) return true;
        const P dir=primaryDirection(a);
        if(dir==primaryDirection(b)) {
            const double ahead=dir.x*(b.p.x-a.p.x)+dir.y*(b.p.y-a.p.y);
            if(ahead>0) fitPassingArm(a,b,dir,wireOutset);
            if(ahead<0) fitPassingArm(b,a,dir,wireOutset);
        }
        if(!intersects(a.obstacle,b.obstacle)) return true;
        const auto aBounds=computeBounds(*a.target), bBounds=computeBounds(*b.target);
        if (!aBounds.finite || !bBounds.finite) return false;
        const auto& paintA=aBounds.world;
        const auto& paintB=bBounds.world;
        fitFacingEdges(paintA.right,paintB.left,wireOutset,a.obstacle.r,b.obstacle.l);
        fitFacingEdges(paintB.right,paintA.left,wireOutset,b.obstacle.r,a.obstacle.l);
        fitFacingEdges(paintA.bottom,paintB.top,wireOutset,a.obstacle.b,b.obstacle.t);
        fitFacingEdges(paintB.bottom,paintA.top,wireOutset,b.obstacle.b,a.obstacle.t);
        // Two aligned, outward-facing ports can also bridge a visible slit
        // too narrow for a turn. The candidate still checks every third object.
        const P d{b.p.x-a.p.x,b.p.y-a.p.y};
        closeFacingBridge=pointsOut(a,d) && pointsOut(b,{-d.x,-d.y}) &&
            ((d.y==0 && (paintA.right<paintB.left || paintB.right<paintA.left)) ||
             (d.x==0 && (paintA.bottom<paintB.top || paintB.bottom<paintA.top)));
        return true;
    }
    void prepareBranches(const semantic::ConnectorContent& cc) {
        if (!fixedAttachment(cc.start) || !fixedAttachment(cc.end) || targetA==targetB ||
            !visible(*targetA) || !visible(*targetB) || !visible(connector)) return;
        const P first=primaryDirection(a), last=primaryDirection(b);
        const double separation=first.x*(b.p.x-a.p.x)+first.y*(b.p.y-a.p.y);
        if (first==P{-last.x,-last.y} && separation>=2*portLead) branchDirection=first;
    }
    void matchBranch(const BranchPeer& peer,unsigned side) {
        if (!peer.object || !peer.ports[side] || sharedMidline) return;
        const auto& expected=side==0 ? a : b;
        const auto direction=side==0 ? branchDirection : P{-branchDirection.x,-branchDirection.y};
        const bool horizontal=branchDirection.x!=0;
        for(unsigned port=1;port<=4;++port) {
            if (!(peer.ports[side] & (1U<<(port-1)))) continue;
            const auto e=resolvedAnchor(*peer.object,semantic::StablePortAnchor{port},{});
            if (!e || primaryDirection(*e)!=direction) continue;
            const auto p=e->p, q=expected.p;
            if (branchCoordinate(horizontal?p.x:p.y)==branchCoordinate(horizontal?q.x:q.y) &&
                branchCoordinate(horizontal?p.y:p.x)!=branchCoordinate(horizontal?q.y:q.x)) {
                sharedMidline=true;return;
            }
        }
    }
    void observeBranchWire(const semantic::ConnectorContent& current,const semantic::ConnectorContent& other) {
        if (other.routing!=semantic::ConnectorRouting::kOrthogonal) return;
        const auto link=[&](const auto& hub,const auto& far) {
            if (hub!=current.start && hub!=current.end) return;
            const auto* attachment=fixedAttachment(far);
            if (!attachment || attachment->target_object_id==targetA->id || attachment->target_object_id==targetB->id) return;
            const unsigned side=hub==current.start ? 1 : 0;
            const auto port=std::get<semantic::StablePortAnchor>(attachment->anchor).port_id;
            auto& peer=branchPeers[attachment->target_object_id];
            const auto bit=1U<<(port-1);
            if (!(peer.ports[side]&bit)) { peer.ports[side]|=bit;matchBranch(peer,side); }
        };
        link(other.start,other.end);link(other.end,other.start);
    }
    void observeBranchTarget(const semantic::ObjectRecord& object,R rect) {
        const bool horizontal=branchDirection.x!=0;
        const double lo=horizontal?rect.l:rect.t, hi=horizontal?rect.r:rect.b;
        const auto near=[&](P p){ const double v=horizontal?p.x:p.y;return v>=lo-1./64 && v<=hi+1./64; };
        if (!near(a.p) && !near(b.p)) return;
        auto& peer=branchPeers[object.id];peer.object=&object;
        matchBranch(peer,0);matchBranch(peer,1);
    }
    bool predicate(P from,P to,const Obstacle& o) {
        if(result.work.predicates>=65536U) {fail(Status::kBudgetExceeded);return false;}
        ++result.work.predicates;
        return !crossing(from,to,o.rect);
    }
    P point(std::size_t v) const {return {xs[v%xs.size()],ys[v/xs.size()]};}
    std::size_t vertex(P p) const {
        return static_cast<std::size_t>(std::lower_bound(xs.begin(),xs.end(),p.x)-xs.begin())+
            xs.size()*static_cast<std::size_t>(std::lower_bound(ys.begin(),ys.end(),p.y)-ys.begin());
    }
    Entry entry(Key k,const Node& n) const {return {n.g.extended(distance(point(k.first),end)),n.bends,-n.g.length,k.first,k.second};}
    void finishPath(const std::vector<P>& path,Status status=Status::kReady,
                    const std::bitset<obstacleLimit>* permittedCrossings=nullptr) {
        result.route.clear();
        for(const auto p:path) {P local{};if(!inverse(connector.transform,p,local)){fail(Status::kInvalidEndpoint);return;}
            result.route.push_back({static_cast<float>(local.x),static_cast<float>(local.y)});}
        result.sketchObstacles.clear();
        for(std::size_t i=0;i<obstacles.size();++i){
            // Rounding and sketching can follow an intentional crossing.
            // The terminal bodies and all unpierced obstacles stay protected.
            if(permittedCrossings && permittedCrossings->test(i))continue;
            const auto& obstacle=obstacles[i];
            // A terminal's reserved straight approach constrains the route.
            // Its fillet only needs to clear the body's avoidance bounds.
            const R& r=a.slotReach && obstacle.id==a.target->id ? a.obstacle
                      : b.slotReach && obstacle.id==b.target->id ? b.obstacle : obstacle.rect;
            foundation::WorldRect box{};bool first=true;
            for(auto p:std::array<P,4>{{{r.l,r.t},{r.r,r.t},{r.r,r.b},{r.l,r.b}}}){P local{};if(!inverse(connector.transform,p,local))continue;foundation::WorldRect point{static_cast<float>(local.x),static_cast<float>(local.y),static_cast<float>(local.x),static_cast<float>(local.y)};box=first?point:foundation::unionRects(box,point);first=false;}
            if(!first)result.sketchObstacles.push_back(box);
        }
        result.status=status; result.work.obstacles=static_cast<std::uint32_t>(obstacles.size());
    }
    void considerPath(std::vector<P> path, Cost cost) {
        path=simplified(std::move(path));
        if(path.size()>256U){fail(Status::kBudgetExceeded);return;}
        cost.length=0;
        for(std::size_t i=1;i<path.size();++i)cost.length+=distance(path[i-1],path[i]);
        const auto bends=path.size()>1?static_cast<unsigned>(path.size()-2):0U;
        if(cost<bestCost||(cost==bestCost&&(bends<bestBends||
            (bends==bestBends&&pathLess(path,best))))) {
            best=std::move(path);bestCost=cost;bestBends=bends;
        }
    }
    std::vector<P> exits(const Endpoint& e) {
        if (!e.target) return {e.p};
        const auto found=std::find_if(obstacles.begin(),obstacles.end(),[&](const auto& o){return o.id==e.target->id;});
        if(found==obstacles.end()) return {};
        std::vector<P> out;
        for(const auto dir:std::array<P,4>{{{1,0},{0,1},{-1,0},{0,-1}}}) {
            if(!pointsOut(e,dir)) continue;
            const P p=exitOnSide(e.p,dir,found->rect);
            bool clear=true;
            for(const auto& o:obstacles) if(o.id!=e.target->id&&!predicate(e.p,p,o)) {clear=false;break;}
            if(clear) out.push_back(p);
        }
        return out;
    }
    P landingGate(bool atStart,bool& grew) {
        const auto& port=atStart?a:b;
        const auto& exits=atStart?exitsA:exitsB;
        const auto i=atStart?0U:best.size()-1,j=atStart?1U:i-1;
        const double run=distance(best[i],best[j]);
        const P dir{(best[j].x-best[i].x)/run,(best[j].y-best[i].y)/run};
        const auto along=[&](P p){return dir.x*(p.x-port.p.x)+dir.y*(p.y-port.p.y);};
        const auto at=[&](double d){return P{port.p.x+dir.x*d,port.p.y+dir.y*d};};
        const auto exit=std::find_if(exits.begin(),exits.end(),[&](P p){return along(p)>0;});
        const P base=exit==exits.end()?port.p:*exit;
        if (!(atStart?result.arrow_start:result.arrow_end)) return base;
        const auto p=result.route[i],q=result.route[j];
        const double local=std::hypot(double(q.x)-p.x,double(q.y)-p.y);
        if (local<=0) return best[j];
        const double gap=port.target?connectorTipGap:0.;
        const double comfortable=gap+(connectorNominalArrowLength+connectorBendClearance)*(run/local);
        const double inset=port.target?connectorAnchorInsetFraction(run,local,true):0.;
        if (!port.target || connectorArrowLength(local*(1-inset),best.size(),false)>=connectorNominalArrowLength-1e-5)
            return at(std::min(run,std::max(along(base),comfortable)));

        double reach=std::clamp(comfortable,portLead,searchPadding.back());
        if(port.slotReach && dir==primaryDirection(port)) reach=std::min(reach,*port.slotReach);
        if(reach<=run+1e-5) return best[j];
        for(const auto& o:obstacles) if(o.id!=port.target->id && !predicate(port.p,at(reach),o))
            reach=std::min(reach,along(exitOnSide(port.p,{-dir.x,-dir.y},o.rect)));
        if(reach<=run+1e-5) return best[j];
        grew=true;
        return at(reach);
    }
    bool retryShortLanding() {
        if(result.status!=Status::kReady || best.size()<3 ||
           (!(a.target && result.arrow_start) && !(b.target && result.arrow_end)) ||
           result.work.predicates+landingScanBound()>65536U) return false;
        bool grew=false;
        const P first=landingGate(true,grew),last=landingGate(false,grew);
        if(!grew) return false;
        if (bestCost.entries || bestCost.overlap>0) {
            // The grid accounts for crossings between the gates. A longer
            // gate must not hide a previously charged crossing in its stub.
            const auto clear=[&](const Endpoint& port,P gate) {
                return gate==port.p || std::all_of(obstacles.begin(),obstacles.end(),[&](const auto& o){
                    return (port.target && o.id==port.target->id) || predicate(port.p,gate,o);
                });
            };
            if(!clear(a,first) || !clear(b,last))return false;
        }
        shorterLanding=CertifiedLanding{bestCost,result.searchedDomain};
        result.status=Status::kPending;
        exitsA={first};exitsB={last};pairIndex=0;
        best.clear();bestCost=unreached;bestBends=0;
        return true;
    }
    std::size_t landingScanBound() const {
        return (bestCost.entries || bestCost.overlap>0 ? 4U : 2U)*obstacles.size();
    }
    bool foldsBackAtGate(P from,P to) const {
        const auto behind=[](P anchor,P gate,P other) {
            return (other.x-gate.x)*(gate.x-anchor.x)+(other.y-gate.y)*(gate.y-anchor.y)<0;
        };
        return (from==start && behind(a.p,start,to)) || (to==end && behind(b.p,end,from));
    }
    bool setupAttempt() {
        if(pairIndex>=exitsA.size()*exitsB.size()) {
            if(shorterLanding) {
                const auto& before=shorterLanding->cost;
                // Simplification can collapse coincident or collinear gates.
                // Only replace the saved route if both final arms reach them.
                if (best.size()>=2 && !foldsBackAtGate(start,best[1]) && !foldsBackAtGate(best[best.size()-2],end)
                    && std::tie(bestCost.entries,bestCost.overlap)<=std::tie(before.entries,before.overlap))
                    finishPath(best);
                else result.status=Status::kReady;
                result.searchedDomain=foundation::unionRects(shorterLanding->domain,result.searchedDomain);
                return false;
            }
            if(!best.empty()) {
                finishPath(best);
                if(!retryShortLanding())return false;
            } else {pairIndex=0;++domainIndex;}
        }
        if(domainIndex>=searchPadding.size()) {fail(Status::kNoRouteInDomain);return false;}
        start=exitsA[pairIndex/exitsB.size()]; end=exitsB[pairIndex%exitsB.size()]; ++pairIndex;
        // Certify common interactive routes before opening the general search.
        // The first/last escape stubs already obey each port's outward normal.
        // Test the middle lane before equal-length routes along object edges.
        preferred.clear();preferredIndex=0;preferredSegment=1;preferredObstacle=0;
        sharedCandidate.reset();branchCrossings.reset();
        const double pad=searchPadding[domainIndex];
        // Two aligned escape points can be on the same side of their shapes.
        // In that case their direct interior segment would collapse the U.
        if(auto path=portPath(a,start,b,end,obstacles,pad); !path.empty()) preferred.push_back(std::move(path));
        if(start.x==end.x||start.y==end.y)preferred.push_back({start,end});
        const bool horizontalA=start!=a.p&&start.y==a.p.y;
        const bool horizontalB=end!=b.p&&end.y==b.p.y;
        const bool verticalA=start!=a.p&&start.x==a.p.x;
        const bool verticalB=end!=b.p&&end.x==b.p.x;
        const bool horizontal=horizontalA||horizontalB, vertical=verticalA||verticalB;
        if(horizontal!=vertical) {
            const auto axis=[horizontal](P p){return horizontal?p.x:p.y;};
            double mid=axis(a.p)+(axis(b.p)-axis(a.p))*.5;
            const double sign=horizontal?branchDirection.x:branchDirection.y;
            if(!shorterLanding && sharedMidline && sign*(axis(start)-axis(a.p))>0 && sign*(axis(end)-axis(b.p))<0) {
                const double common=(branchCoordinate(axis(a.p))+branchCoordinate(axis(b.p)))*.5;
                if(sign*(common-axis(start))>=0 && sign*(axis(end)-common)>=0) {
                    mid=common;sharedCandidate=preferred.size();
                }
            }
            preferred.push_back(horizontal ? std::vector<P>{start,{mid,start.y},{mid,end.y},end}
                                           : std::vector<P>{start,{start.x,mid},{end.x,mid},end});
        }
        preferred.push_back({start,{start.x,end.y},end});
        preferred.push_back({start,{end.x,start.y},end});
        if(shorterLanding)std::erase_if(preferred,[&](const auto& path) {
            for(std::size_t i=1;i<path.size();++i)
                if(foldsBackAtGate(path[i-1],path[i]))return true;
            return false;
        });
        domain={std::min(a.p.x,b.p.x)-pad,std::min(a.p.y,b.p.y)-pad,
                std::max(a.p.x,b.p.x)+pad,std::max(a.p.y,b.p.y)+pad};
        // The certified candidates can wrap the far face of an attached
        // target. Include their turning points in the reported search area.
        for (const auto& path:preferred) for (P p:path) {
            domain.l=std::min(domain.l,p.x);domain.r=std::max(domain.r,p.x);
            domain.t=std::min(domain.t,p.y);domain.b=std::max(domain.b,p.y);
        }
        result.searchedDomain={static_cast<float>(domain.l),static_cast<float>(domain.t),static_cast<float>(domain.r),static_cast<float>(domain.b)};
        xs={domain.l,domain.r,start.x,end.x};ys={domain.t,domain.b,start.y,end.y};
        for(const auto& o:obstacles) if(intersects(o.rect,domain)) {
            if(o.rect.l>=domain.l&&o.rect.l<=domain.r) xs.push_back(o.rect.l);
            if(o.rect.r>=domain.l&&o.rect.r<=domain.r) xs.push_back(o.rect.r);
            if(o.rect.t>=domain.t&&o.rect.t<=domain.b) ys.push_back(o.rect.t);
            if(o.rect.b>=domain.t&&o.rect.b<=domain.b) ys.push_back(o.rect.b);
        }
        const auto unique=[](auto& v){std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());};
        unique(xs);unique(ys);nodes.clear();open.clear();axisObstacles.clear();edge=4;
        Node n{};n.g={0,0,distance(a.p,start)};n.path={a.p};if(start!=a.p)n.path.push_back(start);
        const unsigned direction=start==a.p?2U:start.x==a.p.x?1U:0U;
        const Key k{vertex(start),direction};nodes.emplace(k,n);open.insert(entry(k,n));searching=true;
        return true;
    }
    Status advance() {
        if(result.status!=Status::kPending)return result.status;
        const auto examined=result.work.examined, expanded=result.work.expanded,predicates=result.work.predicates;
        const auto& cc=std::get<semantic::ConnectorContent>(connector.content);
        while(result.status==Status::kPending) {
            if(result.work.examined-examined>=1024U||result.work.expanded-expanded>=1024U||
               result.work.predicates-predicates>=8192U) return result.status;
            if(phase==0) {
                const bool needA=std::holds_alternative<semantic::AttachedEndpoint>(cc.start.value)&&!targetA;
                const bool needB=std::holds_alternative<semantic::AttachedEndpoint>(cc.end.value)&&!targetB;
                if((needA||needB)&&scan<source.size()) {
                    if(result.work.examined>=8192U){fail(Status::kBudgetExceeded);break;}
                    const auto& o=source[scan++];++result.work.examined;
                    if(const auto* v=std::get_if<semantic::AttachedEndpoint>(&cc.start.value);v&&v->target_object_id==o.id)targetA=&o;
                    if(const auto* v=std::get_if<semantic::AttachedEndpoint>(&cc.end.value);v&&v->target_object_id==o.id)targetB=&o;
                    continue;
                }
                if(!endpoint(connector,cc.start,cc.end,targetA,targetB,a)||
                   !endpoint(connector,cc.end,cc.start,targetB,targetA,b)) {fail(Status::kInvalidEndpoint);break;}
                P la{},lb{};
                if(!inverse(connector.transform,a.p,la)||!inverse(connector.transform,b.p,lb)){fail(Status::kInvalidEndpoint);break;}
                const double limit=std::numeric_limits<float>::max();
                if(std::abs(la.x)>limit||std::abs(la.y)>limit||std::abs(lb.x)>limit||std::abs(lb.y)>limit) {
                    fail(Status::kInvalidGeometry);break;
                }
                result.start={static_cast<float>(la.x),static_cast<float>(la.y)};
                result.end={static_cast<float>(lb.x),static_cast<float>(lb.y)};
                if(a.p==b.p){fail(Status::kDegenerate);break;}
                if(cc.routing==semantic::ConnectorRouting::kStraight){finishPath({a.p,b.p});break;}
                if(!prepareTargetBounds()){fail(Status::kInvalidGeometry);break;}
                prepareBranches(cc);
                // A provisional orthogonal path follows the current endpoints
                // during bounded work. It remains Pending until collision
                // checks/search certify a route; never publish it as Ready.
                const auto previewExit=[](const Endpoint& e) {
                    if(!e.target) return e.p;
                    return exitOnSide(e.p,primaryDirection(e),e.obstacle);
                };
                std::vector<P> provisional{a.p};
                // Preview can see both target bounds before the first search
                // domain opens, including the far flank of two touching shapes.
                auto ports=portPath(a,previewExit(a),b,previewExit(b),{},searchPadding.back());
                if(!ports.empty()) {
                    provisional.insert(provisional.end(),ports.begin(),ports.end());
                } else if(a.p.x!=b.p.x&&a.p.y!=b.p.y) {
                    const auto vertical=[](const Endpoint& e) {
                        return e.target&&std::abs(e.normal.y+e.cornerNormal.y)>=
                            std::abs(e.normal.x+e.cornerNormal.x);
                    };
                    const bool va=vertical(a),vb=vertical(b);
                    const bool ha=a.target&&!va,hb=b.target&&!vb;
                    if((va||vb)&&!ha&&!hb) {
                        const double mid=a.p.y+(b.p.y-a.p.y)*0.5;
                        provisional.push_back({a.p.x,mid});provisional.push_back({b.p.x,mid});
                    } else if((ha||hb)&&!va&&!vb) {
                        const double mid=a.p.x+(b.p.x-a.p.x)*0.5;
                        provisional.push_back({mid,a.p.y});provisional.push_back({mid,b.p.y});
                    } else provisional.push_back(ha?P{b.p.x,a.p.y}:P{a.p.x,b.p.y});
                }
                provisional.push_back(b.p);
                finishPath(simplified(std::move(provisional)),Status::kPending);
                if(result.status!=Status::kPending)break;
                const double pad=searchPadding.back();
                finalDomain={std::min(a.p.x,b.p.x)-pad,std::min(a.p.y,b.p.y)-pad,
                             std::max(a.p.x,b.p.x)+pad,std::max(a.p.y,b.p.y)+pad};
                phase=1;scan=0;
                continue;
            } else if(phase==1) {
                if(scan<source.size()) {
                    if(result.work.examined>=8192U){fail(Status::kBudgetExceeded);break;}
                    const auto& o=source[scan++];++result.work.examined;
                    const bool findPeers=branchDirection!=P{} && !sharedMidline;
                    if(findPeers && o.kind==semantic::ObjectKind::kConnector && o.id!=connector.id && visible(o))
                        if(const auto* other=std::get_if<semantic::ConnectorContent>(&o.content))observeBranchWire(cc,*other);
                    if(!admitted(o)||o.id==connector.id||(!visible(o) && &o!=targetA && &o!=targetB))continue;
                    R r{};
                    if (&o==targetA) r=a.obstacle;
                    else if (&o==targetB) r=b.obstacle;
                    else if (!obstacleBounds(o,wireOutset,r)){fail(Status::kInvalidGeometry);break;}
                    if(findPeers && &o!=targetA && &o!=targetB)observeBranchTarget(o,r);
                    if(!intersects(r,finalDomain))continue;
                    if(obstacles.size()>=obstacleLimit){fail(Status::kBudgetExceeded);break;}
                    if (&o!=targetA && &o!=targetB && targetA!=targetB) {
                        const R padded=r;
                        if (fixedAttachment(cc.start)) fitPortSlot(a,b,o,padded,wireOutset,r);
                        if (fixedAttachment(cc.end)) fitPortSlot(b,a,o,padded,wireOutset,r);
                    }
                    obstacles.push_back({o.id,r,&o==targetA || &o==targetB});continue;
                }
                std::sort(obstacles.begin(),obstacles.end(),[](const auto& x,const auto& y){return x.id<y.id;});
                for (auto* port : {&a,&b}) if (port->slotReach) {
                    const P dir=primaryDirection(*port);
                    const double sign=dir.x!=0?dir.x:dir.y;
                    const double turn=(dir.x!=0?port->p.x:port->p.y)+sign*(*port->slotReach);
                    for (auto& o:obstacles) if (o.id==port->target->id) face(o.rect,dir)=turn;
                    auto& bodyFace=face(port->obstacle,dir);
                    bodyFace=sign*std::min(sign*bodyFace,sign*turn);
                }
                if(closeFacingBridge) {
                    const bool clear=std::all_of(obstacles.begin(),obstacles.end(),[&](const Obstacle& o){
                        return o.id==targetA->id || o.id==targetB->id || predicate(a.p,b.p,o);
                    });
                    if(result.status!=Status::kPending) break;
                    if(clear){finishPath({a.p,b.p});break;}
                }
                for(const auto& o:obstacles) if((!a.target&&inside(a.p,o.rect))||(!b.target&&inside(b.p,o.rect))) {
                    fail(Status::kNoRouteInDomain);return result.status;
                }
                exitsA=exits(a);exitsB=exits(b);
                if(result.status!=Status::kPending)break;
                if(exitsA.empty()||exitsB.empty()){fail(Status::kNoRouteInDomain);break;}
                phase=2;
                // Small scenes can finish in the same quantum. The shared
                // counters still enforce the limit before every next step.
                continue;
            } else {
                // Reserve this quantum's bounded port scans before
                // considering an optional refinement of a certified route.
                if(!searching && !shorterLanding && !best.empty() && pairIndex>=exitsA.size()*exitsB.size() &&
                   result.work.predicates-predicates+landingScanBound()>8192U) return result.status;
                if(!searching&&!setupAttempt())break;
                if(preferredIndex<preferred.size()) {
                    const auto& path=preferred[preferredIndex];
                    if(preferredSegment>=path.size()) {
                        auto complete=path;complete.insert(complete.begin(),a.p);complete.push_back(b.p);
                        if(sharedCandidate==preferredIndex) {
                            finishPath(simplified(std::move(complete)),Status::kReady,&branchCrossings);break;
                        }
                        // A corner can have several legal escape directions.
                        // Compare their complete routes, rather than accepting
                        // the first sideways detour before trying a direct exit.
                        considerPath(std::move(complete),{});
                        if(result.status!=Status::kPending)break;
                        if(!shorterLanding&&bestBends==0&&bestCost.entries==0&&bestCost.length==distance(a.p,b.p)) {finishPath(best);break;}
                        searching=false;continue;
                    }
                    if(preferredObstacle>=obstacles.size()) {
                        ++preferredSegment;preferredObstacle=0;continue;
                    }
                    const auto obstacleIndex=preferredObstacle++;
                    const auto& obstacle=obstacles[obstacleIndex];
                    if(!predicate(path[preferredSegment-1],path[preferredSegment],obstacle)) {
                        // A common branch line may pass through intervening objects.
                        // Both targets and the two escape stubs retain their clearance.
                        if(result.status==Status::kPending && sharedCandidate==preferredIndex && !obstacle.terminal) {
                            branchCrossings.set(obstacleIndex);continue;
                        }
                        ++preferredIndex;preferredSegment=1;preferredObstacle=0;
                    }
                    continue;
                }
                if(edge>=4) {
                    if(open.empty() || (!best.empty() && std::get<0>(*open.begin()).extended(distance(end,b.p))>bestCost)) {
                        searching=false;continue;
                    }
                    if(result.work.expanded>=8192U){fail(Status::kBudgetExceeded);break;}
                    const auto top=*open.begin();open.erase(open.begin());current={std::get<3>(top),std::get<4>(top)};
                    auto& n=nodes.at(current);n.closed=true;++result.work.expanded;
                    if(!best.empty()&&n.bends>bestBends&&
                       n.g.extended(distance(point(current.first),end)+distance(end,b.p))>=bestCost)continue;
                    if(point(current.first)==end) {
                        auto path=n.path;if(end!=b.p)path.push_back(b.p);
                        considerPath(std::move(path),n.g);
                        edge=4;continue;
                    }
                    edge=0;predicateIndex=0;candidateValid=false;
                }
                const auto from=point(current.first);
                if(!candidateValid) {
                    const auto xi=current.first%xs.size(), yi=current.first/xs.size();
                    if((edge==0&&xi+1>=xs.size())||(edge==1&&yi+1>=ys.size())||
                       (edge==2&&xi==0)||(edge==3&&yi==0)){++edge;continue;}
                    candidate=point(edge==0?current.first+1:edge==1?current.first+xs.size():
                                    edge==2?current.first-1:current.first-xs.size());
                    if(shorterLanding && foldsBackAtGate(from,candidate)){++edge;continue;}
                    candidateValid=true;predicateIndex=0;
                    candidateCost=nodes.at(current).g.extended(distance(from,candidate));
                    axisKey={edge%2U,edge%2U==0?from.y:from.x};
                    axisBuilding=!axisObstacles.contains(axisKey);axisScan=0;
                    if(axisBuilding)axisObstacles.emplace(axisKey,std::vector<Obstacle>{});
                }
                auto& axis=axisObstacles.at(axisKey);
                while(axisBuilding&&axisScan<obstacles.size()) {
                    if(result.work.predicates-predicates>=8192U)return result.status;
                    if(result.work.predicates>=65536U){fail(Status::kBudgetExceeded);return result.status;}
                    ++result.work.predicates;
                    const auto& o=obstacles[axisScan++];
                    if(axisKey.first==0?from.y>o.rect.t&&from.y<o.rect.b:from.x>o.rect.l&&from.x<o.rect.r)
                        axis.push_back(o);
                }
                axisBuilding=false;
                bool clear=true;
                while(predicateIndex<axis.size()) {
                    if(result.work.predicates-predicates>=8192U)return result.status;
                    const auto& obstacle=axis[predicateIndex++];
                    if(predicate(from,candidate,obstacle))continue;
                    if(result.status!=Status::kPending || domainIndex+1<searchPadding.size() || obstacle.terminal) {
                        clear=false;break;
                    }
                    // Grid edges are split at every obstacle boundary. Charge
                    // one entry per passage, independent of grid subdivisions.
                    if(!inside(from,obstacle.rect))++candidateCost.entries;
                    candidateCost.overlap+=distance(from,candidate);
                }
                if(result.status!=Status::kPending)break;
                if(clear) {
                    const auto& n=nodes.at(current);const unsigned direction=(edge%2U==0)?0U:1U;
                    const Key k{vertex(candidate),direction};
                    const auto g=candidateCost;
                    const unsigned bends=n.bends+((current.second<2U&&current.second!=direction)?1U:0U);
                    auto path=n.path;path.push_back(candidate);path=simplified(std::move(path));
                    if(path.size()>256U){fail(Status::kBudgetExceeded);break;}
                    auto it=nodes.find(k);
                    if(it==nodes.end()) {
                        if(nodes.size()>=8192U||open.size()>=8192U){fail(Status::kBudgetExceeded);break;}
                        it=nodes.emplace(k,Node{}).first;
                        result.work.retained=std::max(result.work.retained,static_cast<std::uint32_t>(nodes.size()));
                    }
                    auto& next=it->second;
                    if(g<next.g||(g==next.g&&(bends<next.bends||(bends==next.bends&&pathLess(path,next.path))))) {
                        open.erase(entry(k,next));next={g,bends,std::move(path),false};open.insert(entry(k,next));
                    }
                }
                ++edge;candidateValid=false;predicateIndex=0;
            }
        }
        return result.status;
    }
};
ConnectorRouteJob::ConnectorRouteJob(const semantic::ObjectRecord& c,std::span<const semantic::ObjectRecord> s)
    :impl_(std::make_unique<Impl>(c,s)) {}
ConnectorRouteJob::~ConnectorRouteJob()=default;
ConnectorRouteJob::ConnectorRouteJob(ConnectorRouteJob&&) noexcept=default;
ConnectorRouteJob& ConnectorRouteJob::operator=(ConnectorRouteJob&&) noexcept=default;
runtime::ConnectorRouteStatus ConnectorRouteJob::advance(){return impl_->advance();}
void ConnectorRouteJob::cancel() noexcept {if(impl_->result.status==Status::kPending)impl_->fail(Status::kCancelled);}
const ConnectorGeometry& ConnectorRouteJob::result() const noexcept {return impl_->result;}
}
