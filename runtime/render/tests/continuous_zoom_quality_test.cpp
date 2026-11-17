#define main continuousZoomPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/skia_navigation_tiles.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/frame_plan.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <filesystem>
#include <thread>
#include "canvas/scene/scene.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "navigation-sharing-serialize.hpp"
#include "../src/continuous_zoom_observation.hpp"
using nlohmann::json;
namespace canvas::render::internal {
struct ContinuousZoomProbe {
 static json policyState(const SkiaNavigationTiles&n){return {{"displayLast",n.displayLastUs_},{"displayEWMA",n.displayEwmaUs_},{"quantumEWMA",n.quantumEwmaUs_},{"qualityAge",n.qualityAge_},{"demands",n.qualityDemands_.size()},{"tiles",n.tiles_.size()},{"job",bool(n.job_)},{"suspended",bool(n.refinement_)}};}
 static void exposeFrontier(SkiaNavigationTiles&n,const foundation::WorldRect&visible){
  const foundation::WorldRect strip{visible.right-1,visible.top,visible.right,visible.bottom};
  for(auto&[key,t]:n.tiles_)if(t.valid.subtract(strip))n.dirty_=true;
 }
 static std::size_t missingBefore(SkiaNavigationTiles&n,const foundation::WorldRect&r){std::vector<SkiaNavigationTiles::Patch>p;std::vector<foundation::WorldRect>m;n.coverage(r,1,p,nullptr,&m);return m.size();}
 static void forceFairFrontier(SkiaNavigationTiles&n){
  n.job_.reset();n.refinement_.reset();n.tiles_.clear();n.sources_.clear();
  for(auto&[key,d]:n.qualityDemands_)d.age=11;
 }
 static void preferActualCachedCandidate(SkiaNavigationTiles&n){for(auto&[key,d]:n.qualityDemands_)d.age=n.sources_.contains(key)&&!n.tiles_.contains(key)?12:0;}
 static void feasibleCosts(SkiaNavigationTiles&n){for(auto&c:n.displayCosts_)c.observe(20);for(auto&c:n.checkpointCosts_)c.observe(1);n.displayEstimateValid_=true;}
 static void expensivePublication(SkiaNavigationTiles&n){n.displayEstimateValid_=true;n.displayLastUs_=n.displayEwmaUs_=250;n.quantumEwmaUs_=1800;n.checkpointCosts_[unsigned(SkiaNavigationTiles::Checkpoint::Publish)].observe(1800);for(auto&v:n.displayCosts_)v.observe(250);}
 static double availableDebt(SkiaNavigationTiles&n,const foundation::WorldRect&r,double device){std::vector<SkiaNavigationTiles::Patch>patches;if(!n.coverage(r,1,patches,nullptr))return -1;double sum=0,area=0;for(const auto&p:patches){if(p.plainBackground)continue;double a=double(p.area.right-p.area.left)*(p.area.bottom-p.area.top);sum+=a*std::max(0.,device/p.density/1.25-1);area+=a;}return area?sum/area:0;}
 static json scheduling(const SkiaNavigationTiles&n){json a=json::array();for(const auto&[key,d]:n.qualityDemands_)a.push_back({{"key",{key.x,key.y,key.scale,key.region}},{"age",d.age},{"seen",d.seen},{"cost_us",d.costUs}});return{{"demands",a},{"display_last_us",n.displayLastUs_},{"display_ewma_us",n.displayEwmaUs_},{"quantum_ewma_us",n.quantumEwmaUs_}};}
 static auto jobs(const SkiaNavigationTiles& n){json a=json::array();for(const auto*j:{n.job_.get(),n.refinement_.get()})if(j)a.push_back({{"id",j->id},{"level",j->key.scale},{"region",j->key.region},{"source",j->source},{"primitive",j->primitive},{"knot",j->knot},{"steps",j->steps},{"prewarm",j->prewarm}});return a;}
};
}
namespace {
#if defined(__APPLE__)
class ZoomTarget final:public SkiaSurfaceProvider {
 NativeMetalTarget actual_;std::shared_ptr<NativeMetalCompletion> completion_;std::uint64_t present_=0;
 public:
 explicit ZoomTarget(std::shared_ptr<NativeMetalCompletion> c):actual_(c),completion_(std::move(c)){}
 std::shared_ptr<GpuCompletionService>gpuCompletionService()const noexcept override{return completion_;}RenderTargetInfo describe()const noexcept override{return actual_.describe();}SkiaSurfaceAcquireResult acquire()noexcept override{return actual_.acquire();}void release()noexcept override{actual_.release();}
 BackendSubmissionResult present()noexcept override{completion_->context->flushAndSubmit(GrSyncCpu::kNo);++present_;return BackendSubmissionResult::accepted();}BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return actual_.resize(w,h);}BackendSubmissionResult readbackRgba(std::span<std::uint8_t>b)noexcept override{return actual_.readbackRgba(b);}std::uint64_t generation()const noexcept override{return actual_.generation();}BackendSubmissionResult advanceGeneration()noexcept override{return actual_.advanceGeneration();}std::uint64_t readbackCount()const noexcept override{return actual_.readbackCount();}std::uint64_t cpuCopyCount()const noexcept override{return actual_.cpuCopyCount();}std::uint64_t presentCount()const noexcept override{return present_;}
};
FrameState zoomFrame(unsigned dpr,unsigned generation=1){return {ViewId{1},CameraState{{915.5,625},1,0,CameraGeneration{generation}},{0,0,1431,1010},{1431,1010,1431*dpr,1010*dpr,float(dpr),1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{generation},{0,0,1431,1010}};}
std::vector<ReferenceTraversalEntry> sparseScene(){
 std::vector<semantic::ObjectRecord> records;
 for(unsigned n=0;n<3;++n){semantic::ObjectRecord object;object.id=ObjectId::fromUint64(200+n);object.kind=semantic::ObjectKind::kShape;object.kind_version=1;object.content=semantic::ShapeContent{n==2?2U:1U,180,100};object.transform.tx=650+n*200;object.transform.ty=380+n*120;object.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.15F,.2F,.55F,1}}}}};records.push_back(object);}
 auto projected=projectRuntimeScene(records,semantic::SemanticGeneration{1});std::vector<ReferenceTraversalEntry> entries;for(auto& e:projected.records)entries.push_back({e,true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(e.content.get())}});auto stroke=source();auto content=std::get<CompactStrokeReferenceCommand>(stroke.command).content.get();content.stroke.config.size=8;for(unsigned i=0;i<content.stroke.knots.size();++i){auto& k=content.stroke.knots[i];k.x=500+i*.12;k.y=650+80*std::sin(i*.012);}semantic::ObjectRecord object;object.id=ObjectId::fromUint64(500);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=content;stroke.record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();stroke.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(stroke.record.content)};entries.push_back(stroke);return entries;
}
json inspectSnapshot(const std::string& file){std::ifstream input(file,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};auto s=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(s.ok(),"fixed Mixed49 decode");auto projection=projectRuntimeScene(s.snapshot->objects,semantic::SemanticGeneration{1});json out=json::array();for(const auto&r:projection.records){auto b=r.worldBounds;out.push_back({{"id",std::to_string(out.size())},{"kind",unsigned(r.kind)},{"bounds",{b.left,b.top,b.right,b.bottom}}});}return out;}
CanonicalViewportTransform curve(unsigned i){
 const unsigned segment=i/60,within=i%60+1,cycle=segment/3;const double starts[]={1,2,.5},ends[]={2,.5,1.5};
 const auto part=segment%3;const double scale=std::pow(1.5,double(cycle))*starts[part]*std::pow(ends[part]/starts[part],within/60.);
 return {float(scale),float(715.5-(715.5+200)*scale),float(505-(505+120)*scale)};
}
json dynamic(unsigned dpr,std::shared_ptr<NativeMetalCompletion>c){
 ZoomTarget target(c);require(target.resize(1431*dpr,1010*dpr).code==BackendSubmissionCode::kAccepted,"dynamic target");auto entries=sparseScene();auto geometry=std::make_shared<PreparedGeometryCache>();RenderCacheOptions options;options.maxBytes=512U*1024U*1024U;options.sharedMaxBytes=1024U*1024U*1024U;options.maxEntryBytes=128U*1024U*1024U;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});
 for(const auto&e:entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){auto body=geometry->request(e,options,b);require(bool(body),"ready body admission");for(unsigned n=0;!body->ready();++n){PreparationAllowance a;geometry->advance(body,a);require(n<2048&&!body->failed(),"ready body preparation");}}
 Scene authoritative(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>());std::vector<SceneRecord> sceneRecords;for(unsigned i=0;i<entries.size();++i){const auto&e=entries[i];sceneRecords.push_back(SceneRecord{.objectId=e.record.objectId,.orderKey=SceneOrderKey(i+1),.flags=SceneRecordFlags::kVisible,.worldBounds=e.record.worldBounds,.contentRevision=ContentRevision{1},.renderPayload={i,1},.hitGeometry={i,1}});}require(bool(authoritative.replace(CompiledSceneSnapshot{SceneRevision{1},sceneRecords})),"authoritative nativeScene");
 NavigationSourceQuery q=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out,NavigationSourceDiagnostics&diag){auto raw=authoritative.query(SceneQuery{r});if(!raw||raw.value().revision!=SceneRevision{1})return false;diag.sceneStamped=true;diag.semanticGeneration=1;diag.sceneRevision=1;if(raw.value().backToFront.empty())diag.sceneEmptyProof=NavigationSceneEmptyProof{r,1,1,1,1,1};for(const auto id:raw.value().backToFront){auto it=std::find_if(entries.begin(),entries.end(),[&](const auto&e){return e.record.objectId==id;});require(it!=entries.end(),"Sceneidmissing");out.push_back(*it);}return true;};SkiaNavigationTiles nav;
 using namespace zoom_observation;Quality shown{};CanonicalViewportTransform shownCamera{};bool observed=false;json rows=json::array();observer=[&](const Present&p){shown=p.quality;shownCamera=p.camera;observed=true;};
 ns::contentCount=0;for(const auto&e:entries)if(ns::contentCount<ns::content.size())ns::content[ns::contentCount++]=ns::rect(e.record.worldBounds);zoom_observation::contentIntersection=[](float l,float t,float r,float b){return ns::contentArea({l,t,r,b});};
 auto initial=zoomFrame(dpr);for(unsigned n=0;;++n){require(nav.render(target,{1,-200,-120},initial,q,1,options,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry,0xffffffffU,{},true).code==BackendSubmissionCode::kAccepted,"dynamic warm");c->context->checkAsyncWorkCompletion();if(!nav.work.pending)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));if(n==8191)std::cerr<<ContinuousZoomProbe::policyState(nav).dump()<<std::endl;require(n<8192,"dynamic warm stalled");}
 double debt=0,severe=0,low=0,staleSeconds=0,contentDebt=0,contentSevere=0,contentLow=0;std::size_t peak=0;unsigned stale=0;
 const auto start=std::chrono::steady_clock::now();auto accounted=start;CanonicalViewportTransform requested{1,-200,-120};json intervals=json::array();
 const auto integrate=[&](auto end){const double dt=std::chrono::duration<double>(end-accounted).count();const bool current=shownCamera.scale==requested.scale&&shownCamera.translationX==requested.translationX&&shownCamera.translationY==requested.translationY;debt+=shown.debt*dt;severe+=shown.severe*dt;low+=shown.low*dt;contentDebt+=shown.contentDebt*dt;contentSevere+=shown.contentSevere*dt;contentLow+=shown.contentLow*dt;if(!current)staleSeconds+=dt;intervals.push_back({{"start_ms",std::chrono::duration<double,std::milli>(accounted-start).count()},{"end_ms",std::chrono::duration<double,std::milli>(end-start).count()},{"displayed_camera",{shownCamera.scale,shownCamera.translationX,shownCamera.translationY}},{"requested_camera",{requested.scale,requested.translationX,requested.translationY}},{"debt",shown.debt},{"severe",shown.severe},{"low",shown.low},{"content_debt",shown.contentDebt},{"content_severe",shown.contentSevere},{"content_low",shown.contentLow},{"current",current}});accounted=end;};
 observer=[&](const Present&p){integrate(std::chrono::steady_clock::now());shown=p.quality;shownCamera=p.camera;observed=true;};
 ns::reset();ns::contentCount=0;for(const auto&e:entries){if(ns::contentCount<ns::content.size())ns::content[ns::contentCount++]=ns::rect(e.record.worldBounds);}
 for(unsigned i=0;i<360;++i){const auto deadline=start+std::chrono::microseconds((i+1)*1000000/60);std::this_thread::sleep_until(deadline);integrate(std::chrono::steady_clock::now());auto camera=curve(i);requested=camera;auto f=zoomFrame(dpr,i+2);auto previous=ContinuousZoomProbe::jobs(nav);auto before=nav.work;
  require(nav.render(target,camera,f,q,1,options,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry,0xffffffffU,{},true).code==BackendSubmissionCode::kAccepted,"dynamic navigation");if(ns::state.count){auto&r=ns::state.rows[ns::state.count-1];r.planned=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline.time_since_epoch()).count();r.actual=r.begin;}
  c->context->checkAsyncWorkCompletion();require(observed,"no actual qualified present");const bool current=shownCamera.scale==camera.scale&&shownCamera.translationX==camera.translationX&&shownCamera.translationY==camera.translationY;stale+=!current;
  integrate(std::chrono::steady_clock::now());peak=std::max(peak,b->used());rows.push_back({{"index",i+1},{"camera",{camera.scale,camera.translationX,camera.translationY}},{"displayed",{shownCamera.scale,shownCamera.translationX,shownCamera.translationY}},{"current",current},{"debt",shown.debt},{"severe",shown.severe},{"low",shown.low},{"maximum",shown.maximum},{"content_debt",shown.contentDebt},{"content_severe",shown.contentSevere},{"content_low",shown.contentLow},{"content_maximum",shown.contentMaximum},{"scheduling",ContinuousZoomProbe::scheduling(nav)},{"presented",nav.work.presented},{"built",nav.work.tilesBuilt-before.tilesBuilt},{"steps",nav.work.knotSteps-before.knotSteps},{"before_jobs",previous},{"jobs",ContinuousZoomProbe::jobs(nav)},{"throttled",nav.work.gpuThrottles-before.gpuThrottles},{"used",b->used()},{"at_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}});
 }
 integrate(std::chrono::steady_clock::now());observer={};ns::state.enabled=false;auto trace=ns::exportRows();return {{"decision_trace",trace},{"intervals",intervals},{"actual_input_window_ms",std::chrono::duration<double,std::milli>(accounted-start).count()},{"stale_seconds",staleSeconds},{"DPR",dpr},{"route","sparse-ready-cold-frontier"},{"rows",rows},{"integrated_content_debt_seconds",contentDebt},{"content_severe_area_seconds",contentSevere},{"content_low_area_seconds",contentLow},{"integrated_debt_seconds",debt},{"severe_area_seconds",severe},{"low_area_seconds",low},{"stale_opportunities",stale},{"peak_charged",peak}};
}
json density(unsigned dpr,std::shared_ptr<NativeMetalCompletion> c){
 ZoomTarget target(c);require(target.resize(1431*dpr,1010*dpr).code==BackendSubmissionCode::kAccepted,"native zoom target");auto entries=sparseScene();auto f=zoomFrame(dpr);SkiaNavigationTiles nav;RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});NavigationSourceQuery q=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>&out){out=entries;return true;};json rows=json::array();
 for(unsigned n=0;;++n){auto r=nav.render(target,{1,-200,-120},f,q,1,options,b);if(n==8191)std::cerr<<ContinuousZoomProbe::policyState(nav).dump()<<std::endl;if(r.code!=BackendSubmissionCode::kAccepted)std::cerr<<"density code="<<unsigned(r.code)<<" reason="<<r.message<<" at="<<n<<std::endl;require(r.code==BackendSubmissionCode::kAccepted&&n<8192,"density convergence");if(nav.work.presented)rows.push_back({{"opportunity",n},{"upscale",nav.work.maximumUpscale},{"low_fraction",nav.work.lowResolutionFraction},{"level",nav.work.selectedLevel},{"tiles",nav.work.tilesBuilt},{"used",b->used()}});if(!nav.work.pending)break;c->context->checkAsyncWorkCompletion();}
 json result={{"DPR",dpr},{"presented",rows},{"final_upscale",nav.work.maximumUpscale},{"final_low",nav.work.lowResolutionFraction},{"coverage_complete",nav.work.coverageComplete},{"charged",b->used()},{"tiles",nav.work.tilesBuilt}};std::cout<<result.dump()<<'\n';return result;
}
#endif
}

