// Real provider pixels, not present counters, are the failure oracle.
#define main g5OriginalCanonicalTestMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include <filesystem>
#include "../src/transient_display_state.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include "canvas/text/text_layout.hpp"
#include "canvas/render/image_resource.hpp"
#include <chrono>
namespace {
ReferenceTraversalEntry displaySource(){
 auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;
 e.record.content.update([](auto& v){auto& c=std::get<semantic::CompactStrokeContent>(v);c.stroke.knots.clear();for(unsigned i=0;i<32;++i)c.stroke.knots.push_back({20.+i*5,64,32768});});
 e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 semantic::SweptCircleMask mask;mask.segments.push_back({{{80,40},2},{{80,80},2},{80,50},{80,70}});e.record.eraseMasks.push_back({ObjectId::fromUint64(500),mask});e.presentationEraseActive=true;
 return e;
}
void saveBuffer(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"persist actual target buffer");}
#if defined(__APPLE__)
void failedPixels(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"real Metal context");
 for(unsigned kib:{320U,352U,384U,416U,448U,480U,512U,576U,640U,1024U,2048U}){
  auto e=displaySource();NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"target resize");auto acquired=target.acquire();
  auto base=renderPlan(e,1,1);auto draw=base.referenceDrawList;draw.entries={e};FramePlan plan{base.frame,std::move(draw),{},false,true};
  require(drawReferencePlanToSkCanvas(*acquired.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"initial complete frame");target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"initial accepted present");
  std::vector<std::uint8_t> before(256*128*4),after(before.size());require(target.readbackRgba(before).code==BackendSubmissionCode::kAccepted,"read initial actual target");const auto count=target.presentCount();
  SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=kib*1024;options.sharedMaxBytes=kib*1024;renderer.setRenderCacheOptions(options);EditingOverlay overlay{plan.frame};
  for(unsigned attempt=0;attempt<8;++attempt){
   const auto status=renderer.renderTransientOverlay(target,plan,&overlay);completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
   require(target.readbackRgba(after).code==BackendSubmissionCode::kAccepted,"read failed actual target after GPU completion");
   std::size_t changed=0;for(std::size_t p=0;p<before.size();p+=4)if(!std::equal(before.begin()+p,before.begin()+p+4,after.begin()+p))++changed;
   const auto w=renderer.renderCacheWork();const auto code=static_cast<unsigned>(status.code);
   nlohmann::json row{{"budget_kib",kib},{"attempt",attempt},{"status",code},{"message",status.message},{"changed_pixels",changed},{"before_present",count},{"after_present",target.presentCount()},{"generation",target.generation()},{"CPU_tiles",w.cpuCoverageTiles},{"GPU_tiles",w.gpuCoverageTiles},{"body_builds",w.preparedBodyBuilds}};
   std::cout<<row.dump()<<std::endl;
   if(status.code==BackendSubmissionCode::kRetry){
    const auto prefix=out/(std::to_string(kib)+"-"+std::to_string(attempt));saveBuffer(prefix.string()+"-before.rgba",before);saveBuffer(prefix.string()+"-after.rgba",after);std::ofstream(prefix.string()+".json")<<row.dump(2)<<'\n';
    require(count==target.presentCount(),"Retry fabricated present");require(changed==0,"ACTUAL_RENDERER_RETRY_DESTROYED_LAST_ACCEPTED_PIXELS");
   }
   if(status.code!=BackendSubmissionCode::kRetry)break;
  }
 }
}

class DisplayTarget final:public SkiaSurfaceProvider {
 public:
 NativeMetalTarget target;bool unavailable=false,refusePresent=false;
 explicit DisplayTarget(std::shared_ptr<NativeMetalCompletion> c):target(std::move(c)){}
 std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return target.gpuCompletionService();}
 RenderTargetInfo describe()const noexcept override{return target.describe();}
 SkiaSurfaceAcquireResult acquire()noexcept override{return unavailable?SkiaSurfaceAcquireResult{}:target.acquire();}
 void release()noexcept override{target.release();}
 BackendSubmissionResult present()noexcept override{return refusePresent?BackendSubmissionResult::rejected("synthetic provider failure after complete publication"):target.present();}
 BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return target.resize(w,h);}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{return target.readbackRgba(out);}
 std::uint64_t generation()const noexcept override{return target.generation();}
 BackendSubmissionResult advanceGeneration()noexcept override{return target.advanceGeneration();}
 std::uint64_t readbackCount()const noexcept override{return target.readbackCount();}
 std::uint64_t cpuCopyCount()const noexcept override{return target.cpuCopyCount();}
 std::uint64_t presentCount()const noexcept override{return target.presentCount();}
};
semantic::EraseMaskRecord activeMask(unsigned tails){
 semantic::SweptCircleMask g;
 for(unsigned i=0;i<tails;++i){const double x=50+35*i;g.segments.push_back({{{x,35},5},{{x+4,90},5},{x,50},{x+4,75}});}
 return {ObjectId::fromUint64(800),std::move(g)};
}
FramePlan displayPlan(unsigned tails,std::uint64_t id,unsigned dpr,unsigned bg=0,unsigned copies=2,bool large=false){
 auto e=displaySource();e.record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=.55;});e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 e.presentationEraseMasks={activeMask(tails)};const auto base=renderPlan(e,id,dpr).frame;
 const float width=large?800:256,height=large?500:128;
 FrameState f{base.viewId,base.camera,{0,0,width,height},
  {width,height,std::uint32_t(width*dpr),std::uint32_t(height*dpr),float(dpr),1},base.sceneGeneration,base.sceneReadToken,base.surfaceGeneration,base.metricsGeneration,base.frameId,{0,0,width,height}};
 ReferenceDrawList draw{f};draw.viewportClip=f.viewportClip;
 for(unsigned i=0;i<copies;++i){auto next=e;next.record.objectId=ObjectId::fromUint64(i+1);next.record.transform.tx=i*3.;draw.entries.push_back(std::move(next));}
 foundation::DocumentBackground background;if(bg==2)background.color={.25F,.5F,.75F,1};
 return {f,std::move(draw),background,bg==1,true};
}

ReferenceTraversalEntry continuityShape(std::uint64_t id, double x, double y,
    float red, float green, float blue) {
 auto object=semantic::ObjectRecord{}; object.id=ObjectId::fromUint64(id);
 object.kind=semantic::ObjectKind::kShape; object.kind_version=1;
 object.content=semantic::ShapeContent{1,90,52}; object.transform.tx=x; object.transform.ty=y;
 object.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{red,green,blue,1.0F}}}}};
 const auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
 return {projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
}

FramePlan objectContinuityPlan(std::uint64_t id, const ReferenceTraversalEntry& before,
    const ReferenceTraversalEntry& active, const ReferenceTraversalEntry& after,
    std::uint64_t surfaceGeneration=1) {
 const auto base=displayPlan(1,id,1).frame;
 FrameState frame{base.viewId,base.camera,{0,0,256,128},
   {256,128,256,128,1,1},semantic::SemanticGeneration{1},SceneRevision{1},
   SurfaceGeneration{surfaceGeneration},base.metricsGeneration,FrameId{id},{0,0,256,128}};
 ReferenceDrawList draw{frame}; draw.viewportClip=frame.viewportClip;
 draw.entries={before,active,after};
 return {frame,std::move(draw),{},false,false};
}

