// Actual draw-side work regression. Reverting indexed range location must
// violate the bound while the independently mapped full-contour pixels stay equal.
#define main rangePriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/compact_raster_internal.hpp"
#include "canvas/foundation/internal/frame_timing.hpp"
#include "include/utils/SkNoDrawCanvas.h"
#include <nlohmann/json.hpp>
#include <iostream>
namespace {
namespace timing=canvas::foundation::internal::frame_timing;
struct CountScope {
    timing::Context context;
    timing::Context* previous=timing::active;
    CountScope(){timing::active=&context;}
    ~CountScope(){timing::active=previous;}
    std::uint64_t get(timing::Counter c)const{return context.counters[std::size_t(c)];}
    nlohmann::json counts()const{
        const auto select=get(timing::Counter::prepared_select_candidates);
        const auto draw=get(timing::Counter::prepared_draw_candidates);
        const auto probes=get(timing::Counter::prepared_range_probes);
        return {{"select",select},{"draw",draw},{"range_probes",probes},{"total",select+draw+probes}};
    }
};
std::vector<std::uint8_t> rangePixels(const sk_sp<SkSurface>& surface){
    const auto info=SkImageInfo::Make(surface->width(),surface->height(),kRGBA_8888_SkColorType,kPremul_SkAlphaType);
    std::vector<std::uint8_t> result(info.computeMinByteSize());
    require(surface->readPixels(info,result.data(),info.minRowBytes(),0,0),"range readback");return result;
}
struct Call {SkPath path;SkRect oval;SkMatrix matrix;SkPaint paint;bool isOval=false;
    bool operator==(const Call&)const=default;};
class Recorder final:public SkNoDrawCanvas {
 public:std::vector<Call> calls;Recorder():SkNoDrawCanvas(1800,180){}
 protected:
    void onDrawPath(const SkPath& p,const SkPaint& paint)override{calls.push_back({p,{},getTotalMatrix(),paint,false});}
    void onDrawOval(const SkRect& r,const SkPaint& paint)override{calls.push_back({{},r,getTotalMatrix(),paint,true});}
};
// Independent oracle descriptors come from the original compiler, never from
// PreparedNavigationGeometry's private groups or reported work.
struct Descriptor {std::size_t begin=0,end=0;SkRect bounds=SkRect::MakeEmpty();};
struct RangeReference {
    std::array<std::vector<Descriptor>,2> passes;
    std::size_t ribbons=0,total=0;
    bool checkpoint=false;
    explicit RangeReference(const semantic::CompactStrokeContent& content,bool replay):checkpoint(replay){
        const auto& s=content.stroke;const auto full=ink::compileCompactContour(s.config,s.knots);
        ribbons=full.patches.empty()?0:full.patches.size()-1;total=ribbons+full.caps.size();
        if(replay){
            ink::CompactContourCompiler compiler(s.config);const auto distance=ink::compactLength(s.knots);
            std::size_t ribbonAt=0,capAt=0;
            for(std::size_t at=0;at<s.knots.size();at+=256){
                ink::CompactContour output;const auto count=std::min(std::size_t(256),s.knots.size()-at);
                compiler.append(std::span(s.knots).subspan(at,count),distance,output);
                if(at+count==s.knots.size())compiler.finish(distance,output);
                const auto n=output.patches.empty()?0:output.patches.size()-1;
                const auto bounds=compactContourBounds(output);
                passes[0].push_back({ribbonAt,ribbonAt+n,bounds});
                passes[1].push_back({capAt,capAt+output.caps.size(),bounds});
                ribbonAt+=n;capAt+=output.caps.size();
            }
            require(ribbonAt==ribbons&&ribbonAt+capAt==total,"reference checkpoint component counts");
        }else{
            for(unsigned pass=0;pass<2;++pass){const auto n=pass?full.caps.size():ribbons;
                for(std::size_t first=0;first<n;first+=32){
                    Descriptor d{first,std::min(first+32,n),SkRect::MakeEmpty()};
                    for(auto i=d.begin;i<d.end;++i){const auto b=pass?compactContourCapBounds(full.caps[i]):compactContourRibbon(std::span(full.patches).subspan(i,2)).getBounds();
                        if(b.isEmpty()||!b.isFinite()){const auto m=std::numeric_limits<float>::max();d.bounds=SkRect::MakeLTRB(-m,-m,m,m);}else d.bounds.join(b);}
                    passes[pass].push_back(d);
                }
            }
        }
    }
    struct Answer {std::size_t next=0,bound=0;nlohmann::json passes=nlohmann::json::array();};
    Answer evaluate(SkCanvas& c,std::size_t first,std::size_t count,bool select)const{
        Answer result;first=std::min(first,total);result.next=first;const auto last=first+std::min(count,total-first);
        const auto clip=compactContourCullBounds(c);if(select&&!clip)return result;
        if(first==total||(!select&&first==last)){result.next=total;return result;}
        for(unsigned pass=0;pass<2;++pass){const auto offset=pass?ribbons:0,end=pass?total:ribbons;
            if(first>=end||(!select&&last<=offset))continue;
            const auto localFirst=first>offset?first-offset:0,localLast=last>offset?last-offset:0;
            const auto& descriptors=passes[pass];const auto n=descriptors.size();std::size_t b=0,s=0;
            if(checkpoint)for(auto v=n;v;v>>=1)++b; // ceil(log2(N+1))
            auto at=std::size_t(0);while(at<n&&descriptors[at].end<=localFirst)++at;
            bool found=false;
            for(;at<n;++at){const auto& d=descriptors[at];
                if(!select&&d.begin>=localLast)break;
                ++s;
                if(select&&d.begin!=d.end&&(!clip||d.bounds.isEmpty()||!d.bounds.isFinite()||SkRect::Intersects(*clip,d.bounds))){
                    result.next=std::max(first,offset+d.begin);found=true;break;}
            }
            result.bound+=b+s+1;result.passes.push_back({{"pass",pass},{"N",n},{"B",b},{select?"S":"D",s},{"bound",b+s+1}});
            if(found)return result;
        }
        if(select)result.next=total;return result;
    }
    nlohmann::json metadata()const{nlohmann::json out=nlohmann::json::array();for(unsigned pass=0;pass<2;++pass){
        nlohmann::json items=nlohmann::json::array();for(const auto& d:passes[pass])items.push_back({{"begin",d.begin},{"end",d.end},
            {"bounds",{d.bounds.left(),d.bounds.top(),d.bounds.right(),d.bounds.bottom()}}});out.push_back(items);}return out;}
};
nlohmann::json rangeCase(bool checkpoint,bool& bounded){
    auto e=source();const auto& stroke=std::get<CompactStrokeReferenceCommand>(e.command).content.get().stroke;
    const auto contour=ink::compileCompactContour(stroke.config,stroke.knots);
    RangeReference referenceRanges(std::get<CompactStrokeReferenceCommand>(e.command).content.get(),checkpoint);
    RenderCacheOptions options;options.maxBytes=8U*1024U*1024U;options.sharedMaxBytes=16U*1024U*1024U;
    if(checkpoint)options.maxEntryBytes=1024U*1024U;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});
    PreparedGeometryCache cache;auto body=cache.get(e,options,budget,true);require(body&&body->ready(),"range body admission");
    const auto pinned=budget->used();nlohmann::json rows=nlohmann::json::array();
    for(unsigned view=0;view<5;++view){
        auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,180));
        auto oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,180));
        for(auto* s:{actual.get(),oracle.get()}){s->getCanvas()->clear(SK_ColorBLACK);
            if(view)s->getCanvas()->clipRect(SkRect::MakeXYWH(float(280*view),0,280,180));}
        drawCompactContourCoverageToSkCanvas(*oracle->getCanvas(),stroke.config,contour);
        CountScope counts;PreparedGeometryDraw work;std::size_t calls=0;nlohmann::json callRows=nlohmann::json::array();
        for(std::size_t first=0;first<body->primitiveCount();){
            const auto expected=referenceRanges.evaluate(*actual->getCanvas(),first,32,true);std::size_t selected=0;
            {CountScope call;selected=body->nextVisiblePrimitive(*actual->getCanvas(),first);auto detail=call.counts();
                for(std::size_t i=0;i<call.context.counters.size();++i)counts.context.counters[i]+=call.context.counters[i];
                detail.update({{"type","select"},{"first",first},{"count",32},{"expected_next",expected.next},{"actual_next",selected},
                    {"passes",expected.passes},{"bound",expected.bound},{"within_bound",detail["total"].get<std::uint64_t>()<=expected.bound}});
                bounded=bounded&&detail["within_bound"].get<bool>();callRows.push_back(detail);}
            require(selected==expected.next,"nextVisiblePrimitive differs from independent interval oracle");first=expected.next;++calls;
            PreparedGeometryDraw slice;const auto allowed=referenceRanges.evaluate(*actual->getCanvas(),first,32,false);
            {CountScope call;slice=body->drawContour(*actual->getCanvas(),first,32);auto detail=call.counts();
                for(std::size_t i=0;i<call.context.counters.size();++i)counts.context.counters[i]+=call.context.counters[i];
                detail.update({{"type","draw"},{"first",first},{"count",32},{"passes",allowed.passes},{"bound",allowed.bound},
                    {"within_bound",detail["total"].get<std::uint64_t>()<=allowed.bound}});
                bounded=bounded&&detail["within_bound"].get<bool>();callRows.push_back(detail);}++calls;
            work.compiledKnots+=slice.compiledKnots;work.chunksVisited+=slice.chunksVisited;
            work.primitivesDrawn+=slice.primitivesDrawn;first+=std::min(std::size_t(32),body->primitiveCount()-first);
        }
        const bool equal=rangePixels(actual)==rangePixels(oracle);require(equal,"range traversal full contour pixel mismatch");
        auto row=counts.counts();
        row.update({{"view",view},{"range_calls",calls},{"intersected_groups",work.chunksVisited},{"calls",callRows},
            {"pixels_exact",equal},{"compiled_knots",work.compiledKnots},{"primitives_drawn",work.primitivesDrawn}});
        rows.push_back(row);
    }
    Recorder actual,oracle;for(std::size_t first=0;first<body->primitiveCount();first+=32)body->drawContour(actual,first,32);
    drawCompactContourCoverageToSkCanvas(oracle,stroke.config,contour);
    require(actual.calls==oracle.calls,"range traversal changed primitive order or geometry");
    const auto empty=body->drawContour(actual,0,0);require(empty.primitivesDrawn==0&&empty.compiledKnots==0,"empty range did work");
    // Actual resumable multi-tile materialization, independent null-prepared
    // Direct raster job as oracle. Work counters include every selection call.
    auto target=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,180));
    auto reference=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,180));
    target->getCanvas()->clear(SK_ColorTRANSPARENT);reference->getCanvas()->clear(SK_ColorTRANSPARENT);
    CompactCoverageTargets targets;targets.budget=budget;CompactRasterWork rasterWork;
    std::size_t steps=0;nlohmann::json jobCounts;
    {CompactStrokeRasterJob job(*target->getCanvas(),stroke.config,stroke.knots,nullptr,false,budget,options.maxEntryBytes,{},true,body);
    {CountScope counts;while(!job.step(*target->getCanvas(),stroke.config,stroke.knots,stroke.seed,targets,false,&rasterWork))
        require(++steps<20000&&!job.resourcePending(),"range job lost bounded progress");jobCounts=counts.counts();}}
    const auto beforeReference=rangePixels(target);
    drawCompactStrokeToSkCanvas(*reference->getCanvas(),stroke.config,stroke.knots,stroke.seed);
    require(beforeReference==rangePixels(reference),"range material job differs from null-prepared Direct");
    require((checkpoint&&cache.statistics().builds==1)||!checkpoint,"checkpoint cache unexpected rebuild");
    targets.clear();target.reset();reference.reset();require(budget->used()==pinned,"range traversal added uncharged retained state");
    cache.clear();body.reset();require(budget->used()==0,"range body last lease leaked");
    return {{"representation",checkpoint?"checkpoint":"full-path"},{"knots",stroke.knots.size()},
        {"reference_metadata",referenceRanges.metadata()},{"rows",rows},{"draw_stream_exact",true},{"job_pixels_exact",true},{"job_steps",steps},{"job_candidates",jobCounts},{"lease_released",true}};
}
nlohmann::json rangeCorrectness(){
    nlohmann::json rows=nlohmann::json::array();
#if defined(__APPLE__)
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    const auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"range Metal device");
    const auto queue=message(device,"newCommandQueue");GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
    auto context=GrDirectContexts::MakeMetal(backend);require(bool(context),"range Metal context");
