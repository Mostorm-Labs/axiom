#include "../src/skia_prepared_geometry.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace canvas;
using namespace canvas::render;
using namespace canvas::render::internal;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<std::uint8_t> pixels(const sk_sp<SkSurface>& surface){
    std::vector<std::uint8_t> result(256*128*4);
    require(surface->readPixels(surface->imageInfo(),result.data(),256*4,0,0),"readback");return result;
}
ReferenceTraversalEntry compact(unsigned variant=0){
    foundation::InkBrushConfig config;config.version=2;config.algorithmVersion=4;config.signalVersion=3;
    config.size=18;config.smoothing=.12;config.thinning=.7;
    config.family=variant%2?foundation::InkFamily::kMarker:foundation::InkFamily::kRound;
    config.markerAngle=.7;config.markerFlatness=.35;
    if(variant&2){config.startTaper=35;config.endTaper=50;config.startCap=false;}
    semantic::CompactStrokeContent content;content.stroke.config=config;content.stroke.seed=42;
    for(unsigned i=0;i<2048;++i)content.stroke.knots.push_back({-900.+i*1.2,64.+std::sin(i*.07)*23,
        static_cast<std::uint16_t>(12000+(i%500)*100)});
    ReferenceTraversalEntry result;result.record.objectId=ObjectId::fromUint64(31);
    result.command=CompactStrokeReferenceCommand{content};result.record.content=content;return result;
}
void localContourParity(){
    PreparedGeometryCache cache;RenderCacheOptions options;
    auto budget=std::make_shared<resources::ResourceBudgetManager>();
    std::uint64_t totalDrawn=0,totalAvailable=0;
    for(unsigned variant=0;variant<6;++variant){
        auto entry=compact(variant);
        auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.stroke;
        if(variant==4)stroke.knots={{40,60,32768}};
        if(variant==5)stroke.knots={{-40,70,32768},{180,70,45000},{20,70,16000},{220,45,33000}};
        const auto prepared=cache.get(entry,options,budget);require(bool(prepared),"contour admission");
        for(unsigned view=0;view<5;++view){
            auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
            auto oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
            for(auto* surface:{actual.get(),oracle.get()}){
                auto* c=surface->getCanvas();c->clear(SK_ColorBLACK);
                c->clipRect(SkRect::MakeLTRB(4,3,250,123));
                if(view==1){c->translate(11.3F,-3.7F);c->scale(.7F,1.2F);}
                if(view==2){c->translate(200,0);c->scale(-1,1);}
                if(view==3){c->rotate(13);c->skew(.2F,-.1F);}
                if(view==4){SkMatrix m;m.setPerspX(.0001F);c->concat(m);}
            }
            const auto work=prepared->drawContour(*actual->getCanvas());
            drawCompactContourCoverageToSkCanvas(*oracle->getCanvas(),stroke.config,
                ink::compileCompactContour(stroke.config,stroke.knots));
            require(pixels(actual)==pixels(oracle),"local contour differs from full paired contour");
            if(variant<4&&view==0){totalDrawn+=work.primitivesDrawn;totalAvailable+=prepared->primitiveCount();}
        }
    }
    require(totalDrawn>0&&totalDrawn*4<totalAvailable,"local contour still draws the entire long source");
    std::cout<<"local contour primitives drawn="<<totalDrawn<<" complete="<<totalAvailable<<'\n';
}
void vectorPathParity(){
    PreparedGeometryCache cache;RenderCacheOptions options;
    auto budget=std::make_shared<resources::ResourceBudgetManager>();
    for(unsigned rule:{1U,2U})for(bool closed:{false,true}){
        semantic::BrushStrokeContent content;content.stroke.snapshot.material_mode=1;
        content.stroke.snapshot.paint={.2,.4,.8,.6,.7,1,1};
        auto& output=content.stroke.vector_output;output.fill_rule=rule;output.closed=closed;
        output.outline={{10,10},{240,110},{20,110},{230,10},{125,125}};
        ReferenceTraversalEntry entry;entry.record.objectId=ObjectId::fromUint64(51);
        entry.command=BrushStrokeReferenceCommand{content};entry.record.content=content;
        const auto prepared=cache.get(entry,options,budget);
        require(prepared&&prepared->vectorPath(),"vector path admission");
        auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
        auto oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
        for(auto* surface:{actual.get(),oracle.get()}){
            surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            surface->getCanvas()->translate(3.7F,-2.1F);surface->getCanvas()->scale(.91F,1.03F);
        }
        drawValidatedIsolatedEntryToSkCanvas(*actual->getCanvas(),entry,nullptr,nullptr,nullptr,false,nullptr,nullptr,nullptr,prepared->vectorPath());
        drawValidatedIsolatedEntryToSkCanvas(*oracle->getCanvas(),entry);
        require(pixels(actual)==pixels(oracle),"prepared vector path changed fill/paint/AA");
    }
}
void smallCapAtLargeOrigin(){
    auto entry=compact();auto& s=std::get<CompactStrokeReferenceCommand>(entry.command).content.stroke;
    s.config.size=.02;s.knots={{1000000.,1000000.,32768}};
    PreparedGeometryCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>();
    auto prepared=cache.get(entry,RenderCacheOptions{},budget);require(bool(prepared),"large-origin cap admission");
    auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
    auto oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
    for(auto* surface:{actual.get(),oracle.get()}){
        auto* c=surface->getCanvas();c->clear(SK_ColorBLACK);
        c->scale(512,512);c->translate(-1000000.F+.25F,-1000000.F+.125F);
    }
    prepared->drawContour(*actual->getCanvas());
    drawCompactContourCoverageToSkCanvas(*oracle->getCanvas(),s.config,ink::compileCompactContour(s.config,s.knots));
    const auto expected=pixels(oracle);
    require(std::any_of(expected.begin(),expected.end(),[](auto channel){return channel>0&&channel<255;}),"large-origin fixture has no AA coverage");
    require(pixels(actual)==expected,"rounded cap bounds culled visible large-origin dot");
}
void sharingAndLifetime(){
    PreparedGeometryCache cache;RenderCacheOptions options;
    auto budget=std::make_shared<resources::ResourceBudgetManager>();auto entry=compact();
    auto first=cache.get(entry,options,budget);require(bool(first),"shared geometry admission");
    auto second=cache.get(entry,options,budget);
    require(first==second&&cache.statistics().builds==1&&cache.statistics().hits==1,"tiles do not share prepared source");
    const auto bytes=cache.statistics().residentBytes;
    require(bytes>0&&budget->used(resources::ResourceClass::kRecorded)==bytes,"prepared geometry not fully charged");
    cache.clear();require(budget->used()==bytes,"in-flight source lease released early");
    first.reset();second.reset();require(budget->used()==0&&cache.statistics().residentBytes==0,"retired source leaked budget");
    options.maxEntries=1;
    auto old=cache.get(entry,options,budget);old.reset();
    auto other=entry;other.record.objectId=ObjectId::fromUint64(32);
    auto replacement=cache.get(other,options,budget);
    require(replacement&&cache.statistics().evictions>0,"unused geometry not evicted");
    auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
    PreparedGeometryCache constrained;
    require(!constrained.get(entry,options,denied)&&constrained.statistics().denied==1&&denied->used()==0,"denial retained unaccounted geometry");
    auto recovered=constrained.get(entry,options,budget);require(bool(recovered),"budget recovery");
    auto changed=entry;std::get<CompactStrokeReferenceCommand>(changed.command).content.stroke.knots[300].y+=80;
    auto latest=constrained.get(changed,options,budget);
    require(latest&&latest!=recovered&&constrained.statistics().builds==2,"content change reused stale geometry");
    constrained.clear();recovered.reset();latest.reset();replacement.reset();cache.clear();require(budget->used()==0,"geometry cleanup leaked reservation");
}
void budgetOwnerChange(){
    PreparedGeometryCache cache;RenderCacheOptions options;auto entry=compact();
    auto owner=std::make_shared<resources::ResourceBudgetManager>();
    auto first=cache.get(entry,options,owner);require(bool(first),"owner fixture");
    auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
    require(!cache.get(entry,options,denied),"geometry lease crossed resource budget owner");
    require(owner->used()>0&&denied->used()==0,"in-flight lease moved between owners");
    first.reset();cache.clear();require(owner->used()==0,"old budget owner leaked after replacement");
}
}
int main(){localContourParity();vectorPathParity();smallCapAtLargeOrigin();sharingAndLifetime();budgetOwnerChange();std::cout<<"prepared geometry parity/locality/lifetime PASS\n";}
