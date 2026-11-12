#define main cpuw6bCompatibilityFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../../interaction/src/partial_erase_geometry.hpp"
#include <iostream>
int main(){try{
    SkiaRenderer renderer;RenderCacheOptions limits;
    limits.maxBytes=256U*1024U*1024U;limits.sharedMaxBytes=512U*1024U*1024U;limits.maxEntryBytes=64U*1024U*1024U;
    renderer.setRenderCacheOptions(limits);renderer.setCpuPendingPreparation(true);
    const auto floor=renderer.reserveCpuWorkerGrant(1,64U*1024U*1024U);require(floor,"floor");
    auto entry=source();auto record=entry.record;record.kind=semantic::ObjectKind::kVectorStroke;record.kindVersion=3;
    std::vector<foundation::WorldPoint> trace;
    for(unsigned i=0;i<=8193;++i)trace.push_back({float(100+i*.00001),64});
    record.eraseMasks.push_back({ObjectId::fromUint64(9),*interaction::partialEraseGeometry(trace,{},2)});
    const std::vector<foundation::WorldPoint> region{{120,-100},{125,-100},{125,200},{120,200}};
    bool completed=false;
    for(unsigned turn=0;turn<10000;++turn){renderer.beginCpuPendingOpportunity();bool pending=false;
        const auto status=renderer.intersectSelectionGeometry(record,region,&pending);
        if(!pending){require(status==SelectionGeometryHit::kHit,"8193 history decision");completed=true;break;}
    }
    require(completed,"LEGAL_HISTORY_ABOVE_WHOLE_WORKER_LIMIT_NEVER_COMPLETED");
    renderer.clearCpuLocalSelection();record.eraseMasks.clear();
    for(unsigned id=1;id<=513;++id){record.objectId=ObjectId::fromUint64(100+id);completed=false;
        for(unsigned turn=0;turn<1000;++turn){renderer.beginCpuPendingOpportunity();bool pending=false;
            const auto status=renderer.intersectSelectionGeometry(record,region,&pending);
            if(!pending){require(status==SelectionGeometryHit::kHit,"513 query decision");completed=true;break;}
        }
        require(completed,"TERMINAL_SELECTION_PINS_STARVED_513TH_OBJECT");
    }
    // The existing domain keeps legacy/unsupported values on its old route.
    auto legacy=record;legacy.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.algorithmVersion=3;});
    bool pending=false;const auto legacyExpected=intersectVisibleSelectionGeometry(legacy,region);
    const auto legacyActual=renderer.intersectSelectionGeometry(legacy,region,&pending);
    std::cout<<"legacy_original="<<unsigned(legacyExpected)<<" legacy_actual="<<unsigned(legacyActual)<<" pending="<<pending<<'\n';
    require(legacyActual==legacyExpected&&!pending,"legacy selection changed original result or became pending");
    auto invisible=record;invisible.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=0;});
    completed=false;
    for(unsigned n=0;n<1000;++n){renderer.beginCpuPendingOpportunity();const auto hit=renderer.intersectSelectionGeometry(invisible,region,&pending);if(!pending){require(hit==SelectionGeometryHit::kMiss,"invisible source hit");completed=true;break;}}
    require(completed,"invisible source pending");
    renderer.clearCpuLocalSelection();
    renderer.clearCpuLocalSelection();require(renderer.releaseCpuWorkerGrant(1,floor),"floor release");
    std::cout<<"8193-segment history and513 terminal selection consumers complete\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