#endif
    for(bool checkpoint:{false,true}){
        auto e=source();auto content=std::get<CompactStrokeReferenceCommand>(e.command).content.get();
        // A real self intersection and transparent material; canonical mask
        // application remains in the independent Direct mapper.
        for(std::size_t i=0;i<content.stroke.knots.size();++i){auto& k=content.stroke.knots[i];
            k.x=160+125*std::sin(double(i)*.005);k.y=90+65*std::sin(double(i)*.01);}
        e.command=CompactStrokeReferenceCommand{content};e.record.content=content;
        const auto& stroke=std::get<CompactStrokeReferenceCommand>(e.command).content.get().stroke;
        const auto full=ink::compileCompactContour(stroke.config,stroke.knots);
        RenderCacheOptions options;options.maxBytes=8U*1024U*1024U;options.sharedMaxBytes=16U*1024U*1024U;
        if(checkpoint)options.maxEntryBytes=1024U*1024U;
        PreparedGeometryCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});
        auto body=cache.get(e,options,budget,true);require(body&&body->primitiveCount()>2,"negative body admission");
        RangeReference ranges(content,checkpoint);const auto ribbonCount=full.patches.size()-1;
        // Nonmonotonic, cap-only, empty, disjoint and unknown-cull requests
        // exercise actual draw commands, not just index outputs.
        for(unsigned basis=0;basis<4;++basis){
            Recorder actual,oracle;for(auto* c:{&actual,&oracle}){
                if(basis==1){c->clipRect(SkRect::MakeLTRB(120,20,190,120));c->translate(7,-2);c->skew(.2F,-.02F);}
                if(basis==2)c->clipRect(SkRect::MakeLTRB(1500,1500,1600,1600));
                if(basis==3){SkMatrix matrix;matrix.setPerspX(.0001F);c->concat(matrix);}
            }
            for(const auto first:{ribbonCount,std::size_t(0),ribbonCount/2,body->primitiveCount(),ribbonCount+1,std::size_t(3)}){
                const auto expected=ranges.evaluate(actual,first,1,true);
                require(body->nextVisiblePrimitive(actual,first)==expected.next,"nonmonotonic/basis select mismatch");
                // An empty request is never allowed a range lookup/replay.
                CountScope counts;body->drawContour(actual,first,0);require(counts.counts()["total"]==0,"empty query scanned descriptors");
            }
        }
        for(unsigned gpu=0;gpu<2;++gpu)for(unsigned dpr:{1,2,4}){
            sk_sp<SkSurface> actual,oracle;
#if defined(__APPLE__)
            if(gpu){actual=SkSurfaces::RenderTarget(context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(320*dpr,180*dpr));
                oracle=SkSurfaces::RenderTarget(context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(320*dpr,180*dpr));}
            else
#endif
            {actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320*dpr,180*dpr));oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320*dpr,180*dpr));}
            require(actual&&oracle,"range correctness target");
            for(auto* s:{actual.get(),oracle.get()}){auto* c=s->getCanvas();c->clear(SK_ColorBLACK);c->scale(float(dpr),float(dpr));
                c->clipRect(SkRect::MakeLTRB(3,2,314,176));c->translate(.25F,.125F);}
            // Opposite request order uses fresh surfaces per segment. Whole
            // stream stays original ribbons then caps, with identical AA.
            for(std::size_t first=0;first<body->primitiveCount();first+=32)body->drawContour(*actual->getCanvas(),first,32);
            drawCompactContourCoverageToSkCanvas(*oracle->getCanvas(),stroke.config,full);
            require(rangePixels(actual)==rangePixels(oracle),"same backend range pixels not exact");
            rows.push_back({{"checkpoint",checkpoint},{"backend",gpu?"Metal":"Raster"},{"DPR",dpr},{"pixels_exact",true}});
        }
        // Two independently owned tile/range jobs interleave on one immutable
        // body, with different clips/matrices. No shared cursor may leak.
        std::array<sk_sp<SkSurface>,2> targets,oracles;CompactCoverageTargets scratch;scratch.budget=budget;
        std::array<std::unique_ptr<CompactStrokeRasterJob>,2> jobs;CompactRasterWork work;
        for(unsigned i=0;i<2;++i){targets[i]=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320,180));oracles[i]=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320,180));
            for(auto* s:{targets[i].get(),oracles[i].get()}){s->getCanvas()->clear(SK_ColorTRANSPARENT);
                if(i){s->getCanvas()->clipRect(SkRect::MakeLTRB(10,5,300,160));s->getCanvas()->translate(7,-2);s->getCanvas()->skew(.1F,-.02F);}}
            jobs[i]=std::make_unique<CompactStrokeRasterJob>(*targets[i]->getCanvas(),stroke.config,stroke.knots,nullptr,false,budget,options.maxEntryBytes,std::function<void(std::size_t)>{},true,body);
        }
        bool done[2]{};unsigned steps=0;while(!done[0]||!done[1]){require(++steps<20000,"interleaved jobs stalled");
            for(unsigned i=0;i<2;++i)if(!done[i])done[i]=jobs[i]->step(*targets[i]->getCanvas(),stroke.config,stroke.knots,stroke.seed,scratch,false,&work);}
        for(unsigned i=0;i<2;++i){drawCompactStrokeToSkCanvas(*oracles[i]->getCanvas(),stroke.config,stroke.knots,stroke.seed);
            require(rangePixels(targets[i])==rangePixels(oracles[i]),"interleaved job basis leaked");jobs[i].reset();}
        scratch.clear();cache.clear();body.reset();require(budget->used()==0,"interleaved jobs leaked leases");
    }
