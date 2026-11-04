#pragma once
#include "../src/skia_prepared_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkPaint.h"
#include "include/utils/SkNoDrawCanvas.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace canvas::render::testing {
inline void requireReferenceDrawStream(){
    using namespace canvas::render::internal;
    const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    struct Call {SkPath path;SkRect oval;SkMatrix matrix;SkPaint paint;bool isOval=false;
        bool operator==(const Call&)const=default;};
    class Recorder final:public SkNoDrawCanvas {
      public:std::vector<Call> calls;Recorder():SkNoDrawCanvas(8192,8192){}
      protected:
        void onDrawPath(const SkPath& p,const SkPaint& paint)override{calls.push_back({p,{},getTotalMatrix(),paint,false});}
        void onDrawOval(const SkRect& r,const SkPaint& paint)override{calls.push_back({{},r,getTotalMatrix(),paint,true});}
    };
    ReferenceTraversalEntry entry;entry.record.objectId=foundation::ObjectId::fromUint64(31);entry.command=CompactStrokeReferenceCommand{};semantic::CompactStrokeContent builder;auto& s=builder.stroke;
    s.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    s.config.startTaper=45;s.config.endTaper=70;s.knots.clear();
    for(unsigned i=0;i<2048;++i)s.knots.push_back({-350.+i*.8,140.+std::sin(i*.035)*65,static_cast<std::uint16_t>(14000+(i%300)*130)});
    std::get<CompactStrokeReferenceCommand>(entry.command).content=builder;
    PreparedGeometryCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>();
    const auto prepared=cache.get(entry,RenderCacheOptions{},budget);check(bool(prepared),"draw stream admission");
    Recorder actual,oracle;for(auto* canvas:{&actual,&oracle}){canvas->translate(2048,2048);canvas->scale(1.2F,.9F);}
    prepared->drawContour(actual);
    drawCompactContourCoverageToSkCanvas(oracle,s.config,ink::compileCompactContour(s.config,s.knots));
    check(!actual.calls.empty()&&actual.calls.size()==oracle.calls.size(),"prepared draw stream count differs from reference");
    for(std::size_t i=0;i<actual.calls.size();++i)if(!(actual.calls[i]==oracle.calls[i])){
        std::cerr<<"draw stream mismatch index="<<i<<" path="<<(actual.calls[i].path==oracle.calls[i].path)
            <<" matrix="<<(actual.calls[i].matrix==oracle.calls[i].matrix)<<" paint="<<(actual.calls[i].paint==oracle.calls[i].paint)<<'\n';
        check(false,"prepared draw stream differs from reference");}
}
}
