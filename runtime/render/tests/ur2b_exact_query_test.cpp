// Reuse the existing native Metal/provider/FramePlan qualification fixture.
// It is test-only and keeps the actual renderer entry point in this test.
#define main ur2bExistingFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/selection_region_geometry_internal.hpp"
#include <nlohmann/json.hpp>
#include <limits>
#include <iostream>
#include "canvas/semantic/snapshot.hpp"
#include <filesystem>
namespace {
using nlohmann::json;
using Hit=SelectionGeometryHit;
semantic::EraseMaskGeometry exactBox(double x,double y,double w,double h){
 semantic::FilledPathMask m;m.path.commands={semantic::MoveTo{{x,y}},semantic::LineTo{{x+w,y}},semantic::LineTo{{x+w,y+h}},semantic::LineTo{{x,y+h}},semantic::ClosePath{}};return m;
}
void actualDrawThenQuery(bool probe){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {
 const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
 auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext context{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 completion->context=GrDirectContexts::MakeMetal(context);require(bool(completion->context),"UR2b Metal context");
 for(unsigned copies:{1U,8U,16U,32U}){
  auto entry=source();entry.record.kind=semantic::ObjectKind::kVectorStroke;entry.record.kindVersion=3;
  auto base=renderPlan(entry,1,2);auto draw=base.referenceDrawList;for(unsigned i=1;i<copies;++i){auto copy=entry;copy.record.objectId=ObjectId::fromUint64(i+1);copy.record.transform.tx=i*.125;draw.entries.push_back(copy);}
  const FramePlan plan{base.frame,std::move(draw),{},false,true};
  NativeMetalTarget provider(completion);require(provider.resize(512,256).code==BackendSubmissionCode::kAccepted,"UR2b target");
  SkiaRenderer renderer;unsigned attempts=0;
  for(;;){const auto r=renderer.renderFrame(provider,plan);++attempts;if(r.code==BackendSubmissionCode::kAccepted)break;require(r.code==BackendSubmissionCode::kRetry&&attempts<16384,"UR2b warm draw progress");}
  const auto before=renderer.renderCacheWork();require(before.preparedBodyBuilds==1&&(before.preparedCanonicalDraws+before.preparedDirectDraws)>0,"real draw did not warm shared body");
  for(unsigned repeat=0;repeat<2;++repeat)for(unsigned i=0;i<copies;++i){auto record=entry.record;record.objectId=ObjectId::fromUint64(i+1);
   const auto q=exactBox(1430,20+repeat*.1,3,110);
   require(renderer.intersectEraseGeometry(record,q)==intersectEraseGeometryComponentOracle(record,q),"warm renderer exact decision");}
  const auto after=renderer.renderCacheWork();
  const auto compiled=after.exactCompileInputs-before.exactCompileInputs;
  std::cout<<json{{"copies",copies},{"actual_draw_builds",before.preparedBodyBuilds},{"canonical_draws",before.preparedCanonicalDraws},{"direct_draws",before.preparedDirectDraws},{"queries",2*copies},{"validation_calls",after.exactFullValidations-before.exactFullValidations},{"validation_knots",after.exactValidationKnots-before.exactValidationKnots},{"compile_inputs",compiled},{"constructed_components",after.exactConstructedComponents-before.exactConstructedComponents},{"body_uses",after.exactBodyUses-before.exactBodyUses},{"structural_red",compiled>0}}.dump()<<'\n';
  if(!probe)require(compiled==0&&after.exactFullValidations==before.exactFullValidations&&after.exactBodyUses-before.exactBodyUses==2*copies,"warm exact query rebuilt full immutable draw body");
  if(!probe&&copies==1){
   // Initialize the same renderer owner with an empty document plane, then
   // admit the source through exact query first. The next actual draw must
   // consume that body rather than creating a second representation.
   auto emptyDraw=base.referenceDrawList;emptyDraw.entries.pop_back();
   const FramePlan empty{base.frame,std::move(emptyDraw),{},false,true};SkiaRenderer reverse;
   require(reverse.renderFrame(provider,empty).code==BackendSubmissionCode::kAccepted,"reverse owner init");
   require(reverse.intersectEraseGeometry(entry.record,exactBox(100,20,10,90))==Hit::kHit,"cold exact admission");
   const auto admitted=reverse.renderCacheWork();require(admitted.preparedBodyBuilds==1&&admitted.exactBodyUses==1,"cold exact did not admit shared body");
   for(unsigned n=0;;++n){const auto drawn=reverse.renderFrame(provider,plan);if(drawn.code==BackendSubmissionCode::kAccepted)break;require(drawn.code==BackendSubmissionCode::kRetry&&n<16384,"reverse draw progress");}
   const auto drawn=reverse.renderCacheWork();require(drawn.preparedBodyBuilds==admitted.preparedBodyBuilds&&drawn.preparedJobAdmissions==admitted.preparedJobAdmissions&&(drawn.preparedCanonicalDraws+drawn.preparedDirectDraws)>0,"exact->draw created a second body");
  }
 }
 }
 message(pool,"drain");
#else
 require(false,"native GPU fixture unavailable");
#endif
}
void parityAndLocality(bool locality){
 for(unsigned variant=0;variant<8;++variant){
  PreparedGeometryCache bodies;EraseMaskPathCache masks;RenderCacheOptions options;auto budget=std::make_shared<resources::ResourceBudgetManager>();
  auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;
  auto c=std::get<semantic::CompactStrokeContent>(e.record.content.get());
  c.stroke.config.family=variant&1?foundation::InkFamily::kMarker:foundation::InkFamily::kRound;
  c.stroke.config.markerAngle=.7;c.stroke.config.markerFlatness=.35;
  if(variant&2){c.stroke.config.startTaper=35;c.stroke.config.endTaper=50;c.stroke.config.startCap=false;}
  if(variant&4)c.stroke.knots={{40,60,32768},{120,60,15000},{100,60,55000},{170,70,0}};
  e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
  auto body=bodies.get(e,options,budget,true);require(body&&body->exactValidated(),"complete finite body proof");
  for(unsigned n=0;n<64;++n){auto r=e.record;
   for(unsigned h=0;h<n%11;++h)r.eraseMasks.push_back({ObjectId::fromUint64(h+100),exactBox(20+(h*17+n*7)%150,20+(h*11+n*13)%90,10+h%9,12+h%7)});
   r.transform={1,.2,.3,1.1,80,-30};const auto q=exactBox(30+(n*19)%180,25+(n*23)%70,n%3?.02:20,n%4?.03:15);
   require(masks.query(r,q,budget,8*1024*1024,&bodies,&options)==intersectEraseGeometryComponentOracle(r,q),"exact local CPU decisions/marker/pressure/taper/history parity");
  }
  ExactComponentWork local;const auto demand=SkRect::MakeLTRB(1420,0,1435,128);body->visitExact(demand,[](const auto&){return true;},local);
  if(locality&&variant<4)require(local.components*4<body->primitiveCount(),"sparse exact query visited whole body");
  std::cout<<json{{"variant",variant},{"full_primitives",body->primitiveCount()},{"local_components",local.components},{"bounds_tests",local.boundsTests},{"index_groups",local.groups}}.dump()<<'\n';
 }
 // Force the existing checkpoint realization; exact replay must be local and
 // must disclose compilation rather than claiming a zero-build fast path.
 PreparedGeometryCache bodies;RenderCacheOptions options;auto budget=std::make_shared<resources::ResourceBudgetManager>();auto e=source();
 auto c=std::get<semantic::CompactStrokeContent>(e.record.content.get());c.stroke.knots.clear();
 for(unsigned i=0;i<100003;++i)c.stroke.knots.push_back({i*.5,64+20*std::sin(i*.01),32768});
 e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 auto body=bodies.get(e,options,budget,true);require(body&&body->exactValidated(),"checkpoint exact body admission");
 ExactComponentWork work;body->visitExact(SkRect::MakeLTRB(24900,0,25000,128),[](const auto&){return true;},work);
 require(work.replayKnots>0&&work.replayKnots<c.stroke.knots.size()/4,"checkpoint exact replay hidden or nonlocal");
 std::cout<<json{{"checkpoint_replay_knots",work.replayKnots},{"materialized_paths",work.materializedPaths},{"index_groups",work.groups}}.dump()<<'\n';
}
void invalidationAndPressure(bool pressure){
 PreparedGeometryCache bodies;EraseMaskPathCache masks;RenderCacheOptions options;auto budget=std::make_shared<resources::ResourceBudgetManager>();
 auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;const auto q=exactBox(100,10,20,110);
 auto body=bodies.get(e,options,budget,true);require(body&&body->exactValidated(),"initial exact proof");
 const auto check=[&](const RuntimeSceneRecord& r,const semantic::EraseMaskGeometry& m){require(masks.query(r,m,budget,8*1024*1024,&bodies,&options)==intersectEraseGeometryComponentOracle(r,m),"negative/invalidation decision mismatch");};
 auto alias=e;alias.record.content=semantic::ObjectContent{e.record.content.get()};alias.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(alias.record.content)};
 const auto before=bodies.statistics();check(alias.record,q);auto shared=bodies.lookup(alias,PreparedConsumer::kExact);
 require(shared==body&&bodies.statistics().contentComparisonKnots>before.contentComparisonKnots,"unknown storage bypassed full equality or shared second body");
 auto fork=alias;fork.record.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.knots[127].y+=1;});fork.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(fork.record.content)};
 check(fork.record,q);require(bodies.lookup(fork,PreparedConsumer::kExact)!=body,"fingerprint collision accepted different full content");
 check(e.record,q);auto masked=e.record;masked.eraseMasks={{ObjectId::fromUint64(500),q}};check(masked,q);require(masks.work().exact.bodyIdentity==body->identity(),"mask-only change invalidated immutable body");
 auto invalid=e.record;invalid.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.knots.back().x=std::numeric_limits<double>::quiet_NaN();});check(invalid,exactBox(-1000,-1000,1,1));
 auto badHistory=e.record;badHistory.eraseMasks={{ObjectId::fromUint64(501),exactBox(std::numeric_limits<double>::quiet_NaN(),0,20,20)}};check(badHistory,q);check(badHistory,exactBox(-1000,-1000,1,1));
 auto zero=e.record;zero.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.opacity=0;});check(zero,q);
 auto legacy=e.record;legacy.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.config.algorithmVersion=3;});check(legacy,q);
 auto wrong=e.record;wrong.content=semantic::ShapeContent{1,20,20};check(wrong,q);
 // Dot exact boundary: no bounds-only positives, preserve tiny-area hits.
 auto dot=e.record;dot.content.update([](auto& v){auto& s=std::get<semantic::CompactStrokeContent>(v).stroke;s.config.size=20;s.config.thinning=0;s.knots={{100,100,32768}};});
 check(dot,exactBox(110,95,10,10));check(dot,exactBox(109.99,99.99,1,.02));
 if(pressure){
  auto tiny=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});PreparedGeometryCache denied;EraseMaskPathCache noMask;
  require(noMask.query(e.record,q,tiny,8*1024*1024,&denied,&options)==intersectEraseGeometryComponentOracle(e.record,q)&&tiny->used()==0,"denial changed exact result/retained quota");
  PreparedGeometryCache pending;auto request=pending.request(e,options,budget);require(request&&!request->ready(),"pending fixture");const auto initial=pending.statistics();
  require(noMask.query(e.record,q,budget,8*1024*1024,&pending,&options)==intersectEraseGeometryComponentOracle(e.record,q),"pending fallback result");
  require(!request->ready()&&pending.statistics().advances==initial.advances&&noMask.work().exact.fallbackReason==3,"exact drained/restarted pending body");
  while(!request->ready()){PreparationAllowance one;one.limits.maxCoverageSteps=1;one.limits.maxMicroseconds=1000000;pending.advance(request,one);}
  require(noMask.query(e.record,q,budget,8*1024*1024,&pending,&options)==intersectEraseGeometryComponentOracle(e.record,q)&&noMask.work().exact.bodyUses>0,"ready pending source not shared");
  // Eviction removes discoverability, not the leases of pinned consumers.
  const auto used=budget->used();pending.clear();require(budget->used()==used,"pinned pending body lost leases at eviction");request.reset();
 }
 shared.reset();body.reset();masks.clear();bodies.clear();require(budget->used()==0,"exact source/index/query/mask lease leak");
}
void finiteDerivedProof(){
 const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
 for(unsigned n=0;n<6;++n){
  const auto path=root/".aegis/results/G5-UR0b/control-review-ur2b-r1/finite-range-r1"/("finite-"+std::to_string(n)+".axsnap");
  std::ifstream f(path,std::ios::binary);require(bool(f),"bound finite witness missing");std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};
  const auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"finite double witness codec");
  const auto& object=decoded.snapshot->objects.front();RuntimeSceneRecord r;r.objectId=object.id;r.kind=object.kind;r.kindVersion=object.kind_version;r.content=object.content;r.transform=object.transform;
  for(unsigned route=0;route<4;++route){
   auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1024*1024,4*1024*1024});
   resources::ResourceLease pressure;
   if(route==2){const auto t=budget->reserve(resources::ResourceClass::kCanonicalFrame,1024*1024,true);require(bool(t),"checkpoint pressure");pressure=resources::ResourceLease(budget,*t);}
   PreparedGeometryCache bodies;EraseMaskPathCache masks;RenderCacheOptions options;
   ReferenceTraversalEntry e;e.record.content=r.content;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(r.content)};
   std::shared_ptr<const PreparedNavigationGeometry> body;
   if(route==1||route==2)body=bodies.get(e,options,budget,true);
   if(route==3){body=bodies.request(e,options,budget);require(body&&!body->ready(),"finite deferred admission");
    while(!body->ready()){PreparationAllowance one;one.limits.maxCoverageSteps=1;one.limits.maxMicroseconds=1000000;bodies.advance(body,one);}}
   if(route)require(body&&body->ready(),"finite prepared witness route");
   const auto center=exactBox(40,90,20,20);
   const semantic::EraseMaskGeometry empty=semantic::FilledPathMask{};
   for(unsigned query=0;query<4;++query){auto record=r;auto mask=query==1?exactBox(-500,-500,10,10):query==2?empty:center;
    if(query==3)record.eraseMasks={{ObjectId::fromUint64(700),center}};
    const auto expected=intersectEraseGeometryComponentOracle(record,mask);
    if(n>=3)require(expected==Hit::kInvalid,"old kernel finite overflow authority changed");
    const auto actual=masks.query(record,mask,budget,8*1024*1024,&bodies,&options);
    if(actual!=expected)std::fprintf(stderr,"FINITE_DERIVED_RED witness=%u route=%u query=%u old=%u actual=%u\n",n,route,query,unsigned(expected),unsigned(actual));
    require(actual==expected,"finite derived path was hidden by prepared early-out");
   }
   if(body)require(body->exactValidated()==(n<3),"derived finite proof is incomplete");
   body.reset();bodies.clear();masks.clear();pressure.reset();require(budget->used()==0,"finite proof lease leak");
  }
 }
 std::cout<<"finite derived full/cold/checkpoint/deferred/early-out parity PASS\n";
}
}
int main(int argc,char** argv){try{
 const auto mode=argc>1?std::string_view(argv[1]):"sharing";
 if(mode=="probe"||mode=="BodySharing")actualDrawThenQuery(mode=="probe");
 else if(mode=="Parity"||mode=="Locality")parityAndLocality(mode=="Locality");
 else {finiteDerivedProof();invalidationAndPressure(mode=="Pressure");}
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