#if defined(__APPLE__)
    context->flushAndSubmit(GrSyncCpu::kYes);context.reset();message(pool,"drain");
#endif
    return {{"pixels",rows},{"nonmonotonic_cap_empty_disjoint_perspective",true},{"two_jobs_changed_clip_matrix_grid",true},{"leases_released",true}};
}
void rangeCapBoundary(){
    auto e=source();auto content=std::get<CompactStrokeReferenceCommand>(e.command).content.get();
    const auto contour=ink::compileCompactContour(content.stroke.config,content.stroke.knots);
    const auto ribbons=contour.patches.size()-1;require(ribbons%32!=0,"cap boundary fixture aligned groups");
    PreparedGeometryCache cache;auto budget=std::make_shared<resources::ResourceBudgetManager>();
    auto body=cache.get(e,RenderCacheOptions{},budget);require(bool(body),"cap boundary fullpath body");
    RangeReference ref(content,false);
    for(const auto clip:{SkRect::MakeXYWH(1410,35,10,10),SkRect::MakeXYWH(1438,90,10,10),SkRect::MakeXYWH(1600,100,10,10)}){
        Recorder canvas;canvas.clipRect(clip);
        for(const auto first:{ribbons-1,ribbons,ribbons+1,body->primitiveCount()}){
            const auto expected=ref.evaluate(canvas,first,1,true);const auto actual=body->nextVisiblePrimitive(canvas,first);
            std::cout<<"cap boundary first="<<first<<" expected="<<expected.next<<" actual="<<actual<<'\n';
            require(actual==expected.next,"CAP_ONLY_SELECT_REVISITS_FINISHED_RIBBON_GROUP");
        }
    }
    // Many real reversal caps span several independently bounded cap groups.
    // Range requests may move backward or start beyond the ribbon pass.
    content.stroke.knots.clear();content.stroke.config.smoothing=0;
    for(unsigned i=0;i<256;++i)content.stroke.knots.push_back({20.+i*4.,i%2?130.:40.,32768});
    e.record.content=content;e.command=CompactStrokeReferenceCommand{content};
    auto many=cache.get(e,RenderCacheOptions{},budget);require(bool(many),"multiple cap group body");
    const auto full=ink::compileCompactContour(content.stroke.config,content.stroke.knots);
    require(full.caps.size()>32,"multiple cap groups not exercised");const auto r=full.patches.size()-1;
    RangeReference manyRef(content,false);Recorder mapped,original;
    drawCompactContourCoverageToSkCanvas(original,content.stroke.config,full);
    for(const auto first:{r+33,r,r-1,std::size_t(0),r+1,many->primitiveCount()}){
        const auto previous=mapped.calls.size();many->drawContour(mapped,first,7);
        const auto end=std::min(first+7,many->primitiveCount());
        require(mapped.calls.size()-previous==end-first,"nonmonotonic range draw count");
        for(auto i=first;i<end;++i)require(mapped.calls[previous+i-first]==original.calls[i],"nonmonotonic cap/ribbon draw order or geometry");
    }
    for(const auto clip:{SkRect::MakeXYWH(100,30,120,110),SkRect::MakeXYWH(1500,30,50,110)}){
        Recorder canvas;canvas.clipRect(clip);
        for(const auto first:{r-1,r,r+1,r+33,many->primitiveCount()}){
            const auto expected=manyRef.evaluate(canvas,first,7,true);
            require(many->nextVisiblePrimitive(canvas,first)==expected.next,"multiple cap groups select mismatch");
        }
    }
}
}
int main(int argc,char** argv){try{
    if(argc==2&&std::string_view(argv[1])=="--cap-boundary"){rangeCapBoundary();return 0;}
    require(argc==2,"range output");bool bounded=true;
    nlohmann::json result{{"schema","g5-render-range-traversal-native-v1"},{"cases",nlohmann::json::array()}};
    for(bool checkpoint:{false,true})result["cases"].push_back(rangeCase(checkpoint,bounded));
    rangeCapBoundary();result["cap_boundary_exact"]=true;
    result["correctness"]=rangeCorrectness();
    const auto stress=result["cases"][0]["rows"][0]["total"].get<std::uint64_t>()+result["cases"][1]["rows"][0]["total"].get<std::uint64_t>();
    result["stress_total"]=stress;result["old_stress_total"]=645;result["50_percent_reduction"]=2*stress<=645;
    result["per_call_bounds"]=bounded;bounded=bounded&&2*stress<=645;
    result["bounded_repeat_work"]=bounded;std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<"stress="<<stress<<" bounds="<<result["per_call_bounds"]<<'\n';
    require(bounded,"REPEATED_RANGE_PREFIX_CANDIDATES_EXCEED_BOUND");return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