std::vector<std::uint8_t> pixels(DisplayTarget&,const FramePlan&);
std::string pixelDigest(const std::vector<std::uint8_t>& bytes) {
 std::uint64_t hash=1469598103934665603ULL;
 for(const auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}
 std::ostringstream value;value<<std::hex<<hash;return value.str();
}

void objectContinuityStaticReuse(const std::filesystem::path& out) {
 std::filesystem::create_directories(out);
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
 auto completion=std::make_shared<NativeMetalCompletion>();
 GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 completion->context=GrDirectContexts::MakeMetal(backend);
 DisplayTarget target(completion),reference(completion);
 require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"object continuity target");
 require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"object continuity oracle");
 auto before=continuityShape(41001,8,24,.15F,.25F,.85F);
 auto active=continuityShape(41002,82,38,.85F,.20F,.15F);
 auto after=continuityShape(41003,166,60,.15F,.70F,.25F);
 SkiaRenderer cached,oracle; RenderCacheOptions disabled; disabled.enabled=false; oracle.setRenderCacheOptions(disabled);
 const auto initial=objectContinuityPlan(100, before, active, after);
 require(cached.renderFrame(target,initial).code==BackendSubmissionCode::kAccepted,"object continuity canonical warm");
 require(oracle.renderFrame(reference,initial).code==BackendSubmissionCode::kAccepted,"object continuity oracle warm");
 EditingOverlay overlay(initial.frame);
 require(overlay.update({.selectedObject=active.record.objectId,.worldBounds=active.record.worldBounds,
                         .objectTransform=active.record.transform,.selected=true,.active=true}),
         "object continuity selection chrome");
 const auto directBefore=cached.renderCacheWork().directGroups;
 for(unsigned i=0;i<3;++i){
   active.record.transform.tx+=7.0;
   const auto moved=objectContinuityPlan(101+i,before,active,after);
   require(cached.renderTransientOverlay(target,moved,&overlay).code==BackendSubmissionCode::kAccepted,
           "object continuity cached move");
   require(oracle.renderTransientOverlay(reference,moved,&overlay).code==BackendSubmissionCode::kAccepted,
           "object continuity direct move");
   const auto actual=pixels(target,moved),expected=pixels(reference,moved);
   saveBuffer(out/(std::to_string(i)+"-actual.rgba"),actual);
   saveBuffer(out/(std::to_string(i)+"-oracle.rgba"),expected);
   std::ofstream(out/(std::to_string(i)+"-pixels.json"))<<nlohmann::json{
     {"actual_digest",pixelDigest(actual)},{"oracle_digest",pixelDigest(expected)},
     {"oracle_static_builds",oracle.objectInteractionWork().builds},
     {"oracle_static_hits",oracle.objectInteractionWork().hits}}<<'\n';
   require(oracle.objectInteractionWork().builds==0&&oracle.objectInteractionWork().hits==0,
           "disabled independent oracle entered static optimization");
   require(actual==expected,"object continuity pixel/order mismatch");
 }
 const auto directDelta=cached.renderCacheWork().directGroups-directBefore;
 const auto work=cached.objectInteractionWork();
 const auto actual=pixels(target,objectContinuityPlan(104,before,active,after));
 const auto expected=pixels(reference,objectContinuityPlan(104,before,active,after));
 std::ofstream(out/"old-static-reuse-red.json")
     << nlohmann::json{{"direct_groups_delta",directDelta},{"moves",3},
       {"static_reuse_builds",work.builds},{"static_reuse_hits",work.hits},
       {"static_reuse_invalidations",work.invalidations},
       {"full_transient_scene_draws",work.fullSceneDraws},{"active_object_draws",work.activeDraws},
       {"static_reuse_lease_bytes",work.leaseBytes},
       {"actual_pixel_digest",pixelDigest(actual)},{"oracle_pixel_digest",pixelDigest(expected)}}
     << '\n';
 // The old renderer redraws all three non-ink entries on every move. The
 // production implementation must keep the unchanged prefix/suffix stable.
 require(directDelta<=5,"static scene redraw was not reduced to active object work");
}

