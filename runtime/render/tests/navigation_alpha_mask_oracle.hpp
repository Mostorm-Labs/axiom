#pragma once
#include "../src/skia_prepared_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include <cmath>
#include <stdexcept>

namespace canvas::render::testing {
inline void requirePreparedAlphaMaskParity(SkSurface& root){
    const auto check=[](bool value,const char* text){if(!value)throw std::runtime_error(text);};
    for(bool replay:{false,true})for(unsigned variant=0;variant<4;++variant){
        semantic::CompactStrokeContent content;auto& stroke=content.stroke;
        stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        stroke.config.size=18;stroke.config.opacity=.37;stroke.config.color={.17,.32,.69,.71};
        stroke.config.thinning=.8;stroke.config.smoothing=.5;
        if(variant&1){stroke.config.family=foundation::InkFamily::kMarker;stroke.config.markerFlatness=.35;stroke.config.markerAngle=.7;}
        if(variant&2){stroke.config.startCap=false;stroke.config.startTaper=35;stroke.config.endTaper=50;}
        const unsigned count=replay?100003:2048;
        for(unsigned i=0;i<count;++i)stroke.knots.push_back({double(i)*.5,64.+std::sin(i*.013)*42,static_cast<std::uint16_t>(12000+(i%500)*100)});
        for(unsigned i=29;i<38;++i)stroke.knots[i]={i%2?12.:18.,64.,32768};
        ReferenceTraversalEntry entry;entry.record.objectId=ObjectId::fromUint64(89020+variant);
        entry.record.content=content;entry.command=CompactStrokeReferenceCommand{content};
        auto budget=std::make_shared<resources::ResourceBudgetManager>();
        internal::PreparedGeometryCache cache;RenderCacheOptions options;
        auto prepared=cache.get(entry,options,budget);check(prepared&&prepared->isContour(),"alpha mask prepared source admission");
        auto alpha=SkSurfaces::Raster(SkImageInfo::MakeA8(256,128));
        auto direct=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
        check(alpha&&direct,"alpha mask test allocation");
        alpha->getCanvas()->clear(SK_ColorTRANSPARENT);direct->getCanvas()->clear(SK_ColorBLACK);
        for(auto* c:{alpha->getCanvas(),direct->getCanvas()}){
            c->clipRect(SkRect::MakeLTRB(7,5,249,122));
            if(variant&1){c->translate(256,0);c->scale(-1,1);c->skew(.05F,-.015F);}
            if(replay)c->translate(-25000,0);
        }
        const auto work=prepared->drawContour(*alpha->getCanvas());
        check(work.primitivesDrawn>0,"alpha mask parity fixture drew no contour");
        internal::drawCompactContourCoverageToSkCanvas(*direct->getCanvas(),stroke.config,ink::compileCompactContour(stroke.config,stroke.knots));
        const auto info=root.imageInfo().makeWH(256,128);
        std::vector<std::uint8_t> got(info.computeMinByteSize()),want(got.size());
        for(unsigned mode=0;mode<2;++mode){
            auto target=root.makeSurface(info);check(bool(target),"alpha material test target");target->getCanvas()->clear(SK_ColorWHITE);
            auto mask=(mode?direct:alpha)->makeImageSnapshot();check(bool(mask),"alpha mask snapshot");
            internal::compositeCompactCoverageToSkCanvas(*target->getCanvas(),*mask,stroke.config,42);
            auto& bytes=mode?want:got;
            check(target->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"alpha material readback");
        }
        check(got==want,"prepared A8 coverage changed independent N32 Direct material pixels");
        cache.clear();prepared.reset();check(budget->used()==0,"alpha prepared source lease leaked");
    }
}
}