#if defined(__APPLE__)
json continuationCase(std::shared_ptr<NativeMetalCompletion>c,unsigned allowance,std::string mode="normal"){
 ZoomTarget target(c);require(target.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"continuationtarget");auto entries=sparseScene();RenderCacheOptions o;o.maxBytes=512U*1024U*1024U;o.sharedMaxBytes=1024U*1024U*1024U;o.maxEntryBytes=128U*1024U*1024U;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{o.maxBytes,o.sharedMaxBytes});auto geometry=std::make_shared<PreparedGeometryCache>();
 for(const auto&e:entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){auto body=geometry->request(e,o,b);while(!body->ready()){PreparationAllowance a;geometry->advance(body,a);require(!body->failed(),"continuationready");}}
 NavigationSourceQuery q=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out){for(const auto&e:entries){const auto&w=e.record.worldBounds;if(w.left<r.right&&w.right>r.left&&w.top<r.bottom&&w.bottom>r.top)out.push_back(e);}return true;};SkiaNavigationTiles nav;auto clock=std::chrono::steady_clock::time_point{};ns::clock=[&]{return clock;};ns::gateHook=[&](unsigned){clock+=std::chrono::microseconds(1);};zoom_observation::Quality shown{};zoom_observation::observer=[&](const auto&p){shown=p.quality;};
 for(unsigned i=0;!nav.work.coverageComplete;++i){clock={};require(i<4096,"continuationbootstrap");require(nav.render(target,{1,-200,-120},zoomFrame(2),q,1,o,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry).code==BackendSubmissionCode::kAccepted,"continuationbootstraprender");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();}
 ContinuousZoomProbe::feasibleCosts(nav);
 if(mode=="cached"||mode=="active"){
   o.navigationMaxSteps=1;o.navigationMaxQueries=1;clock={};
   require(nav.render(target,{1,-200,-120},zoomFrame(2),q,1,o,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry).code==BackendSubmissionCode::kAccepted,"primequalifiedsource");
   c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();
   require(!ContinuousZoomProbe::jobs(nav).empty(),"primeactivejobmissing");
   if(mode=="cached"){ContinuousZoomProbe::preferActualCachedCandidate(nav);nav.cancelWork();c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();}
   o.navigationMaxSteps=64;
 }
 o.navigationMaxQueries=allowance;
 if(mode=="deadline")o.navigationMicroseconds=0;
 if(mode=="step")o.navigationMaxSteps=1;
 if(mode=="gpu"){o.navigationMaxGpuCoarseContourPrimitives=1;o.navigationMaxGpuContourPrimitives=1;}