semantic::RichTextContent denseLabel(std::uint64_t id,const char* value) {
 const semantic::TextStyle style{semantic::ResourceId{ObjectId::fromUint64(87000)},16,400,false,false,{.08F,.12F,.18F,1}};
 return {{{semantic::Paragraph{ObjectId::fromUint64(id),
     {semantic::ParagraphAlignment::kLeft,1,0,0},{{value,style}}}}}};
}
ReferenceTraversalEntry denseObject(unsigned kind,text::RichTextLayoutService& layout) {
 semantic::ObjectRecord object;object.id=ObjectId::fromUint64(88000+kind);object.kind_version=1;
 object.transform.tx=140+kind*105;object.transform.ty=100+kind*58;
 switch(kind){
 case 0:object.kind=semantic::ObjectKind::kShape;object.kind_version=3;
   object.content=semantic::ShapeContent{1,150,95,denseLabel(89000,"Shape label")};break;
 case 1:object.kind=semantic::ObjectKind::kSticky;object.content=semantic::StickyContent{170,110};break;
 case 2:{object.kind=semantic::ObjectKind::kTable;semantic::TableContent table;
   table.rows={{{ObjectId::fromUint64(89101)},40},{{ObjectId::fromUint64(89102)},40}};
   table.columns={{{ObjectId::fromUint64(89103)},85},{{ObjectId::fromUint64(89104)},85}};
   for(unsigned i=0;i<4;++i)table.cells.push_back({denseLabel(89110+i,"Cell"),{.92F,.96F,1,1}});
   object.content=std::move(table);break;}
 case 3:{object.kind=semantic::ObjectKind::kImage;semantic::ImageContent image;
   image.resource_id.value=ObjectId::fromUint64(87001);image.width=160;image.height=100;
   image.intrinsic_width=1;image.intrinsic_height=1;object.content=image;break;}
 default:throw std::runtime_error("dense object kind");
 }
 object.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.35F,.65F,.85F,1}}}},{0x3U,.72F}};
 const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1},&layout);
 ReferenceTraversalEntry entry;entry.record=projection.records.front();
 if(kind==0)entry.command=ShapeReferenceCommand{scene::ImmutableValue<semantic::ShapeContent>::alias(entry.record.content),entry.record.textLayout,true};
 if(kind==1){entry.record.textLayout=layout.resolveStickyLabel(object.id,denseLabel(89120,"Sticky label"),std::get<semantic::StickyContent>(object.content),true,object.transform);
   entry.command=StickyReferenceCommand{scene::ImmutableValue<semantic::StickyContent>::alias(entry.record.content),entry.record.textLayout,true};}
 if(kind==2)entry.command=TableReferenceCommand{scene::ImmutableValue<semantic::TableContent>::alias(entry.record.content),entry.record.textLayout,1};
 if(kind==3)entry.command=ImageReferenceCommand{scene::ImmutableValue<semantic::ImageContent>::alias(entry.record.content)};
 return entry;
}
FramePlan denseObjectPlan(const ReferenceTraversalEntry& line,std::vector<ReferenceTraversalEntry> objects,
    unsigned active,unsigned dpr,unsigned frame,double offset,std::uint64_t generation=1,bool reverse=false,
    unsigned logicalWidth=0,unsigned logicalHeight=0) {
 // The desktop reconstruction is 1431x1010. DPR4 uses an 800x500 logical
 // viewport so its mandatory complete display and isolation planes can fit
 // the unchanged 96MiB contract; report both dimensions in every observation.
 const unsigned width=logicalWidth?logicalWidth:(dpr==4?800:1431),height=logicalHeight?logicalHeight:(dpr==4?500:1010);
 FrameState state{ViewId{1},CameraState{{0,0},1,0,CameraGeneration{1}},{0,0,float(width),float(height)},
   {float(width),float(height),width*dpr,height*dpr,float(dpr),1},semantic::SemanticGeneration{1},SceneRevision{1},
   SurfaceGeneration{generation},MetricsGeneration{1},FrameId{frame},{0,0,float(width),float(height)}};
 ReferenceDrawList draw{state};draw.viewportClip=state.viewportClip;
 if(active<objects.size()){
  objects[active].record.transform.tx+=offset;
  const auto original=objects[active].record.worldBounds;
  objects[active].record.worldBounds={original.left+float(offset),original.top,original.right+float(offset),original.bottom};
  objects[active].record.visualBounds=objects[active].record.worldBounds;
 }
 for(auto& object:objects)if(object.record.textLayout){
   object.record.textLayout=std::make_shared<text::TextLayoutSnapshot>(*object.record.textLayout);
   std::visit([&](auto& command){
     if constexpr(requires{command.labelLayout;})command.labelLayout=object.record.textLayout;
     else if constexpr(requires{command.layout;})command.layout=object.record.textLayout;
   },object.command);
 }
 for(unsigned i=0;i<19;++i){
   auto copy=line;copy.record.objectId=ObjectId::fromUint64(90000+i);
   copy.record.transform.ty=45+i*13;copy.record.worldBounds={0,float(70+i*13),1440,float(150+i*13)};
   if(active==4&&i==0){copy.record.transform.tx+=offset;
     copy.record.worldBounds.left+=float(offset);copy.record.worldBounds.right+=float(offset);}
   copy.record.visualBounds=copy.record.worldBounds;
   draw.entries.push_back(std::move(copy));
   if(i>=3&&i<=15&&i%4==3)draw.entries.push_back(objects[(i-3)/4]);
 }
 if(reverse)std::reverse(draw.entries.begin(),draw.entries.end());
 return {state,std::move(draw),{},false,true};
}
std::vector<std::uint8_t> denseIndependentBoundary(const FramePlan& plan) {
 const auto width=plan.frame.metrics.physicalWidth,height=plan.frame.metrics.physicalHeight;
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeA8(width,height));
 auto* canvas=surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
 SkPaint boundary;boundary.setColor(SK_ColorWHITE);boundary.setStyle(SkPaint::kStroke_Style);
 boundary.setStrokeWidth(2);boundary.setAntiAlias(true);
 // Same independent per-primitive, two-device-pixel band as the existing
 // canonical working-set oracle. Never derive the band from candidate pixels.
 const auto first=std::find_if(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){return std::holds_alternative<CompactStrokeReferenceCommand>(e.command);});
 require(first!=plan.referenceDrawList.entries.end(),"dense independent ink source");
 const auto& stroke=std::get<CompactStrokeReferenceCommand>(first->command).content.get().stroke;
 const auto contour=ink::compileCompactContour(stroke.config,stroke.knots);
 for(const auto& entry:plan.referenceDrawList.entries){
   if(!std::holds_alternative<CompactStrokeReferenceCommand>(entry.command))continue;
   const auto& t=entry.record.transform;
   auto matrix=SkMatrix::Scale(plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth,
       plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight);
   const auto& v=plan.referenceDrawList.worldToView;
   matrix.preConcat(SkMatrix::MakeAll(v.a,v.c,v.tx,v.b,v.d,v.ty,0,0,1));
   matrix.preConcat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));
   for(std::size_t i=0;i+1<contour.patches.size();++i)
     canvas->drawPath(compactContourRibbon(std::span(contour.patches).subspan(i,2)).makeTransform(matrix),boundary);
   for(const auto& cap:contour.caps){SkPathBuilder path;path.addCircle(float(cap.x),float(cap.y),float(cap.radius));canvas->drawPath(path.detach().makeTransform(matrix),boundary);}
 }
 std::vector<std::uint8_t> mask(std::size_t(width)*height);
 require(surface->readPixels(SkImageInfo::MakeA8(width,height),mask.data(),width,0,0),"dense independent boundary readback");
 return mask;
}
void denseObjectContinuity(const std::filesystem::path& out,bool budgetComparison=false) {
 std::filesystem::create_directories(out);
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
 auto completion=std::make_shared<NativeMetalCompletion>();
 GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"dense Metal context");
 runtime::MemoryResourceProvider resources;
 std::ifstream font(G47_LATIN_FONT,std::ios::binary);std::vector<std::uint8_t> fontBytes{std::istreambuf_iterator<char>(font),{}};
 require(!fontBytes.empty(),"dense locked font");resources.publish(ObjectId::fromUint64(87000),fontBytes);
 resources.publish(ObjectId::fromUint64(87001),{0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
   0,0,0,0x0d,0x49,0x48,0x44,0x52,0,0,0,1,0,0,0,1,8,6,0,0,0,0x1f,0x15,0xc4,0x89,
   0,0,0,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0,5,0,1,0xff,0x89,0x99,
   0x3d,0x1d,0,0,0,0,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82});
 text::RichTextLayoutService layouts(resources,{170,1});layouts.registerFont({{ObjectId::fromUint64(87000)},std::string(text::kRobotoSha256),0});
 std::vector<ReferenceTraversalEntry> objects;for(unsigned kind=0;kind<4;++kind)objects.push_back(denseObject(kind,layouts));
 for(unsigned kind=0;kind<3;++kind)require(objects[kind].record.textLayout&&!objects[kind].record.textLayout->fonts.empty(),"dense shaped text layout absent");
 auto line=source();line.record.kind=semantic::ObjectKind::kVectorStroke;line.record.kindVersion=3;
 line.record.content.update([](auto& v){auto& stroke=std::get<semantic::CompactStrokeContent>(v).stroke;stroke.config.opacity=.55;});
 line.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(line.record.content)};
 for(unsigned h=0;h<2;++h){semantic::SweptCircleMask mask;
   for(unsigned j=0;j<4;++j){const double x=100+h*140+j*9;mask.segments.push_back({{{x,25},6},{{x+5,105},6},{x,45},{x+5,85}});}
   line.record.eraseMasks.push_back({ObjectId::fromUint64(91000+h),std::move(mask)});
 }
 nlohmann::json observations=nlohmann::json::array();
 for(unsigned profile=0;profile<(budgetComparison?2U:1U);++profile)
 for(unsigned area=0;area<(budgetComparison?3U:1U);++area)
 for(unsigned dpr:(budgetComparison?std::vector<unsigned>{area==2?4U:2U}:std::vector<unsigned>{1U,2U,4U}))
 for(unsigned kind=0;kind<(budgetComparison?5U:4U);++kind){
   const bool desktop=budgetComparison&&profile==1;
   const auto planFor=[&](const ReferenceTraversalEntry& body,std::vector<ReferenceTraversalEntry> content,
       unsigned active,unsigned ratio,unsigned frame,double offset,std::uint64_t generation=1,bool reverse=false){
     return denseObjectPlan(body,std::move(content),active,ratio,frame,offset,generation,reverse,
       budgetComparison?(area==0?600U:area==1?1431U:800U):0,
       budgetComparison?(area==0?900U:area==1?1010U:500U):0);
   };
   const auto caseOut=out/(std::to_string(profile)+"-"+std::to_string(area)+"-"+std::to_string(dpr)+"-"+std::to_string(kind));std::filesystem::create_directories(caseOut);
   auto initial=planFor(line,objects,kind,dpr,1,0);
   DisplayTarget target(completion),reference(completion);const auto width=initial.frame.metrics.physicalWidth,height=initial.frame.metrics.physicalHeight;
   require(target.resize(width,height).code==BackendSubmissionCode::kAccepted&&reference.resize(width,height).code==BackendSubmissionCode::kAccepted,"dense targets");
   SkiaRenderer candidate,oracle;ImageResourceResolver candidateImages(resources),oracleImages(resources);
   RenderCacheOptions limits;
   if(desktop){limits.maxBytes=256U*1024U*1024U;limits.sharedMaxBytes=512U*1024U*1024U;limits.maxEntryBytes=64U*1024U*1024U;}
   candidate.setRenderCacheOptions(limits);
   candidate.setImageResolver(&candidateImages);oracle.setImageResolver(&oracleImages);
   RenderCacheOptions disabled;disabled.enabled=false;oracle.setRenderCacheOptions(disabled);
   auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);target.release();
   std::vector<std::uint8_t> last=pixels(target,initial);unsigned retries=0,accepted=0;
   const auto independentBoundary=denseIndependentBoundary(initial);
   const auto boundaryPixels=std::count_if(independentBoundary.begin(),independentBoundary.end(),[](auto p){return p!=0;});
   saveBuffer(caseOut/"independent-boundary.a8",independentBoundary);
   std::shared_ptr<resources::ResourceBudgetManager> candidateBudget;
   nlohmann::json timings=nlohmann::json::array();std::size_t peakLeased=0;
   auto submit=[&](const FramePlan& plan,bool canonical,bool inject=false){
     const auto started=std::chrono::steady_clock::now();
     EditingOverlay overlay(plan.frame);
     const auto activeId=kind==4?ObjectId::fromUint64(90000):objects[kind].record.objectId;
     const auto active=std::find_if(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[&](const auto& e){return e.record.objectId==activeId;});
     require(active!=plan.referenceDrawList.entries.end(),"dense active present");
     require(overlay.update({.selectedObject=active->record.objectId,.worldBounds=active->record.worldBounds,.objectTransform=active->record.transform,.selected=true,.active=!canonical}),"dense selection");
     RenderPreparationBudget bounded;BackendSubmissionResult result;unsigned calls=0;
     do{
       const auto presents=target.presentCount(),submits=candidate.submissionCount();
       if(inject)transientDisplayFault.beforePublish=true;
       result=canonical?candidate.renderFrame(target,plan,&overlay,&bounded):candidate.renderTransientOverlay(target,plan,&overlay);
       if(!canonical){candidateBudget=transientDisplayBudget.lock();if(candidateBudget)peakLeased=std::max(peakLeased,candidateBudget->used());}
       completion->context->flushAndSubmit(GrSyncCpu::kYes);candidate.serviceResourceRetirements();++calls;
       if(result.code==BackendSubmissionCode::kRetry){++retries;auto failed=pixels(target,plan);
         require(failed==last&&target.presentCount()==presents&&candidate.submissionCount()==submits,"dense Retry modified accepted pixels/receipt");
         if(inject)return calls;
       }
       if(result.code==BackendSubmissionCode::kRejected)std::cerr<<result.message<<'\n';
       require(result.code!=BackendSubmissionCode::kRejected&&calls<512,"dense finite stable request failed");
     }while(result.code!=BackendSubmissionCode::kAccepted);
     timings.push_back({{"frame",plan.frame.frameId.value()},{"canonical",canonical},{"calls",calls},
       {"submit_and_gpu_completion_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()}});
     ++accepted;last=pixels(target,plan);
     const auto expected=canonical?oracle.renderFrame(reference,plan,&overlay):oracle.renderTransientOverlay(reference,plan,&overlay);
     require(expected.code==BackendSubmissionCode::kAccepted,"dense disabled Direct reference");
     const auto expectedPixels=pixels(reference,plan);
     const auto cameraBoundary=kind!=4&&plan.referenceDrawList.worldToView==WorldToViewAffine{}?
       std::vector<std::uint8_t>{}:denseIndependentBoundary(plan);
     const auto& band=cameraBoundary.empty()?independentBoundary:cameraBoundary;
     std::size_t changedPixels=0,interiorFailures=0;unsigned maxDifference=0;
     for(std::size_t i=0;i<last.size();i+=4){bool changed=false;for(unsigned c=0;c<4;++c){
       const auto difference=unsigned(std::abs(int(last[i+c])-int(expectedPixels[i+c])));
       changed|=difference!=0;maxDifference=std::max(maxDifference,difference);
       if(!band[i/4]&&difference>2)++interiorFailures;
     }changedPixels+=changed;}
     const auto prefix=caseOut/std::to_string(plan.frame.frameId.value());
     if(!budgetComparison||plan.frame.frameId.value()==4||interiorFailures){
       saveBuffer(prefix.string()+"-actual.rgba",last);saveBuffer(prefix.string()+"-oracle.rgba",expectedPixels);}
     const auto oracleWork=oracle.objectInteractionWork();
     const nlohmann::json row{{"DPR",dpr},{"kind",kind},{"frame",plan.frame.frameId.value()},{"canonical",canonical},{"calls",calls},
       {"width",width},{"height",height},{"actual",pixelDigest(last)},{"oracle",pixelDigest(expectedPixels)},
       {"oracle_static_builds",oracleWork.builds},{"oracle_static_hits",oracleWork.hits},{"pixel_equal",last==expectedPixels},
       {"changed_pixels",changedPixels},{"max_channel_difference",maxDifference},{"non_aa_failures",interiorFailures}};
     std::ofstream(prefix.string()+"-pixels.json")<<row.dump(2)<<'\n';
     require(oracleWork.builds==0&&oracleWork.hits==0,"dense Direct oracle reused candidate static scene");
     require(interiorFailures==0,"dense static order/mask/opacity/text/image mismatch outside frozen AA band");return calls;
   };
   submit(initial,true);
   for(unsigned move=0;move<3;++move)submit(planFor(line,objects,kind,dpr,2+move,7.5*(move+1)),false);
   auto changed=planFor(line,objects,kind,dpr,5,28);
   submit(changed,false,true);submit(changed,false);
   submit(planFor(line,objects,kind,dpr,6,0),false); // drag cancel
   submit(planFor(line,objects,kind,dpr,7,0,1,true),false); // order invalidation
   if(dpr==1&&kind==0){
     auto changedObjects=objects;
     changedObjects[1].record.properties.entries.back().value=.4F;
     submit(denseObjectPlan(line,changedObjects,kind,dpr,8,0),false); // static effect/content basis
     submit(denseObjectPlan(line,objects,kind,dpr,9,0),false);
     const auto current=denseObjectPlan(line,objects,kind,dpr,10,0);
     auto clipped=current.referenceDrawList;clipped.viewportClip={8,8,1400,990};
     submit(FramePlan{current.frame,std::move(clipped),current.background,false,true},false);
     submit(denseObjectPlan(line,objects,kind,dpr,11,0),false);
     auto changedHistory=line;semantic::SweptCircleMask extra;
     extra.segments.push_back({{{400,30},5},{{400,100},5},{400,45},{400,85}});
     changedHistory.record.eraseMasks.push_back({ObjectId::fromUint64(91999),extra});
     submit(denseObjectPlan(changedHistory,objects,kind,dpr,12,0),false);
     submit(denseObjectPlan(line,objects,kind,dpr,13,0),false);
     auto camera=denseObjectPlan(line,objects,kind,dpr,14,0);
     auto translated=camera.referenceDrawList;translated.worldToView.tx=3.5;
     submit(FramePlan{camera.frame,std::move(translated),camera.background,false,true},false);
     submit(denseObjectPlan(line,objects,kind,dpr,15,0),false);
     auto background=denseObjectPlan(line,objects,kind,dpr,16,0);
     foundation::DocumentBackground blue;blue.color={.88F,.93F,1,1};
     submit(FramePlan{background.frame,background.referenceDrawList,blue,false,true},false);
     submit(denseObjectPlan(line,objects,kind,dpr,17,0),false);
     // An epoch change invalidates source pixels even when the resource owner
     // pointer is the same. Both independent renderers bind their own resolver.
     candidate.setImageResolver(&candidateImages);oracle.setImageResolver(&oracleImages);
     completion->context->flushAndSubmit(GrSyncCpu::kYes);candidate.serviceResourceRetirements();oracle.serviceResourceRetirements();
     submit(denseObjectPlan(line,objects,kind,dpr,18,0),false);
     require(candidateImages.decodeCount()==1&&oracleImages.decodeCount()==1,"dense image decoded every move");
   }
   const auto work=candidate.renderCacheWork();const auto interaction=candidate.objectInteractionWork();
   require(work.cpuCoverageTiles==0&&work.preparedMaxAdvanceKnots<=32,"dense changed GPU/quantum policy");
   require(work.preparedBodyBuilds==(dpr==1&&kind==0?2U:1U),"dense equal body prepared more than once per resource epoch");
   if(dpr==1)require(interaction.builds>0&&interaction.hits>=2,"dense affordable static reuse not reached");
   if(desktop&&dpr==2&&kind<4)require(interaction.builds>0&&interaction.hits>=2,"DPR2 desktop static reuse not reached");
   auto liveBudget=transientDisplayBudget.lock();require(bool(liveBudget)&&liveBudget->used()<=96U*1024U*1024U,"dense oracle budget witness");
   require(candidateBudget&&candidateBudget->used()<=limits.sharedMaxBytes,"dense candidate budget witness");
   nlohmann::json memory=nlohmann::json::array();
   for(unsigned resource=0;resource<unsigned(resources::ResourceClass::kCount);++resource){
     const auto usage=candidateBudget->snapshot(static_cast<resources::ResourceClass>(resource));
     memory.push_back({{"class",resource},{"used",usage.used},{"staging",usage.staging},{"pending_release",usage.pendingRelease}});}
   // The last observer is the oracle; it owns a separate default manager.
   auto rebound=planFor(line,objects,kind,dpr,8,0,target.generation());
   const auto presents=target.presentCount();require(target.advanceGeneration().code==BackendSubmissionCode::kAccepted,"dense generation");
   EditingOverlay stale(rebound.frame);require(candidate.renderTransientOverlay(target,rebound,&stale).code==BackendSubmissionCode::kRejected&&target.presentCount()==presents,"dense stale target presented");
   if(dpr==1&&kind==0){
     require(reference.advanceGeneration().code==BackendSubmissionCode::kAccepted,"dense oracle generation");
     submit(denseObjectPlan(line,objects,kind,dpr,19,0,target.generation()),false);
   }
   oracle.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);oracle.serviceResourceRetirements();
   require(liveBudget->used()==0,"dense oracle lease release");
   candidate.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);candidate.serviceResourceRetirements();
   require(candidateBudget&&candidateBudget->used()==0,"dense candidate leases not retired");
   observations.push_back({{"DPR",dpr},{"kind",kind},{"width",width},{"height",height},{"lines",19},{"knots_per_body",7199},{"historical_masks",2},
     {"accepted",accepted},{"retries",retries},{"body_builds",work.preparedBodyBuilds},{"body_compile",work.preparedCompileInputs},
     {"max_advance_knots",work.preparedMaxAdvanceKnots},{"static_builds",interaction.builds},{"static_hits",interaction.hits},
     {"static_invalidations",interaction.invalidations},{"static_denials",interaction.denials},{"lease_bytes",interaction.leaseBytes},
     {"gpu_marks",interaction.gpuLeaseMarks},{"cpu_tiles",work.cpuCoverageTiles},{"non_aa_pixels_match",true},
     {"candidate_clear_bytes",candidateBudget->used()},{"oracle_clear_bytes",liveBudget->used()},
     {"budget_comparison",budgetComparison},{"profile",desktop?"desktop":"conservative"},
     {"optional_bytes",limits.maxBytes},{"shared_bytes",limits.sharedMaxBytes},{"entry_bytes",limits.maxEntryBytes},
     {"cache_hits",work.hits},{"cache_misses",work.misses},{"cache_evictions",work.evictions},
     {"coverage_builds",work.coverageBuilds},{"coverage_hits",work.coverageHits},{"coverage_steps",work.preparationCoverageSteps},
     {"lease_peak_bytes",peakLeased},{"memory_before_clear",memory},{"timings",timings}});
   observations.back()["independent_band_pixels"]=boundaryPixels;
   observations.back()["independent_band_fraction"]=double(boundaryPixels)/independentBoundary.size();
   observations.back()["non_aa_channel_tolerance"]=2;
   std::cout<<observations.back().dump()<<std::endl;
 }
 std::ofstream(out/"dense-mixed-counters.json")<<observations.dump(2)<<'\n';
}
void canonicalMaskOnly(const std::filesystem::path& out,bool verify){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
 auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 completion->context=GrDirectContexts::MakeMetal(backend);DisplayTarget target(completion),oracle(completion);
 require(target.resize(512,256).code==BackendSubmissionCode::kAccepted&&oracle.resize(512,256).code==BackendSubmissionCode::kAccepted,"canonical mask-only targets");
 SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;renderer.setRenderCacheOptions(options);
 auto line=source();line.record.kind=semantic::ObjectKind::kVectorStroke;line.record.kindVersion=3;
 line.record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=.55;});line.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(line.record.content)};
 nlohmann::json rows=nlohmann::json::array();
 for(unsigned round=0;round<4;++round){
  if(round){semantic::SweptCircleMask mask;const double x=40+round*38;
   for(unsigned i=0;i<8;++i)mask.segments.push_back({{{x+i,30},3},{{x+i,100},3},{x+i,45},{x+i,85}});
   // Insert before prior IDs, proving no back()/count-based append assumption.
   line.record.eraseMasks.push_back({ObjectId::fromUint64(100-round),std::move(mask)});
   std::sort(line.record.eraseMasks.begin(),line.record.eraseMasks.end(),[](const auto&a,const auto&b){return a.id<b.id;});
  }
  auto base=renderPlan(line,round+1,2);auto draw=base.referenceDrawList;draw.entries={line};
  FramePlan plan{base.frame,std::move(draw),{},false,true};RenderPreparationBudget bounded;
  const auto before=renderer.renderCacheWork();unsigned attempts=0;
  for(;;){const auto result=renderer.renderFrame(target,plan,nullptr,&bounded);completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();++attempts;
   if(result.code==BackendSubmissionCode::kAccepted)break;require(result.code==BackendSubmissionCode::kRetry&&attempts<1024,"canonical mask-only progress");}
  const auto after=renderer.renderCacheWork();auto a=oracle.acquire();
  require(drawReferencePlanToSkCanvas(*a.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"independent canonical mask oracle");oracle.release();require(oracle.present().code==BackendSubmissionCode::kAccepted,"oracle present");
  const auto actual=pixels(target,plan),expected=pixels(oracle,plan),band=denseIndependentBoundary(plan);std::size_t failures=0;
  for(std::size_t i=0;i<actual.size();++i)if(!band[i/4]&&std::abs(int(actual[i])-int(expected[i]))>2)++failures;
  saveBuffer(out/(std::to_string(round)+"-actual.rgba"),actual);saveBuffer(out/(std::to_string(round)+"-oracle.rgba"),expected);
  rows.push_back({{"round",round},{"attempts",attempts},{"coverage_steps",after.preparationCoverageSteps-before.preparationCoverageSteps},
   {"body_tiles",after.bodyTileBuilds-before.bodyTileBuilds},{"body_hits",after.bodyTileHits-before.bodyTileHits},
   {"mask_draws",after.maskPrimitiveDraws-before.maskPrimitiveDraws},{"mask_resets",after.maskTileResets-before.maskTileResets},{"non_aa_failures",failures}});
  std::ofstream(out/"canonical.json")<<rows.dump(2)<<'\n';require(failures==0,"canonical mask-only pixels");
  if(verify&&round)require(after.preparationCoverageSteps==before.preparationCoverageSteps,"mask-only canonical replayed unchanged body coverage");
 }
 renderer.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
 for(const auto& row:rows)std::cout<<row.dump()<<'\n';
}
std::vector<std::uint8_t> pixels(DisplayTarget& t,const FramePlan& p){std::vector<std::uint8_t> out(std::size_t(p.frame.metrics.physicalWidth)*p.frame.metrics.physicalHeight*4);require(t.readbackRgba(out).code==BackendSubmissionCode::kAccepted,"actual target capture");return out;}
void settle(SkiaRenderer& r,DisplayTarget& t,const FramePlan& p,const std::shared_ptr<NativeMetalCompletion>& c){
 EditingOverlay overlay{p.frame};
 for(unsigned i=0;i<2048;++i){auto result=r.renderTransientOverlay(t,p,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();if(result.code==BackendSubmissionCode::kAccepted)return;if(result.code!=BackendSubmissionCode::kRetry)std::cerr<<result.message<<'\n';require(result.code==BackendSubmissionCode::kRetry,"newest request rejected");}
 require(false,"stable feasible transient demand did not progress");
}
void continuity(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);
 for(unsigned dpr:{1U,2U,4U})for(unsigned bg=0;bg<3;++bg){
  DisplayTarget target(c),reference(c);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted&&reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"continuity targets");
  SkiaRenderer r;auto a=displayPlan(1,1,dpr,bg);settle(r,target,a,c);const auto before=pixels(target,a);const auto presents=target.presentCount(),submits=r.submissionCount(),builds=r.renderCacheWork().preparedBodyBuilds;
  const auto fail=[&](const FramePlan& p,const char* stage){EditingOverlay overlay{p.frame};const auto result=r.renderTransientOverlay(target,p,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();auto after=pixels(target,p);
   saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-"+stage+"-before.rgba"),before);saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-"+stage+"-after.rgba"),after);
   std::cout<<nlohmann::json{{"DPR",dpr},{"background",bg},{"stage",stage},{"status",static_cast<unsigned>(result.code)},{"message",result.message},{"pixels_preserved",after==before},{"presents",target.presentCount()},{"body_builds",r.renderCacheWork().preparedBodyBuilds}}.dump()<<std::endl;
   require(result.code!=BackendSubmissionCode::kAccepted,"failure stage accepted");require(after==before,"failed attempt changed last display");require(target.presentCount()==presents&&r.submissionCount()==submits,"failed attempt counted as successful display");};
  auto b=displayPlan(2,2,dpr,bg);transientDisplayFault.beforeObject=1;fail(b,"after-one-object");
  auto newestBase=displayPlan(3,3,dpr,bg);auto newestDraw=newestBase.referenceDrawList;
  for(auto& entry:newestDraw.entries)entry.record.transform.tx+=.125;
  FramePlan newest{newestBase.frame,std::move(newestDraw),newestBase.background,newestBase.transparentBackground,newestBase.isolateEraseMasks};
  transientDisplayFault.afterTile=0;fail(newest,"after-one-tile");
  target.unavailable=true;fail(displayPlan(3,4,dpr,bg),"acquire-unavailable");target.unavailable=false;
  transientDisplayFault.beforeObject=0;fail(displayPlan(3,5,dpr,bg),"before-first-object");
  transientDisplayFault.beforePublish=true;fail(displayPlan(3,6,dpr,bg),"prepublication-refusal");
  auto final=displayPlan(3,7,dpr,bg);settle(r,target,final,c);auto actual=pixels(target,final);
  SkiaRenderer full;settle(full,reference,final,c);auto expected=pixels(reference,final);saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-latest.rgba"),actual);saveBuffer(out/(std::to_string(dpr)+"-"+std::to_string(bg)+"-full-reference.rgba"),expected);
  require(actual==expected,"cumulative failed B/C damage lost or opacity applied twice");require(r.renderCacheWork().preparedBodyBuilds==builds,"failed display continuity caused full body rebuild");require(r.renderCacheWork().eraseDamageFrames>0,"test did not enter incremental erase");require(r.renderCacheWork().cpuCoverageTiles==0,"strict v4 used CPU coverage");
  // Independent original null-cache/full-history CPU drawing remains separate.
  DisplayTarget cpu(c);require(cpu.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"CPU reference target");auto acq=cpu.acquire();require(drawReferencePlanToSkCanvas(*acq.frame.surface->getCanvas(),final).code==BackendSubmissionCode::kAccepted,"original full reference");cpu.release();require(cpu.present().code==BackendSubmissionCode::kAccepted,"reference present");auto original=pixels(cpu,final);std::vector<bool> edge(original.size()/4);
  for(const auto& entry:final.referenceDrawList.entries){auto e=independentEdges(entry,dpr);for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||e[i];}
  for(std::size_t i=0;i<original.size();++i)if(!edge[i/4])require(std::abs(int(actual[i])-int(original[i]))<=2,"latest outside original reference AA band");
  std::cout<<nlohmann::json{{"DPR",dpr},{"background",bg},{"latest_equals_independent_full_gpu",true},{"original_AA_reference",true},{"body_builds",builds},{"damage_frames",r.renderCacheWork().eraseDamageFrames}}.dump()<<std::endl;
 }
}
void recovery(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);DisplayTarget target(c);require(target.resize(3200,2000).code==BackendSubmissionCode::kAccepted,"DPR4 pressure target");SkiaRenderer r;r.setRenderCacheOptions(RenderCacheOptions{});auto a=displayPlan(1,1,4,0,16,true);settle(r,target,a,c);auto before=pixels(target,a);auto budget=transientDisplayBudget.lock();require(bool(budget),"test-only existing budget witness");
 const auto available=96U*1024U*1024U-budget->used();const auto token=budget->reserve(resources::ResourceClass::kRecorded,available-1024,true);require(bool(token),"admit held competing quota");resources::ResourceLease hold(budget,*token);const auto presents=target.presentCount();
 for(unsigned i=0;i<3;++i){auto pBase=displayPlan(i+2,i+2,4,0,16,true);auto pDraw=pBase.referenceDrawList;
  for(auto& entry:pDraw.entries)entry.record.content.update([](auto& value){++std::get<semantic::CompactStrokeContent>(value).stroke.seed;});
  for(auto& entry:pDraw.entries)entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
  FramePlan p{pBase.frame,std::move(pDraw),pBase.background,pBase.transparentBackground,pBase.isolateEraseMasks};EditingOverlay overlay{p.frame};auto result=r.renderTransientOverlay(target,p,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();auto after=pixels(target,p);require(result.code==BackendSubmissionCode::kRetry&&after==before&&target.presentCount()==presents,"genuine quota Retry changed display");std::cout<<nlohmann::json{{"pressure_attempt",i},{"used_bytes",budget->used()},{"hard_bytes",96U*1024U*1024U},{"pixels_preserved",true},{"GPU_epochs",budget->pendingGpuEpochCount()}}.dump()<<std::endl;}
 hold.reset();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();auto newest=displayPlan(4,9,4,0,16,true);settle(r,target,newest,c);require(target.presentCount()==presents+1,"quota release did not publish newest exactly once");require(budget->used()<=96U*1024U*1024U,"new target escaped original hard cap");
 const auto count=target.presentCount(),submits=r.submissionCount();target.refusePresent=true;EditingOverlay overlay{newest.frame};require(r.renderTransientOverlay(target,newest,&overlay).code!=BackendSubmissionCode::kAccepted,"provider rejection accepted");require(target.presentCount()==count&&r.submissionCount()==submits,"provider rejection fabricated receipt");target.refusePresent=false;settle(r,target,displayPlan(4,10,4,0,16,true),c);
 require(target.advanceGeneration().code==BackendSubmissionCode::kAccepted,"generation advance");require(r.renderTransientOverlay(target,newest,&overlay).code==BackendSubmissionCode::kRejected,"stale target presented");
 r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"derived leases survived clear plus real GPU completion");std::cout<<nlohmann::json{{"DPR4_default96_progress",true},{"retries",3},{"after_clear_bytes",budget->used()},{"no_fake_present",true}}.dump()<<std::endl;
 // The accepted P36 NotAdmitted path must also progress behind the new
 // transactional display target, not only through the lower-level helper.
 DisplayTarget historyTarget(c);require(historyTarget.resize(1024,512).code==BackendSubmissionCode::kAccepted,"history display target");auto start=displayPlan(1,11,4,0,1);settle(r,historyTarget,start,c);auto stable=pixels(historyTarget,start);r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"history pressure fixture must first retire evictable work");
 auto draw=start.referenceDrawList;semantic::SweptCircleMask largeHistory;largeHistory.segments.assign(40000,semantic::EraseCubicSegment{{{80,40},2},{{80,80},2},{80,50},{80,70}});draw.entries[0].record.eraseMasks={{ObjectId::fromUint64(950),std::move(largeHistory)}};FramePlan large{start.frame,std::move(draw),{},false,true};EditingOverlay largeOverlay{large.frame};
 const auto held=budget->reserve(resources::ResourceClass::kRecorded,96U*1024U*1024U-budget->used()-1024,true);require(bool(held),"large history quota hold");resources::ResourceLease historyPressure(budget,*held);
 for(unsigned i=0;i<2;++i){require(r.renderTransientOverlay(historyTarget,large,&largeOverlay).code==BackendSubmissionCode::kRetry,"history pressure did not retry");c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(pixels(historyTarget,large)==stable,"history pressure destroyed last display");}
 historyPressure.reset();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();settle(r,historyTarget,large,c);require(r.renderCacheWork().maskHistory.denied>0&&r.renderCacheWork().cpuCoverageTiles==0,"history nonadmission bypassed GPU recovery");
 std::cout<<nlohmann::json{{"actual_renderer_40000_history_progress",true},{"history_denials",r.renderCacheWork().maskHistory.denied},{"CPU_tiles",r.renderCacheWork().cpuCoverageTiles},{"used_bytes",budget->used()}}.dump()<<std::endl;
 r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"history display leases did not retire");

}


