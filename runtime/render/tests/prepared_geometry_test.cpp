#include "../src/skia_prepared_geometry.hpp"
#include "exact_contour_replay_oracle.hpp"
#include "navigation_alpha_mask_oracle.hpp"
#include "exact_contour_locality_oracle.hpp"
#include "reference_draw_stream_oracle.hpp"
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
void replayGeometryParity(){
    PreparedGeometryCache cache;RenderCacheOptions options;
    auto budget=std::make_shared<resources::ResourceBudgetManager>();
    for(unsigned variant=0;variant<2;++variant){
        auto entry=compact(variant);auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
        auto& s=content.stroke;s.config.smoothing=.5;s.config.startTaper=35;s.config.endTaper=50;s.knots.clear();
        for(unsigned i=0;i<100003;++i)s.knots.push_back({i*.5+(variant?30*std::sin(i*.02):0),64.+std::sin(i*.01)*20,32768});
        if(variant)for(unsigned i=500;i<516;++i)s.knots[i]={i%2?110.:150.,64,32768};
        entry.command=CompactStrokeReferenceCommand{content};entry.record.content=content;
        auto prepared=cache.get(entry,options,budget);
        require(bool(prepared),"large v4 source denied compact local replay despite small checkpoint footprint");
        auto shared=cache.get(entry,options,budget);require(shared==prepared,"replay source was not shared");shared.reset();
        const auto full=ink::compileCompactContour(s.config,s.knots);
        if(variant)require(!full.caps.empty(),"checkpoint reversal fixture emitted no corner caps");
        require(prepared->primitiveCount()==full.patches.size()-1+full.caps.size(),"replay duplicated or lost boundary ribbons/caps");
        for(unsigned view=0;view<4;++view){
            auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
            auto expected=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
            for(auto* surface:{actual.get(),expected.get()}){
                auto* c=surface->getCanvas();c->clear(SK_ColorBLACK);c->clipRect(SkRect::MakeLTRB(4,3,250,123));
                if(view==1){c->translate(128,0);c->scale(-1,1);c->translate(-25000,0);}
                if(view==2){c->skew(.2F,-.0001F);c->translate(-49800,0);}
                if(view==3){SkMatrix matrix;matrix.setPerspX(.0001F);c->concat(matrix);}
            }
            const auto work=prepared->drawContour(*actual->getCanvas());
            drawCompactContourCoverageToSkCanvas(*expected->getCanvas(),s.config,full);
            require(pixels(actual)==pixels(expected),"checkpoint replay differs from full-history contour pixels");
            if(view<3)require(work.primitivesTested*8<prepared->primitiveCount(),"checkpoint replay did not cull offscreen geometry");
        }
        cache.clear();require(budget->used()>0,"in-flight replay lost source lease");
        prepared.reset();require(budget->used()==0,"replay geometry leaked its admitted storage");
    }
}

void replayAdmission(){
    auto entry=compact();auto content=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();
    content.stroke.knots.clear();
    for(unsigned i=0;i<100000;++i)content.stroke.knots.push_back({i*.5,64,32768});
    entry.record.content=content;entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{3*1024*1024,5*1024*1024});
    const auto token=budget->reserve(resources::ResourceClass::kTile,1024*1024,true);require(bool(token),"replay pressure setup");
    resources::ResourceLease pressure(budget,*token);PreparedGeometryCache cache;RenderCacheOptions options;
    auto source=cache.retainSource(entry,budget,true);require(bool(source),"visible replay source admission");
    require(!cache.get(entry,options,budget),"optional replay escaped soft limit");
    auto visible=cache.get(entry,options,budget,true);require(bool(visible),"visible compact replay denied within hard limit");
    require(budget->used()>3*1024*1024&&budget->used()<=5*1024*1024,"replay priority changed budget bounds");
    auto changed=entry;changed.record.content.update([](auto& c){std::get<semantic::CompactStrokeContent>(c).stroke.knots[512].y+=30;});
    changed.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(changed.record.content)};
    auto tiny=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
    require(!cache.get(changed,options,tiny,true)&&tiny->used()==0,"replay exceeded hard denial or crossed budget owner");
    require(budget->used()>0,"replay lost in-flight old owner lease");
    cache.clear();visible.reset();source.reset();pressure.reset();require(budget->used()==0,"replay lease retirement leaked");
    auto owner=std::make_shared<resources::ResourceBudgetManager>();auto old=cache.get(entry,options,owner);
    auto replacement=cache.get(changed,options,owner);require(old&&replacement&&old!=replacement,"replay reused stale immutable source content");
    cache.clear();old.reset();replacement.reset();require(owner->used()==0,"changed replay retained unowned storage");
}

