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
    const unsigned count=benchmark?3U:40U;
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
        }
        scene::ConnectorRouteJob job(c,objects);unsigned quanta=0;
        if(fixture==36) { (void)job.advance();job.cancel(); }

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
        std::cout<<"]}";
    }
    std::cout<<"]\n";
}
