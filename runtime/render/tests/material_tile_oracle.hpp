#pragma once
#include "../src/compact_raster_internal.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include <cmath>
#include <stdexcept>

namespace canvas::render::testing {
// The break caught here is consuming coverage/advancing alpha on a denied
// material reservation,or reusing transient quota before its GPU reads finish.
inline void requireMaterialTileLifetime(SkSurface& root){
    using namespace internal;
    if(!root.getCanvas()->recordingContext())return; // CPU is a negative qualifier.
    const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    const auto info=root.imageInfo().makeWH(512,384);
    auto actual=root.makeSurface(info),oracle=root.makeSurface(info),full=root.makeSurface(info);
    auto pressureSurface=root.makeSurface(info.makeWH(1024,896));
    check(actual&&oracle&&full&&pressureSurface,"material lifetime target allocation");
    auto config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    config.color={.17,.37,.71,.83};config.opacity=.57;config.size=29;
    std::vector<foundation::InkKnot> knots;for(unsigned i=0;i<192;++i)knots.push_back({10.+i*2.5,184.+std::sin(i*.17)*150,32768});
    auto linear=root.makeSurface(info.makeColorSpace(SkColorSpace::MakeSRGBLinear()));
    auto opaque=root.makeSurface(info.makeAlphaType(kOpaque_SkAlphaType));
    check(linear&&opaque&&!supportsIsolatedMaterialTiles(*linear->getCanvas(),config)&&!supportsIsolatedMaterialTiles(*opaque->getCanvas(),config),
          "unsupported target color/alpha encoding entered material isolation");
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{64*1024,4*1024*1024});
    const auto pressure=budget->reserve(resources::ResourceClass::kRaster,3584*1024,true);check(bool(pressure),"material pressure admission");
    resources::ResourceLease held(budget,*pressure);CompactCoverageTargets targets;targets.budget=budget;
    actual->getCanvas()->clear(SK_ColorWHITE);oracle->getCanvas()->clear(SK_ColorWHITE);full->getCanvas()->clear(SK_ColorTRANSPARENT);
    for(auto* canvas:{actual->getCanvas(),full->getCanvas()}){canvas->translate(-7.25F,13.375F);canvas->scale(1.37F,.91F);}
    const auto read=[&](SkSurface& surface){std::vector<std::uint8_t> bytes(info.computeMinByteSize());
        check(surface.readPixels(info,bytes.data(),info.minRowBytes(),0,0),"material lifetime readback");return bytes;};
    const auto original=read(*actual);CompactRasterWork work;
    const auto epoch=budget->beginGpuEpoch(resources::resourceClassMask(resources::ResourceClass::kScratch),0);check(bool(epoch),"material lifetime GPU epoch");
    {
        CompactStrokeRasterJob job(*actual->getCanvas(),config,knots,nullptr,true,budget,1024*1024,{},true,true);
        unsigned steps=0;while(!job.resourcePending()){check(!job.step(*actual->getCanvas(),config,knots,42,targets,true,&work),"material pressure escaped admission");check(++steps<1024,"material denial did not reach bounded checkpoint");}
        check(work.materialIsolationTiles==0&&work.cpuTiles>0&&job.pendingResourceBytes()>256*256*4,"material denial did not retain completed coverage");
        const auto compiled=work.compiledContourKnots;
        check(!job.step(*actual->getCanvas(),config,knots,42,targets,true,&work)&&job.resourcePending()&&work.compiledContourKnots==compiled&&read(*actual)==original,
              "material retry recompiled coverage or deposited partial alpha");
        pressureSurface.reset();held.reset();steps=0;
        while(!job.step(*actual->getCanvas(),config,knots,42,targets,true,&work))check(++steps<1024&&!job.resourcePending(),"material quota release did not resume");
        check(work.materialIsolationTiles>1,"material lifetime fixture did not cross tile boundaries");
    }
    targets.clear();check(work.materialIsolationReservedBytes>0&&budget->snapshot(resources::ResourceClass::kScratch).pendingRelease>=work.materialIsolationReservedBytes,"transient material charge retired before GPU completion");
    check(budget->endGpuEpoch(*epoch),"material lifetime GPU scope end");
    // Test-only readback proves the GPU has executed this scope. Production
    // uses the provider's asynchronous finished callback and never waits here.
    const auto got=read(*actual);
    drawCompactStrokeToSkCanvas(*full->getCanvas(),config,knots,42);
    oracle->getCanvas()->drawImage(full->makeImageSnapshot(),0,0);
    check(got==read(*oracle),"material denial/resume changed full-frame isolation pixels");
    check(budget->used()>0,"readback alone silently granted retirement permit");
    check(budget->completeGpuEpoch(*epoch)&&budget->used()==0,"completed material scope leaked quota");

    // Cancellation releases derived state but keeps asynchronous charges until
    // the same real completion boundary. No layer is left open across a yield.
    CompactCoverageTargets cancelled;cancelled.budget=budget;
    const auto cancelEpoch=budget->beginGpuEpoch(resources::resourceClassMask(resources::ResourceClass::kScratch),0);check(bool(cancelEpoch),"material cancel epoch");
    {CompactStrokeRasterJob job(*actual->getCanvas(),config,knots,nullptr,true,budget,1024*1024,{},true,true);
        check(!job.step(*actual->getCanvas(),config,knots,42,cancelled,true,nullptr),"material cancel setup completed");}
    cancelled.clear();check(budget->used()>0,"cancelled material scope released queued storage early");
    check(budget->endGpuEpoch(*cancelEpoch),"material cancel scope end");(void)read(*actual);
    check(budget->completeGpuEpoch(*cancelEpoch)&&budget->used()==0,"material cancel leaked derived storage");
}
}
