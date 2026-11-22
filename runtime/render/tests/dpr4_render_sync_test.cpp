#define main dpr4InheritedFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "canvas/ink/brush_package_catalog.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include "../src/skia_gpu_lifetime.hpp"
#include <filesystem>
using nlohmann::json;

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
json mechanism(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned algorithm){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"mechanism target");
    auto plan=pressurePlan(algorithm,7199,8,2,false,target.generation());
    SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());
    RenderPreparationBudget one;one.maxCoverageSteps=1;
    auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);target.release();
    const auto started=std::chrono::steady_clock::now();const auto result=renderer.renderFrame(target,plan,nullptr,&one);
    const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    const auto work=renderer.renderCacheWork();
    return {{"algorithm",algorithm},{"consumer","SkiaRenderer::renderFrame"},{"firstCallMs",milliseconds},
        {"result",int(result.code)},{"presentCount",target.presentCount()},
        {"supportKnots",work.preparedSupportKnots},{"coverageSteps",work.preparationCoverageSteps},
        {"directEntries",work.preparationDirectEntries},{"directGroups",work.directGroups},
        {"pass",result.code==BackendSubmissionCode::kRetry&&target.presentCount()==0&&
            work.preparedSupportKnots>0&&work.preparedSupportKnots<=32&&work.directGroups==0}};
}
json downMechanism(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned algorithm){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"Down target");
    auto base=pressurePlan(algorithm,7199,8,2,false,target.generation());auto draw=base.referenceDrawList;
    const auto masks=draw.entries[1].record.eraseMasks;
    for(auto& e:draw.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){
        e.presentationEraseActive=true;e.presentationEraseMasks=masks;e.record.eraseMasks.clear();}
    FramePlan plan{base.frame,std::move(draw),{},false,false};EditingOverlay bare{plan.frame};
    SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());
    const auto before=target.presentCount();const auto start=std::chrono::steady_clock::now();
    const auto r=renderer.renderTransientOverlay(target,plan,&bare);
    const auto w=renderer.renderCacheWork();
    return {{"consumer","SkiaRenderer::renderTransientOverlay"},{"algorithm",algorithm},
        {"firstCallMs",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()},
        {"supportKnots",w.preparedSupportKnots},{"coverageSteps",w.preparationCoverageSteps},
        {"directGroups",w.directGroups},{"presentCount",target.presentCount()-before},
        {"pass",r.code==BackendSubmissionCode::kRetry&&target.presentCount()==before&&w.directGroups==0}};
}
std::vector<std::uint8_t> pressurePixels(NativeMetalTarget& target){
    auto a=target.acquire();std::vector<std::uint8_t> pixels(a.frame.surface->width()*a.frame.surface->height()*4);target.release();
    require(target.readbackRgba(pixels).code==BackendSubmissionCode::kAccepted,"pressure readback");return pixels;
}
json pixels(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned algorithm,bool isolated,unsigned dpr,bool inkOnly=false){
    NativeMetalTarget target(completion),reference(completion);
    require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted&&reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"pixel targets");
    const auto base=pressurePlan(algorithm,257,3,dpr,isolated,target.generation());auto draw=base.referenceDrawList;
    if(inkOnly){draw.entries.erase(draw.entries.begin());draw.entries.front().record.eraseMasks.clear();}
    FramePlan plan{base.frame,std::move(draw),{},false,isolated};
    SkiaRenderer renderer,independent;renderer.setRenderCacheOptions(pressureOptions());
    auto disabled=pressureOptions();disabled.enabled=false;independent.setRenderCacheOptions(disabled);
    unsigned attempts=0;
    for(;attempts<4096;++attempts){auto r=renderer.renderFrame(target,plan);if(r.code==BackendSubmissionCode::kAccepted)break;
        require(r.code==BackendSubmissionCode::kRetry&&target.presentCount()==0,"pressure pending partial/rejected");}
    require(attempts<4096,"pressure canonical stalled");
    require(independent.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"pressure independent Direct");
    auto a=pressurePixels(target),b=pressurePixels(reference);unsigned maximum=0;std::size_t changed=0;
    for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[i]){++changed;maximum=std::max(maximum,unsigned(std::abs(int(a[i])-int(b[i]))));}
    auto work=renderer.renderCacheWork();return {{"algorithm",algorithm},{"isolated",isolated},{"dpr",dpr},
        {"inkOnly",inkOnly},{"attempts",attempts+1},{"maximumPixelDifference",maximum},{"changedBytes",changed},
        {"preparedCanonical",work.preparedCanonicalDraws},{"preparedDirect",work.preparedDirectDraws},
        {"GPUtiles",work.gpuCoverageTiles},{"CPUtiles",work.cpuCoverageTiles},{"pass",maximum<=2}};
}
json boundedComplete(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned algorithm,bool preparation=false){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"bounded complete target");
    const auto base=pressurePlan(algorithm,7199,8,2,false,target.generation());auto draw=base.referenceDrawList;
    if(preparation){draw.entries.erase(draw.entries.begin());draw.entries.front().record.eraseMasks.clear();}
    FramePlan plan{base.frame,std::move(draw),{},false,false};
    SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());
    std::uint64_t maximum=0,total=0;unsigned calls=0;
    RenderPreparationBudget budget;
    for(;calls<8192;++calls){const auto before=pressureTestCommands;auto r=renderer.renderFrame(target,plan,nullptr,preparation?&budget:nullptr);
        const auto commands=pressureTestCommands-before;maximum=std::max(maximum,commands);total+=commands;
        if(commands>128)return {{"consumer","canonical-with-preparation"},{"algorithm",algorithm},{"calls",calls+1},{"maximumCommands",maximum},{"pass",false}};
        if(r.code==BackendSubmissionCode::kAccepted)break;
        require(r.code==BackendSubmissionCode::kRetry&&target.presentCount()==0,"pending changed provider");}
    require(calls<8192,"bounded producer did not converge");
    return {{"consumer",preparation?"canonical-with-preparation":"canonical-completion"},{"algorithm",algorithm},{"calls",calls+1},
        {"maximumCommands",maximum},{"totalCommands",total},{"complete",true},{"pass",true}};
}
json lifecycle(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned algorithm){
    NativeMetalTarget target(completion),reference(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted&&reference.resize(512,256).code==BackendSubmissionCode::kAccepted,"lifecycle targets");
    auto plan=pressurePlan(algorithm,257,3,2,false,target.generation());SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());
    auto disabled=pressureOptions();disabled.enabled=false;SkiaRenderer oracle;oracle.setRenderCacheOptions(disabled);
    const auto finish=[&](const FramePlan& p){unsigned calls=0;std::uint64_t maximum=0;
        for(;calls<4096;++calls){const auto before=pressureTestCommands;auto r=renderer.renderFrame(target,p);maximum=std::max(maximum,pressureTestCommands-before);
            require(pressureTestCommands-before<=128,"lifecycle aggregate command allowance");
            if(r.code==BackendSubmissionCode::kAccepted)break;require(r.code==BackendSubmissionCode::kRetry,"lifecycle retry rejected");}
        require(calls<4096,"lifecycle producer stalled");require(oracle.renderFrame(reference,p).code==BackendSubmissionCode::kAccepted,"lifecycle oracle");
        auto a=pressurePixels(target),b=pressurePixels(reference);unsigned difference=0;for(std::size_t i=0;i<a.size();++i)difference=std::max(difference,unsigned(std::abs(int(a[i])-int(b[i]))));
        require(difference<=2,"lifecycle pixels changed");return std::pair{calls+1,maximum};};
    finish(plan);const auto before=pressureTestCommands;
    auto draw=plan.referenceDrawList;for(auto& e:draw.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){
        auto mask=e.record.eraseMasks.front();mask.id=ObjectId::fromUint64(900);auto& sweep=std::get<semantic::SweptCircleMask>(mask.geometry);
        sweep.segments.front().p0.position.x+=25;e.record.eraseMasks.push_back(mask);}
    FramePlan second{plan.frame,std::move(draw),{},false,false};
    // Count only the candidate, not the independent oracle's work.
    std::uint64_t candidateCommands=0;unsigned calls=0;
    for(;calls<4096;++calls){const auto n=pressureTestCommands;const auto r=renderer.renderFrame(target,second);candidateCommands+=pressureTestCommands-n;
        if(r.code==BackendSubmissionCode::kAccepted)break;require(r.code==BackendSubmissionCode::kRetry,"mask append retry");}
    require(calls<4096&&candidateCommands==0,"history-only change rebuilt pressure body");
    require(oracle.renderFrame(reference,second).code==BackendSubmissionCode::kAccepted&&pressurePixels(target)==pressurePixels(reference),"mask append exact pixels");
    auto transformed=second.referenceDrawList;transformed.entries.back().record.transform.tx+=.25;
    FramePlan moved{second.frame,std::move(transformed),{},false,false};finish(moved);
    auto camera=moved.referenceDrawList;camera.worldToView.tx+=1;FramePlan panned{moved.frame,std::move(camera),{},false,false};finish(panned);
    renderer.setImageResolver(nullptr);finish(panned);
    require(target.advanceGeneration().code==BackendSubmissionCode::kAccepted&&reference.advanceGeneration().code==BackendSubmissionCode::kAccepted,"generation change");
    auto rebound=pressurePlan(algorithm,257,3,2,false,target.generation(),2);finish(rebound);
    RenderPreparationBudget one;one.maxCoverageSteps=1;
    auto pending=pressurePlan(algorithm,7199,8,2,false,target.generation());const auto presents=target.presentCount();
    require(renderer.renderFrame(target,pending,nullptr,&one).code==BackendSubmissionCode::kRetry&&target.presentCount()==presents,"cancel seed published");
    renderer.clearRenderCache();finish(rebound);
    return {{"algorithm",algorithm},{"consumer","mask-transform-camera-resource-target-cancel"},
        {"maskOnlyCommands",candidateCommands},{"historyReused",true},{"pixelParity",true},{"pass",pressureTestCommands>=before}};
}
json quotaAndLease(const std::shared_ptr<NativeMetalCompletion>& completion){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"quota target");
    auto plan=pressurePlan(5,7199,2,2,false,target.generation());auto acquired=target.acquire();auto* canvas=acquired.frame.surface->getCanvas();
    const auto limits=pressureOptions();auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{limits.maxBytes,limits.sharedMaxBytes});
    auto sources=std::make_shared<SourceRetentionPool>();auto geometry=std::make_shared<PreparedGeometryCache>();geometry->setRetentionPool(sources);
    SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=sources;cache.preparedGeometry=geometry;cache.options=limits;
    auto held=budget->reserve(resources::ResourceClass::kScratch,limits.sharedMaxBytes-4096,true);require(bool(held),"quota hold");
    cache.begin(plan,&target,*acquired.frame.surface,0);PreparationAllowance denied;
    const auto r=cache.prepareBodies(*canvas,plan,denied);
    require(r==SkiaRenderCache::Preparation::kResourcePending||cache.stagedBodies(),"quota denial escaped bounded preparation");
    require(target.presentCount()==0,"quota denial presented");budget->release(*held);
    const auto epoch=budget->beginGpuEpoch();require(bool(epoch),"lease epoch");
    RenderPreparationBudget one;one.maxCoverageSteps=1;bool ready=false;std::uint64_t maximum=0;
    for(unsigned calls=0;calls<8192;++calls){cache.begin(plan,&target,*acquired.frame.surface,0);PreparationAllowance allowance(one);
        auto status=cache.prepareBodies(*canvas,plan,allowance);
        if(status==SkiaRenderCache::Preparation::kReady)status=cache.preparePressureBodies(*canvas,plan,allowance);
        maximum=std::max(maximum,std::uint64_t(allowance.steps));require(allowance.steps<=1,"fresh per-object allowance");
        if(status==SkiaRenderCache::Preparation::kReady){ready=true;break;}
        require(status==SkiaRenderCache::Preparation::kPending||status==SkiaRenderCache::Preparation::kResourcePending,"quota retry rejected");}
    require(ready,"released quota did not resume");
    cache.clear();geometry->clear();require(budget->used()>0&&budget->snapshot(resources::ResourceClass::kScratch).pendingRelease>0,"pressure coverage retired before GPU completion");
    require(budget->endGpuEpoch(*epoch)&&budget->completeGpuEpoch(*epoch),"lease completion");
    require(budget->used()==0,"pressure retained state leaked after completion");target.release();
    return {{"consumer","actual-cache-pressure-producer"},{"quotaRetry",true},{"maximumSteps",maximum},
        {"pendingNoPresent",true},{"lastReleaseBytes",budget->used()},{"pass",true}};
}
json oversize(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned variant=0){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"oversize target");
    auto plan=pressurePlan(5,129,3,2,false,target.generation());SkiaRenderer renderer;
    auto options=pressureOptions();
    if(variant==0)options.maxEntries=1;
    if(variant==1)options.maxBytes=1024*1024;
    if(variant==2)options.maxEntryBytes=256*1024;
    renderer.setRenderCacheOptions(options);RenderPreparationBudget one;one.maxCoverageSteps=1;
    for(unsigned call=0;call<8192;++call){const auto before=pressureTestCommands;auto result=renderer.renderFrame(target,plan,nullptr,&one);
        if(result.code==BackendSubmissionCode::kRejected)return {{"pass",false},{"consumer","oversize-staged-canonical"},{"variant",variant},{"calls",call+1},{"reason",result.message}};
        require(pressureTestCommands-before<=128,"oversize synchronous pressure replay");
        if(result.code==BackendSubmissionCode::kAccepted)return {{"pass",true},{"consumer","oversize-staged-canonical"},{"variant",variant},{"calls",call+1}};
        require(target.presentCount()==0,"oversize partial present");
    }
    return {{"pass",false},{"consumer","oversize-staged-canonical"},{"reason","no progress"}};
}
json suspendedTile(const std::shared_ptr<NativeMetalCompletion>& completion){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"suspended tile target");
    const auto plan=pressurePlan(5,257,1,2,false,target.generation());auto acquired=target.acquire();auto* canvas=acquired.frame.surface->getCanvas();
    auto limits=pressureOptions();auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{limits.maxBytes,limits.sharedMaxBytes});
    auto pool=std::make_shared<SourceRetentionPool>();auto geometry=std::make_shared<PreparedGeometryCache>();geometry->setRetentionPool(pool);
    SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=pool;cache.preparedGeometry=geometry;cache.options=limits;
    RenderPreparationBudget one;one.maxCoverageSteps=1;bool interrupted=false,ready=false;
    for(unsigned n=0;n<4096;++n){cache.begin(plan,&target,*acquired.frame.surface,0);PreparationAllowance a(one);
        auto r=cache.prepareBodies(*canvas,plan,a);if(r==SkiaRenderCache::Preparation::kReady)r=cache.preparePressureBodies(*canvas,plan,a);
        if(!interrupted&&cache.work.preparationCoverageSteps){
            // This is the production navigation release entry, not a forged slot.
            cache.releaseIdleWorkBuffers(plan.frame,plan.referenceDrawList.worldToView,&target,0);interrupted=true;
        }
        if(r==SkiaRenderCache::Preparation::kReady){ready=true;break;}
        require(r==SkiaRenderCache::Preparation::kPending||r==SkiaRenderCache::Preparation::kResourcePending,"suspended tile rejected");
    }
    cache.clear();geometry->clear();target.release();return {{"pass",interrupted&&ready},{"consumer","suspended-pressure-navigation-release"},{"completed",ready}};
}
json interleavedSource(const std::shared_ptr<NativeMetalCompletion>& completion){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"interleaved target");
    auto base=pressurePlan(5,257,2,2,false,target.generation());auto draw=base.referenceDrawList;
    auto added=draw.entries.back();added.record.objectId=ObjectId::fromUint64(404);
    added.record.content.update([](auto& value){++std::get<semantic::CompactStrokeContent>(value).stroke.seed;});
    added.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(added.record.content)};
    draw.entries.insert(draw.entries.begin()+1,std::move(added));FramePlan changed{base.frame,std::move(draw),{},false,false};
    auto acquired=target.acquire();auto* canvas=acquired.frame.surface->getCanvas();auto limits=pressureOptions();
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{limits.maxBytes,limits.sharedMaxBytes});
    auto pool=std::make_shared<SourceRetentionPool>();auto geometry=std::make_shared<PreparedGeometryCache>();geometry->setRetentionPool(pool);
    SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=pool;cache.preparedGeometry=geometry;cache.options=limits;
    RenderPreparationBudget one;one.maxCoverageSteps=1;unsigned calls=0;
    const auto advance=[&](const FramePlan& plan){cache.begin(plan,&target,*acquired.frame.surface,0);PreparationAllowance a(one);
        auto r=cache.prepareBodies(*canvas,plan,a);if(r==SkiaRenderCache::Preparation::kReady)r=cache.preparePressureBodies(*canvas,plan,a);return r;};
    while(!cache.work.preparationCoverageSteps){advance(base);require(++calls<256,"interleave seed");}
    bool complete=false;
    for(unsigned n=0;n<4096;++n){auto r=advance(changed);if(r==SkiaRenderCache::Preparation::kReady){complete=true;break;}
        require(r==SkiaRenderCache::Preparation::kPending||r==SkiaRenderCache::Preparation::kResourcePending,"interleaved rejected");}
    require(complete,"new prefix source lost suspended tile");
    canvas->clear(SK_ColorWHITE);drawValidatedReferenceRangeToSkCanvas(*canvas,changed,0,changed.referenceDrawList.entries.size(),nullptr,nullptr,true,&cache);
    auto actual=pressurePixels(target);cache.clear();geometry->clear();target.release();
    SkiaRenderer direct;auto disabled=limits;disabled.enabled=false;direct.setRenderCacheOptions(disabled);
    require(direct.renderFrame(target,changed).code==BackendSubmissionCode::kAccepted&&actual==pressurePixels(target),"interleaved coverage corrupted pixels");
    return {{"pass",true},{"consumer","prefix-insert-before-suspended-pressure"},{"completed",true},{"exactPixels",true}};
}
// Regression: optional entry size must not reject a legal mandatory pressure body.
json geometryEntryLimit(const std::shared_ptr<NativeMetalCompletion>& completion,unsigned algorithm){
    constexpr std::size_t mib=1048576;
    NativeMetalTarget target(completion),reference(completion);
    require(target.resize(256,128).code==BackendSubmissionCode::kAccepted&&
        reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"entry limit targets");
    auto plan=pressurePlan(algorithm,40000,1,1,false,target.generation(),0);
    auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);target.release();
    const auto prior=pressurePixels(target);
    SkiaRenderer renderer;auto options=pressureOptions();options.maxBytes=32*mib;
    options.sharedMaxBytes=96*mib;options.maxEntryBytes=mib;renderer.setRenderCacheOptions(options);
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    temporaryRetirementFault=[&](auto&,const auto& actual){budget=actual;};
    RenderPreparationBudget slice;std::uint64_t maximum=0;unsigned calls=0;
    BackendSubmissionResult result=BackendSubmissionResult::retry("not started");
    for(;calls<8192;++calls){
        const auto before=pressureTestCommands;result=renderer.renderFrame(target,plan,nullptr,&slice);
        temporaryRetirementFault={};maximum=std::max(maximum,pressureTestCommands-before);
        require(maximum<=128,"oversized geometry escaped pressure command allowance");
        require(budget&&budget->used()<=96*mib,"oversized geometry uncharged/hard quota exceeded");
        if(result.code==BackendSubmissionCode::kRejected)return {{"algorithm",algorithm},{"calls",calls+1},
            {"reason",result.message},{"pass",false}};
        if(result.code==BackendSubmissionCode::kAccepted)break;
        require(target.presentCount()==0,"oversized geometry pending presented");
        if(calls==0)require(pressurePixels(target)==prior,"entry denial changed qualified provider pixels");
    }
    require(calls<8192,"oversized geometry permanent retry");
    SkiaRenderer independent;auto disabled=options;disabled.enabled=false;independent.setRenderCacheOptions(disabled);
    require(independent.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"entry limit independent null-cache");
    const auto actual=pressurePixels(target),expected=pressurePixels(reference);unsigned difference=0;
    for(std::size_t i=0;i<actual.size();++i)difference=std::max(difference,unsigned(std::abs(int(actual[i])-int(expected[i]))));
    require(difference<=2,"oversized geometry pixels changed");
    // Check real shared admission with optional entry limits unchanged.
    PreparedGeometryCache geometry;const auto& entry=plan.referenceDrawList.entries[1];
    auto quota=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{32*mib,96*mib});
    const auto hold=quota->reserve(resources::ResourceClass::kRecorded,96*mib-4096,true);require(bool(hold),"entry quota hold");
    require(!geometry.request(entry,options,quota)&&!geometry.requestRejected(),"shared pressure not retryable");
    quota->release(*hold);auto body=geometry.request(entry,options,quota);require(bool(body),"released shared quota did not readmit");
    RenderPreparationBudget one;one.maxCoverageSteps=1;
    while(!body->ready()){PreparationAllowance a(one);const auto before=geometry.statistics();geometry.advance(body,a);
        const auto after=geometry.statistics();require(after.supportKnots-before.supportKnots<=32&&
            after.distanceInputs-before.distanceInputs<=32,"oversized body hidden full-history metrics");}
    require(quota->used()>mib&&quota->used()<=96*mib,"mandatory source/body not charged beyond optional entry");
    geometry.clear();body.reset();require(quota->used()==0,"mandatory geometry lease leaked");
    renderer.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
    return {{"algorithm",algorithm},{"consumer","canonical-40000-knot-entry-limit"},{"calls",calls+1},
        {"maximumCommands",maximum},{"maximumPixelDifference",difference},{"presentCount",target.presentCount()},
        {"sharedQuotaRetry",true},{"pass",target.presentCount()==1}};
}
// Actual owner resumes work inside the same execution/lifetime scope used by renderer preflight.
// Missing work marking would resolve the ticket as unused and immediately release its pixels.
enum class ContinuationCase { kPressure, kMask, kCachedMaterial, kStagedBackground };
json continuationLifetime(const std::shared_ptr<NativeMetalCompletion>& completion,ContinuationCase test){
    const bool maskOnly=test==ContinuationCase::kMask,cached=test==ContinuationCase::kCachedMaterial,staged=test==ContinuationCase::kStagedBackground;
    NativeMetalTarget target(completion);require(target.resize(cached?512:256,cached?256:128).code==BackendSubmissionCode::kAccepted,"continuation target");
    auto base=pressurePlan(5,maskOnly?65:257,(cached||staged)?3:1,cached?2:1,true,target.generation(),0);auto draw=base.referenceDrawList;
    if(maskOnly){auto& entry=draw.entries.back();semantic::SweptCircleMask sweep;
        for(unsigned n=0;n<512;++n){const double x=95+double(n%20),y=20+double(n%90);
            sweep.segments.push_back({{{x,y},2},{{x+3,y+2},2},{x+1,y},{x+2,y+1}});}
        entry.record.eraseMasks={{ObjectId::fromUint64(700),std::move(sweep)}};
    }
    FramePlan plan{base.frame,std::move(draw),{},false,true};auto acquired=target.acquire();
    auto* canvas=acquired.frame.surface->getCanvas();auto limits=pressureOptions();if(staged)limits.maxEntries=1;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{limits.maxBytes,limits.sharedMaxBytes});
    auto geometry=std::make_shared<PreparedGeometryCache>();auto sources=std::make_shared<SourceRetentionPool>();geometry->setRetentionPool(sources);
    SkiaRenderCache cache;cache.options=limits;cache.budget=budget;cache.sourcePool=sources;cache.preparedGeometry=geometry;
    cache.maskHistory=std::make_shared<EraseMaskPathCache>();cache.maskHistory->setRetentionPool(sources);
    cache.begin(plan,&target,*acquired.frame.surface,0);
    while(true){PreparationAllowance a;const auto r=cache.prepareBodies(*canvas,plan,a);
        if(r==SkiaRenderCache::Preparation::kReady)break;require(r==SkiaRenderCache::Preparation::kPending,"continuation metrics");}
    GpuResourceRetirement retirement;
    RenderPreparationBudget one;one.maxCoverageSteps=1;unsigned calls=0;bool tested=false,marked=false,unretired=false;std::size_t pending=0;bool pressureReady=false,lookupChecked=false;unsigned cachedReuses=0;
    for(;calls<1024&&!tested;++calls){
        const auto pressureBefore=pressureTestCommands,maskBefore=materialMaskTestDraws;
        const auto reusesBefore=cache.work.executionPixelReuses;
        const auto coverageBefore=cache.work.executionGpuTiles,allocationsBefore=cache.work.executionGpuAllocations;
        {
            RenderExecutionScope execution(ExecutionPurpose::kCanonical,plan,*acquired.frame.surface,&target,0,true,limits.sharedMaxBytes,cache.work,target.describe().kind);
            execution.bindTargets(cache.executionTargets());
            SkiaGpuLifetime gpu(retirement,budget,target,completion->context.get(),
                GpuResourceRetirement::canonicalAllocations,GpuResourceRetirement::canonicalReads);
            require(gpu.ready(),"continuation real lifetime admission");
            const auto epoch=gpu.epoch();cache.begin(plan,&target,*acquired.frame.surface,0);PreparationAllowance a(one);
            const bool materialStep=cached&&pressureReady;
            if(staged)a.steps=one.maxCoverageSteps; // exhausted by earlier CPU preparation; background still writes first.
            canvas->save();if(materialStep)canvas->scale(2,2);
            const auto status=staged?cache.prepareWorkingSet(*canvas,plan,a,nullptr,nullptr,false):
                (maskOnly||materialStep)?cache.prepare(*canvas,plan,one,nullptr,nullptr,&a):cache.preparePressureBodies(*canvas,plan,a);
            canvas->restore();
            if(status!=SkiaRenderCache::Preparation::kPending&&status!=SkiaRenderCache::Preparation::kReady)
                std::cerr<<"continuation test="<<int(test)<<" call="<<calls<<" status="<<int(status)<<'\n';
            if(cached&&!materialStep&&status==SkiaRenderCache::Preparation::kReady)pressureReady=true;
            require(status==SkiaRenderCache::Preparation::kPending||status==SkiaRenderCache::Preparation::kReady,"continuation preparation failed");
            const auto pressure=pressureTestCommands-pressureBefore,masks=materialMaskTestDraws-maskBefore;
            const bool noNewCoverage=cache.work.executionGpuTiles==coverageBefore&&cache.work.executionGpuAllocations==allocationsBefore;
            if(cached&&materialStep&&cache.work.executionPixelReuses>reusesBefore)++cachedReuses;
            const bool resumed=staged?(status==SkiaRenderCache::Preparation::kPending&&pressure==0&&masks==0&&noNewCoverage):cached?(materialStep&&status==SkiaRenderCache::Preparation::kPending&&
                cachedReuses>1&&cache.work.executionPixelReuses>reusesBefore&&pressure==0&&masks==0&&noNewCoverage):
                maskOnly?(masks>0&&pressure==0):(pressure>0&&noNewCoverage);
            if(resumed){
                require(status==SkiaRenderCache::Preparation::kPending,"continuation must exercise early return");
                marked=execution.gpuWorkRecorded();
                // No service/readback/CPU fence before observation: finish must leave a
                // real submitted epoch pending, rather than synchronous unused retirement.
                require(gpu.finish(),"continuation actual GPU submit failed");
                unretired=!budget->gpuEpochRetired(epoch);cache.clear();geometry->clear();cache.maskHistory.reset();
                for(auto kind:{resources::ResourceClass::kScratch,resources::ResourceClass::kRaster,resources::ResourceClass::kCanonicalFrame})pending+=budget->snapshot(kind).pendingRelease;
                tested=true;
            }
        }
        if(!tested){completion->context->flushAndSubmit(GrSyncCpu::kYes);retirement.service();}
        if(cached&&pressureReady&&!lookupChecked){
            // A ready tile lookup in capture-only mode issues no material draw.
            RenderExecutionScope execution(ExecutionPurpose::kCanonical,plan,*acquired.frame.surface,&target,0,true,limits.sharedMaxBytes,cache.work,target.describe().kind);
            execution.bindTargets(cache.executionTargets());
            SkiaGpuLifetime gpu(retirement,budget,target,completion->context.get(),
                GpuResourceRetirement::canonicalAllocations,GpuResourceRetirement::canonicalReads);
            require(gpu.ready(),"lookup-only GPU scope");const auto epoch=gpu.epoch();
            const auto& entry=plan.referenceDrawList.entries[1];const auto body=cache.readyBody(entry,PreparedConsumer::kCanonical);
            const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
            const auto& t=entry.record.transform;const auto& v=plan.referenceDrawList.worldToView;
            SkMatrix object,view;object.setAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1);
            view.setAll(float(v.a),float(v.c),float(v.tx),float(v.b),float(v.d),float(v.ty),0,0,1);
            canvas->save();canvas->setMatrix(SkMatrix::Concat(SkMatrix::Scale(cached?2:1,cached?2:1),SkMatrix::Concat(view,object)));
            CompactDeviceGrid grid{0,0,cached?512:256,cached?256:128};
            CompactStrokeRasterJob lookup(*canvas,stroke.config,stroke.knots,&grid,true,budget,limits.maxEntryBytes,{},true,body);
            lookup.setCaptureOnly();CompactRasterWork work;const auto reuses=cache.work.executionPixelReuses;
            lookup.step(*canvas,stroke.config,stroke.knots,stroke.seed,cache.executionTargets(),true,&work);canvas->restore();
            require(cache.work.executionPixelReuses>reuses&&!execution.gpuWorkRecorded(),"capture-only pixel lookup pretended to draw GPU work");
            require(gpu.finish()&&budget->gpuEpochRetired(epoch),"lookup-only scope should remain unused");
            retirement.service();lookupChecked=true;
        }
    }
    // Poll the actual Metal completion; only it may drain the released pixels.
    completion->context->flushAndSubmit(GrSyncCpu::kYes);completion->service();retirement.service();
    cache.clear();geometry->clear();cache.maskHistory.reset();retirement.service();target.release();
    require(budget->pendingGpuEpochCount()==0&&budget->used()==0,"continuation lease final drain");
    return {{"consumer",staged?"staged-pressure-background-yield":cached?"cached-pressure-composite-yield":maskOnly?"body-complete-mask-only-early-return":"capture-pressure-continuation"},
        {"calls",calls},{"tested",tested},{"cachedReusesBeforeProbe",cachedReuses},{"lookupOnlyChecked",lookupChecked},{"workMarked",marked},{"epochUnretiredBeforeService",unretired},
        {"pendingPixelBytes",pending},{"lastReleaseBytes",budget->used()},{"pass",tested&&marked&&unretired&&pending>0}};
}
json maskAllowance(const std::shared_ptr<NativeMetalCompletion>& completion){
    NativeMetalTarget target(completion);require(target.resize(512,256).code==BackendSubmissionCode::kAccepted,"mask allowance target");
    auto base=pressurePlan(5,65,2,2,true,target.generation());auto draw=base.referenceDrawList;
    for(auto& entry:draw.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(entry.command)){
        semantic::SweptCircleMask sweep;
        for(unsigned n=0;n<1024;++n){const double x=95+double(n%20),y=20+double(n%90);
            sweep.segments.push_back({{{x,y},2},{{x+3,y+2},2},{x+1,y},{x+2,y+1}});}
        entry.record.eraseMasks={{ObjectId::fromUint64(700),std::move(sweep)}};
    }
    FramePlan plan{base.frame,std::move(draw),{},false,true};SkiaRenderer renderer;renderer.setRenderCacheOptions(pressureOptions());
    RenderPreparationBudget one;one.maxCoverageSteps=1;std::uint64_t maximum=0;unsigned calls=0;
    for(;calls<8192;++calls){const auto before=materialMaskTestDraws;auto result=renderer.renderFrame(target,plan,nullptr,&one);
        maximum=std::max(maximum,materialMaskTestDraws-before);
        if(maximum>32)return {{"pass",false},{"maximumMaskDraws",maximum},{"consumer","canonical-history-mask-allowance"}};
        if(result.code==BackendSubmissionCode::kAccepted)break;
        require(result.code==BackendSubmissionCode::kRetry&&target.presentCount()==0,"mask allowance partial frame");}
    NativeMetalTarget reference(completion);require(reference.resize(512,256).code==BackendSubmissionCode::kAccepted,"mask pixel reference target");
    SkiaRenderer independent;auto disabled=pressureOptions();disabled.enabled=false;independent.setRenderCacheOptions(disabled);
    require(independent.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted&&pressurePixels(target)==pressurePixels(reference),"segmented history pixels changed");
    return {{"pass",calls<8192},{"maximumMaskDraws",maximum},{"calls",calls+1},{"consumer","canonical-history-mask-allowance"}};
}
#endif
}
int main(int argc,char** argv){
    json report={{"task","G5-DPR4-render-sync-r1"},{"rows",json::array()}};bool pass=false;
    try{
#if defined(__APPLE__)
        auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
        {auto completion=std::make_shared<NativeMetalCompletion>();
            const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
            GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
            completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"Metal context");
            if(argc>2&&std::string_view(argv[2])=="entry-limit")for(unsigned algorithm:{5U,6U})report["rows"].push_back(geometryEntryLimit(completion,algorithm));
            else if(argc>2&&std::string_view(argv[2])=="continuation")for(auto test:{ContinuationCase::kPressure,ContinuationCase::kMask,ContinuationCase::kCachedMaterial,ContinuationCase::kStagedBackground})report["rows"].push_back(continuationLifetime(completion,test));
            else if(argc>2&&std::string_view(argv[2])=="mask-budget")report["rows"].push_back(maskAllowance(completion));
            else if(argc>2&&std::string_view(argv[2])=="interleaved")report["rows"].push_back(interleavedSource(completion));
            else if(argc>2&&std::string_view(argv[2])=="oversize")for(unsigned variant:{0U,1U,2U})report["rows"].push_back(oversize(completion,variant));
            else if(argc>2&&std::string_view(argv[2])=="suspended")report["rows"].push_back(suspendedTile(completion));
            else if(argc>2&&std::string_view(argv[2])=="quota")report["rows"].push_back(quotaAndLease(completion));
            else for(unsigned algorithm:{5U,6U}){
                if(argc>2&&std::string_view(argv[2])=="prepared")report["rows"].push_back(boundedComplete(completion,algorithm,true));
                else if(argc>2&&std::string_view(argv[2])=="bounded")report["rows"].push_back(boundedComplete(completion,algorithm));
                else if(argc>2&&std::string_view(argv[2])=="lifecycle")report["rows"].push_back(lifecycle(completion,algorithm));
                else if(argc>2&&std::string_view(argv[2])=="pixels"){
                    for(bool isolate:{false,true})for(unsigned dpr:{1U,2U,4U})report["rows"].push_back(pixels(completion,algorithm,isolate,dpr));
                    for(unsigned dpr:{1U,4U})report["rows"].push_back(pixels(completion,algorithm,false,dpr,true));
                }
                else if(argc>2&&std::string_view(argv[2])=="down")report["rows"].push_back(downMechanism(completion,algorithm));
                else report["rows"].push_back(mechanism(completion,algorithm));
            }
        }message(pool,"drain");
#endif
        pass=!report["rows"].empty();for(const auto& row:report["rows"])pass=pass&&row["pass"].get<bool>();
    }catch(const std::exception& e){report["error"]=e.what();}
    report["pass"]=pass;if(argc>1)std::ofstream(argv[1])<<report.dump(2)<<'\n';
    std::cout<<report.dump()<<'\n';return pass?0:1;
}
