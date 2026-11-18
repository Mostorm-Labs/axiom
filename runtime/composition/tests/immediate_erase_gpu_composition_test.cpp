// Real GPU Composition + AutoIntent + actual deferred CpuExactPathJob.
#define AXIOM_CPU_PENDING_FIXTURE_MAIN ienOldCpuRuntimeMain
#include "cpu_pending_runtime_test.cpp"
#undef AXIOM_CPU_PENDING_FIXTURE_MAIN
#if defined(__APPLE__)
class CompositionMetalTarget final:public NativeMetalTarget{public:using NativeMetalTarget::NativeMetalTarget;RenderTargetInfo describe()const noexcept override{const auto info=NativeMetalTarget::describe();return{info.profileId,info.kind,info.backend,info.format,{1431,1010,2862,2020,2,1},info.capabilities};}};
void gpuComposition(const char* fixture,const char* output,const std::string& mode){
 std::ifstream file(fixture,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"GPU snapshot");auto s=*decoded.snapshot;
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
 input(1,input::PointerPhase::kUp,p.first,p.second);const auto generation=editor.semanticGeneration().value();require(core.cpuPendingFacts().pending==1,"GPU actual frozen intent");
 // No frame/stable barrier after Up. Actual first-finger tentative eraser
 // routes through the same AutoIntent path that the direct renderer skipped.
 input(2,input::PointerPhase::kDown,635.5,505,true);input(3,input::PointerPhase::kDown,795.5,505,true);
 unsigned valid=0;std::vector<std::uint8_t> held;json attempts=json::array();
 for(unsigned i=1;i<=16;++i){const auto before=target->presentCount();const float factor=mode=="pinch"?(1+float(i)*.01F):1;
  const float x=mode=="pinch"?0:float(i),y=mode=="pinch"?0:-float(i)*.5F;
  input(2,input::PointerPhase::kMove,715.5F-80*factor+x,505+y,true);input(3,input::PointerPhase::kMove,715.5F+80*factor+x,505+y,true);frame();if(held.empty())held=core.takeCpuPendingRequest();const auto after=target->presentCount();valid+=after>before;const auto shown=read();unsigned visible=0;for(std::size_t k=0;k<shown.size();k+=4)visible+=shown[k]<240||shown[k+1]<240||shown[k+2]<240;require(visible>100,"GPU feedback blank");attempts.push_back({{"camera",editor.cameraGeneration()},{"presents",after},{"submitted",after>before},{"pending",core.cpuPendingFacts().state}});
 }
 require(!held.empty(),"GPU actual CpuExactPathJob encoded request held");json result{{"mode",mode},{"current_camera_submissions",valid},{"requested_camera_changes",16},{"pending",core.cpuPendingFacts().pending},{"semantic",editor.semanticGeneration().value()},{"held_request_bytes",held.size()},{"attempts",attempts},{"actualGPU",true},{"realArbitration",true},{"UpNoFinalFrameWait",true}};std::ofstream(output)<<result.dump(2)<<'\n';
 require(valid>=15,"ACTUAL_GPU_COMPOSITION_TENTATIVE_CANCEL_STARVES_FEEDBACK");require(editor.semanticGeneration().value()==generation,"GPU navigation mutated pendingsemantic");
 // Execute the real encoded kernel and deliver only after navigation.
 const auto work=render::internal::cpuDecodeRequest(held);require(bool(work),"GPU decode real request");render::internal::CpuExactPathJob job(work->geometry,work->prefixCount,work->prefixPath);while(!job.ready())require(job.advance(8,2000),"GPU real kernel");const auto n=job.path().writeToMemory(nullptr);std::vector<std::uint8_t> response(128+n);std::copy_n(held.begin(),128,response.begin());const auto word=[&](unsigned p,std::uint32_t v){for(unsigned k=0;k<4;++k)response[p+k]=v>>(k*8);};word(0,0x31525043);word(4,1);word(96,n);word(100,work->piece?3:2);if(!work->piece)for(unsigned k=104;k<128;++k)response[k]=0;job.path().writeToMemory(response.data()+128);require(core.queueCpuPendingCompletion(work->identity.words,response,true),"GPU matching delayedcompletion");
 input(2,input::PointerPhase::kUp,0,0,true);input(3,input::PointerPhase::kUp,0,0,true);
 unsigned kernelRequests=1;for(unsigned n=0;n<2048&&core.cpuPendingFacts().pending;++n){frame();auto next=core.takeCpuPendingRequest();if(!next.empty()){solve(next);++kernelRequests;}}
 require(!core.cpuPendingFacts().pending&&core.cpuPendingFacts().committed==1&&editor.semanticGeneration().value()==generation+1,"GPU oneatomiccommit");
 for(unsigned n=0;n<2048&&core.canonicalPresentationDirty();++n)frame();require(!core.canonicalPresentationDirty(),"GPU exactconverge");
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
