#define main ur2cExistingFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/selection_region_geometry_internal.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include "canvas/semantic/validator.hpp"
namespace {
semantic::EraseMaskRecord historyMask(unsigned n){
 semantic::SweptCircleMask m;
 for(unsigned i=0;i<32;++i){double x=20+n*12+i*.2; m.segments.push_back({{{x,40},2},{{x+.2,80},2},{x+.1,50},{x+.1,70}});}
 return {ObjectId::fromUint64(500+n),m};
}
void historyProbe(bool probe){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();
 GrMtlBackendContext context{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(context);require(bool(completion->context),"UR2c GPU fixture");
 for(unsigned history:{0U,1U,2U,5U,10U})for(unsigned copies:{1U,8U,16U,32U}){
  auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;for(unsigned n=0;n<history;++n)e.record.eraseMasks.push_back(historyMask(n));
  auto surface=SkSurfaces::RenderTarget(completion->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(256,128));require(bool(surface),"probe surface");
  auto budget=std::make_shared<resources::ResourceBudgetManager>();auto sources=std::make_shared<SourceRetentionPool>();auto prepared=std::make_shared<PreparedGeometryCache>();prepared->setRetentionPool(sources);
  RenderCacheOptions options;auto body=prepared->get(e,options,budget,true);require(body&&body->ready(),"probe ready body");
  SkiaInkLayers layers;CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork work;
  auto draw=[&]{for(unsigned n=0;n<copies;++n){e.record.objectId=ObjectId::fromUint64(n+1);bool painted=false,pending=false;
   require(layers.draw(*surface->getCanvas(),*surface,e,SkMatrix::I(),SkIRect::MakeWH(256,128),nullptr,options,budget,sources,targets,work,painted,pending,prepared,body)&&!pending&&painted,"real ink layers draw");}};
  draw();auto before=work;draw();draw();
  EraseMaskPathCache paths;semantic::FilledPathMask q;q.path.commands={semantic::MoveTo{{200,50}},semantic::LineTo{{205,50}},semantic::LineTo{{205,75}},semantic::LineTo{{200,75}},semantic::ClosePath{}};
  require(paths.query(e.record,q,budget,8*1024*1024,prepared.get(),&options)==intersectEraseGeometryComponentOracle(e.record,q),"probe exact oracle");
  const auto pw=paths.work();for(unsigned r=0;r<2;++r)for(unsigned n=0;n<copies;++n){e.record.objectId=ObjectId::fromUint64(n+1);require(paths.query(e.record,q,budget,8*1024*1024,prepared.get(),&options)!=SelectionGeometryHit::kInvalid,"warm exact query");}
  const auto pe=paths.work();std::cout<<nlohmann::json{{"exact_history",history},{"copies",copies},{"path_value_checks",pe.pathValueComparisons-pw.pathValueComparisons},{"path_primitive_checks",pe.pathPrimitiveComparisons-pw.pathPrimitiveComparisons},{"path_builds",pe.builds-pw.builds}}.dump()<<'\n';
  const auto flat=work.historyFlattenedPrimitives-before.historyFlattenedPrimitives,bytes=work.historyCopiedBytes-before.historyCopiedBytes,compared=work.historyComparedPrimitives-before.historyComparedPrimitives;
  std::cout<<nlohmann::json{{"copies",copies},{"history",history},{"knots",7199},{"warm_draws",copies*2},{"flattened",flat},{"copied_bytes",bytes},{"compared",compared},{"body_builds",work.bodyTileBuilds},{"body_hits",work.bodyTileHits},{"structural_red",flat>0}}.dump()<<'\n';
  if(!probe)require(flat==0&&bytes==0&&compared==0,"unchanged historical mask preparation repeated");
  layers.clear();body.reset();prepared->clear();targets.clear();completion->context->flushAndSubmit(GrSyncCpu::kYes);
 }
 }message(pool,"drain");
#else
 require(false,"native GPU unavailable");
#endif
}

void historyStorageAndLeases(bool pressure){
 auto budget=std::make_shared<resources::ResourceBudgetManager>();auto pool=std::make_shared<SourceRetentionPool>();EraseMaskPathCache service;service.setRetentionPool(pool);
 scene::SharedVector<semantic::EraseMaskRecord> records{historyMask(1),historyMask(2)};
 auto a=service.history(records,budget,8*1024*1024);require(a&&a->primitives.size()==64,"history preparation");
 const auto start=service.work();require(service.history(records,budget,8*1024*1024)==a&&service.work().history.valuePrimitives==start.history.valuePrimitives,"known immutable owner compared history");
 scene::SharedVector<semantic::EraseMaskRecord> unknown{std::vector<semantic::EraseMaskRecord>(records.begin(),records.end())};
 auto b=service.history(unknown,budget,8*1024*1024);require(b&&b!=a&&b->primitives.front()==a->primitives.front()&&service.work().history.valuePrimitives>start.history.valuePrimitives,"unknown owner skipped complete value proof or rebuilt prefix paths");
 auto old=records.read();records.push_back(historyMask(3));auto c=service.history(records,budget,8*1024*1024,a);require(c&&c->primitives.size()==96&&c->primitives.front()==a->primitives.front()&&old.size()==2,"append old storage/prefix");
 auto changed=records;std::get<semantic::SweptCircleMask>(changed[0].geometry).segments.back().control2.x+=.1;
 auto fork=service.history(changed,budget,8*1024*1024,c);require(fork&&fork->primitives.front()!=c->primitives.front(),"same ID changed geometry reused prepared mask");
 auto reordered=records;std::swap(reordered[0],reordered[1]);auto r=service.history(reordered,budget,8*1024*1024,c);require(r&&r->prefixPrimitives==0,"reordered history trusted count/IDs");
 auto shortHistory=records;shortHistory.pop_back();auto u=service.history(shortHistory,budget,8*1024*1024,c);require(u&&u->primitives.size()==64&&u->prefixPrimitives<c->primitives.size(),"undo/truncate did not reset full result prefix");
 require(bool(service.historicalPath(a,0)),"lazy CPU exact path");const auto pw=service.work();require(bool(service.historicalPath(a,0))&&service.work().pathValueComparisons==pw.pathValueComparisons,"resident historical exact path deep lookup");
 if(pressure){
  const auto bytes=budget->used();service.clear();require(budget->used()==bytes,"history eviction released pinned leases");
  auto other=std::make_shared<resources::ResourceBudgetManager>();auto foreign=service.history(records,other,8*1024*1024,c);require(foreign&&foreign->primitives.front()!=c->primitives.front(),"foreign budget reused old path leases");
  auto tiny=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});require(!service.history(records,tiny,8*1024*1024)&&tiny->used()==0,"denied history retained quota");
  foreign.reset();service.clear();require(other->used()==0,"foreign owner lease leak");
 }
 a.reset();b.reset();c.reset();fork.reset();r.reset();u.reset();service.clear();require(budget->used()==0,"history/path/source lease leak");
 for(unsigned i=0;i<600;++i){scene::SharedVector<semantic::EraseMaskRecord> one{historyMask(100+i)};require(bool(service.history(one,budget,8*1024*1024)),"bounded history LRU stopped progressing");}
 require(service.work().bytes<=8U*1024U*1024U&&budget->used()<=96U*1024U*1024U,"history exceeds frozen quotas");service.clear();require(budget->used()==0,"bounded history LRU leaked retained source");
 std::cout<<"{\"stable_provenance\":true,\"complete_unknown_comparison\":true,\"append_suffix\":32,\"fork_reorder_truncate\":true,\"bounded_history_LRU\":600,\"last_release_bytes\":0}\n";
}
void historyPixels(){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);
 for(unsigned dpr:{1U,2U,4U})for(unsigned family=0;family<2;++family)for(unsigned variant=0;variant<3;++variant){
  auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;
  e.record.content.update([&](auto& value){auto& s=std::get<semantic::CompactStrokeContent>(value).stroke;s.config.family=family?foundation::InkFamily::kMarker:foundation::InkFamily::kRound;s.config.opacity=.55;s.config.markerAngle=.6;s.config.markerFlatness=.4;s.knots.clear();for(unsigned i=0;i<40;++i)s.knots.push_back({20.+i*5,64+15*std::sin(i*.3),std::uint16_t(20000+i*1000)});});
  e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};e.record.transform={1,.04,-.02,1,4,0};e.record.eraseMasks={historyMask(1)};
  semantic::FilledPathMask filled;filled.path.commands={semantic::MoveTo{{80,0}},semantic::LineTo{{94,0}},semantic::LineTo{{94,128}},semantic::LineTo{{80,128}},semantic::ClosePath{}};
  NativeMetalTarget target(completion),reference(completion);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted&&reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"history pixel targets");SkiaRenderer renderer;
  for(unsigned state=0;state<6;++state){
   if(state==1)e.record.eraseMasks.push_back({ObjectId::fromUint64(700),filled});
   if(state==2)std::swap(e.record.eraseMasks[0],e.record.eraseMasks[1]);
   if(state==3)std::get<semantic::FilledPathMask>(e.record.eraseMasks[0].geometry).path.commands[1]=semantic::LineTo{{100,0}};
   if(state==4)e.record.eraseMasks.pop_back();
   if(state==5)e.record.eraseMasks={historyMask(1),{ObjectId::fromUint64(700),filled}};
   auto e2=e;e2.record.objectId=ObjectId::fromUint64(2);e2.record.transform={.95,.05,-.05,.95,15,7};
   const auto base=renderPlan(e,state+1,dpr);auto draw=base.referenceDrawList;draw.entries={e,e2};
   foundation::DocumentBackground bg;if(variant==2)bg.color={.25,.5,.75,1};FramePlan plan{base.frame,draw,bg,variant==1,true};
   for(unsigned tries=0;;++tries){auto result=renderer.renderFrame(target,plan);if(result.code==BackendSubmissionCode::kAccepted)break;require(result.code==BackendSubmissionCode::kRetry&&tries<16384,"history pixel canonical progress");}
   auto a=reference.acquire();require(internal::drawReferencePlanToSkCanvas(*a.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"independent null-cache reference");reference.release();require(reference.present().code==BackendSubmissionCode::kAccepted,"independent present");
   std::vector<std::uint8_t> actual(256*dpr*128*dpr*4),expected(actual.size());require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"history pixel capture");
   auto edges=independentEdges(e,dpr),edges2=independentEdges(e2,dpr);
   for(std::size_t i=0;i<actual.size();++i)if(!edges[i/4]&&!edges2[i/4])require(std::abs(int(actual[i])-int(expected[i]))<=2,"history pixels changed outside original CPU geometric AA band");
  }
  require(renderer.renderCacheWork().maskHistory.admissions>0,"canonical did not consume history service");
  std::cout<<nlohmann::json{{"native_dpr",dpr},{"family",family},{"backdrop_variant",variant},{"ordered_mask_states",6},{"original_AA_pixels",true},{"canonical_history_consumer",true}}.dump()<<'\n';
 }
 }message(pool,"drain");
