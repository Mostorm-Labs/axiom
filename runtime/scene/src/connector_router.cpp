#include "canvas/scene/connector_router.hpp"
#include <algorithm>
#include <array>
#include <cmath>
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
struct Endpoint { P p{},normal{},cornerNormal{}; const semantic::ObjectRecord* target=nullptr; };
// Resolve once in target-local f64; transform once to world. No recursive
// perimeter dependency: an attached opposite endpoint contributes its center.
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
    if (shape&&shape->shape_kind==2U) {
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
}

struct ConnectorRouteJob::Impl {
    semantic::ObjectRecord connector;
    std::span<const semantic::ObjectRecord> source;
    ConnectorGeometry result{};
    const semantic::ObjectRecord *targetA=nullptr,*targetB=nullptr;
    Endpoint a{},b{};
    struct Obstacle { semantic::ObjectId id; R rect; };
    std::vector<Obstacle> obstacles;
    std::vector<P> exitsA,exitsB;
    std::vector<double> xs,ys;
    std::size_t scan=0,phase=0,domainIndex=0,pairIndex=0;
    R finalDomain{},domain{};
    P start{},end{};
    struct Node { double g=INFINITY; unsigned bends=0; std::vector<P> path; bool closed=false; };
    using Key=std::pair<std::size_t,unsigned>;
    using Entry=std::tuple<double,unsigned,double,std::size_t,unsigned>;
    std::map<Key,Node> nodes;
    std::set<Entry> open;
    std::map<std::pair<unsigned,double>,std::vector<Obstacle>> axisObstacles;
    std::pair<unsigned,double> axisKey{};
    std::size_t axisScan=0;
    bool axisBuilding=false;
    Key current{}; unsigned edge=4; std::size_t predicateIndex=0; P candidate{}; bool candidateValid=false;
    std::vector<P> best;
    double bestLength=INFINITY; unsigned bestBends=0;
    bool searching=false;

