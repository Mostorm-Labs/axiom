// Real GPU Composition + AutoIntent + actual deferred CpuExactPathJob.
#define AXIOM_CPU_PENDING_FIXTURE_MAIN ienOldCpuRuntimeMain
#include "cpu_pending_runtime_test.cpp"
#undef AXIOM_CPU_PENDING_FIXTURE_MAIN
#include "../../render/src/navigation_sharing_observation.hpp"
#include "include/core/SkPathUtils.h"
#if defined(__APPLE__)
class CompositionMetalTarget final:public NativeMetalTarget{public:using NativeMetalTarget::NativeMetalTarget;RenderTargetInfo describe()const noexcept override{const auto info=NativeMetalTarget::describe();return{info.profileId,info.kind,info.backend,info.format,{1431,1010,2862,2020,2,1},info.capabilities};}};
void gpuComposition(const char* fixture,const char* output,const std::string& mode){
 std::ifstream file(fixture,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"GPU snapshot");auto s=*decoded.snapshot;
 // Native-only nonvacuous suffix witness at the final16 samples. Fixed40 is
 // retained and one canonical compact stroke crosses the otherwise empty end.
 auto sentinel=*std::find_if(s.objects.begin(),s.objects.end(),[](const auto& o){return std::holds_alternative<semantic::CompactStrokeContent>(o.content);});sentinel.id=ObjectId::fromUint64(960001);sentinel.transform={1,0,0,1,0,0};sentinel.erase_masks.clear();
 auto& body=std::get<semantic::CompactStrokeContent>(sentinel.content);body.stroke.knots={{370,693,32768},{470,693,32768}};body.stroke.config.size=18;sentinel.placement.order_key=semantic::OrderKey({250});s.objects.push_back(std::move(sentinel));
 require(semantic::SnapshotCodec::populateRequirements(s)&&semantic::SnapshotCodec::bindDigests(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1),"GPU native suffix fixture binding");
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};c->context=GrDirectContexts::MakeMetal(backend);require(bool(c->context),"GPU context");
 runtime::RuntimeCompositionCore core;require(core.openDocument(s.document_id),"GPU open");require(core.restoreSnapshotBeforeSurface(s,{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"GPU restore");auto& editor=core.qualificationRuntime();render::RenderCacheOptions options;options.maxBytes=512U*1048576;options.sharedMaxBytes=1024U*1048576;options.maxEntryBytes=128U*1048576;ink_playground::InkPlaygroundHistoryTestAccess::setProfile(editor,options);
 auto provider=std::make_unique<CompositionMetalTarget>(c);auto* target=provider.get();require(target->resize(2862,2020).code==BackendSubmissionCode::kAccepted,"GPU target prepare");require(core.registerSurfaceProvider("ien-gpu",std::move(provider)),"GPU register");require(core.bindSurface(render::SurfaceMetrics{1431,1010,2862,2020,2,1})&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"GPU bind");
 auto floor=editor.reserveCpuWorkerGrant(1,64U*1048576);require(floor&&core.bindCpuPendingExecutor(1),"GPU pending executor");
 const auto frame=[&]{runtime::RuntimeFrameOpportunity out;require(core.serviceFrameOpportunity(false,out),"GPU real frame");if(out.previewWork)require(core.finishFrameOpportunity({out.previewWork,true}),"GPU preview");if(out.canonicalWork)require(core.receivePlatformPresented({*out.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"GPU receipt");c->context->flushAndSubmit(GrSyncCpu::kYes);editor.serviceResourceRetirements();};
 for(unsigned n=0;core.canonicalPresentationDirty()&&n<2048;++n)frame();require(!core.canonicalPresentationDirty(),"GPU initial display");
 require(editor.setViewportZoomAt(.34F,0,0),"GPU zoom");auto cam=editor.viewportGesture();require(editor.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,cam.translationX-120,cam.translationY-260}),"GPU translate");for(unsigned n=0;core.canonicalPresentationDirty()&&n<2048;++n)frame();
 auto& f=core.controlFacade();std::uint64_t request=0,sequence=0;
 auto control=[&](runtime::CanvasControlPayload p){runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++request;q.target=f.mountedCanvasTargets().front();q.payload=p;require(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"GPU control");};
 runtime::CanvasControlPayload ctl{};ctl.kind=runtime::CanvasControlPayloadKind::kSelectTool;ctl.tool=runtime::CanvasToolKind::kEraser;control(ctl);ctl={};ctl.kind=runtime::CanvasControlPayloadKind::kEraserOptions;ctl.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);ctl.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(32);control(ctl);
 const auto input=[&](unsigned id,input::PointerPhase phase,float x,float y,bool touch=false){input::PlatformPointerSample p{};p.source=71;p.pointer=id;p.sequence=++sequence;p.timestampNs=90000000000+sequence*16666667;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;if(touch)p.capabilities.tool=input::PointerTool::kFinger;require(core.submit({{p}}),"GPU actualinput");};
 const auto point=[&](unsigned i){const auto c=editor.viewportGesture();return std::pair{float((420+80*std::sin(2*3.141592653589793*i/160))*c.scale+c.translationX),float((160+540.*i/480)*c.scale+c.translationY)};};
 const auto solve=[&](const std::vector<std::uint8_t>& request){const auto work=render::internal::cpuDecodeRequest(request);require(bool(work),"GPU decode actualrequest");render::internal::CpuExactPathJob kernel(work->geometry,work->prefixCount,work->prefixPath);while(!kernel.ready())require(kernel.advance(8,2000),"GPU real exactkernel");const auto size=kernel.path().writeToMemory(nullptr);std::vector<std::uint8_t> result(128+size);std::copy_n(request.begin(),128,result.begin());const auto word=[&](unsigned at,std::uint32_t v){for(unsigned k=0;k<4;++k)result[at+k]=v>>(k*8);};word(0,0x31525043);word(4,1);word(96,size);word(100,work->piece?3:2);if(!work->piece)for(unsigned k=104;k<128;++k)result[k]=0;kernel.path().writeToMemory(result.data()+128);require(core.queueCpuPendingCompletion(work->identity.words,result,true),"GPU actualcompletion");};
 const auto read=[&]{std::vector<std::uint8_t> v(2862U*2020U*4U);require(target->readbackRgba(v).code==BackendSubmissionCode::kAccepted,"GPU actualpixels");return v;};
 auto p=point(0);input(1,input::PointerPhase::kDown,p.first,p.second);
 for(unsigned i=1;i<=480;++i){p=point(i);input(1,input::PointerPhase::kMove,p.first,p.second);if(i%16==0&&i<480){for(unsigned k=0;k<128&&editor.transientOverlayDirty();++k)frame();}}
 input(1,input::PointerPhase::kUp,p.first,p.second);
 const auto confirmed=ink_playground::InkPlaygroundHistoryTestAccess::frozenTrace(editor);
 const auto oracleFrame=[&](unsigned camera){const auto vp=editor.viewportGesture();return FrameState{ViewId{1},CameraState{{(715.5F-vp.translationX)/vp.scale,(505.F-vp.translationY)/vp.scale},vp.scale,0,CameraGeneration{camera}},{-vp.translationX/vp.scale,-vp.translationY/vp.scale,(1431-vp.translationX)/vp.scale,(1010-vp.translationY)/vp.scale},{1431,1010,2862,2020,2,1},semantic::SemanticGeneration{editor.semanticGeneration().value()},SceneRevision{editor.sceneRevision()},SurfaceGeneration{target->generation()},MetricsGeneration{1},FrameId{10000+camera},{0,0,1431,1010}};};
 const auto fullFrame=oracleFrame(editor.cameraGeneration());auto fullSources=ink_playground::InkPlaygroundHistoryTestAccess::renderSources(editor,fullFrame);
 auto prefixSources=fullSources;
 for(auto* sources:{&fullSources,&prefixSources})for(auto& entry:sources->entries)if(entry.record.kind==semantic::ObjectKind::kVectorStroke||entry.record.kind==semantic::ObjectKind::kDabStroke){const auto count=sources==&fullSources?confirmed.size():std::min<std::size_t>(465,confirmed.size());auto g=interaction::partialEraseGeometry(std::span(confirmed).first(count),entry.record.transform,16);require(bool(g),"GPU independent normalized mask");entry.presentationEraseActive=true;entry.presentationEraseMasks.push_back({{},*g});}
 const auto oracleImage=[&](ReferenceDrawList sources){NativeMetalTarget independent(c);require(independent.resize(2862,2020).code==BackendSubmissionCode::kAccepted,"GPU oracle target");SkiaRenderer fresh;fresh.setRenderCacheOptions(options);FramePlan plan{fullFrame,std::move(sources),s.background};EditingOverlay bare{fullFrame};bool complete=false;for(unsigned k=0;k<1024;++k){auto r=fresh.renderTransientOverlay(independent,plan,&bare);c->context->flushAndSubmit(GrSyncCpu::kYes);fresh.serviceResourceRetirements();require(r.code!=BackendSubmissionCode::kRejected,"GPU independent fullhistory");if(r.code==BackendSubmissionCode::kAccepted){complete=true;break;}}require(complete,"GPU independent latestsuffix ready");auto a=independent.acquire();auto result=a.frame.surface->makeImageSnapshot();independent.release();return result;};
 struct Boundary {semantic::Transform2D pose;SkPath path;};std::vector<Boundary> boundaries;
 for(const auto& entry:fullSources.entries)if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command)){
  const auto& stroke=compact->content.get().stroke;const auto contour=ink::compileCompactContour(stroke.config,stroke.knots);
  for(std::size_t i=0;i+1<contour.patches.size();++i)boundaries.push_back({entry.record.transform,render::internal::compactContourRibbon(std::span(contour.patches).subspan(i,2))});
  for(const auto& cap:contour.caps){SkPathBuilder p;p.addCircle(cap.x,cap.y,cap.radius);boundaries.push_back({entry.record.transform,p.detach()});}
  for(const auto& masks:{std::span(entry.presentationEraseMasks),std::span(entry.record.eraseMasks)})for(const auto& mask:masks)if(const auto* f=std::get_if<semantic::FilledPathMask>(&mask.geometry))boundaries.push_back({entry.record.transform,render::internal::semanticPath(f->path)});else for(const auto& part:std::get<semantic::SweptCircleMask>(mask.geometry).segments){SkPathBuilder p;p.moveTo(part.p0.position.x,part.p0.position.y);p.cubicTo(part.control1.x,part.control1.y,part.control2.x,part.control2.y,part.p1.position.x,part.p1.position.y);SkPaint paint;paint.setStyle(SkPaint::kStroke_Style);paint.setStrokeWidth(2*std::max(part.p0.radius,part.p1.radius));paint.setStrokeCap(SkPaint::kRound_Cap);boundaries.push_back({entry.record.transform,skpathutils::FillPathWithPaint(p.detach(),paint)});}
 }
 const auto stableEdges=[&]{const auto vp=editor.viewportGesture();auto edge=SkSurfaces::Raster(SkImageInfo::MakeA8(2862,2020));edge->getCanvas()->clear(SK_ColorTRANSPARENT);SkPaint line;line.setColor(SK_ColorWHITE);line.setStyle(SkPaint::kStroke_Style);line.setStrokeWidth(2);line.setAntiAlias(true);for(const auto& b:boundaries){auto matrix=SkMatrix::Scale(2,2);matrix.preConcat(SkMatrix::MakeAll(vp.scale,0,vp.translationX,0,vp.scale,vp.translationY,0,0,1));const auto& t=b.pose;matrix.preConcat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));edge->getCanvas()->drawPath(b.path.makeTransform(matrix),line);}return edge->makeImageSnapshot();}();
 const auto edgePixels=[&]{const auto vp=editor.viewportGesture();const double ratio=double(vp.scale)/double(.34F);auto edge=SkSurfaces::Raster(SkImageInfo::MakeA8(2862,2020));auto* canvas=edge->getCanvas();canvas->clear(SK_ColorTRANSPARENT);canvas->translate(float((double(vp.translationX)-ratio*120)*2),float((double(vp.translationY)-ratio*260)*2));canvas->scale(float(ratio),float(ratio));canvas->drawImage(stableEdges,0,0,SkSamplingOptions(SkFilterMode::kLinear));std::vector<std::uint8_t> pixels(2862U*2020U);require(edge->readPixels(SkImageInfo::MakeA8(2862,2020),pixels.data(),2862,0,0),"GPU original resampled boundary");return pixels;};
 const auto latestImage=oracleImage(std::move(fullSources)),prefixImage=oracleImage(std::move(prefixSources));
 const auto transformed=[&](const sk_sp<SkImage>& image){const auto vp=editor.viewportGesture();const double ratio=double(vp.scale)/double(.34F);auto surface=SkSurfaces::RenderTarget(c->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(2862,2020));auto* canvas=surface->getCanvas();canvas->clear(SK_ColorWHITE);canvas->translate(float((double(vp.translationX)-ratio*120)*2),float((double(vp.translationY)-ratio*260)*2));canvas->scale(float(ratio),float(ratio));canvas->drawImage(image,0,0,SkSamplingOptions(SkFilterMode::kLinear));std::vector<std::uint8_t> pixels(2862U*2020U*4U);require(surface->readPixels(SkImageInfo::Make(2862,2020,kRGBA_8888_SkColorType,kPremul_SkAlphaType),pixels.data(),2862*4,0,0),"GPU independent camera read");return pixels;};
 const auto generation=editor.semanticGeneration().value();require(core.cpuPendingFacts().pending==1,"GPU actual frozen intent");
 // No frame/stable barrier after Up. Actual first-finger tentative eraser
 // routes through the same AutoIntent path that the direct renderer skipped.
 input(2,input::PointerPhase::kDown,635.5,505,true);input(3,input::PointerPhase::kDown,795.5,505,true);
 unsigned valid=0,pixelQualified=0;std::size_t latestSuffixWitness=0;std::vector<std::uint8_t> held;json attempts=json::array();
 for(unsigned i=1;i<=16;++i){const auto before=target->presentCount();const float factor=mode=="pinch"?(1+float(i)*.01F):1;
  const float x=mode=="pinch"?0:float(i),y=mode=="pinch"?0:-float(i)*.5F;
  input(2,input::PointerPhase::kMove,715.5F-80*factor+x,505+y,true);input(3,input::PointerPhase::kMove,715.5F+80*factor+x,505+y,true);frame();if(held.empty())held=core.takeCpuPendingRequest();const auto after=target->presentCount();valid+=after>before;const auto shown=read();if(after>before){const auto expected=transformed(latestImage),old=transformed(prefixImage);const auto edge=edgePixels();std::size_t differing=0;unsigned maxDelta=0;for(std::size_t k=0;k<shown.size();++k)if(shown[k]!=expected[k]){++differing;const auto delta=unsigned(std::abs(int(shown[k])-int(expected[k])));maxDelta=std::max(maxDelta,delta);if(!edge[k/4]||delta>2)std::fprintf(stderr,"IEN_OUTSIDE x=%zu y=%zu c=%zu delta=%u edge=%u\n",k/4%2862,k/4/2862,k%4,delta,unsigned(edge[k/4]));require(edge[k/4]&&delta<=2,"GPU_COMPOSITION_LATEST_SUFFIX_TARGET_CAMERA_PIXELS");}std::fprintf(stderr,"IEN_PIXEL i=%u differing=%zu max=%u\n",i,differing,maxDelta);++pixelQualified;for(std::size_t k=0;k<old.size();k+=4)latestSuffixWitness+=old[k]+old[k+1]+old[k+2]+12<expected[k]+expected[k+1]+expected[k+2];}unsigned visible=0;for(std::size_t k=0;k<shown.size();k+=4)visible+=shown[k]<240||shown[k+1]<240||shown[k+2]<240;require(visible>100,"GPU feedback blank");attempts.push_back({{"camera",editor.cameraGeneration()},{"presents",after},{"submitted",after>before},{"pending",core.cpuPendingFacts().state}});
 }
 require(pixelQualified>=15&&latestSuffixWitness>100,"GPU latest suffix witness vacuous");
 require(!held.empty(),"GPU actual CpuExactPathJob encoded request held");json result{{"mode",mode},{"current_camera_submissions",valid},{"requested_camera_changes",16},{"pending",core.cpuPendingFacts().pending},{"semantic",editor.semanticGeneration().value()},{"held_request_bytes",held.size()},{"attempts",attempts},{"actualGPU",true},{"realArbitration",true},{"UpNoFinalFrameWait",true}};std::ofstream(output)<<result.dump(2)<<'\n';
 require(valid>=15,"ACTUAL_GPU_COMPOSITION_TENTATIVE_CANCEL_STARVES_FEEDBACK");require(editor.semanticGeneration().value()==generation,"GPU navigation mutated pendingsemantic");
 // Execute the real encoded kernel and deliver only after navigation.
 const auto work=render::internal::cpuDecodeRequest(held);require(bool(work),"GPU decode real request");render::internal::CpuExactPathJob job(work->geometry,work->prefixCount,work->prefixPath);while(!job.ready())require(job.advance(8,2000),"GPU real kernel");const auto n=job.path().writeToMemory(nullptr);std::vector<std::uint8_t> response(128+n);std::copy_n(held.begin(),128,response.begin());const auto word=[&](unsigned p,std::uint32_t v){for(unsigned k=0;k<4;++k)response[p+k]=v>>(k*8);};word(0,0x31525043);word(4,1);word(96,n);word(100,work->piece?3:2);if(!work->piece)for(unsigned k=104;k<128;++k)response[k]=0;job.path().writeToMemory(response.data()+128);require(core.queueCpuPendingCompletion(work->identity.words,response,true),"GPU matching delayedcompletion");
 // Old completed worker tokens cannot qualify a second active gesture or
 // clear its display; cancellation still uses the real source-owner path.
 input(2,input::PointerPhase::kUp,0,0,true);input(3,input::PointerPhase::kUp,0,0,true);
 unsigned kernelRequests=1;for(unsigned n=0;n<2048&&core.cpuPendingFacts().pending;++n){frame();auto next=core.takeCpuPendingRequest();if(!next.empty()){solve(next);++kernelRequests;}}
 require(!core.cpuPendingFacts().pending&&core.cpuPendingFacts().committed==1&&editor.semanticGeneration().value()==generation+1,"GPU oneatomiccommit");
 for(unsigned n=0;n<2048&&core.canonicalPresentationDirty();++n)frame();require(!core.canonicalPresentationDirty(),"GPU exactconverge");
 // Settled pinch: a completed canonical plane must not be held behind the
 // obsolete pending-commit comparison. Real input, no fabricated identity.
 input(4,input::PointerPhase::kDown,635.5,505,true);input(5,input::PointerPhase::kDown,795.5,505,true);