#endif
}
ReferenceTraversalEntry progressSource(unsigned segments){
 semantic::CompactStrokeContent c;c.stroke.config.version=2;c.stroke.config.algorithmVersion=4;c.stroke.config.signalVersion=3;c.stroke.config.size=18;c.stroke.seed=42;
 for(unsigned i=0;i<32;++i)c.stroke.knots.push_back({20.+i*5,64,32768});
 semantic::SweptCircleMask sweep;sweep.segments.assign(segments,semantic::EraseCubicSegment{{{80,40},2},{{80,80},2},{80,50},{80,70}});
 semantic::ObjectRecord object;object.id=ObjectId::fromUint64(1);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=c;object.erase_masks={semantic::EraseMaskRecord{ObjectId::fromUint64(500),std::move(sweep)}};
 semantic::Operation op;semantic::AddEraseMasksOp payload;payload.items.push_back({object.id,object.erase_masks});op.payload=std::move(payload);require(semantic::validatePayloadStructure(op).ok(),"progress witness is not a legal AddEraseMasks payload");
 const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});require(projection.records.size()==1,"progress scene projection");
 ReferenceTraversalEntry e;e.record=projection.records.front();e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};e.presentationEraseActive=true;return e;
}
void historyProgress(bool probe){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"progress real Metal");
 for(unsigned dpr:{1U,2U,4U})for(unsigned segments:{1U,40000U}){
  const auto width=256*dpr,height=128*dpr;
  auto entry=progressSource(segments);NativeMetalTarget target(completion);require(target.resize(width,height).code==BackendSubmissionCode::kAccepted,"progress target");auto acquired=target.acquire();
  SkiaRenderCache cache;cache.budget=std::make_shared<resources::ResourceBudgetManager>();cache.sourcePool=std::make_shared<SourceRetentionPool>();cache.preparedGeometry=std::make_shared<PreparedGeometryCache>();cache.preparedGeometry->setRetentionPool(cache.sourcePool);cache.maskHistory=std::make_shared<EraseMaskPathCache>();cache.maskHistory->setRetentionPool(cache.sourcePool);
  auto body=cache.preparedGeometry->get(entry,cache.options,cache.budget,true);require(body&&body->ready(),"progress body must already be ready");
  const auto plan=renderPlan(entry,1,dpr);const CompactDeviceGrid grid{0,0,int(width),int(height)};cache.begin(plan,&target,*acquired.frame.surface,1);
  bool allDrawn=true;for(unsigned attempt=0;attempt<3;++attempt){
   auto* canvas=acquired.frame.surface->getCanvas();canvas->clear(SK_ColorBLUE);const auto before=cache.maskHistory->work();
   canvas->save();canvas->scale(dpr,dpr); // Same caller setup as drawValidatedReferenceRangeToSkCanvas.
   const auto result=cache.drawDamagedInk(*canvas,plan,entry,nullptr,nullptr,&grid);canvas->restore();const auto after=cache.maskHistory->work();
   const bool drawn=result==SkiaRenderCache::InkDraw::kDrawn;allDrawn=allDrawn&&drawn;if(drawn){target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"progress present");}else completion->context->flushAndSubmit(GrSyncCpu::kYes);
   std::cout<<nlohmann::json{{"production_caller","drawDamagedInk"},{"dpr",dpr},{"segments",segments},{"attempt",attempt},{"drawn",drawn},{"retry",result==SkiaRenderCache::InkDraw::kRetry},{"history_primitive_builds",after.history.primitiveBuilds-before.history.primitiveBuilds},{"history_denials",after.history.denied-before.history.denied},{"body_tile_builds",cache.work.bodyTileBuilds},{"gpu_tiles",cache.work.gpuCoverageTiles},{"cpu_tiles",cache.work.cpuCoverageTiles},{"present_count",target.presentCount()},{"used_bytes",cache.budget->used()}}.dump()<<'\n';
  }
  if(!probe)require(allDrawn&&target.presentCount()==3,"valid oversized optional mask history stalled production GPU draw/present after flush");
  if(!probe){
   require(cache.work.gpuCoverageTiles>0&&cache.work.cpuCoverageTiles==0,"progress escaped to CPU coverage");
   NativeMetalTarget reference(completion);require(reference.resize(width,height).code==BackendSubmissionCode::kAccepted,"progress reference target");auto ref=reference.acquire();
   require(drawReferencePlanToSkCanvas(*ref.frame.surface->getCanvas(),plan).code==BackendSubmissionCode::kAccepted,"independent progress null-cache reference");reference.release();require(reference.present().code==BackendSubmissionCode::kAccepted,"progress reference present");
   std::vector<std::uint8_t> actual(width*height*4),expected(actual.size());require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"progress pixels");
   const auto edges=independentEdges(entry,dpr);for(std::size_t i=0;i<actual.size();++i)if(!edges[i/4])require(std::abs(int(actual[i])-int(expected[i]))<=2,"progress fallback changed pixels outside original independent CPU AA band");
  }
  cache.clear();body.reset();cache.preparedGeometry->clear();cache.maskHistory.reset();completion->context->flushAndSubmit(GrSyncCpu::kYes);
 }
 }message(pool,"drain");
