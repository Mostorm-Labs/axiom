#define main dpr4PostupInheritedMain
#include "canonical_prepared_body_test.cpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;
#undef main
#include <thread>
#include "../src/skia_navigation_tiles.hpp"
#include "../src/transient_display_state.hpp"
namespace {
#if defined(__APPLE__)
RenderCacheOptions pressureOptions(){
    RenderCacheOptions o;o.maxBytes=512U*1048576;o.sharedMaxBytes=1024U*1048576;
    o.maxEntryBytes=128U*1048576;return o;
}
FramePlan pressurePlan(unsigned algorithm,unsigned count,unsigned copies,unsigned dpr,
                       bool isolated,std::uint64_t surface=1,unsigned masks=1){
    auto loaded=ink::BrushPackageCatalog{}.loadDefault(algorithm==5?"crayon-wax-v1":"chalk-compact-v1",3);
    require(bool(loaded)&&loaded.package.compactConfig.has_value(),"pressure package");
    semantic::CompactStrokeContent content;content.stroke.config=*loaded.package.compactConfig;
    content.stroke.config.size=18;content.stroke.config.opacity=.65;
    content.stroke.config.color={.12,.27,.61,1};content.stroke.config.startTaper=9;
    content.stroke.config.endTaper=13;content.stroke.seed=0x13579bdf;
    for(unsigned i=0;i<count;++i)content.stroke.knots.push_back({128+100*std::sin(i*.012),64+42*std::sin(i*.019),std::uint16_t(18000+(i%190)*240)});
    semantic::ObjectRecord object;object.id=ObjectId::fromUint64(1);
    object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=std::move(content);
    for(unsigned n=0;n<masks;++n){semantic::SweptCircleMask mask;
        mask.segments.push_back({{{125.+n*20,15},5},{{125.+n*20,112},5},{125.+n*20,40},{125.+n*20,80}});
        object.erase_masks.push_back({ObjectId::fromUint64(700+n),mask});}
    ReferenceTraversalEntry e;e.record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();
    e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
    auto base=renderPlan(e,1,dpr,surface);auto draw=base.referenceDrawList;
    for(unsigned i=0;i<copies;++i){auto copy=e;copy.record.objectId=ObjectId::fromUint64(i+1);
        copy.record.transform={.97,.03,-.03,.97,double(i*24),double(i*12)};
        if(i==0)draw.entries[1]=copy;else draw.entries.push_back(std::move(copy));}
    return {base.frame,std::move(draw),{},false,isolated};
}

std::vector<std::uint8_t> pressurePixels(NativeMetalTarget& target){auto a=target.acquire();std::vector<std::uint8_t> bytes(a.frame.surface->width()*a.frame.surface->height()*4);target.release();require(target.readbackRgba(bytes).code==BackendSubmissionCode::kAccepted,"read pixels");return bytes;}
FramePlan navigationPlan(const FramePlan& basis,std::vector<ReferenceTraversalEntry> entries,
                         std::uint64_t semantic,std::uint64_t id,float scale=1,float tx=0,float ty=0){
    const auto& f=basis.frame;
    const FrameState frame{f.viewId,{{(128-tx)/scale,(64-ty)/scale},scale,0,CameraGeneration{id}},
        {-tx/scale,-ty/scale,(256-tx)/scale,(128-ty)/scale},f.metrics,
        semantic::SemanticGeneration{semantic},SceneRevision{semantic},f.surfaceGeneration,f.metricsGeneration,FrameId{id},f.viewportClip};
    ReferenceDrawList draw{frame};draw.viewportClip=f.viewportClip;draw.worldToView={scale,0,0,scale,tx,ty};draw.entries=std::move(entries);
    return {frame,std::move(draw),basis.background,false,basis.isolateEraseMasks};
}
json committedNavigation(const std::shared_ptr<NativeMetalCompletion>& completion,bool tail,bool pinch=false,bool inkOnly=false,bool longMask=false){
    NativeMetalTarget target(completion),oracle(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted&&oracle.resize(512,256).code==BackendSubmissionCode::kAccepted,"navigation targets");
    auto base=pressurePlan(5,385,2,2,!inkOnly,target.generation());auto liveEntries=base.referenceDrawList.entries;
    if(inkOnly)liveEntries.erase(liveEntries.begin());
    semantic::SweptCircleMask active;active.segments.push_back({{{110,25},6},{{115,80},6},{111,40},{113,60}});
    for(auto& e:liveEntries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){e.presentationEraseActive=true;e.presentationEraseMasks={{ObjectId{},active}};}
    auto live=navigationPlan(base,std::move(liveEntries),1,1);EditingOverlay bare{live.frame};SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());
    TransientDisplayIdentity identity{std::make_shared<int>(1),1,1};unsigned warm=0;
    for(;warm<4096;++warm){auto s=renderer.renderTransientOverlay(target,live,&bare,identity);if(s.code==BackendSubmissionCode::kAccepted)break;require(s.code==BackendSubmissionCode::kRetry,"live feedback failed");}
    require(warm<4096,"live feedback stalled");
    auto final=live.referenceDrawList.entries;
    for(auto& e:final)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){
        auto mask=active;if(tail)mask.segments.push_back({{{115,80},6},{{180,100},8},{135,92},{165,97}});
        if(longMask)for(unsigned n=0;n<380;++n){auto segment=mask.segments.back();segment.p0=segment.p1;segment.p1.position={100.+double(n%70),30.+double(n%60)};segment.control1=segment.p0.position;segment.control2=segment.p1.position;mask.segments.push_back(segment);}
        e.record.eraseMasks.push_back({ObjectId::fromUint64(8888),mask});e.presentationEraseActive=false;e.presentationEraseMasks.clear();

    }
    const float scale=pinch?1.25F:1,tx=pinch?-32.F:-4.F,ty=pinch?-16.F:0.F;
    auto moved=navigationPlan(base,final,2,2,scale,tx,ty),fixed=navigationPlan(base,final,2,3);
    const NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& e,NavigationSourceDiagnostics& d){e=final;d.sceneStamped=true;d.semanticGeneration=2;d.sceneRevision=2;return true;};
    const auto before=pressureTestCommands,presents=target.presentCount();unsigned calls=0;bool presented=false;std::uint64_t maxPressure=0,maxMasks=0;
    for(;calls<4096;++calls){const auto pressureBefore=pressureTestCommands,masksBefore=materialMaskTestDraws;auto s=renderer.renderNavigation(target,{scale,tx,ty},moved.frame,query,false,nullptr,[](const auto&){return true;},&moved);
        maxPressure=std::max(maxPressure,pressureTestCommands-pressureBefore);maxMasks=std::max(maxMasks,materialMaskTestDraws-masksBefore);
        require(s.code!=BackendSubmissionCode::kRejected,"committed navigation rejected");
        if(renderer.navigationWork().presented){presented=true;++calls;break;}require(target.presentCount()==presents,"pending terminal masks changed provider");}
    const auto queued=pressureTestCommands-before;
    unsigned difference=256;std::size_t outsideEdge=0,edgeDifferences=0;
    if(presented){
        SkiaRenderer independent;auto limits=pressureOptions();limits.enabled=false;independent.setRenderCacheOptions(limits);
        require(independent.renderFrame(oracle,fixed).code==BackendSubmissionCode::kAccepted,"stable independent reference");
        auto a=oracle.acquire();auto image=a.frame.surface->makeImageSnapshot();oracle.release();
        auto expected=SkSurfaces::RenderTarget(completion->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(512,256));
        expected->getCanvas()->clear(SK_ColorWHITE);expected->getCanvas()->translate(tx*2,ty*2);expected->getCanvas()->scale(scale,scale);SkPaint copy;copy.setBlendMode(SkBlendMode::kSrc);expected->getCanvas()->drawImage(image,0,0,SkSamplingOptions(SkFilterMode::kLinear),&copy);
        auto actual=pressurePixels(target);std::vector<std::uint8_t> bytes(actual.size());const auto info=SkImageInfo::Make(512,256,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
        require(expected->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"feedback oracle read");
        std::vector<bool> edges(512*256,false);
        if(longMask){
            // Independent CPU mask coverage identifies the accepted AA edge
            // allowance. It does not use the candidate's prepared history.
            for(const auto& e:final)if(!e.record.eraseMasks.empty()){
                auto mask=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(512,256));auto* mc=mask->getCanvas();mc->clear(SK_ColorWHITE);mc->translate(tx*2,ty*2);mc->scale(scale*2,scale*2);
                const auto& t=e.record.transform;mc->concat(SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1));drawEraseMasksToSkCanvas(*mc,e);
                std::vector<std::uint8_t> alpha(bytes.size());require(mask->readPixels(info,alpha.data(),info.minRowBytes(),0,0),"independent mask boundary read");
                for(int y=0;y<256;++y)for(int x=0;x<512;++x){const auto a=alpha[(y*512+x)*4+3];if(a>0&&a<255)
                    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)if(x+dx>=0&&x+dx<512&&y+dy>=0&&y+dy<256)edges[(y+dy)*512+x+dx]=true;}
            }
        }
        difference=0;for(std::size_t i=0;i<bytes.size();++i){auto d=unsigned(std::abs(int(actual[i])-int(bytes[i])));difference=std::max(difference,d);if(d>2){if(edges[i/4])++edgeDifferences;else ++outsideEdge;}}

    }
    return {{"maximumPressurePerOpportunity",maxPressure},{"maximumMaskPerOpportunity",maxMasks},{"outsideIndependentAAEdge",outsideEdge},{"AAEdgeDifferences",edgeDifferences},{"case",longMask?"long-terminal-mask-bounded":inkOnly?"ordered-clear-ink-only":pinch?"committed-tail-pinch":tail?"committed-tail-pan":"equal-final-same-owner"},{"calls",calls},{"presented",presented},{"presentDelta",target.presentCount()-presents},
        {"newPressureCommands",queued},{"maximumPixelDifference",difference},{"preparedBuilds",renderer.renderCacheWork().preparedBodyBuilds},
        {"navigationKnots",renderer.navigationWork().knotSteps},{"samplingBasis","full final committed scene on retained device grid then linear camera resample"},{"pass",presented&&outsideEdge==0&&maxPressure<=128&&maxMasks<=128&&(!tail?queued==0:true)}};
}
struct LiveFixture {
    FramePlan base;
    std::optional<FramePlan> live;
    std::vector<ReferenceTraversalEntry> terminal;
    TransientDisplayIdentity identity{std::make_shared<int>(77),1,1};
    LiveFixture(std::uint64_t surface,bool exposure=false):base(pressurePlan(5,385,2,2,true,surface)){
        auto entries=base.referenceDrawList.entries;
        if(exposure){
            semantic::ObjectRecord o;o.id=ObjectId::fromUint64(199);o.kind=semantic::ObjectKind::kShape;o.kind_version=1;
            o.content=semantic::ShapeContent{1,40,192};o.transform.tx=-32;o.transform.ty=-32;
            o.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}}};
            auto record=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1}).records.front();
            entries.insert(entries.begin(),ReferenceTraversalEntry{record,true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});
        }
        if(exposure){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(201);o.kind=semantic::ObjectKind::kShape;o.kind_version=1;o.content=semantic::ShapeContent{1,8,12};o.transform.tx=248;o.transform.ty=10;
            o.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,1,0,1}}}}};auto record=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1}).records.front();
            entries.push_back(ReferenceTraversalEntry{record,true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});}
        semantic::SweptCircleMask active;active.segments.push_back({{{110,25},6},{{115,80},6},{111,40},{113,60}});
        for(auto& e:entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){
            e.presentationEraseActive=true;e.presentationEraseMasks={{ObjectId{},active}};
        }
        live.emplace(navigationPlan(base,std::move(entries),1,1));
        terminal=live->referenceDrawList.entries;
        if(exposure){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(200);o.kind=semantic::ObjectKind::kShape;o.kind_version=1;o.content=semantic::ShapeContent{1,24,640};o.transform.tx=-32;o.transform.ty=-256;
            o.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}}};auto record=projectRuntimeScene({&o,1},semantic::SemanticGeneration{2}).records.front();
            terminal.insert(terminal.begin(),ReferenceTraversalEntry{record,true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}});}
        for(auto& e:terminal)if(e.presentationEraseActive){
            auto mask=active;mask.segments.push_back({{{115,80},6},{{180,100},8},{135,92},{165,97}});
            e.record.eraseMasks.push_back({ObjectId::fromUint64(8888),mask});
            e.presentationEraseMasks.clear();e.presentationEraseActive=false;
        }
    }
    void seed(SkiaRenderer& r,NativeMetalTarget& t){EditingOverlay bare{live->frame};
        for(unsigned i=0;i<4096;++i){auto status=r.renderTransientOverlay(t,*live,&bare,identity);
            if(status.code==BackendSubmissionCode::kAccepted)return;require(status.code==BackendSubmissionCode::kRetry,"seed failed");}
        require(false,"seed stalled");
    }
    NavigationSourceQuery query(bool valid=true)const{return [&,valid](const auto& region,auto& entries,auto& d){for(const auto& e:terminal)if(e.record.worldBounds.intersects(region))entries.push_back(e);d.sceneStamped=valid;d.semanticGeneration=2;d.sceneRevision=2;return true;};}
};
json negativeAndQuota(const std::shared_ptr<NativeMetalCompletion>& completion){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"negative target");
    LiveFixture f(target.generation());SkiaRenderer r;auto options=pressureOptions();options.navigationMaxSteps=1;r.setRenderCacheOptions(options);f.seed(r,target);
    const auto before=pressurePixels(target);const auto presents=target.presentCount();auto plan=navigationPlan(f.base,f.terminal,2,2,1,-4,0);
    const auto nav=[&](const FramePlan& p,const NavigationSourceQuery& query){return r.renderNavigation(target,{1,-4,0},p.frame,query,false,nullptr,{},&p);};
    auto invalid=nav(plan,f.query(false));require(!r.navigationWork().presented&&target.presentCount()==presents&&pressurePixels(target)==before,"invalid Scene query published");(void)invalid;
    auto wrong=f.terminal;wrong.back().record.transform.tx+=.5;auto wrongPlan=navigationPlan(f.base,wrong,2,3,1,-4,0);
    auto wrongQuery=[&](const auto&,auto& entries,auto& d){entries=wrong;d.sceneStamped=true;d.semanticGeneration=2;d.sceneRevision=2;return true;};
    nav(wrongPlan,wrongQuery);require(!r.navigationWork().presented&&target.presentCount()==presents&&pressurePixels(target)==before,"changed pose reused feedback");
    NativeMetalTarget foreign(completion);require(foreign.resize(512,256).code==BackendSubmissionCode::kAccepted,"foreign target");
    (void)r.renderNavigation(foreign,{1,-4,0},plan.frame,{},false,nullptr,{},&plan);require(foreign.presentCount()==0,"foreign target reused feedback");
    auto staleBase=pressurePlan(5,385,2,2,true,target.generation()+1);auto stale=navigationPlan(staleBase,f.terminal,2,4,1,-4,0);
    require(nav(stale,f.query()).code!=BackendSubmissionCode::kAccepted&&target.presentCount()==presents,"stale generation published");
    // Hold actual owner quota. No denied private repair may change the target.
    auto budget=transientDisplayBudget.lock();require(bool(budget),"existing private budget witness");
    auto token=budget->reserve(resources::ResourceClass::kScratch,options.sharedMaxBytes-budget->used(),true);require(bool(token),"quota hold");
    const auto denied=nav(plan,f.query());require(denied.code!=BackendSubmissionCode::kRejected&&target.presentCount()==presents&&pressurePixels(target)==before,"quota denial corrupted provider");budget->release(*token);
    unsigned calls=0;for(;calls<4096;++calls){auto status=nav(plan,f.query());require(status.code==BackendSubmissionCode::kAccepted,"admitted repair must continue navigation accepted+pending");
        if(r.navigationWork().presented)break;require(target.presentCount()==presents&&pressurePixels(target)==before,"one-step mask repair partially published");}
    if(calls>=4096){r.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();return {{"case","negative-quota"},{"error","stalled"},{"pass",false}};}require(target.presentCount()==presents+1,"one-step recovery present count");
    const auto qualifiedPresents=target.presentCount();
    auto changedMask=f.terminal;
    for(auto& e:changedMask)if(!e.record.eraseMasks.empty()){auto& g=std::get<semantic::SweptCircleMask>(e.record.eraseMasks.back().geometry);g.segments.back().p1.radius+=4;break;}
    auto maskPlan=navigationPlan(f.base,changedMask,2,5,1,-4,0);nav(maskPlan,{});
    require(target.presentCount()==qualifiedPresents,"wrong final mask reused qualified feedback");
    auto body=f.terminal;body.back().record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.seed^=1;});
    body.back().command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(body.back().record.content)};
    auto sourcePlan=navigationPlan(f.base,body,2,6,1,-4,0);nav(sourcePlan,{});require(target.presentCount()==qualifiedPresents,"wrong source reused qualified feedback");
    auto dark=plan.background;dark.color={.2F,.1F,.4F,1};FramePlan darkPlan{plan.frame,plan.referenceDrawList,dark,false,plan.isolateEraseMasks};
    nav(darkPlan,f.query());require(target.presentCount()==qualifiedPresents,"wrong background reused qualified region");
    FramePlan policy{plan.frame,plan.referenceDrawList,plan.background,false,!plan.isolateEraseMasks};
    nav(policy,f.query());require(target.presentCount()==qualifiedPresents,"wrong isolation reused qualified region");
    r.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();
    require(budget->pendingGpuEpochCount()==0&&budget->used()==0,"navigation repair last-release leak");
    return {{"case","wrong-query-source-mask-pose-background-isolation-provider-generation-quota-recovery"},{"recoveryCalls",calls+1},{"pendingNoWrite",true},{"leaseFinalDrain",true},{"pass",true}};
}
json exposedNavigation(const std::shared_ptr<NativeMetalCompletion>& completion){
    NativeMetalTarget target(completion),reference(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted&&reference.resize(512,256).code==BackendSubmissionCode::kAccepted,"exposure targets");
    LiveFixture f(target.generation(),true);SkiaRenderer r;r.setRenderCacheOptions(pressureOptions());f.seed(r,target);
    auto visible=f.terminal;std::erase_if(visible,[](const auto& e){return e.record.objectId==ObjectId::fromUint64(201);});
    auto plan=navigationPlan(f.base,std::move(visible),2,2,1,16,0),stable=navigationPlan(f.base,f.terminal,2,3);
    require(f.terminal.size()==f.live->referenceDrawList.entries.size()+1,"new viewport-only source fixture");
    const auto bg=[&](const foundation::WorldRect& area){return std::none_of(f.terminal.begin(),f.terminal.end(),[&](const auto& e){return e.contributesPixels&&e.record.worldBounds.intersects(area);});};
    require(!bg({-16,0,0,128}),"nonempty exposure witness");
    unsigned calls=0;for(;calls<4096;++calls){auto status=r.renderNavigation(target,{1,16,0},plan.frame,f.query(),false,nullptr,bg,&plan);
        require(status.code!=BackendSubmissionCode::kRejected,"exposure rejected");if(r.navigationWork().presented)break;
        completion->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();}
    require(calls<4096,"nonempty exposure stalled");
    const auto work=r.navigationWork();auto actual=pressurePixels(target);
    // Independent complete scene on retained grid for the overlap. The exposed
    // strip is an opaque constant blue shape, independently rendered at the
    // requested grid: it is invariant under the navigation LOD's resampling.
    SkiaRenderer oracle;auto disabled=pressureOptions();disabled.enabled=false;oracle.setRenderCacheOptions(disabled);
    require(oracle.renderFrame(reference,stable).code==BackendSubmissionCode::kAccepted,"exposure stable oracle");
    auto a=reference.acquire();auto image=a.frame.surface->makeImageSnapshot();reference.release();
    auto expected=SkSurfaces::RenderTarget(completion->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(512,256));
    auto* ec=expected->getCanvas();ec->clear(SK_ColorBLUE);ec->translate(32,0);SkPaint copy;copy.setBlendMode(SkBlendMode::kSrc);ec->drawImage(image,0,0,SkSamplingOptions{},&copy);
    std::vector<std::uint8_t> pixels(actual.size());const auto info=SkImageInfo::Make(512,256,kRGBA_8888_SkColorType,kPremul_SkAlphaType);require(expected->readPixels(info,pixels.data(),info.minRowBytes(),0,0),"exposure oracle pixels");
    unsigned overlapDiff=0,stripDiff=0;for(unsigned y=0;y<256;++y)for(unsigned x=0;x<512;++x)for(unsigned c=0;c<4;++c){auto i=(y*512+x)*4+c;auto d=unsigned(std::abs(int(actual[i])-int(pixels[i])));(x<30?stripDiff:overlapDiff)=std::max(x<30?stripDiff:overlapDiff,d);}
    // Pixel x30..33 straddles the retained/LOD seam. It must remain opaque;
    // the two independently assembled source bases may differ at that seam.
    bool opaque=true;for(unsigned y=0;y<256;++y)for(unsigned x=30;x<34;++x)opaque&=actual[(y*512+x)*4+3]==255;
    return {{"case","retained-overlap-nonempty-exposure"},{"calls",calls+1},{"retainedFraction",work.retainedFraction},{"tilesBuilt",work.tilesBuilt},{"newViewportOnlyObject",true},{"oldViewportOnlyObjectCulled",true},{"knotSteps",work.knotSteps},{"overlapMaximumDifference",overlapDiff},{"exposedInteriorMaximumDifference",stripDiff},{"seamOpaque",opaque},{"pass",work.retainedFraction>0&&work.tilesBuilt>0&&overlapDiff<=2&&stripDiff<=2&&opaque}};
}

