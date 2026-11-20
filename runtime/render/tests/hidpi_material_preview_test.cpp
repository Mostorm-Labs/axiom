#define main hdPreparedReferenceMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
namespace canvas::render {
struct HidpiPreviewQualification {
    static auto budget(SkiaRenderer& r){return r.resourceBudget_;}
};
}

// Exercises the production BrushSession -> controller -> renderer with a real
// Metal target. Generated work is measured at the draw seam, not source text.
#if defined(__APPLE__)
namespace {
// Task-local acquisition adapter. Each sample submits asynchronously just as
// the production Web provider does; only the final drain waits for completion.
class PreviewGrowthTarget final:public NativeMetalTarget {
    std::shared_ptr<NativeMetalCompletion> completion_;
    std::uint64_t presents_=0;
 public:
    explicit PreviewGrowthTarget(std::shared_ptr<NativeMetalCompletion> c)
        :NativeMetalTarget(c),completion_(std::move(c)){}
    BackendSubmissionResult present()noexcept override {
        completion_->context->flushAndSubmit(GrSyncCpu::kNo);
        completion_->service();++presents_;return BackendSubmissionResult::accepted();
    }
    std::uint64_t presentCount()const noexcept override{return presents_;}
};
void growth(const char* profile,unsigned dpr,unsigned count,const char* input,const char* output){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    {
        auto completion=std::make_shared<NativeMetalCompletion>();
        auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
        GrMtlBackendContext context{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
        completion->context=GrDirectContexts::MakeMetal(context);
        require(bool(completion->context),"HD real GPU unavailable");
        PreviewGrowthTarget target(completion);
        require(target.resize(1431*dpr,1010*dpr).code==BackendSubmissionCode::kAccepted,"HD target");
        auto package=ink::BrushPackageCatalog{}.loadDefault(profile,3).package;
        require(bool(package.compactConfig),"HD package");
        package.compactConfig->size=32;package.compactConfig->opacity=.65;
        package.compactConfig->color={.12,.27,.61,1};
        package.compactConfig->startTaper=0;package.compactConfig->endTaper=0;
        package.compactConfig->startCap=true;package.compactConfig->endCap=true;
        ink::BrushSession session(1,ink::resolveBrushState(package,0x13579bdf));
        require(bool(session.begin()),"HD session");
        SkiaRenderer renderer;RenderCacheOptions options;
        options.maxBytes=512U*1048576;options.sharedMaxBytes=1024U*1048576;options.maxEntryBytes=128U*1048576;
        renderer.setRenderCacheOptions(options);
        PreviewSurfaceController controller(renderer,target);
        require(controller.begin(1,1,1,target.generation()),"HD controller");
        controller.setSurfaceMetrics({1431,1010,1431*dpr,1010*dpr,float(dpr),1});
        PreviewStyleOverride style;style.preferCapturedPaint=true;
        std::ifstream stream(input);nlohmann::json trace;stream>>trace;
        require(trace.size()>=count+1,"HD trace size");
        const auto started=std::chrono::steady_clock::now();
        for(unsigned i=0;i<=count;++i){
            const auto& v=trace.at(i);
            ink::BrushSample sample{v.at("x"),v.at("y"),.5,false,i+1,v.at("ns").get<std::uint64_t>(),false};
            ink::BrushPreviewDelta delta;
            require(session.append({&sample,1},{},delta)==ink::BrushSessionError::kNone,"HD append");
            require(controller.update(delta)&&controller.renderIfDirty(style),"HD preview");
        }
        const auto work=renderer.compactWork();const auto engine=session.compactMetrics();
        const auto before=renderer.submissionCount();
        require(controller.renderIfDirty(style)&&renderer.submissionCount()==before,"HD nochange submitted");
        nlohmann::json row{{"profile",profile},{"revision",3},{"dpr",dpr},{"moves",count},
            {"gpu",bool(target.acquire().frame.surface->recordingContext())},{"width",1431*dpr},{"height",1010*dpr},
            {"generatedKnots",work.generatedKnots},{"retainedBytes",work.retainedBytes},{"temporaryBytes",work.temporaryBytes},
            {"engineSamples",engine.samples},{"engineCopiedKnots",engine.copiedKnots},{"submissions",before},
            {"seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()}};
        target.release();
        // Outside the work-count collection. No GPU completion is fabricated.
        completion->context->flushAndSubmit(GrSyncCpu::kYes);completion->service();
        row["finalDrainComplete"]=true;row["samplePresent"]="asynchronous";
        std::ofstream(output)<<row.dump(2)<<'\n';std::cout<<row.dump()<<'\n';
    }
    message(pool,"drain");
}
std::vector<std::uint8_t> pixels(SkiaSurfaceProvider& p,unsigned width,unsigned height){
    std::vector<std::uint8_t> out(width*height*4);require(p.readbackRgba(out).code==BackendSubmissionCode::kAccepted,"HD pixels");return out;
}
void correctness(const char* output,bool observeOnly=false,const char* baseline=nullptr){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    nlohmann::json rows=nlohmann::json::array();
    {
        auto completion=std::make_shared<NativeMetalCompletion>();auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
        GrMtlBackendContext context{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(context);require(bool(completion->context),"HD pixel GPU");
        for(const auto profile:{"crayon-wax-v1","chalk-compact-v1","watercolor-lite-v1"})for(unsigned dpr:{1U,2U,4U}){
            NativeMetalTarget target(completion),reference(completion);const unsigned width=320*dpr,height=200*dpr;
            require(target.resize(width,height).code==BackendSubmissionCode::kAccepted&&reference.resize(width,height).code==BackendSubmissionCode::kAccepted,"HD pixel surfaces");
            auto package=ink::BrushPackageCatalog{}.loadDefault(profile,3).package;package.compactConfig->size=24;package.compactConfig->opacity=.65;
            package.compactConfig->color={.12,.27,.61,1};package.compactConfig->startTaper=20;package.compactConfig->endTaper=30;
            ink::BrushSession session(1,ink::resolveBrushState(package,0x13579bdf));require(bool(session.begin()),"HD pixel session");
            SkiaRenderer renderer;PreviewSurfaceController controller(renderer,target);require(controller.begin(1,1,1,target.generation()),"HD pixel controller");
            controller.setSurfaceMetrics({320,200,width,height,float(dpr),1});controller.setPresentationDeferred(true);PreviewStyleOverride style;style.preferCapturedPaint=true;controller.setPresentationStyle(style);
            ink::BrushPreviewDelta delta;
            for(unsigned i=0;i<=240;++i){
                ink::BrushSample sample{160+120*std::sin(i*.033),100+68*std::sin(i*.071),.5,false,i+1,std::uint64_t(i)*16666667,false};
                require(session.append({&sample,1},{},delta)==ink::BrushSessionError::kNone&&controller.update(delta)&&controller.renderIfDirty(style),"HD pixel append");
            }
            const auto verify=[&](const char* scenario){
                const auto& geometry=controller.geometry();auto acquired=reference.acquire();auto* canvas=acquired.frame.surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
                canvas->scale(float(dpr),float(dpr));canvas->translate(geometry.viewport.translationX,geometry.viewport.translationY);canvas->scale(geometry.viewport.scale,geometry.viewport.scale);
                for(const auto& contour:geometry.contours)if(contour.compactChunks){
                    std::vector<const ink::BrushPreviewDelta::Chunk*> chain;for(auto c=contour.compactChunks;c;c=c->previous)chain.push_back(c.get());
                    std::vector<foundation::InkKnot> full;for(auto i=chain.rbegin();i!=chain.rend();++i){
                        const auto& knots=(*i)->knots;const auto first=std::uint64_t(width)*height*4<=16U*1024U*1024U&&!full.empty()&&!knots.empty()&&full.back()==knots.front()?1U:0U;
                        full.insert(full.end(),knots.begin()+first,knots.end());}
                    // Independent full-history coverage, no retained prefix or
                    // candidate cache. Small-view B uses a full device surface;
                    // preserve that grid rather than introduce a tiled oracle.
                    auto coverage=acquired.frame.surface->makeSurface(acquired.frame.surface->imageInfo());
                    auto* mask=coverage->getCanvas();mask->clear(SK_ColorBLACK);mask->setMatrix(canvas->getTotalMatrix());
                    internal::drawCompactCoverageToSkCanvas(*mask,*contour.compactConfig,full,0,-1,contour.compactSeed);
                    internal::compositeCompactCoverageToSkCanvas(*canvas,*coverage->makeImageSnapshot(),*contour.compactConfig,contour.compactSeed);
                }
                canvas->resetMatrix();reference.release();reference.present();
                const auto a=pixels(target,width,height),b=pixels(reference,width,height);unsigned max=0;std::size_t changed=0,over2=0,nonzero=0;
                for(std::size_t i=0;i<a.size();++i){const auto error=unsigned(std::abs(int(a[i])-int(b[i])));max=std::max(max,error);changed+=error>0;over2+=error>2;nonzero+=b[i]>0;}
                rows.push_back({{"profile",profile},{"dpr",dpr},{"scenario",scenario},{"maximum",max},{"changed",changed},{"over2",over2},{"nonemptyChannels",nonzero}});
                std::ofstream(output)<<nlohmann::json{{"rows",rows}}.dump(2)<<'\n';
                if(over2){std::ofstream(std::string(output)+"-actual.rgba",std::ios::binary).write(reinterpret_cast<const char*>(a.data()),a.size());
                    std::ofstream(std::string(output)+"-reference.rgba",std::ios::binary).write(reinterpret_cast<const char*>(b.data()),b.size());}
                const auto witness=std::string(output)+"-"+profile+"-"+std::to_string(dpr)+"-"+scenario+".rgba";
                if(observeOnly)std::ofstream(witness,std::ios::binary).write(reinterpret_cast<const char*>(a.data()),a.size());
                if(baseline){
                    const auto original=std::string(baseline)+"-"+profile+"-"+std::to_string(dpr)+"-"+scenario+".rgba";
                    std::ifstream file(original,std::ios::binary);std::vector<std::uint8_t> old{std::istreambuf_iterator<char>(file),{}};
                    require(old.size()==a.size(),"HD baseline witness missing");unsigned maxB=0;
                    for(std::size_t i=0;i<a.size();++i)maxB=std::max(maxB,unsigned(std::abs(int(a[i])-int(old[i]))));
                    rows.back()["sameGPUbaselineMaximum"]=maxB;
                    std::ofstream(output)<<nlohmann::json{{"rows",rows}}.dump(2)<<'\n';
                    require(maxB<=2,"HD preview differs real baseline GPU");
                }
                if(!observeOnly)require(nonzero>0&&over2==0,"HD preview differs independent full material");
            };
            verify("self-overlap-taper-opacity-texture");
            const auto before=renderer.compactWork().generatedKnots;require(controller.updateViewport(1.1F,-8,3)&&controller.renderIfDirty(style),"HD camera");verify("pose-rebuild");
            require(renderer.compactWork().generatedKnots>before,"HD cold rebuild hidden");
            auto predicted=ink::BrushSample{290,180,.5,false,242,241*16666667ULL,false};
            require(session.append({}, {&predicted,1},delta)==ink::BrushSessionError::kNone&&controller.update(delta)&&controller.renderIfDirty(style),"HD predicted");verify("predicted-replace");
            require(session.append({}, {},delta)==ink::BrushSessionError::kNone&&controller.update(delta)&&controller.renderIfDirty(style),"HD retract");verify("predicted-retract");
            const auto work=renderer.compactWork().generatedKnots;require(controller.begin(1,2,1,target.generation()),"HD secondcontact");
            auto second=delta;second.revision=1;second.compactSeed+=7;require(controller.updateForSession(2,second)&&controller.renderIfDirty(style),"HD second draw");
            const auto both=renderer.compactWork().generatedKnots;require(both>work,"HD second session missing");verify("ordered-two-sessions");
            require(controller.retireSession(2,target.generation())&&controller.renderIfDirty(style),"HD retiresecond");
            const auto after=renderer.compactWork().generatedKnots;if(!observeOnly)require(after-both<=delta.compactChunks->knots.size()+1,"HD second retirement rebuilt unrelated prefix");verify("retire-only-owned-entry");
            auto budget=HidpiPreviewQualification::budget(renderer);require(bool(budget),"HD budgetowner");
            const auto pending=budget->snapshot(resources::ResourceClass::kRaster);if(!observeOnly)require(pending.used>=width*height*4,"HD stable uncharged");
            require(controller.cancelSession(1)&&controller.renderIfDirty(style),"HD cancel");
            renderer.serviceResourceRetirements();if(!observeOnly)require(renderer.compactWork().retainedBytes==0,"HD retired resident counter");
        }
    }
    message(pool,"drain");std::cout<<nlohmann::json{{"rows",rows}}.dump()<<'\n';
}
void lifecycle(const char* output){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    {
        auto completion=std::make_shared<NativeMetalCompletion>();auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
        GrMtlBackendContext context{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(context);require(bool(completion->context),"HD lifecycle GPU");
        NativeMetalTarget target(completion);require(target.resize(1024,768).code==BackendSubmissionCode::kAccepted,"HD lifecycle target");
        auto package=ink::BrushPackageCatalog{}.loadDefault("crayon-wax-v1",3).package;
        ink::BrushSession session(1,ink::resolveBrushState(package,42));require(session.begin(),"HD lifecycle session");
        ink::BrushPreviewDelta delta;for(unsigned i=0;i<120;++i){ink::BrushSample s{20.+i*4,100.+30*std::sin(i*.1),.5,false,i+1,i*16666667ULL,false};require(session.append({&s,1},{},delta)==ink::BrushSessionError::kNone,"HD lifecycle append");}
        SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=16U*1048576;options.sharedMaxBytes=24U*1048576;options.maxEntryBytes=8U*1048576;renderer.setRenderCacheOptions(options);
        PreviewSurfaceController controller(renderer,target);PreviewStyleOverride style;style.preferCapturedPaint=true;controller.setPresentationStyle(style);
        require(controller.begin(1,1,1,target.generation())&&controller.update(delta)&&controller.renderIfDirty(style),"HD lifecycle first");
        auto budget=HidpiPreviewQualification::budget(renderer);const auto bytes=1024U*768U*4;
        require(budget->used(resources::ResourceClass::kRaster)==bytes&&budget->used(resources::ResourceClass::kScratch)==bytes,"HD actual pixel leases");
        const auto epoch=budget->beginGpuEpoch(resources::kPixelResourceClasses,resources::resourceClassMask(resources::ResourceClass::kRaster)|resources::resourceClassMask(resources::ResourceClass::kScratch));require(bool(epoch),"HD held real epoch");
        auto acquired=target.acquire();auto sampled=acquired.frame.surface->makeSurface(16,16);sampled->getCanvas()->drawImage(acquired.frame.surface->makeTemporaryImage(),0,0);target.release();
        require(budget->endGpuEpoch(*epoch),"HD held epoch end");struct Gate{bool done=false;} gate;GrFlushInfo flush;
        flush.fFinishedContext=&gate;flush.fFinishedProc=[](void* p){static_cast<Gate*>(p)->done=true;};completion->context->flush(flush);require(completion->context->submit(GrSyncCpu::kNo),"HD real read submit");sampled.reset();
        require(controller.cancelSession(1)&&controller.renderIfDirty(style),"HD cancel clear");
        require(budget->snapshot(resources::ResourceClass::kRaster).pendingRelease>=bytes&&budget->snapshot(resources::ResourceClass::kScratch).pendingRelease>=bytes,"HD cancel freed in-flight leases");
        const auto quota=budget->reserve(resources::ResourceClass::kRecorded,options.maxBytes-budget->used()-bytes,true);require(bool(quota),"HD quota pressure");resources::ResourceLease pressure(budget,*quota);
        require(controller.begin(1,2,2,target.generation())&&controller.update(delta),"HD restart");
        const auto before=pixels(target,1024,768);const auto presents=target.presentCount();
        const auto result=renderer.renderPreview(target,controller.geometry(),style);
        require(result.code==BackendSubmissionCode::kRetry&&pixels(target,1024,768)==before&&target.presentCount()==presents,"HD pending quota published or destroyed feedback");
        completion->context->flushAndSubmit(GrSyncCpu::kYes);completion->service();require(gate.done,"HD real completion not observed");
        require(budget->completeGpuEpoch(*epoch),"HD acknowledge real held epoch");pressure.reset();renderer.serviceResourceRetirements();
        require(controller.renderIfDirty(style),"HD quota release failed progress");
        require(!controller.retireSession(99,target.generation()),"HD wrong retire accepted");
        require(controller.retireSession(2,target.generation()),"HD final retirement");
        completion->context->flushAndSubmit(GrSyncCpu::kYes);completion->service();renderer.serviceResourceRetirements();
        require(budget->used(resources::ResourceClass::kRaster)==0&&budget->used(resources::ResourceClass::kScratch)==0,"HD inactive final owner retains pixels without next stroke");
        SkiaRenderer tiny;options.maxEntryBytes=1024;tiny.setRenderCacheOptions(options);
        const auto prior=pixels(target,1024,768);auto geometry=controller.geometry();geometry.revision=1;geometry.surfaceGeneration=SurfaceGeneration{target.generation()};
        geometry.contours.push_back({});geometry.contours.back().session=1;geometry.contours.back().compactConfig=delta.compactConfig;geometry.contours.back().compactChunks=delta.compactChunks;
        require(tiny.renderPreview(target,geometry,style).code==BackendSubmissionCode::kRejected&&pixels(target,1024,768)==prior,"HD permanently impossible quota retries or writes");
        nlohmann::json row{{"actualStableLeaseBytes",bytes},{"actualScratchLeaseBytes",bytes},{"heldGPUCompletionObserved",gate.done},{"pendingQuotaPreservesPixels",true},{"recoveryAccepted",true},{"finalInactivePixelBytes",budget->used(resources::ResourceClass::kRaster)+budget->used(resources::ResourceClass::kScratch)},{"permanentQuotaRejected",true}};
        std::ofstream(output)<<row.dump(2)<<'\n';std::cout<<row.dump()<<'\n';
    }
    message(pool,"drain");
}
}
#endif
int main(int argc,char** argv)try{
#if defined(__APPLE__)
    if(argc>=3&&std::string_view(argv[1])=="--pixels"){correctness(argv[2],false,argc>3?argv[3]:nullptr);return 0;}
    if(argc==3&&std::string_view(argv[1])=="--baseline-pixels"){correctness(argv[2],true);return 0;}
    if(argc==3&&std::string_view(argv[1])=="--lifecycle"){lifecycle(argv[2]);return 0;}
    require(argc==7,"profile dpr count input output growth");
    growth(argv[1],unsigned(std::stoul(argv[2])),unsigned(std::stoul(argv[3])),argv[4],argv[5]);
#else
    throw std::runtime_error("HD real GPU fixture requires Metal");
#endif
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
