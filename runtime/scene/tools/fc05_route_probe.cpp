#include "canvas/scene/connector_router.hpp"
#include <chrono>
#include <array>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string_view>
using namespace canvas;
static semantic::ObjectRecord box(std::uint64_t id,double x,double y,double w,double h) {
    semantic::ObjectRecord r{};r.id=foundation::ObjectId::fromUint64(id);r.kind=semantic::ObjectKind::kShape;
    r.kind_version=1;r.content=semantic::ShapeContent{1,w,h};r.transform.tx=x;r.transform.ty=y;return r;
}
static void benchmarks() {
    std::cout<<std::setprecision(17)<<"[";
    for(unsigned profile=0;profile<3;++profile) {
        const unsigned count=std::array<unsigned,3>{100,1000,10000}[profile];
        const unsigned connectors=std::array<unsigned,3>{20,200,1000}[profile];
        std::vector<semantic::ObjectRecord> objects;
        for(unsigned i=0;i<count;++i)objects.push_back(box(i+1,40+(i%20)*15,30+(i/20)*20,7,10));
        std::vector<double> times;unsigned ready=0,budget=0,other=0,maxExpanded=0,maxPredicates=0;
        for(unsigned i=0;i<connectors;++i) {
            semantic::ObjectRecord c{};c.id=foundation::ObjectId::fromUint64(900000+i);
            c.kind=semantic::ObjectKind::kConnector;c.kind_version=1;
            c.content=semantic::ConnectorContent{{semantic::FreePointEndpoint{{0,0}}},
                {semantic::FreePointEndpoint{{400,200+static_cast<double>(i%4)*20}}},semantic::ConnectorRouting::kOrthogonal};
            const auto start=std::chrono::steady_clock::now();
            scene::ConnectorRouteJob job(c,objects);
            while(job.advance()==runtime::ConnectorRouteStatus::kPending){}
            times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
            if(job.result().status==runtime::ConnectorRouteStatus::kReady)++ready;
            else if(job.result().status==runtime::ConnectorRouteStatus::kBudgetExceeded)++budget;
            else ++other;
            maxExpanded=std::max(maxExpanded,job.result().work.expanded);
            maxPredicates=std::max(maxPredicates,job.result().work.predicates);
        }
        std::sort(times.begin(),times.end());if(profile)std::cout<<",";
        std::cout<<"{\"objects\":"<<count<<",\"connectors\":"<<connectors<<",\"ready\":"<<ready
            <<",\"budget_exceeded\":"<<budget<<",\"other\":"<<other<<",\"max_expanded\":"<<maxExpanded
            <<",\"max_predicates\":"<<maxPredicates<<",\"p50_us\":"<<times[times.size()/2]
            <<",\"p95_us\":"<<times[(times.size()*95-1)/100]<<"}";
    }
    std::cout<<"]\n";
}
int main(int argc,char** argv) {
    const bool benchmark=argc>1&&std::string_view(argv[1])=="--benchmark";
    if(benchmark){benchmarks();return 0;}
    std::cout<<std::setprecision(17)<<"[";
    const unsigned count=133U;
    for(unsigned fixture=0;fixture<count;++fixture) {
        if(fixture)std::cout<<",";
        semantic::ObjectRecord c{};c.id=foundation::ObjectId::fromUint64(900000);
        c.kind=semantic::ObjectKind::kConnector;c.kind_version=1;
        c.content=semantic::ConnectorContent{{semantic::FreePointEndpoint{{0,0}}},
            {semantic::FreePointEndpoint{{400,200}}},semantic::ConnectorRouting::kOrthogonal};
        std::vector<semantic::ObjectRecord> objects;
        const unsigned total=benchmark?std::array<unsigned,3>{100,1000,10000}[fixture]:fixture%12U;
        for(unsigned i=0;i<total;++i) {
            objects.push_back(box(i+1,40+(i%20)*15,30+(i/20)*20,7,10));
        }
        if(!benchmark&&fixture>=16) {
            auto a=box(80001,-60,-40,40,80),b=box(80002,420,160,80,80);
            if(fixture%3==0){a.kind_version=2;std::get<semantic::ShapeContent>(a.content).shape_kind=3;}
            if(fixture%3==1)std::get<semantic::ShapeContent>(b.content).shape_kind=2;
            objects.push_back(a);objects.push_back(b);
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{fixture%4+1}};
            cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{(fixture+2)%4+1}};
        }
        if(fixture>=32) {
            objects.clear();
            auto& cc=std::get<semantic::ConnectorContent>(c.content);
            cc.start.value=semantic::FreePointEndpoint{{0,0}};
            cc.end.value=semantic::FreePointEndpoint{{400,200}};
            if(fixture==32) {objects.push_back(box(1,-20,-20,40,40));}
            if(fixture==33) for(unsigned i=0;i<257;++i)
                objects.push_back(box(i+1,40+(i%20)*15,30+(i/20)*20,7,10));
            if(fixture==34)cc.start.value=semantic::AttachedEndpoint{foundation::ObjectId::fromUint64(999),semantic::StablePortAnchor{1}};
            if(fixture==35)cc.end=cc.start;
            if(fixture==37)c.transform.a=0;
            if(fixture==38) {
                auto a=box(1,-60,-40,40,80);a.kind_version=2;
                std::get<semantic::ShapeContent>(a.content).shape_kind=3;objects.push_back(a);
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{1}};
            }
            if(fixture==39)c.transform={0,1,-1,0,0,0};
            if(fixture>=41 && fixture<=43) {
                auto a=box(1,0,0,100,100),b=box(2,0,300,100,100);
                a.kind_version=b.kind_version=2;
                std::get<semantic::ShapeContent>(a.content).shape_kind=3;
                std::get<semantic::ShapeContent>(b.content).shape_kind=3;
                objects={a,b};const bool reverse=fixture==42;
                cc.start.value=semantic::AttachedEndpoint{reverse?b.id:a.id,semantic::StablePortAnchor{reverse?1U:3U}};
                cc.end.value=semantic::AttachedEndpoint{reverse?a.id:b.id,semantic::StablePortAnchor{reverse?3U:1U}};
                if(fixture==43)objects.push_back(box(3,35,180,30,40));
            }
            if(fixture>=44 && fixture<=48) {
                const auto a=box(1,50,fixture==47?360:97,150,150), b=box(2,432,147,150,150);
                objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{1}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{fixture==48?1U:4U}};
                if(fixture==45)std::swap(cc.start,cc.end);
                if(fixture==46)objects.push_back(box(3,300,90,40,100));
            }
            if(fixture>=49 && fixture<=56) {
                const double left=fixture==51 || fixture==52 ? 100 : fixture>=53 && fixture<=55 ? 103 : 108;
                auto a=box(1,0,0,100,100), b=box(2,left,0,100,100);
                const auto stroke=semantic::StrokeStyleValue{semantic::SolidStroke{
                    {0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}};
                a.properties.entries=b.properties.entries=c.properties.entries={{0x101U,stroke}};
                objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{fixture==51?3U:2U}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{fixture>=53 && fixture<=55?4U:1U}};
                if(fixture==50)std::swap(cc.start,cc.end);
                if(fixture==54)objects.push_back(box(3,102,55,2,5));
                if(fixture==55){
                    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::AutoPerimeterAnchor{}};
                    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::AutoPerimeterAnchor{}};
                }
                if(fixture==56) for(unsigned n=0;n<1500;++n)objects.push_back(box(10+n,100000+n*20,100000,10,10));
            }
            if(fixture>=57 && fixture<=64) {
                auto a=box(1,0,0,100,100),b=box(2,300,0,100,100);
                unsigned port=3;
                switch(fixture) {
                case 58: port=1;break;
                case 59: case 60: b.transform.tx=0;b.transform.ty=300;port=fixture==59 ? 2U : 4U;break;
                case 61: b.transform.ty=.25;break;
                }
                objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{port}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{port}};
                if(fixture==62)objects.push_back(box(3,160,124,40,12));
                if(fixture==63)objects.push_back(box(3,-35,138,470,15));
                if(fixture==64)for(unsigned n=0;n<1500;++n)objects.push_back(box(10+n,50000+n*50,50000,6,6));
            }
            if(fixture>=65 && fixture<=72) {
                const bool touching=fixture==67 || fixture==68 || fixture==69 || fixture==71;
                const bool transpose=fixture==66 || fixture==68;
                const auto rect=[&](unsigned id,double x,double y,double w,double h) {
                    return transpose ? box(id,y,x,h,w) : box(id,x,y,w,h);
                };
                const auto a=rect(1,0,0,180,110);
                const auto b=rect(2,touching?180:276,touching?(fixture==69?24:0):110,180,110);
                objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{transpose?2U:3U}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{transpose?4U:1U}};
                if(fixture==69) std::swap(cc.start,cc.end);
                if(fixture==70) objects.push_back(rect(3,64,150,52,18));
                if(fixture==71) objects.push_back(rect(3,-40,22,50,66));
                if(fixture==72) for(unsigned n=0;n<1100;++n)objects.push_back(box(10+n,50000+n*20,50000,6,6));
            }
            if(fixture>=73 && fixture<=79) {
                const bool horizontal=fixture==75;
                const auto stroke=semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}};
                const auto rect=[&](unsigned id,double x,double y){
                    auto o=horizontal ? box(id,y,x,120,176) : box(id,x,y,176,120);
                    o.properties.entries={{0x101U,stroke}};return o;
                };
                const auto a=rect(1,0,0), b=rect(2,fixture==76 ? -177 : fixture==78 ? 12 : 177,136);
                objects={a,b};c.properties.entries={{0x101U,stroke},{0x201U,semantic::ConnectorDecorationValue::kArrow}};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{horizontal?2U:3U}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{horizontal?4U:1U}};
                if(fixture==74)std::swap(cc.start,cc.end);
                if(fixture==77)objects.push_back(box(3,125,128,10,8));
                if(fixture==79)for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            }
            if(fixture>=80 && fixture<=87) {
                const auto a=box(1,0,0,80,60), b=box(2,0,208,80,60);
                objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
                objects.push_back(fixture==80 ? box(3,-30,108,140,12) : box(3,-2048,108,4096,12));
                if(fixture>=82)objects.push_back(box(4,20,147,40,18));
                if(fixture==83)std::reverse(objects.begin(),objects.end());
                if(fixture==84)std::swap(cc.start,cc.end);
                if(fixture==85)objects.push_back(box(5,30,53,20,20));
                if(fixture==86) {
                    objects.erase(objects.begin(),objects.begin()+2);
                    cc.start.value=semantic::FreePointEndpoint{{40,60}};
                    cc.end.value=semantic::FreePointEndpoint{{40,208}};
                }
                if(fixture==87)for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            }
            if(fixture>=88 && fixture<=95) {
                const double h=fixture==94 ? 224 : 80;
                const auto a=box(1,0,0,120,h), b=box(2,350,62,120,h);
                const bool lower=fixture==88 || fixture==90 || fixture==92 || fixture==94;
                objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{lower?3U:4U}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{lower?2U:1U}};
                if(fixture==90)std::swap(cc.start,cc.end);
                if(fixture==91) {
                    cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{1}};
                    cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{2}};
                }
                if(fixture==92 || fixture==93)objects.push_back(box(3,170,lower?157:-42,64,22));
                if(fixture==95)for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            }
            if(fixture>=96 && fixture<104) {
                const bool column=fixture==100;
                const auto rect=[column](unsigned id,double x,double y) {
                    return column ? box(id,y,x,80,120) : box(id,x,y,120,80);
                };
                const auto hub=rect(1,0,0),right=rect(2,180,228);
                auto peer=rect(3,fixture==101?-3500:0,fixture==99?214:228);
                const auto blocker=rect(4,194,fixture==96?69:103);
                cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{column?2U:3U}};
                cc.end.value=semantic::AttachedEndpoint{right.id,semantic::StablePortAnchor{column?4U:1U}};
                auto other=c;other.id=foundation::ObjectId::fromUint64(5);
                auto& related=std::get<semantic::ConnectorContent>(other.content);
                related.end.value=semantic::AttachedEndpoint{peer.id,semantic::StablePortAnchor{column?4U:1U}};
                objects={hub,right,peer,blocker,other};
                if(fixture==98){std::swap(cc.start,cc.end);std::reverse(objects.begin(),objects.end());}
                if(fixture==102)objects.push_back(rect(6,174,205));
                if(fixture==103)for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            }
            if(fixture>=104 && fixture<113) {
                const bool vertical=fixture==108, far=fixture!=105 && fixture!=107;
                constexpr std::array gaps{28.5,28.5,19.5,19.5,19.5,19.5,0.,8.,38.};
                const double gap=gaps.at(fixture-104);
                const auto rect=[&](unsigned id,double x,double y) {
                    auto o=vertical ? box(id,y,x,80,120) : box(id,x,y,120,80);
                    o.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{{0,0,0,1},2}}}};
                    return o;
                };
                const auto hub=rect(1,0,0), first=rect(2,200,98), second=rect(3,320+gap,98);
                objects={hub,first,second};
                cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{vertical?3U:2U}};
                cc.end.value=semantic::AttachedEndpoint{far?second.id:first.id,semantic::StablePortAnchor{
                    far?(vertical?1U:4U):(vertical?3U:2U)}};
                c.properties=hub.properties;
                if(fixture==109)std::swap(cc.start,cc.end);
                if(fixture==112)c.transform={2,0,0,.75,17,-9};
                c.properties.entries.push_back({fixture==109?0x200U:0x201U,semantic::ConnectorDecorationValue::kArrow});
            }
            if(fixture>=113 && fixture<125) {
                const bool vertical=fixture==115 || fixture==123, reverse=fixture==116 || fixture==122;
                const auto make=[&](unsigned id,double x,double y) {
                    auto r=vertical?box(id,y,x,80,120):box(id,x,y,120,80);
                    if(fixture==117){r.transform.a=-1;r.transform.tx=-r.transform.tx;}
                    r.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{
                        {0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}}}};
                    return r;
                };
                const auto hub=make(1,0,0), middle=make(2,fixture==114?387.5:382.5,fixture==124?47.:50.5), lower=make(3,238.5,177);
                objects={middle,hub,lower};const unsigned port=vertical?3U:2U;
                cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{port}};
                cc.end.value=semantic::AttachedEndpoint{lower.id,semantic::StablePortAnchor{port}};
                if(fixture>=121) {
                    cc.end.value=semantic::AttachedEndpoint{middle.id,semantic::StablePortAnchor{port}};
                    auto outer=c;outer.id=foundation::ObjectId::fromUint64(4);
                    std::get<semantic::ConnectorContent>(outer.content).start.value=
                        semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{vertical?4U:1U}};
                    objects.push_back(outer);
                }
                if(fixture==118)cc.end.value=semantic::AttachedEndpoint{lower.id,semantic::AutoPerimeterAnchor{semantic::Vec2{1,.5}}};
                if(reverse)std::swap(cc.start,cc.end);
                c.properties=hub.properties;
                if(reverse || fixture==119)c.properties.entries.push_back({0x200U,semantic::ConnectorDecorationValue::kArrow});
                if(!reverse)c.properties.entries.push_back({0x201U,semantic::ConnectorDecorationValue::kArrow});
                if(fixture==120)for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            }
            if(fixture>=125) {
                const bool transpose=fixture==128, reverse=fixture==127;
                const auto rect=[transpose](unsigned id,double x,double y) {
                    auto r=transpose?box(id,y,x,80,120):box(id,x,y,120,80);
                    r.properties.entries={{0x101U,semantic::StrokeStyleValue{semantic::SolidStroke{
                        {0,0,0,1},2,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}}}};
                    return r;
                };
                const auto hub=rect(1,0,0), target=rect(2,320,0);
                objects={hub,target,rect(3,fixture==129?215:320,-120),rect(4,320,176)};
                cc.start.value=semantic::AttachedEndpoint{hub.id,semantic::StablePortAnchor{transpose?1U:4U}};
                cc.end.value=semantic::AttachedEndpoint{target.id,semantic::StablePortAnchor{transpose?4U:1U}};
                if(reverse)std::swap(cc.start,cc.end);
                c.properties=hub.properties;
                if(fixture!=126)c.properties.entries.push_back({reverse?0x200U:0x201U,semantic::ConnectorDecorationValue::kArrow});
                if(fixture==130 || fixture==131)objects.push_back(rect(5,fixture==130?-143:-120,0));
                if(fixture==132)for(unsigned n=0;n<1100;++n)objects.push_back(box(30+n,50000+n*20,50000,10,10));
            }
            if(fixture==40) {
                const auto a=box(1,0,0,100,100),b=box(2,50,300,100,100);objects={a,b};
                cc.start.value=semantic::AttachedEndpoint{a.id,semantic::StablePortAnchor{3}};
                cc.end.value=semantic::AttachedEndpoint{b.id,semantic::StablePortAnchor{1}};
                for(unsigned n=0;n<1500;++n)objects.push_back(box(n+10,100000+n*20,100000,10,10));
            }

        }
        scene::ConnectorRouteJob job(c,objects);unsigned quanta=0;
        if(fixture==36)job.cancel();

        const auto start=std::chrono::steady_clock::now();
        while(job.advance()==runtime::ConnectorRouteStatus::kPending)++quanta;
        const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        const auto& result=job.result();
        std::cout<<"{\"fixture\":"<<fixture<<",\"objects\":"<<objects.size()<<",\"status\":"<<static_cast<unsigned>(result.status)
            <<",\"expanded\":"<<result.work.expanded<<",\"predicates\":"<<result.work.predicates
            <<",\"examined\":"<<result.work.examined<<",\"quanta\":"<<quanta;
        if(benchmark)std::cout<<",\"microseconds\":"<<us;
        std::cout<<",\"route\":[";
        for(std::size_t i=0;i<result.route.size();++i) {
            if(i)std::cout<<",";
            std::cout<<"["<<result.route[i].x<<","<<result.route[i].y<<"]";
        }
        const auto deferred=scene::connectorGeometry(c,objects,true);
        std::cout<<"],\"deferred_status\":"<<static_cast<unsigned>(deferred->status)<<",\"deferred_route\":[";
        for(std::size_t n=0;n<deferred->route.size();++n) {
            if(n)std::cout<<",";
            std::cout<<"["<<deferred->route[n].x<<","<<deferred->route[n].y<<"]";
        }
        std::cout<<"]}";
    }
    std::cout<<"]\n";
}
