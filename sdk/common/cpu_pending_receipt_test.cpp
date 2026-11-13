#include "session.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include <stdexcept>
#include <iostream>

using namespace canvas;
namespace {
// This host provides a real shared Runtime raster surface. It does not own
// editing or document state and is not a renderer/pressure test double.
class RasterHost final : public sdk::SessionPlatform {
    runtime::RuntimeComposition* owner_=nullptr;
public:
    sdk::Json open(runtime::RuntimeComposition& owner,const sdk::Json&) override {
        owner_=&owner;return {{"ok",owner.bindSurface(256,256)}};
    }
    bool loadFonts() override{return false;}
    bool resize(float w,float h,float) override{return owner_&&owner_->bindSurface(unsigned(w),unsigned(h));}
    bool lose() override{return false;}
    sdk::Json frame(bool) override{return {{"ok",owner_&&owner_->ownerPump()}};}
    bool shutdown() override{return true;}
    void release() noexcept override{owner_=nullptr;}
    std::uint64_t nowNs()const noexcept override{return 1'000'000'000;}
};
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
}
int main(){try{
 {sdk::Session ordinary(std::make_unique<RasterHost>());auto opened=sdk::Json::parse(ordinary.call(sdk::Json{{"command","open"},{"documentId",std::string(32,'8')},{"builtinFonts",false}}.dump()));require(opened.at("ok").get<bool>(),"ordinary open");auto ready=sdk::Json::parse(ordinary.call(sdk::Json{{"command","cpuPending"},{"action","closePreflight"}}.dump()));require(ready.at("ok").get<bool>(),"ordinary noop preflight");auto done=sdk::Json::parse(ordinary.call(sdk::Json{{"command","close"}}.dump()));require(done.at("ok").get<bool>(),"ordinary noop close");}
 sdk::Session session(std::make_unique<RasterHost>());const auto call=[&](sdk::Json j){auto result=sdk::Json::parse(session.call(j.dump()));require(result.value("ok",false),result.dump().c_str());return result;};
 const auto build=call({{"command","version"}});require(build.at("sdkVersion").get<std::string>()==build.at("sdkSemver").get<std::string>()+"."+std::to_string(build.at("buildNumber").get<std::uint64_t>()),"local matching build identity");
 call({{"command","open"},{"documentId",std::string(32,'9')},{"builtinFonts",false}});const auto floor=runtime::RuntimePlatformAccess::reserveCpuWorkerGrant(*session.composition(),1,64U*1024U*1024U);require(floor,"worker floor");call({{"command","cpuPending"},{"action","bind"},{"executor","1"}});
 call({{"command","control"},{"kind","tool"},{"value",2}});
 std::uint64_t time=1000000;
 for(unsigned phase:{0U,1U,2U}){time+=10000000;call({{"command","pointer"},{"source",1},{"pointer",1},{"timestampNs",time},{"phase",phase},{"x",20+phase*10},{"y",40}});}
 const auto table=call({{"command","table"},{"action","create"},{"generation","0"},{"rows",2},{"columns",2},{"x",80},{"y",80}});require(table.contains("pendingToken"),"pending table ticket");
 auto token=table.at("pendingToken");require(token.at("runtimeInstance")!="0","valid pending owner");
 auto receipt=token;receipt["command"]="pendingReceipt";require(call(receipt).at("done")==false,"premature terminal");

 auto& core=runtime::RuntimePlatformAccess::core(*session.composition());auto& editor=core.qualificationRuntime();editor.serviceCpuPendingErases();editor.serviceCpuPendingErases();
 require(call(receipt).at("done").is_boolean(),"pending ticket changed under presentation");
 token["lifetimeGeneration"]="999";token["command"]="pendingReceipt";const auto stale=sdk::Json::parse(session.call(token.dump()));require(!stale.at("ok").get<bool>(),"foreign pending lifetime accepted");
 call({{"command","data"},{"action","attach"},{"capacity",32}});
 // Real pending creation and publication packet custody, no fake document.
 const auto create=call({{"command","table"},{"action","create"},{"generation",std::to_string(editor.semanticGeneration().value())},{"rows",1},{"columns",1},{"x",120},{"y",120}});
 for(unsigned n=0;n<16&&core.cpuPendingFacts().pending;++n)editor.serviceCpuPendingErases();
 require(core.cpuPendingFacts().pending==0,"publication setup pending");
 auto packet=call({{"command","data"},{"action","next"}});require(!packet.at("value").is_null(),"actual publication packet");
 const auto closing=sdk::Json::parse(session.call(sdk::Json{{"command","close"}}.dump()));require(!closing.at("ok").get<bool>()&&closing.at("code")=="PUBLICATION_PENDING","publication barrier");
 const auto preflight=call({{"command","cpuPending"},{"action","closePreflight"}});require(preflight.at("ok").get<bool>(),"readonly preflight during publicationclosing");
 for(const auto* action:{"bind","take","complete","suspend","unavailable","foreign"}){const auto denied=sdk::Json::parse(session.call(sdk::Json{{"command","cpuPending"},{"action",action},{"executor","2"}}.dump()));require(!denied.at("ok").get<bool>()&&denied.at("code")=="DOCUMENT_CLOSING","closing private mutation admitted");}
 call({{"command","data"},{"action","release"},{"packet",packet.at("value")}});
 const auto released=sdk::Json::parse(session.call(sdk::Json{{"command","data"},{"action","release"},{"packet",packet.at("value")}}.dump()));require(released.at("ok").get<bool>(),"exact release must remain idempotent");
 auto wrongPacket=packet.at("value");wrongPacket["commit"]["ordinal"]="999";const auto wrong=sdk::Json::parse(session.call(sdk::Json{{"command","data"},{"action","release"},{"packet",wrongPacket}}.dump()));require(!wrong.at("ok").get<bool>(),"foreign packet release accepted");
 call({{"command","cpuPending"},{"action","closePreflight"}});

 require(editor.releaseCpuWorkerGrant(1,floor),"floor release before teardown");
 call({{"command","close"}});
 std::cout<<"pending receipt owns stable private runtime basis"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