    Impl(const semantic::ObjectRecord& c,std::span<const semantic::ObjectRecord> s):connector(c),source(s) {
        result.status=Status::kPending;
        if (const auto* cc=std::get_if<semantic::ConnectorContent>(&c.content)) {
            for(const auto& p:c.properties.entries) if(p.field_id==0x201U)
                if(const auto* v=std::get_if<semantic::ConnectorDecorationValue>(&p.value)) result.arrow_end=*v==semantic::ConnectorDecorationValue::kArrow;
            (void)cc;
        } else fail(Status::kInvalidEndpoint);
    }
    void fail(Status s) {
        result.status=s; result.work.obstacles=static_cast<std::uint32_t>(obstacles.size());
        nodes.clear();open.clear();
        // Diagnostic chord is deliberately non-avoiding and carries its status.
        result.route={result.start,result.end};
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
    Entry entry(Key k,const Node& n) const {return {n.g+distance(point(k.first),end),n.bends,-n.g,k.first,k.second};}
    void finishPath(const std::vector<P>& path) {
        result.route.clear();
        for(const auto p:path) {P local{};if(!inverse(connector.transform,p,local)){fail(Status::kInvalidEndpoint);return;}
            result.route.push_back({static_cast<float>(local.x),static_cast<float>(local.y)});}
        result.status=Status::kReady; result.work.obstacles=static_cast<std::uint32_t>(obstacles.size());
    }
    std::vector<P> exits(const Endpoint& e) {
        if (!e.target) return {e.p};
        const auto found=std::find_if(obstacles.begin(),obstacles.end(),[&](const auto& o){return o.id==e.target->id;});
        if(found==obstacles.end()) return {};
        std::vector<P> out;
        for(const auto dir:std::array<P,4>{{{1,0},{0,1},{-1,0},{0,-1}}}) {
            if(dir.x*e.normal.x+dir.y*e.normal.y<=0 &&
               dir.x*e.cornerNormal.x+dir.y*e.cornerNormal.y<=0) continue;
            P p=e.p;
            if(dir.x>0) p.x=found->rect.r; else if(dir.x<0) p.x=found->rect.l;
            else if(dir.y>0) p.y=found->rect.b;else p.y=found->rect.t;
            bool clear=true;
            for(const auto& o:obstacles) if(o.id!=e.target->id&&!predicate(e.p,p,o)) {clear=false;break;}
            if(clear) out.push_back(p);
        }
        return out;
    }
    bool setupAttempt() {
        if(pairIndex>=exitsA.size()*exitsB.size()) {
            if(!best.empty()) {finishPath(best);return false;}
            pairIndex=0;++domainIndex;
        }
        if(domainIndex>=3) {fail(Status::kNoRouteInDomain);return false;}
        start=exitsA[pairIndex/exitsB.size()]; end=exitsB[pairIndex%exitsB.size()]; ++pairIndex;
        const double pad=std::array<double,3>{64,256,1024}[domainIndex];
        domain={std::min(a.p.x,b.p.x)-pad,std::min(a.p.y,b.p.y)-pad,
                std::max(a.p.x,b.p.x)+pad,std::max(a.p.y,b.p.y)+pad};
        domain.l=std::min({domain.l,start.x,end.x});domain.r=std::max({domain.r,start.x,end.x});
        domain.t=std::min({domain.t,start.y,end.y});domain.b=std::max({domain.b,start.y,end.y});
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
        Node n{};n.g=distance(a.p,start);n.path={a.p};if(start!=a.p)n.path.push_back(start);
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
                finalDomain={std::min(a.p.x,b.p.x)-1024,std::min(a.p.y,b.p.y)-1024,
                             std::max(a.p.x,b.p.x)+1024,std::max(a.p.y,b.p.y)+1024};
                phase=1;scan=0;
                return result.status;
            } else if(phase==1) {
                if(scan<source.size()) {
                    if(result.work.examined>=8192U){fail(Status::kBudgetExceeded);break;}
                    const auto& o=source[scan++];++result.work.examined;
                    if(!admitted(o)||o.id==connector.id||(!visible(o) && &o!=targetA && &o!=targetB))continue;
                    R r{};if(!aabb(o,r)){fail(Status::kInvalidGeometry);break;}
                    const double margin=8+stroke(connector)+stroke(o);
                    if(!std::isfinite(margin)){fail(Status::kInvalidGeometry);break;}
                    r={std::floor((r.l-margin)*64)/64,std::floor((r.t-margin)*64)/64,
                       std::ceil((r.r+margin)*64)/64,std::ceil((r.b+margin)*64)/64};
                    if(!intersects(r,finalDomain))continue;
                    if(obstacles.size()>=256U){fail(Status::kBudgetExceeded);break;}
                    obstacles.push_back({o.id,r});continue;
                }
                std::sort(obstacles.begin(),obstacles.end(),[](const auto& x,const auto& y){return x.id<y.id;});
                for(const auto& o:obstacles) if((!a.target&&inside(a.p,o.rect))||(!b.target&&inside(b.p,o.rect))) {
                    fail(Status::kNoRouteInDomain);return result.status;
                }
                exitsA=exits(a);exitsB=exits(b);
                if(result.status!=Status::kPending)break;
                if(exitsA.empty()||exitsB.empty()){fail(Status::kNoRouteInDomain);break;}
                phase=2;
                // Escape checks are bounded (at most 4*256 predicates). Yield
                // before search so the next quantum has its full budget.
                return result.status;
            } else {
                if(!searching&&!setupAttempt())break;
                if(edge>=4) {
                    if(open.empty() || (!best.empty() && std::get<0>(*open.begin())+distance(end,b.p)>bestLength)) {
                        searching=false;continue;
                    }
                    if(result.work.expanded>=8192U){fail(Status::kBudgetExceeded);break;}
                    const auto top=*open.begin();open.erase(open.begin());current={std::get<3>(top),std::get<4>(top)};
                    auto& n=nodes.at(current);n.closed=true;++result.work.expanded;
                    if(!best.empty()&&n.bends>bestBends)continue;
                    if(point(current.first)==end) {
                        auto path=n.path;if(end!=b.p)path.push_back(b.p);path=simplified(std::move(path));
                        if(path.size()>256U){fail(Status::kBudgetExceeded);break;}
                        double length=0;for(std::size_t i=1;i<path.size();++i)length+=distance(path[i-1],path[i]);
                        const auto bends=path.size()>1?static_cast<unsigned>(path.size()-2):0U;
                        if(length<bestLength||(length==bestLength&&(bends<bestBends||
                            (bends==bestBends&&pathLess(path,best))))) {best=std::move(path);bestLength=length;bestBends=bends;}
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
                    candidateValid=true;predicateIndex=0;
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
                    if(!predicate(from,candidate,axis[predicateIndex++])){clear=false;break;}
                }
                if(result.status!=Status::kPending)break;
                if(clear) {
                    const auto& n=nodes.at(current);const unsigned direction=(edge%2U==0)?0U:1U;
                    const Key k{vertex(candidate),direction};
                    const double g=n.g+distance(from,candidate);
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