#else
 require(false,"progress native GPU unavailable");
#endif
}
void historyAdmission(){
 auto budget=std::make_shared<resources::ResourceBudgetManager>();auto pool=std::make_shared<SourceRetentionPool>();EraseMaskPathCache service;service.setRetentionPool(pool);MaskHistoryAdmission admission;
 auto oversized=progressSource(40000).record.eraseMasks;
 for(unsigned attempt=0;attempt<3;++attempt){const auto before=service.work();require(!service.history(oversized,budget,8*1024*1024,{},&admission)&&admission==MaskHistoryAdmission::kNotAdmitted,"permanent local refusal became a resource wait");require(service.work().history.primitiveBuilds==before.history.primitiveBuilds,"structurally impossible history prepared a prefix");}
 auto partial=progressSource(256).record.eraseMasks;const auto before=service.work();
 require(!service.history(partial,budget,128*1024,{},&admission)&&admission==MaskHistoryAdmission::kNotAdmitted,"bounded local preparation refusal");
 const auto after=service.work();require(after.history.primitiveBuilds>before.history.primitiveBuilds,"partial-preparation negative witness fixture did not exercise actual work");
 const auto* original=partial.storage().get();require(!service.history(partial,budget,128*1024,{},&admission)&&service.work().refusalHits>after.refusalHits&&service.work().history.primitiveBuilds==after.history.primitiveBuilds,"same nonresident identity rebuilt its unusable prefix");
 std::get<semantic::SweptCircleMask>(partial[0].geometry).segments.back().control2.x+=1;
 const auto known=service.work();require(partial.storage().get()!=original,"negative source pin did not preserve immutable COW identity");
 require(!service.history(partial,budget,128*1024,{},&admission)&&service.work().refusalHits==known.refusalHits,"mutated same ID/count trusted old negative proof");
 auto trimmed=partial;trimmed[0].geometry=semantic::SweptCircleMask{{semantic::EraseCubicSegment{{{80,40},2},{{80,80},2},{80,50},{80,70}}}};
 auto ready=service.history(trimmed,budget,128*1024,{},&admission);require(ready&&admission==MaskHistoryAdmission::kReady,"truncated history could not recover residency");
 service.clear();require(budget->used()>0,"held ready plan lost its lease");ready.reset();require(budget->used()==0,"negative/ready retention leaked quota");
 auto held=budget->reserve(resources::ResourceClass::kScratch,96U*1024U*1024U,true);require(bool(held),"temporary global pressure setup");
 require(!service.history(trimmed,budget,8*1024*1024,{},&admission)&&admission==MaskHistoryAdmission::kResourcePending,"temporary global refusal was cached as permanent");
 budget->release(*held);ready=service.history(trimmed,budget,8*1024*1024,{},&admission);require(ready&&admission==MaskHistoryAdmission::kReady,"temporary pressure release failed to recover");
 auto foreign=std::make_shared<resources::ResourceBudgetManager>();auto rebound=service.history(trimmed,foreign,8*1024*1024,{},&admission);require(rebound&&rebound!=ready,"foreign owner reused old admission witness");
 auto newPool=std::make_shared<SourceRetentionPool>();service.setRetentionPool(newPool);auto reset=service.history(trimmed,foreign,8*1024*1024,{},&admission);require(reset&&reset!=rebound,"pool/context reset reused prior history plan");
 ready.reset();rebound.reset();reset.reset();service.clear();require(budget->used()==0&&foreign->used()==0,"owner reset/last release leaked");
 for(unsigned i=0;i<600;++i){auto records=progressSource(256).record.eraseMasks;records[0].id=ObjectId::fromUint64(1000+i);require(!service.history(records,budget,128*1024,{},&admission)&&admission==MaskHistoryAdmission::kNotAdmitted,"local refusal stopped bounded negative progress");require(service.work().bytes<=128U*1024U,"negative witnesses exceeded original local capacity");}
 service.clear();require(budget->used()==0,"bounded refusal witnesses leaked on close");
 std::cout<<"{\"permanent_local_not_pending\":true,\"structural_prefix_builds\":0,\"partial_refusal_reused\":true,\"sameID_mutation_COW\":true,\"truncate_recovery\":true,\"global_release_recovery\":true,\"owner_pool_reset\":true,\"bounded_refusals\":600,\"last_release_bytes\":0}\n";
}
void historyProgressSafety(){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"safety Metal context");
 bool layerFailure=false,callerFailure=false;
 for(bool caller:{false,true})for(unsigned kib=128;kib<=1024;kib+=16){
  auto entry=progressSource(1);NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"safety target");auto acquired=target.acquire();auto* canvas=acquired.frame.surface->getCanvas();
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{kib*1024,kib*1024});auto sources=std::make_shared<SourceRetentionPool>();auto prepared=std::make_shared<PreparedGeometryCache>();prepared->setRetentionPool(sources);RenderCacheOptions options;
  auto body=prepared->get(entry,options,budget,true);require(body&&body->ready(),"safety ready body");auto history=std::make_shared<EraseMaskPathCache>();history->setRetentionPool(sources);
  SkiaInkLayers layers;CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork work;bool painted=false,pending=false;
  SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=sources;cache.preparedGeometry=prepared;cache.maskHistory=history;const auto plan=renderPlan(entry,1,1);const CompactDeviceGrid grid{0,0,256,128};if(caller)cache.begin(plan,&target,*acquired.frame.surface,1);
  auto epoch=budget->beginGpuEpoch();require(bool(epoch),"safety GPU epoch");canvas->clear(SK_ColorBLUE);bool observed=false;
  if(caller){const auto result=cache.drawDamagedInk(*canvas,plan,entry,nullptr,nullptr,&grid);
   observed=result==SkiaRenderCache::InkDraw::kRetry&&cache.work.maskTileUpdates>0;
   if(observed){require(cache.work.directGroups==0&&cache.work.cpuCoverageTiles==0&&target.presentCount()==0,"painted attempt escaped through Direct/CPU/present");callerFailure=true;}
  }else {const auto drawn=layers.draw(*canvas,*acquired.frame.surface,entry,SkMatrix::I(),SkIRect::MakeWH(256,128),nullptr,options,budget,sources,targets,work,painted,pending,prepared,body,history);
   observed=!drawn&&painted&&pending;if(observed)layerFailure=true;
  }
  cache.clear();layers.clear();history->clear();targets.clear();body.reset();prepared->clear();require(budget->endGpuEpoch(*epoch),"safety end GPU epoch");completion->context->flushAndSubmit(GrSyncCpu::kYes);require(budget->completeGpuEpoch(*epoch)&&budget->used()==0,"safety deferred/last release leak");
  if(observed){std::cout<<nlohmann::json{{"caller",caller},{"painted_failure",true},{"budget_kib",kib},{"direct_replay",false},{"provider_present",0},{"retired_bytes",0}}.dump()<<'\n';break;}
 }
 require(layerFailure&&callerFailure,"GPU safety fixture did not reach a failure after an actual partial composite");
 }message(pool,"drain");
#else
 require(false,"safety native GPU unavailable");
#endif
}
}
int main(int argc,char** argv){try{const auto mode=argc>1?std::string_view(argv[1]):"--reuse";if(mode=="--progress"||mode=="--progress-probe")historyProgress(mode=="--progress-probe");else if(mode=="--admission")historyAdmission();else if(mode=="--progress-safety")historyProgressSafety();else if(mode=="--invalidation"){historyStorageAndLeases(false);historyPixels();}else if(mode=="--lease")historyStorageAndLeases(true);else if(mode=="--exact"){historyStorageAndLeases(false);historyProbe(false);}else historyProbe(mode=="--probe");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