class AsyncNavigationTarget final:public NativeMetalTarget {
    std::uint64_t asynchronousPresents=0;
 public:
    using NativeMetalTarget::NativeMetalTarget;
    bool asynchronous=false;
    BackendSubmissionResult present()noexcept override{if(!asynchronous)return NativeMetalTarget::present();++asynchronousPresents;return BackendSubmissionResult::accepted();}
    std::uint64_t presentCount()const noexcept override{return NativeMetalTarget::presentCount()+asynchronousPresents;}
};
json retainedGpuLease(const std::shared_ptr<NativeMetalCompletion>& completion){
    AsyncNavigationTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"lease target");
    LiveFixture f(target.generation(),true);SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());f.seed(renderer,target);
    auto plan=navigationPlan(f.base,f.terminal,2,2,1,16,0);auto query=f.query();
    const auto bg=[&](const foundation::WorldRect& area){return std::none_of(f.terminal.begin(),f.terminal.end(),[&](const auto& e){return e.record.worldBounds.intersects(area);});};
    // Complete private terminal repair and exposure; then sample the same
    // retained region under an asynchronous real GPU present, with no readback.
    for(unsigned i=0;i<1024;++i){auto status=renderer.renderNavigation(target,{1,16,0},plan.frame,query,false,nullptr,bg,&plan);require(status.code!=BackendSubmissionCode::kRejected,"lease preparation rejected");if(renderer.navigationWork().presented)break;completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();}
    completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
    auto budget=transientDisplayBudget.lock();require(bool(budget),"lease owner witness");target.asynchronous=true;
    auto moved=navigationPlan(f.base,f.terminal,2,3,1,15,0);
    const auto status=renderer.renderNavigation(target,{1,15,0},moved.frame,query,false,nullptr,bg,&moved);
    require(status.code==BackendSubmissionCode::kAccepted&&renderer.navigationWork().presented&&renderer.navigationWork().retainedFraction>0,"retained region was not sampled");
    renderer.clearRenderCache();std::size_t pending=0;for(unsigned k=0;k<unsigned(resources::ResourceClass::kCount);++k)pending+=budget->snapshot(resources::ResourceClass(k)).pendingRelease;
    const auto epochs=budget->pendingGpuEpochCount();
    completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
    require(pending>0&&epochs>0&&budget->used()==0&&budget->pendingGpuEpochCount()==0,"retained display not held through real GPU completion");
    return {{"case","retained-region-real-gpu-completion"},{"pendingPixelBytesBeforeCompletion",pending},{"pendingEpochsBeforeCompletion",epochs},{"finalBytes",budget->used()},{"pass",true}};
}

#endif
}
int main(int argc,char**argv){json report={{"task","G5-DPR4-postup-navigation-r1"},{"rows",json::array()}};bool pass=false;
 try{
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {auto c=std::make_shared<NativeMetalCompletion>();auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);require(bool(c->context),"navigation Metal");for(bool tail:{false,true})report["rows"].push_back(committedNavigation(c,tail));report["rows"].push_back(committedNavigation(c,true,true));report["rows"].push_back(committedNavigation(c,true,false,true));report["rows"].push_back(committedNavigation(c,true,false,false,true));report["rows"].push_back(negativeAndQuota(c));report["rows"].push_back(exposedNavigation(c));if(argc<3||std::string_view(argv[2])!="mechanism")report["rows"].push_back(retainedGpuLease(c));}
 message(pool,"drain");
#endif
 pass=!report["rows"].empty();for(const auto& r:report["rows"])pass=pass&&r["pass"].get<bool>();
 }catch(const std::exception&e){report["error"]=e.what();}report["pass"]=pass;if(argc>1)std::ofstream(argv[1])<<report.dump(2)<<'\n';std::cout<<report.dump()<<'\n';return pass?0:1;
}