FramePlan handoffPlan(unsigned copies,std::uint64_t id,std::uint64_t generation=1){auto p=workingSetPlan(copies,true,4);const auto& b=p.frame;
 FrameState f{b.viewId,b.camera,{0,0,1024,576},{1024,576,4096,2304,4,1},semantic::SemanticGeneration{id},SceneRevision{id},SurfaceGeneration{generation},b.metricsGeneration,FrameId{id},{0,0,1024,576}};
 ReferenceDrawList d{f};d.viewportClip=f.viewportClip;d.entries=p.referenceDrawList.entries;for(auto& e:d.entries)e.presentationEraseActive=true;return{f,std::move(d),{},false,true};}
bool handoffProgress(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);bool all=true;
 for(bool warm:{false,true}){DisplayTarget target(c);require(target.resize(4096,2304).code==BackendSubmissionCode::kAccepted,"handoff target");auto acq=target.acquire();acq.frame.surface->getCanvas()->clear(SK_ColorWHITE);target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"initial valid provider display");SkiaRenderer r;
  if(warm){const auto a=handoffPlan(1,1);settle(r,target,a,c);}
  const auto next=handoffPlan(513,2);EditingOverlay o{next.frame};const auto before=pixels(target,next);const auto priorPresents=target.presentCount();saveBuffer(out/(warm?"warm-before.rgba":"fresh-before.rgba"),before);bool accepted=false;unsigned calls=0;
  for(;calls<1024;++calls){auto result=r.renderTransientOverlay(target,next,&o);c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();const auto actual=pixels(target,next);const auto b=transientDisplayBudget.lock();const auto w=r.renderCacheWork();
   std::cout<<nlohmann::json{{"warm",warm},{"attempt",calls},{"status",unsigned(result.code)},{"message",result.message},{"used",b->used()},{"canonical_frame_bytes",b->used(resources::ResourceClass::kCanonicalFrame)},{"scratch_bytes",b->used(resources::ResourceClass::kScratch)},{"GPU_epochs",b->pendingGpuEpochCount()},{"body_builds",w.preparedBodyBuilds},{"presents",target.presentCount()}}.dump()<<std::endl;
   require(b->used()<=96U*1024U*1024U,"handoff exceeded original hard cap");if(result.code==BackendSubmissionCode::kAccepted){accepted=true;saveBuffer(out/(warm?"warm-accepted.rgba":"fresh-accepted.rgba"),actual);break;}
   require(result.code==BackendSubmissionCode::kRetry,"handoff unexpectedly rejected");if(actual!=before)saveBuffer(out/(warm?"warm-failed.rgba":"fresh-failed.rgba"),actual);require(actual==before&&target.presentCount()==priorPresents,"handoff Retry changed actual last display");
  }
  std::cout<<nlohmann::json{{"handoff_summary",true},{"warm",warm},{"accepted",accepted},{"calls",calls+unsigned(accepted)},{"CPU_tiles",r.renderCacheWork().cpuCoverageTiles}}.dump()<<std::endl;
  all&=accepted;require(r.renderCacheWork().preparedBodyBuilds<=2,"handoff Retry rebuilt identical body");require(r.renderCacheWork().cpuCoverageTiles==0,"handoff CPU fallback");r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();
 }
 if(!all)std::cerr<<"G5_HO01_FEASIBLE_WARM_STAGED_DEMAND_DID_NOT_ACCEPT\n";return all;
}