const auto before=nav.work;const auto oldDebt=shown.debt;const auto presents=target.presentCount();clock={};ns::reset();require(nav.render(target,{1,-200,-120},zoomFrame(2),q,1,o,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry).code==BackendSubmissionCode::kAccepted,"continuationrender");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();ns::state.enabled=false;auto trace=ns::exportRows();ns::clock={};ns::gateHook={};zoom_observation::observer={};const auto&row=trace["rows"][0];
 auto result=json{{"allowance",allowance},{"mode",mode},{"virtual_clock_not_performance",true},{"real_Metal",true},{"built",nav.work.tilesBuilt-before.tilesBuilt},{"content_publications",row["content_publications"]},{"content_keys",row["content_keys"]},{"queries",row["queries"]},{"initializations",row["inits"]},{"steps",row["steps"]},{"gpu_primitives",row["gpu_primitives"]},{"old_debt",oldDebt},{"displayed_debt",shown.debt},{"present_delta",target.presentCount()-presents},{"pending",nav.work.pending},{"trace",trace}};std::cout<<result.dump()<<std::endl;
 require(row["queries"].get<unsigned>()<=allowance&&row["inits"].get<unsigned>()<=std::max(1U,allowance),"QUERY_OR_SETUP_ALLOWANCE_EXCEEDED");
 require(row["steps"].get<unsigned>()<=o.navigationMaxSteps&&row["gpu_primitives"].get<unsigned>()<=std::max(o.navigationMaxGpuContourPrimitives,o.navigationMaxGpuCoarseContourPrimitives),"STEP_GPU_CAP_EXCEEDED");
 if(mode=="normal"&&allowance>=2){require(row["content_publications"].get<unsigned>()>=2,"QUALITY_SINGLE_CHOICE_PREMATURE_NOELIGIBLE");require(shown.debt<oldDebt&&target.presentCount()==presents+1,"MULTIPLE_REGION_DID_NOT_REACH_SAME_PRESENT");}
 if(mode=="deadline")require(row["queries"]==0&&row["steps"]==0,"EXPIRED_DEADLINE_DID_WORK");
 if(mode=="cached"||mode=="active")require(row["queries"]==0&&row["steps"].get<unsigned>()>0,"QUALIFIED_ZERO_QUERY_WORK_STARVED");
 if(mode=="active")require(row["inits"]==0,"ACTIVE_RESUME_CHARGED_NEW_SETUP");
 return result;
}
json targetAndStreaming(std::shared_ptr<NativeMetalCompletion> c){
 ZoomTarget target(c);require(target.resize(2862,4040).code==BackendSubmissionCode::kAccepted,"anisotropictarget");auto f=zoomFrame(2);FrameState anisotropic{f.viewId,f.camera,f.worldViewport,{1431,1010,2862,4040,2,1},f.sceneGeneration,f.sceneReadToken,f.surfaceGeneration,f.metricsGeneration,f.frameId,f.viewportClip};
 SkiaNavigationTiles nav;RenderCacheOptions o;o.maxBytes=512U*1024U*1024U;o.sharedMaxBytes=1024U*1024U*1024U;o.maxEntryBytes=128U*1024U*1024U;o.maxEntries=8;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{o.maxBytes,o.sharedMaxBytes});
 NavigationSourceQuery empty=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&,NavigationSourceDiagnostics&d){d.sceneStamped=true;d.sceneRevision=1;d.semanticGeneration=1;d.sceneEmptyProof=NavigationSceneEmptyProof{r,1,1,1,1,1};return true;};
 const auto result=nav.render(target,{1,-200,-120},anisotropic,empty,1,o,b,false,nullptr,nullptr,nullptr,{}, {},{}, {},0xffffffffU,{},true);
 require(result.code==BackendSubmissionCode::kAccepted,"TARGET_DENSITY_REJECTED_BY_RESIDENT_CAPACITY");require(nav.work.selectedLevel==2,"DPR_MAX_AXIS_NOT_USED");
 TileGrid grid;auto full=grid.coverage({0,0,20000,20000},4,8);require(full.empty(),"streamtestmustexceedfullcapacity");auto first=grid.coverageWindow({0,0,20000,20000},4,8,0),next=grid.coverageWindow({0,0,20000,20000},4,8,8);require(first.size()==8&&next.size()==8&&first.back()<next.front(),"FINITE_STREAM_WINDOW_NOT_ADVANCED");
 return{{"selected_level",nav.work.selectedLevel},{"physical_ratios",{2,4}},{"resident_entries",8},{"desired_grid_exceeds_capacity",true},{"bounded_windows_advance",true}};
}
json staticTarget(std::shared_ptr<NativeMetalCompletion>c){
 json rows=json::array();for(unsigned dpr:{2U,4U}){ZoomTarget target(c);require(target.resize(1431*dpr,1010*dpr).code==BackendSubmissionCode::kAccepted,"statictarget");auto entries=sparseScene();RenderCacheOptions o;o.maxBytes=512U*1024U*1024U;o.sharedMaxBytes=1024U*1024U*1024U;o.maxEntryBytes=128U*1024U*1024U;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{o.maxBytes,o.sharedMaxBytes});auto geometry=std::make_shared<PreparedGeometryCache>();for(const auto&e:entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){auto body=geometry->request(e,o,b);while(!body->ready()){PreparationAllowance a;geometry->advance(body,a);require(!body->failed(),"staticreadybody");}}
 NavigationSourceQuery q=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out,NavigationSourceDiagnostics&d){for(const auto&e:entries){const auto&w=e.record.worldBounds;if(w.left<r.right&&w.right>r.left&&w.top<r.bottom&&w.bottom>r.top)out.push_back(e);}d.sceneStamped=true;d.semanticGeneration=1;d.sceneRevision=1;if(out.empty())d.sceneEmptyProof=NavigationSceneEmptyProof{r,1,1,1,1,1};return true;};SkiaNavigationTiles nav;auto clock=std::chrono::steady_clock::time_point{};ns::clock=[&]{return clock;};ns::gateHook=[&](unsigned){clock+=std::chrono::microseconds(25);};zoom_observation::observer=[](const auto&){};unsigned calls=0;
 for(;;){clock={};require(nav.render(target,{1,-200,-120},zoomFrame(dpr),q,1,o,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry,0xffffffffU,{},true).code==BackendSubmissionCode::kAccepted,"staticrender");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();++calls;if(!nav.work.pending)break;require(calls<8192,"STATIC_TARGET_DID_NOT_CONVERGE");}
 require(nav.work.maximumUpscale<=1.25,"STATIC_TARGET_DENSITY_DOWNGRADED");rows.push_back({{"DPR",dpr},{"max_upscale",nav.work.maximumUpscale},{"calls",calls},{"budget",b->used()}});ns::clock={};ns::gateHook={};zoom_observation::observer={};}return{{"virtual_clock_not_latency",true},{"rows",rows}};
}
json comparableCostRED(std::shared_ptr<NativeMetalCompletion> c){
 ZoomTarget target(c);require(target.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"costtarget");auto e=sparseScene();SkiaNavigationTiles nav;RenderCacheOptions options;options.maxBytes=512U*1024U*1024U;options.sharedMaxBytes=1024U*1024U*1024U;options.maxEntryBytes=128U*1024U*1024U;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});auto geometry=std::make_shared<PreparedGeometryCache>();
 for(const auto&v:e)if(std::holds_alternative<CompactStrokeReferenceCommand>(v.command)){auto body=geometry->request(v,options,b);require(bool(body),"costbody");while(!body->ready()){PreparationAllowance a;geometry->advance(body,a);require(!body->failed(),"costbodyready");}}
 NavigationSourceQuery query=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out){for(const auto&v:e){const auto&w=v.record.worldBounds;if(w.left<r.right&&w.right>r.left&&w.top<r.bottom&&w.bottom>r.top)out.push_back(v);}return true;};auto frame=zoomFrame(2);const CanonicalViewportTransform camera{1,-200,-120};
 for(unsigned i=0;!nav.work.coverageComplete;++i){require(i<4096,"costbootstrap");require(nav.render(target,camera,frame,query,1,options,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry).code==BackendSubmissionCode::kAccepted,"costwarm");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();}
 require(nav.work.maximumUpscale>1.25,"costtestneedsactualqualitydemand");ContinuousZoomProbe::expensivePublication(nav);const auto before=nav.work.sourceSteps+nav.work.knotSteps+nav.work.tilesBuilt;auto clock=std::chrono::steady_clock::time_point{};ns::clock=[&]{return clock;};ns::gateHook=[&](unsigned){clock+=std::chrono::microseconds(50);};json rows=json::array();ns::reset();zoom_observation::observer=[](const auto&){};
 for(unsigned i=0;i<12;++i){clock={};const auto result=nav.render(target,camera,frame,query,1,options,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry);require(result.code==BackendSubmissionCode::kAccepted,"costrender");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();rows.push_back({{"index",i},{"useful",nav.work.sourceSteps+nav.work.knotSteps+nav.work.tilesBuilt-before},{"scheduling",ContinuousZoomProbe::policyState(nav)}});}
 ns::clock={};ns::gateHook={};zoom_observation::observer={};ns::state.enabled=false;auto result=json{{"virtual_clock_not_performance",true},{"real_Metal_jobs",true},{"rows",rows},{"trace",ns::exportRows()}};std::cout<<result.dump()<<std::endl;require(nav.work.sourceSteps+nav.work.knotSteps+nav.work.tilesBuilt>before,"EXPENSIVE_PUBLICATION_BLOCKS_UNRELATED_REAL_QUALITY_QUANTA");return result;
}
json frontierProgress(std::shared_ptr<NativeMetalCompletion> c){
 ZoomTarget target(c);require(target.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"fairtarget");auto e=sparseScene();SkiaNavigationTiles nav;RenderCacheOptions options;options.maxBytes=512U*1024U*1024U;options.sharedMaxBytes=1024U*1024U*1024U;options.maxEntryBytes=128U*1024U*1024U;auto b=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});auto geometry=std::make_shared<PreparedGeometryCache>();
 for(const auto&v:e)if(std::holds_alternative<CompactStrokeReferenceCommand>(v.command)){auto body=geometry->request(v,options,b);require(bool(body),"fairbody");while(!body->ready()){PreparationAllowance a;geometry->advance(body,a);require(!body->failed(),"fairbodyready");}}
 NavigationSourceQuery query=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out){for(const auto&v:e){const auto&w=v.record.worldBounds;if(w.left<r.right&&w.right>r.left&&w.top<r.bottom&&w.bottom>r.top)out.push_back(v);}return true;};auto frame=zoomFrame(2);CanonicalViewportTransform camera{2,-800,-800};
 for(unsigned i=0;!nav.work.coverageComplete;++i){require(i<4096,"fairbootstrap");require(nav.render(target,camera,frame,query,1,options,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry).code==BackendSubmissionCode::kAccepted,"fairwarm");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();}
