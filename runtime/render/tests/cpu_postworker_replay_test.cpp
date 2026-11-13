// Work regression: small adjacent draw ranges must not repeatedly compile the
// same immutable checkpoint. Pixel oracle is the original full contour mapper.
#define main postworkerPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include <nlohmann/json.hpp>
#include <iostream>
int main(int argc,char** argv){try{
    require(argc==2,"output");auto e=source();
    // Force the existing256-knot checkpoint realization through its entry cap.
    RenderCacheOptions options;options.maxBytes=8U*1024U*1024U;options.sharedMaxBytes=16U*1024U*1024U;options.maxEntryBytes=1024U*1024U;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});
    PreparedGeometryCache cache;auto prepared=cache.get(e,options,budget,true);require(prepared&&prepared->exactValidated(),"checkpoint body");
    const auto beforeBytes=budget->used();
    auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,180));
    auto full=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,180));
    for(auto* surface:{actual.get(),full.get()})surface->getCanvas()->clear(SK_ColorBLACK);
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(e.command).content.get().stroke;
    drawCompactContourCoverageToSkCanvas(*full->getCanvas(),stroke.config,ink::compileCompactContour(stroke.config,stroke.knots));
    PreparedGeometryDraw work;
    for(std::size_t first=0;first<prepared->primitiveCount();first+=32){
        const auto slice=prepared->drawContour(*actual->getCanvas(),first,32);
        work.compiledKnots+=slice.compiledKnots;work.primitivesDrawn+=slice.primitivesDrawn;
    }
    auto info=SkImageInfo::Make(1800,180,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
    std::vector<std::uint8_t> a(1800*180*4),b(a.size());
    require(actual->readPixels(info,a.data(),7200,0,0)&&full->readPixels(info,b.data(),7200,0,0),"readback");
    const bool equal=a==b;
    nlohmann::json result{{"knots",stroke.knots.size()},{"primitives",prepared->primitiveCount()},
        {"compiled_knots",work.compiledKnots},{"pixels_equal",equal},{"budget_before",beforeBytes},{"budget_after",budget->used()}};
    // Exact visits use the same scratch. They must invalidate any memoized draw
    // block; drawing afterward must compile the requested block again, exactly.
    (void)prepared->drawContour(*actual->getCanvas(),0,32);
    actual->getCanvas()->clear(SK_ColorBLACK);
    ExactComponentWork exact;prepared->visitExact(SkRect::MakeLTRB(1300,0,1420,150),[](const auto&){return true;},exact);
    const auto resumed=prepared->drawContour(*actual->getCanvas(),0,32);
    result["exact_visit_replay_knots"]=exact.replayKnots;result["draw_after_exact_compiled_knots"]=resumed.compiledKnots;
    std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
    require(equal,"checkpoint reuse changed pixels/order");
    require(work.compiledKnots>0,"fixture did not select checkpoint realization");
    require(work.compiledKnots<=stroke.knots.size(),"ADJACENT_DRAW_RANGES_RECOMPILED_SAME_CHECKPOINT");
    require(exact.replayKnots>0&&resumed.compiledKnots>0,"exact/draw scratch invalidation");
    for(std::size_t first=32;first<prepared->primitiveCount();first+=32)prepared->drawContour(*actual->getCanvas(),first,32);
    require(actual->readPixels(info,a.data(),7200,0,0)&&a==b,"exact visit left wrong cached checkpoint output");
    require(budget->used()==beforeBytes,"checkpoint scratch reuse increased resident bytes");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
