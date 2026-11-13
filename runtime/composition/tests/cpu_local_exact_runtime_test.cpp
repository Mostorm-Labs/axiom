// The regression is the real pending owner requesting an entire long union
// before it can decide even an early hit. Production must request local work.
#define main cpuw6bPriorFixtureMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "../../render/src/cpu_worker_protocol.hpp"
#include <iostream>
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static void profile(interaction::CanvasEditingRuntime& r){render::RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;r.skiaRenderer_.setRenderCacheOptions(o);}
};
}
int main(int argc,char** argv) {try {
    require(argc==2,"original Fixed40");
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
    require(decoded.ok(),"fixture");const auto& s=*decoded.snapshot;
    runtime::RuntimeCompositionCore core;require(core.openDocument(s.document_id),"open");
    require(core.restoreSnapshotBeforeSurface(s,{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
    auto& editor=core.qualificationRuntime();ink_playground::InkPlaygroundHistoryTestAccess::profile(editor);
    require(core.bindSurface(1431,1010)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"bind");
    const auto floor=editor.reserveCpuWorkerGrant(1,64U*1024U*1024U);
    require(floor&&core.bindCpuPendingExecutor(1),"executor");
    require(editor.selectTool(interaction::CanvasEditingRuntime::ToolMode::kPartialEraser)&&editor.setEraserOptions(2,32),"tool");
    std::uint64_t sequence=0;
    const auto pointer=[&](input::PointerPhase phase,float x,float y){
        input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++sequence;
        p.timestampNs=90000000000+sequence*8000000;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;
        require(core.submit({{p}}),"input");
    };
    pointer(input::PointerPhase::kDown,420,160);
    for(unsigned i=1;i<=7200;++i)pointer(input::PointerPhase::kMove,float(420+7*std::sin(i*.17)),float(160+170.*i/7200));
    pointer(input::PointerPhase::kUp,999,999);
    for(unsigned i=0;i<1024;++i){
        editor.serviceCpuPendingErases();auto request=core.takeCpuPendingRequest();if(request.empty())continue;
        const auto value=render::internal::cpuDecodeRequest(request);require(bool(value),"request decode");
        const auto count=std::get<semantic::SweptCircleMask>(value->geometry).segments.size();
        std::cout<<"actual_requested_segments="<<count<<'\n';
        require(count<=32,"LOCAL_EXACT_REQUEST_REBUILT_FULL_7200_UNION");std::cout.flush();return 0;
    }
    require(false,"local preparation made no progress");return 1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
