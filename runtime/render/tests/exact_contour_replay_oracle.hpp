#pragma once
#include "../src/compact_raster_internal.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "include/core/SkCanvas.h"
#include <cmath>
#include <stdexcept>

namespace canvas::render::testing {
inline void requireExactContourReplay(SkSurface& root){
    using namespace internal;
    const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const auto info=root.imageInfo().makeWH(512,384);
    for(unsigned variant=0;variant<13;++variant)for(const auto cap:{std::size_t(0),std::size_t(1200),RenderCacheOptions{}.maxEntryBytes}){
        auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        config.size=18;config.opacity=.55;config.thinning=.99;config.smoothing=.61;
        if(variant&1){config.family=foundation::InkFamily::kMarker;config.markerFlatness=.35;config.markerAngle=.7;}
        if(variant&2){config.startCap=false;config.startTaper=35;config.endTaper=50;}
        const unsigned count=variant==4?1:variant==5?33:variant==7?2:variant==8?32:1024;
        std::vector<foundation::InkKnot> knots;
        for(unsigned i=0;i<count;++i)knots.push_back({5.+i*.49,190.+std::sin(i*.04)*160,static_cast<std::uint16_t>(12000+(i%500)*100)});
        if(variant==12)for(unsigned i=29;i<38;++i)knots[i]={i%2?120.:150.,190.,32768};
        const CompactDeviceGrid grid{-17,-11,529,395};
        const auto* gridPtr=variant==11?&grid:nullptr;
        auto actual=root.makeSurface(info),oracle=root.makeSurface(info);check(actual&&oracle,"exact replay target allocation");
        actual->getCanvas()->clear(SK_ColorTRANSPARENT);oracle->getCanvas()->clear(SK_ColorTRANSPARENT);
        for(auto* c:{actual->getCanvas(),oracle->getCanvas()}){
            if(variant==2){c->translate(15,12);c->scale(.85F,.9F);c->skew(.1F,-.05F);}
            if(variant==9){c->clipRect(SkRect::MakeLTRB(11,9,501,376));c->translate(512,0);c->scale(-1,1);}
            if(variant==10){auto matrix=SkMatrix::I();matrix.setPerspX(.0002F);c->concat(matrix);}
            if(variant==11)c->clipRect(SkRect::MakeLTRB(7,13,507,371));
        }
        if(variant==3){for(auto& k:knots){k.x+=1000000;k.y+=1000000;}
            actual->getCanvas()->translate(-1000000,-1000000);oracle->getCanvas()->translate(-1000000,-1000000);}
        auto budget=std::make_shared<resources::ResourceBudgetManager>(variant==6?resources::BudgetLimits{1,4U*1024U*1024U}:resources::BudgetLimits{});
        CompactCoverageTargets targets;targets.budget=budget;CompactRasterWork work;
        {
            CompactStrokeRasterJob job(*actual->getCanvas(),config,knots,gridPtr,true,budget,cap);
            for(unsigned i=0;!job.step(*actual->getCanvas(),config,knots,42,targets,true,&work);++i)
                check(i<10000,"exact replay did not converge under bounded stepping");
            if(variant!=6&&variant!=10&&cap==RenderCacheOptions{}.maxEntryBytes&&work.cpuTiles>1){
                check(work.compiledContourKnots==knots.size(),"exact tiles recompiled the same contour input");
                check(work.contourRecordReplays>0,"exact tiles did not reuse the coverage stream");
            }
            if(!cap||variant==6||variant==10)check(work.contourRecordReplays==0,"denied/unsafe replay bypassed admission");
            check(budget->used(resources::ResourceClass::kRecorded)<=cap,"exact replay exceeded per-entry memory");
        }
        check(budget->used(resources::ResourceClass::kRecorded)==0,"completed exact job leaked replay records");
        // Complete/read the first GPU stream before drawing the independent
        // reference, matching the existing Ganesh oracle ordering contract.
        std::vector<std::uint8_t> got(info.computeMinByteSize()),want(got.size());
        check(actual->readPixels(info,got.data(),info.minRowBytes(),0,0),"exact replay readback");
        drawCompactStrokeToSkCanvas(*oracle->getCanvas(),config,knots,42,nullptr,nullptr,gridPtr);
        check(oracle->readPixels(info,want.data(),info.minRowBytes(),0,0),"exact Direct readback");
        check(got==want,"exact contour replay changed Direct pixels");
        targets.clear();check(budget->used()==0,"exact replay retained budget after teardown");
    }
    // A genuine, dispensable surface owns the soft budget at first admission.
    // Reclamation runs only after reserve fails; denial without it is covered
    // above. The callback releases no canonical state or in-flight draw target.
    {
        auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        std::vector<foundation::InkKnot> knots;for(unsigned i=0;i<1024;++i)knots.push_back({i*.49,190.+std::sin(i*.04)*160,32768});
        auto actual=root.makeSurface(info),oracle=root.makeSurface(info),dispensable=root.makeSurface(info);
        auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{2U*1024U*1024U,4U*1024U*1024U});
        const auto token=budget->reserve(resources::ResourceClass::kTile,2U*1024U*1024U,true);
        check(token.has_value(),"reclamation fixture reservation");resources::ResourceLease lease(budget,*token);
        CompactCoverageTargets targets;targets.budget=budget;CompactRasterWork work;unsigned reclaimed=0;
        actual->getCanvas()->clear(SK_ColorTRANSPARENT);oracle->getCanvas()->clear(SK_ColorTRANSPARENT);
        {CompactStrokeRasterJob job(*actual->getCanvas(),config,knots,nullptr,true,budget,RenderCacheOptions{}.maxEntryBytes,
            [&](std::size_t requested){check(budget->used()>2U*1024U*1024U-requested,"unnecessary exact reclamation");
                ++reclaimed;dispensable.reset();lease.reset();});
            for(unsigned i=0;!job.step(*actual->getCanvas(),config,knots,42,targets,true,&work);++i)check(i<10000,"reclaimed exact did not converge");}
        check(reclaimed==1&&work.contourRecordReplays>0&&work.compiledContourKnots==knots.size(),
            "actual admission pressure did not reclaim and reuse exact records");
        std::vector<std::uint8_t> got(info.computeMinByteSize()),want(got.size());
        check(actual->readPixels(info,got.data(),info.minRowBytes(),0,0),"reclaimed exact readback");
        drawCompactStrokeToSkCanvas(*oracle->getCanvas(),config,knots,42);
        check(oracle->readPixels(info,want.data(),info.minRowBytes(),0,0)&&got==want,"reclamation changed canonical pixels");
        targets.clear();check(budget->used()==0,"reclaimed exact leaked lease");
    }
    auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    std::vector<foundation::InkKnot> knots;for(unsigned i=0;i<1024;++i)knots.push_back({double(i)/2,190.,32768});
    auto surface=root.makeSurface(info);auto budget=std::make_shared<resources::ResourceBudgetManager>();
    CompactCoverageTargets targets;targets.budget=budget;
    {CompactStrokeRasterJob job(*surface->getCanvas(),config,knots,nullptr,false,budget);
        check(!job.step(*surface->getCanvas(),config,knots,42,targets,false,nullptr),"cancel fixture completed before interruption");}
    check(budget->used(resources::ResourceClass::kRecorded)==0,"cancelled exact job retained replay records");
    targets.clear();check(budget->used()==0,"cancelled coverage target retained lease");
    // Picture admission can fail after a few batches were compiled. Drop all
    // optional pictures but preserve compiler checkpoints/the complete spatial
    // index, rather than omitting a prefix or replaying a partial index.
    {
        auto actual=root.makeSurface(info),oracle=root.makeSurface(info);
        auto pressure=root.makeSurface(info.makeWH(512,1024));check(bool(pressure),"partial denial pressure surface");
        actual->getCanvas()->clear(SK_ColorTRANSPARENT);oracle->getCanvas()->clear(SK_ColorTRANSPARENT);
        auto owner=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1024U*1024U,4U*1024U*1024U});
        CompactCoverageTargets slots;slots.budget=owner;CompactRasterWork measured;
        {
            CompactStrokeRasterJob job(*actual->getCanvas(),config,knots,nullptr,true,owner);
            check(!job.step(*actual->getCanvas(),config,knots,42,slots,true,&measured),"partial denial completed too soon");
            check(owner->used(resources::ResourceClass::kRecorded)>0,"partial denial did not start recorded admission");
            const auto token=owner->reserve(resources::ResourceClass::kScratch,2U*1024U*1024U,true);check(token.has_value(),"partial denial pressure lease");
            resources::ResourceLease lease(owner,*token);
            for(unsigned i=0;!job.step(*actual->getCanvas(),config,knots,42,slots,true,&measured);++i)check(i<10000,"partial denial fallback stalled");
            check(owner->used(resources::ResourceClass::kRecorded)==0&&measured.contourRecordReplays==0,"partial pictures survived failed admission");
            check(measured.contourCheckpointReplays>0,"partial denial discarded valid checkpoint locality");
        }
        std::vector<std::uint8_t> got(info.computeMinByteSize()),want(got.size());
        check(actual->readPixels(info,got.data(),info.minRowBytes(),0,0),"partial denial readback");
        drawCompactStrokeToSkCanvas(*oracle->getCanvas(),config,knots,42);
        check(oracle->readPixels(info,want.data(),info.minRowBytes(),0,0)&&got==want,"partial denial changed canonical pixels");
        slots.clear();check(owner->used()==0,"partial denial leaked index storage");
    }
}
}