void handoffInflight(const std::filesystem::path& out){
 std::filesystem::create_directories(out);auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);DisplayTarget target(c);require(target.resize(4096,2304).code==BackendSubmissionCode::kAccepted,"inflight target");SkiaRenderer r;auto a=handoffPlan(1,1);settle(r,target,a,c);auto before=pixels(target,a);auto budget=transientDisplayBudget.lock();auto attempt=transientDisplayAttempt.lock();require(bool(attempt)&&bool(budget),"private charged attempt observer");
 const auto epoch=budget->beginGpuEpoch(resources::kPixelResourceClasses,resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame));require(bool(epoch),"delayed real GPU read epoch");require(attempt->pixels.markGpuUse(),"pin actual attempt read");
 const auto token=budget->reserve(resources::ResourceClass::kScratch,16*16*4,true);require(bool(token),"sample target quota");resources::ResourceLease sampleLease(budget,*token);auto sample=attempt->surface->getCanvas()->makeSurface(SkImageInfo::MakeN32Premul(16,16));require(bool(sample),"GPU sample target");sample->getCanvas()->drawImage(attempt->surface->makeImageSnapshot(),0,0);require(budget->endGpuEpoch(*epoch),"end actual read epoch");
 struct Gate{bool physicalDone=false;} gate;GrFlushInfo info;info.fFinishedContext=&gate;info.fFinishedProc=[](void* p){static_cast<Gate*>(p)->physicalDone=true;};c->context->flush(info);require(c->context->submit(GrSyncCpu::kNo),"actual delayed sample submit");sample.reset();sampleLease.reset();attempt.reset();
 auto large=handoffPlan(513,2);EditingOverlay overlay{large.frame};const auto presents=target.presentCount();auto result=r.renderTransientOverlay(target,large,&overlay);c->context->flushAndSubmit(GrSyncCpu::kYes);c->service();r.serviceResourceRetirements();
 require(result.code==BackendSubmissionCode::kRetry,"unacknowledged old GPU read must keep quota pending");require(gate.physicalDone,"physical GPU completion not observed");require(transientDisplayAttempt.expired(),"staged path still owns obsolete attempt");require(budget->snapshot(resources::ResourceClass::kCanonicalFrame).pendingRelease>=36U*1024U*1024U,"dropping owner freed inflight quota early");require(pixels(target,large)==before&&target.presentCount()==presents,"inflight handoff damaged last provider display");
 std::cout<<nlohmann::json{{"stage","delayed-completion"},{"GPU_epoch",*epoch},{"physical_finished",gate.physicalDone},{"pending_release",budget->snapshot(resources::ResourceClass::kCanonicalFrame).pendingRelease},{"used",budget->used()},{"actual_pixels_preserved",true}}.dump()<<std::endl;
 require(budget->completeGpuEpoch(*epoch),"ack real finished read epoch");r.serviceResourceRetirements();settle(r,target,large,c);require(target.presentCount()==presents+1,"latest staged frame did not publish once");
 for(unsigned cycle=0;cycle<2;++cycle){auto small=handoffPlan(1,3+cycle*2);settle(r,target,small,c);require(!transientDisplayAttempt.expired(),"smaller demand never returned to normal residency");auto next=handoffPlan(513,4+cycle*2);settle(r,target,next,c);require(transientDisplayAttempt.expired(),"repeat staged switch retained unused third plane");}
 require(budget->used()<=96U*1024U*1024U&&r.renderCacheWork().cpuCoverageTiles==0,"switch escaped original GPU/budget policy");
 require(target.resize(4096,2304).code==BackendSubmissionCode::kAccepted,"logical rebind");const auto oldCount=target.presentCount();require(r.renderTransientOverlay(target,large,&overlay).code==BackendSubmissionCode::kRejected&&target.presentCount()==oldCount,"stale generation presented");auto rebound=handoffPlan(1,8,target.generation());settle(r,target,rebound,c);r.clearRenderCache();c->context->flushAndSubmit(GrSyncCpu::kYes);r.serviceResourceRetirements();require(budget->used()==0,"completed switch resources did not converge to zero");
 std::cout<<nlohmann::json{{"stage","switches-complete"},{"cycles",2},{"generation",target.generation()},{"used",budget->used()},{"CPU_tiles",r.renderCacheWork().cpuCoverageTiles}}.dump()<<std::endl;
}

#endif
}
int main(int argc,char** argv){try{
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");{const std::string mode=argc>1?argv[1]:"--pixels";const auto out=std::filesystem::path(argc>2?argv[2]:"out/transient-display-pixels");if(mode=="--ec-canonical"||mode=="--ec-canonical-probe")canonicalMaskOnly(out,mode=="--ec-canonical");else if(mode=="--handoff-inflight")handoffInflight(out);else if(mode=="--handoff"){if(!handoffProgress(out)){message(pool,"drain");return 1;}}else if(mode=="--continuity")continuity(out);else if(mode=="--object-continuity")objectContinuityStaticReuse(out);else if(mode=="--dense-objects"||mode=="--desktop-budget")denseObjectContinuity(out,mode=="--desktop-budget");else if(mode=="--recovery")recovery(out);else failedPixels(mode=="--pixels"?out:std::filesystem::path(mode));}message(pool,"drain");
#else
 (void)argc;(void)argv;
#endif
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
