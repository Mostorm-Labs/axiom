#pragma once
#include "../src/compact_raster_internal.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "include/core/SkCanvas.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace canvas::render::testing {
inline void requireExactContourLocality(SkSurface& root){
    using namespace internal;
    const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    for(unsigned admission=0;admission<3;++admission){
    const bool softDenied=admission==1,hardDenied=admission==2;
    auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    config.size=18;config.opacity=.55;config.smoothing=.61;
    std::vector<foundation::InkKnot> knots;
    for(unsigned i=0;i<2048;++i)knots.push_back({8.+i*.49,380.+std::sin(i*.031)*300.,
        static_cast<std::uint16_t>(12000+(i%500)*100)});
    const auto info=root.imageInfo().makeWH(1024,768);
    auto actual=root.makeSurface(info),fallback=root.makeSurface(info);check(actual&&fallback,"local exact targets");
    std::array<std::vector<std::uint8_t>,2> pixels;
    std::array<std::uint64_t,2> steps{};std::array<CompactRasterWork,2> work{};
    for(unsigned mode=0;mode<2;++mode){
        auto& target=mode?fallback:actual;target->getCanvas()->clear(SK_ColorTRANSPARENT);
        auto budget=std::make_shared<resources::ResourceBudgetManager>(softDenied?resources::BudgetLimits{1,8U*1024U*1024U}:hardDenied?resources::BudgetLimits{8U*1024U*1024U,8U*1024U*1024U}:resources::BudgetLimits{});
        CompactCoverageTargets targets;targets.budget=budget;
        {
            auto pressure=hardDenied?root.makeSurface(info.makeWH(1024,2048)):nullptr;
            const auto token=hardDenied?budget->reserve(resources::ResourceClass::kScratch,8U*1024U*1024U,true):std::nullopt;
            if(hardDenied)check(pressure&&token,"hard-denial fixture admission");
            resources::ResourceLease pressureLease(budget,token.value_or(0));
            CompactStrokeRasterJob job(*target->getCanvas(),config,knots,nullptr,true,budget,
                mode?0:RenderCacheOptions{}.maxEntryBytes);
            pressure.reset();pressureLease.reset();
            bool done=false;while(!done){check(++steps[mode]<10000,"local exact convergence");
                const auto checks=work[mode].contourRecordCandidates+work[mode].contourRecordGroups;
                done=job.step(*target->getCanvas(),config,knots,42,targets,true,&work[mode]);
                check(work[mode].contourRecordCandidates+work[mode].contourRecordGroups-checks<=32,"unbounded exact metadata search");}
            check(budget->used(resources::ResourceClass::kRecorded)<=RenderCacheOptions{}.maxEntryBytes,"local exact record admission");
        }
        check(budget->used(resources::ResourceClass::kRecorded)==0,"local exact job record retirement");
        pixels[mode].resize(info.computeMinByteSize());
        check(target->readPixels(info,pixels[mode].data(),info.minRowBytes(),0,0),"local exact readback");
        targets.clear();check(budget->used()==0,"local exact coverage target retirement");
    }
    check(work[0].scalarCoverageTiles>0,"v4 exact coverage did not use scalar Skia mask targets");
    check(pixels[0]==pixels[1],"local exact differs from independent no-record compiler pixels");
    if(!softDenied&&!hardDenied)check(work[0].compiledContourKnots==knots.size(),"local exact recompiled the same source history");
    if(softDenied)check(work[0].contourRecordReplays==0&&work[0].contourCheckpointReplays>0,"soft denial did not preserve checkpoint locality");
    if(hardDenied){check(work[0].contourRecordReplays==0&&work[0].contourCheckpointReplays==0&&steps[0]==steps[1],"hard denial did not retain full fallback");continue;}
    std::cout<<"exact locality softDenied="<<softDenied<<" steps="<<steps[0]<<" fallback="<<steps[1]
             <<" record_replays="<<work[0].contourRecordReplays<<'\n';
    check(steps[0]*2<steps[1],"exact tiles still visit unrelated full-stroke batches");
    check(work[0].contourRecordReplays*2<steps[1]-knots.size()/32,"exact replay did not select local batches");
    check(work[0].contourRecordsRejected>0&&work[0].contourRecordGroups>0,"local exact index was not exercised");
    }
}
}
