#include "native_session.hpp"
#include "session.hpp"
#include <canvas/runtime/internal/runtime_composition_core.hpp>
#include <canvas/runtime/internal/runtime_platform_access.hpp>
#include <stdexcept>
#include <thread>
#include <iostream>
using namespace canvas;
using namespace canvas::sdk;
namespace {
void expect(bool v,const char* message){if(!v)throw std::runtime_error(message);}
class Platform final: public NativeSessionPlatform {
 runtime::RuntimeComposition* owner_=nullptr;
public:
 Json open(runtime::RuntimeComposition& owner,const Json&)override{owner_=&owner;return {{"ok",owner.bindSurface(256,256)}};}
 bool attach(const NativeView&)override{return owner_&&runtime::RuntimePlatformAccess::core(*owner_).rebindSurface();}
 bool detach()override{return owner_&&runtime::RuntimePlatformAccess::core(*owner_).loseSurface();}
 bool loadFonts()override{return true;}
 bool resize(float,float,float)override{return true;}
 bool lose()override{return true;}
 Json frame(bool)override{return {{"ok",owner_&&owner_->ownerPump()}};}
 bool shutdown()override{return true;}
 void release()noexcept override{owner_=nullptr;}
 std::uint64_t nowNs()const noexcept override{return 1000000000;}
};
}
int main(){
 NativeSessions sessions([](const NativeView&){return std::make_unique<Platform>();});
 const auto token=sessions.create(R"({"documentId":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","builtinFonts":false})",{});
 expect(token.size()==32,"opaque session token");
 expect(Json::parse(sessions.call(token,R"({"command":"state"})",{})).at("code")=="NOT_ATTACHED","detached admission");
 const auto first=sessions.attach(token,NativeView{reinterpret_cast<void*>(1),reinterpret_cast<void*>(3),256,256,1});
 expect(first!=0,"initial native attachment");
 expect(Json::parse(sessions.call(token,R"({"command":"state"})",{})).at("value").at("documentId")==std::string(32,'a'),"real document owner");
 expect(sessions.attach(token,NativeView{reinterpret_cast<void*>(2),reinterpret_cast<void*>(4),256,256,1})==0,"single view binding");
 input::PlatformPointerBatch down;input::PlatformPointerSample sample;
 sample.source=1;sample.pointer=7;sample.timestampNs=1000;sample.x=30;sample.y=30;sample.phase=input::PointerPhase::kDown;down.samples.push_back(sample);
 expect(sessions.input(token,first,down),"native pointer admission");
 expect(Json::parse(sessions.call(token,R"({"command":"state"})",{})).at("value").at("activeContacts")==1,"native contact tracked by Session admission");
 sample.phase=input::PointerPhase::kCancel;sample.timestampNs=2000;down.samples={sample};
 expect(sessions.input(token,first,down),"native pointer cancellation");
 expect(Json::parse(sessions.call(token,R"({"command":"state"})",{})).at("value").at("activeContacts")==0,"native terminal contact retired");
 std::string foreign;std::thread thread([&]{foreign=sessions.call(token,R"({"command":"state"})",{});});thread.join();expect(Json::parse(foreign).at("code")=="WRONG_THREAD","owner thread");
 expect(!sessions.detach(token,first+1),"stale detach");
 expect(sessions.detach(token,first),"detach");
 expect(Json::parse(sessions.call(token,R"({"command":"state"})",{})).at("code")=="NOT_ATTACHED","no commands without view");
 const auto next=sessions.attach(token,NativeView{reinterpret_cast<void*>(2),reinterpret_cast<void*>(4),256,256,1});expect(next>first,"remount generation");
 expect(!sessions.frame(token,first),"stale frame");
 expect(Json::parse(sessions.call(token,R"({"command":"control","kind":"preset","profileId":"crayon-wax-v1","revision":3})",{})).at("ok"),"shared Runtime control");
 expect(Json::parse(sessions.call(token,R"({"command":"pointer","source":1,"pointer":1,"timestampNs":1,"phase":0,"x":40,"y":40})",{})).at("code")=="HOST_ONLY","no JS input shortcut");
 expect(Json::parse(sessions.call(token,R"({"command":"close"})",{})).at("code")=="HOST_ONLY","explicit close lane");
 expect(sessions.close(token),"close");expect(!sessions.close(token),"closed token");
 expect(Json::parse(sessions.call(token,R"({"command":"state"})",{})).at("code")=="INVALID_SESSION","late command");
 std::cout<<"Native session real Runtime ownership/token/thread/detach/remount PASS\n";
}