// A DPR4 canonical target can already exceed the optional cache ceiling.
// Visible preparation still needs a local replay of short strokes, just as it
// does for long strokes. Optional misses must continue to respect that ceiling.
void shortCriticalReplayAdmission(){
    constexpr std::size_t mib=1024*1024;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{mib,4*mib});
    const auto token=budget->reserve(resources::ResourceClass::kCanonicalFrame,mib,true);
    require(bool(token),"short replay pressure setup");
    resources::ResourceLease pressure(budget,*token);
    PreparedGeometryCache cache;RenderCacheOptions options;
    for(unsigned variant=0;variant<4;++variant){
        auto entry=compact(variant);
        entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
        require(!cache.get(entry,options,budget)&&budget->used()==mib,"optional short replay escaped soft limit");
        auto visible=cache.get(entry,options,budget,true);
        require(visible&&visible->isContour(),"visible short replay denied despite hard-cap headroom");
        require(budget->used()>mib&&budget->used()<=4*mib&&budget->used(resources::ResourceClass::kScratch)==0,
                "short replay was not charged or retained optional staging");
        require(cache.get(entry,options,budget,true)==visible,"short replay was not shared across visible demands");
        const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
        for(unsigned view=0;view<3;++view){
            auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
            auto oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));
            for(auto* surface:{actual.get(),oracle.get()}){
                auto* c=surface->getCanvas();c->clear(SK_ColorBLACK);c->clipRect(SkRect::MakeLTRB(4,3,250,123));
                if(view==1){c->translate(180,0);c->scale(-1,1);}
                if(view==2){c->translate(7,-2);c->skew(.2F,-.02F);}
            }
            const auto work=visible->drawContour(*actual->getCanvas());
            drawCompactContourCoverageToSkCanvas(*oracle->getCanvas(),stroke.config,ink::compileCompactContour(stroke.config,stroke.knots));
            require(pixels(actual)==pixels(oracle),"critical short replay changed full-history contour pixels");
            require(work.primitivesTested*2<visible->primitiveCount(),"critical short replay did not cull offscreen work");
        }
        cache.clear();require(budget->used()>mib,"short replay released a consumer's live source");
        visible.reset();require(budget->used()==mib,"short replay leaked charged source/checkpoints");
        options.maxEntryBytes=256;
        require(!cache.get(entry,options,budget,true)&&budget->used()==mib,"short replay bypassed per-entry denial");
        options.maxEntryBytes=8*mib;
    }
    auto full=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{mib,mib});
    const auto held=full->reserve(resources::ResourceClass::kCanonicalFrame,mib,true);require(bool(held),"hard-cap fixture");
    resources::ResourceLease pinned(full,*held);
    require(!cache.get(compact(),options,full,true)&&full->used()==mib,"critical short replay bypassed shared hard cap");
    cache.clear();pinned.reset();pressure.reset();require(full->used()==0&&budget->used()==0,"short replay owner cleanup");
}

void localContourParity(){
    PreparedGeometryCache cache;RenderCacheOptions options;
    auto budget=std::make_shared<resources::ResourceBudgetManager>();
    std::uint64_t totalDrawn=0,totalAvailable=0;
    for(unsigned variant=0;variant<6;++variant){
        auto entry=compact(variant);
        auto builder=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();auto& stroke=builder.stroke;
        if(variant==4)stroke.knots={{40,60,32768}};
        if(variant==5)stroke.knots={{-40,70,32768},{180,70,45000},{20,70,16000},{220,45,33000}};
        std::get<CompactStrokeReferenceCommand>(entry.command).content=builder;
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
            PreparedGeometryDraw work;
            if(view==0)for(std::size_t first=0;first<prepared->primitiveCount();first+=32){
                const auto slice=prepared->drawContour(*actual->getCanvas(),first,32);
                work.primitivesDrawn+=slice.primitivesDrawn;
            }else work=prepared->drawContour(*actual->getCanvas());
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
    auto entry=compact();auto builder=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();auto& s=builder.stroke;
    s.config.size=.02;s.knots={{1000000.,1000000.,32768}};
    PreparedGeometryCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>();
    std::get<CompactStrokeReferenceCommand>(entry.command).content=builder;
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
    auto changed=entry;std::get<CompactStrokeReferenceCommand>(changed.command).content.update([](auto& c){c.stroke.knots[300].y+=80;});
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
void stagingAdmission(){
    auto entry=compact();auto builder=std::get<CompactStrokeReferenceCommand>(entry.command).content.get();auto& stroke=builder.stroke;
    stroke.config.smoothing=.5;stroke.config.startTaper=40;stroke.config.endTaper=60;
    stroke.knots.clear();
    for(unsigned i=0;i<16384;++i)stroke.knots.push_back({40.+i*.035,64.+std::sin(i*.035)*42,32768});
    PreparedGeometryCache cache;RenderCacheOptions options;
    auto budget=std::make_shared<resources::ResourceBudgetManager>();
    std::get<CompactStrokeReferenceCommand>(entry.command).content=builder;
    auto acquired=cache.get(entry,options,budget);
    require(acquired&&budget->used(resources::ResourceClass::kScratch)==0,
            "long contour staging not admitted or temporary lease retained");
    require(budget->used()==cache.statistics().residentBytes&&budget->used()<=options.maxEntryBytes,
            "long contour resident lease does not match bounded storage");
    cache.clear();acquired.reset();require(budget->used()==0,"long contour staging leaked");
    stroke.config.smoothing=0;stroke.knots.resize(512);
    std::get<CompactStrokeReferenceCommand>(entry.command).content=builder;
    PreparedGeometryCache constrained;options.maxEntryBytes=256*1024;
    require(!constrained.get(entry,options,budget)&&budget->used()==0,
            "denied path admission leaked its successful staging lease");
    options.maxEntryBytes=8*1024*1024;
    require(bool(constrained.get(entry,options,budget)),"denied staging did not recover with an adequate budget");
}

}
int main(){try{shortCriticalReplayAdmission();auto root=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(512,384));testing::requirePreparedAlphaMaskParity(*root);testing::requireExactContourLocality(*root);testing::requireExactContourReplay(*root);replayAdmission();replayGeometryParity();localContourParity();vectorPathParity();smallCapAtLargeOrigin();sharingAndLifetime();budgetOwnerChange();stagingAdmission();testing::requireReferenceDrawStream();std::cout<<"prepared geometry parity/locality/lifetime PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