#if defined(AXIOM_NAV_SHARING_OBSERVATION)
 render::internal::ns::reset();
#endif
 unsigned settled=0;for(unsigned i=1;i<=24;++i){const float factor=1.25F-float(i)*.5F/24;
  const auto before=target->presentCount();input(4,input::PointerPhase::kMove,715.5F-80*factor,505,true);input(5,input::PointerPhase::kMove,715.5F+80*factor,505,true);frame();settled+=target->presentCount()>before;std::fprintf(stderr,"IEN_SETTLED i=%u factor=%f presented=%u\n",i,double(factor),unsigned(target->presentCount()>before));
#if defined(AXIOM_NAV_SHARING_OBSERVATION)
 if(render::internal::ns::state.count){const auto& r=render::internal::ns::state.rows[render::internal::ns::state.count-1];std::fprintf(stderr,"IEN_NS reason=%u budget=%llu elapsed=%llu queries=%u steps=%u missing=%u builds=%llu\n",r.terminalReason,(unsigned long long)r.budgetNs,(unsigned long long)(r.end-r.begin),r.queries,r.steps,r.missingCount,(unsigned long long)r.built);}
#endif

 }
 input(4,input::PointerPhase::kUp,0,0,true);input(5,input::PointerPhase::kUp,0,0,true);
 result["pixel_qualified_camera_submissions"]=pixelQualified;result["latest_suffix_witness_pixels"]=latestSuffixWitness;result["settled_pinch_submissions"]=settled;std::ofstream(output)<<result.dump(2)<<'\n';require(settled>=23,"SETTLED_PINCH_REUSE_STARVED_BY_COMMIT_PROOF");
 for(unsigned k=0;k<512&&core.canonicalPresentationDirty();++k)frame();
 const auto committedGeneration=editor.semanticGeneration().value();const auto contactsBefore=ink_playground::InkPlaygroundHistoryTestAccess::displays(editor);
 p=point(10);input(8,input::PointerPhase::kDown,p.first,p.second);p=point(60);input(8,input::PointerPhase::kMove,p.first,p.second);
 const auto displaysDuring=ink_playground::InkPlaygroundHistoryTestAccess::displays(editor);
 require(displaysDuring>=contactsBefore,"new gesture display missing");
 require(!core.queueCpuPendingCompletion(work->identity.words,response,true),"old worker completion accepted for new gesture");
 require(editor.semanticGeneration().value()==committedGeneration&&ink_playground::InkPlaygroundHistoryTestAccess::displays(editor)==displaysDuring,"old completion retired new gesture");
 input(8,input::PointerPhase::kCancel,p.first,p.second);require(editor.semanticGeneration().value()==committedGeneration&&!core.cpuPendingFacts().pending,"cancel mutated canonical");
 result["old_completion_rejected_new_gesture_preserved"]=true;result["cancel_no_operation"]=true;
 result["real_kernel_requests"]=kernelRequests;result["committed"]=core.cpuPendingFacts().committed;result["final_presents"]=target->presentCount();result["realGPUCommitCrossing"]=true;std::ofstream(output)<<result.dump(2)<<'\n';
 require(editor.releaseCpuWorkerGrant(1,floor),"GPU grantrelease");require(core.close(),"GPU close");

}
#endif
int main(int argc,char**argv){try{
#if defined(__APPLE__)
 require(argc>=4,"fixture output mode");auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");gpuComposition(argv[1],argv[2],argv[3]);message(pool,"drain");
#else
 (void)argc;(void)argv;
#endif
 return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