ns::contentCount=0;for(const auto&v:e)ns::content[ns::contentCount++]=ns::rect(v.record.worldBounds);auto clock=std::chrono::steady_clock::time_point{};ns::clock=[&]{return clock;};ns::gateHook=[&](unsigned){clock+=std::chrono::microseconds(25);};ns::reset();zoom_observation::observer=[](const auto&){};
 unsigned exposed=0;for(unsigned i=0;i<60;++i){clock={};camera.translationX=float(-800-i*.5);ContinuousZoomProbe::exposeFrontier(nav,{-camera.translationX/2,400,(1431-camera.translationX)/2,905});exposed+=ContinuousZoomProbe::missingBefore(nav,{-camera.translationX/2,400,(1431-camera.translationX)/2,905})>0;
  require(nav.render(target,camera,frame,query,1,options,b,false,nullptr,nullptr,nullptr,{}, {},{},geometry).code==BackendSubmissionCode::kAccepted,"fairrender");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();
 }
 ns::clock={};ns::gateHook={};zoom_observation::observer={};ns::state.enabled=false;auto trace=ns::exportRows();unsigned windows=0,failures=0;const auto&rows=trace["rows"];
 for(unsigned first=0;first+12<=rows.size();++first){bool applicable=true;std::uint64_t useful=0;for(unsigned i=first;i<first+12;++i){const auto&r=rows[i];applicable&=r["content_low_area"].get<double>()>0;useful+=r["quality_advances"].get<std::uint64_t>();}if(applicable){++windows;failures+=useful==0;}}
 auto result=json{{"virtual_clock_not_latency",true},{"actual_jobs",true},{"exposed_frontier_before_calls",exposed},{"sliding_windows",windows},{"zero_progress_windows",failures},{"trace",trace}};std::cout<<result.dump()<<std::endl;require(exposed==60,"NO_REPEATED_FRONTIER");require(windows>0,"FAIRNESS_ORACLE_VACUOUS");require(failures==0,"VISIBLE_QUALITY_STARVED_BY_REPEATED_FRONTIER");return result;
}
json samePresentationRED(std::shared_ptr<NativeMetalCompletion> c){
 ZoomTarget target(c);require(target.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"handofftarget");auto entries=sparseScene();auto geometry=std::make_shared<PreparedGeometryCache>();RenderCacheOptions options;options.maxBytes=512U*1024U*1024U;options.sharedMaxBytes=1024U*1024U*1024U;options.maxEntryBytes=128U*1024U*1024U;auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});
 for(const auto&e:entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){auto body=geometry->request(e,options,budget);require(bool(body),"handoffbody");while(!body->ready()){PreparationAllowance q;geometry->advance(body,q);require(!body->failed(),"handoffbodyready");}}
 NavigationSourceQuery query=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out){for(const auto&e:entries){const auto&b=e.record.worldBounds;if(b.left<r.right&&b.right>r.left&&b.top<r.bottom&&b.bottom>r.top)out.push_back(e);}return true;};SkiaNavigationTiles nav;zoom_observation::Quality shown{};bool present=false;zoom_observation::observer=[&](const auto&p){shown=p.quality;present=true;};json attempts=json::array();bool witnessed=false,positive=false;auto start=std::chrono::steady_clock::time_point{};
 // Native-only controllable monotonicclock; each actual gated checkpoint costs
 // 100us of deterministic virtual budget. Pixel/provider/job work stays real.
 std::chrono::steady_clock::time_point clock=start;ns::clock=[&]{return clock;};ns::gateHook=[&](unsigned){clock+=std::chrono::microseconds(100);};
 auto f=zoomFrame(2);
 for(unsigned i=0;i<800;++i){clock=start;ns::state.enabled=false;present=false;auto before=nav.work.tilesBuilt;const double old=shown.debt;const auto camera=CanonicalViewportTransform{1,float(-200-i*.001),-120};const double beforeAvailable=ContinuousZoomProbe::availableDebt(nav,{float(-camera.translationX),120,float(1431-camera.translationX),1130},2.);const auto result=nav.render(target,camera,f,query,1,options,budget,false,nullptr,nullptr,nullptr,{}, {},{},geometry,0xffffffffU,{},true);require(result.code==BackendSubmissionCode::kAccepted,"handoffrender");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();
  const double available=ContinuousZoomProbe::availableDebt(nav,{float(-camera.translationX),120,float(1431-camera.translationX),1130},2.);
  if(i<12||nav.work.tilesBuilt>before)attempts.push_back({{"debug_invocation",i},{"built",nav.work.tilesBuilt},{"new_regions",nav.work.tilesBuilt-before},{"debt_shown",shown.debt},{"debt_available",available},{"present",present},{"level",nav.work.selectedLevel},{"pending",nav.work.pending}});
  if(present&&beforeAvailable>=0&&available>=0&&available+1e-7<beforeAvailable&&nav.work.tilesBuilt>before&&std::abs(available-shown.debt)<1e-6){positive=true;attempts.push_back({{"positive_same_present",true},{"before_available_debt",beforeAvailable},{"after_displayed_debt",shown.debt},{"new_regions",nav.work.tilesBuilt-before}});break;}
  if(present&&nav.work.tilesBuilt>before&&available>=0&&available+1e-7<shown.debt){witnessed=true;attempts.push_back({{"invocation",i},{"completed_regions",nav.work.tilesBuilt-before},{"displayed_debt_before",old},{"displayed_debt_after",shown.debt},{"available_completed_debt",available},{"pending",nav.work.pending}});break;}
  require(target.presentCount()<=i+1,"morethanonepresent");
 }
 ns::clock={};ns::gateHook={};zoom_observation::observer={};json out={{"real_Metal",true},{"deterministic_clock_only_not_latency",true},{"same_presentation_refinement_delayed",witnessed},{"nonempty_finer_content_positive",positive},{"witness",attempts}};std::cout<<out.dump()<<'\n';require(!witnessed,"OLD_READY_REFINEMENT_HELD_UNTIL_NEXT_PRESENTATION");require(positive,"NO_POSITIVE_SAME_PRESENT_CONTENT_REFINEMENT");return out;
}
#endif
int main(int argc,char**argv){try{
#if defined(__APPLE__)
 require(argc>=2,"output path");auto autorelease=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");auto d=MTLCreateSystemDefaultDevice(),q=message(d,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(d),sk_cfp<GrMTLHandle>(q)};c->context=GrDirectContexts::MakeMetal(backend);require(bool(c->context),"required actual Metal");
 if(argc>1&&std::string_view(argv[1])=="--continuation"){continuationCase(c,argc>2?unsigned(std::stoul(argv[2])):4,argc>3?argv[3]:"normal");return 0;}
 if(argc>1&&std::string_view(argv[1])=="--static-target"){std::cout<<staticTarget(c).dump()<<std::endl;return 0;}
 if(argc>1&&std::string_view(argv[1])=="--target-streaming"){std::cout<<targetAndStreaming(c).dump()<<std::endl;return 0;}
 if(argc>1&&std::string_view(argv[1])=="--frontier-progress"){frontierProgress(c);return 0;}
 if(argc>1&&std::string_view(argv[1])=="--cost-red"){comparableCostRED(c);return 0;}
 if(argc>1&&std::string_view(argv[1])=="--handoff-red"){samePresentationRED(c);return 0;}
 json result;if(argc>2)result["Mixed49"]=inspectSnapshot(argv[2]);if(argc>3&&std::string_view(argv[3])=="--dynamic"){result["dynamic"]=json::array();result["dynamic"].push_back(dynamic(unsigned(std::stoul(argv[4])),c));std::ofstream(std::string(argv[1])+".trace.bin",std::ios::binary).write(reinterpret_cast<const char*>(ns::state.rows.data()),ns::state.count*sizeof(ns::Row));std::ofstream(argv[1])<<result.dump(2)<<'\n';c.reset();message(autorelease,"drain");return 0;}result["density"]=json::array();for(unsigned dpr:{2U,4U})result["density"].push_back(density(dpr,c));std::ofstream(argv[1])<<result.dump(2)<<'\n';bool sharp=true;for(const auto&r:result["density"])sharp&=r["final_upscale"].get<double>()<=1.25;require(sharp,"AVOIDABLE_DPR4_DENSITY_DOWNGRADE_FIXED_KEY_LIMIT");c.reset();message(autorelease,"drain");return 0;
#else
 return 2;
#endif
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
