#include "native_session.hpp"
#include <canvas/runtime/internal/runtime_platform_access.hpp>
#include <canvas/runtime/internal/runtime_composition_core.hpp>
#include <random>
#include <limits>
#include <cmath>

namespace canvas::sdk {
namespace {
std::string error(const char* code){return Json{{"ok",false},{"code",code}}.dump();}
bool validDimensions(float w,float h,float dpr){return std::isfinite(w)&&std::isfinite(h)&&std::isfinite(dpr)&&w>0&&h>0&&dpr>0&&w*dpr<=16384&&h*dpr<=16384;}
bool valid(const NativeView& v){return v.canonical&&v.preview&&v.canonical!=v.preview&&validDimensions(v.width,v.height,v.dpr);}
std::string opaque(){std::random_device random;std::string s;constexpr char hex[]="0123456789abcdef";for(unsigned i=0;i<16;++i){const auto b=random()&255;s+=hex[b>>4];s+=hex[b&15];}return s;}
}
class NativeSessions::Entry final {
public:
 Json options;
 std::vector<std::uint8_t> snapshot;
 std::unique_ptr<Session> session;
 std::uint64_t generation=0;
 bool attached=false,busy=false;
 std::map<std::pair<input::InputSourceId,input::PointerId>,input::PointerKey> contacts;
 std::uint64_t sequence=0;
 ~Entry(){if(session){session->clearCallbacks();session->platform().release();}}
 NativeSessionPlatform& platform(){return static_cast<NativeSessionPlatform&>(session->platform());}
 bool cancel(){
  if(!session)return true;
  for(const auto& [id,key]:contacts){(void)id;if(!session->composition()->cancelContact(key,++sequence,platform().nowNs()))return false;}
  contacts.clear();return true;
 }
};
NativeSessions::NativeSessions(PlatformFactory factory):factory_(std::move(factory)){}
NativeSessions::~NativeSessions()=default;
NativeSessions::Entry* NativeSessions::find(const std::string& token){const auto i=sessions_.find(token);return i==sessions_.end()?nullptr:i->second.get();}
std::string NativeSessions::create(std::string_view json,std::span<const std::uint8_t> bytes){
 if(std::this_thread::get_id()!=thread_)throw std::runtime_error("WRONG_THREAD");
 if(json.size()>1048576||bytes.size()>134217728||sessions_.size()>=32)throw std::runtime_error("CAPACITY");
 const auto options=Json::parse(json);
 const auto id=options.at("documentId").get<std::string>();
 if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos||id.find_first_not_of('0')==std::string::npos)throw std::invalid_argument("documentId");
 auto entry=std::make_unique<Entry>();entry->options=options;entry->snapshot={bytes.begin(),bytes.end()};
 std::string token;do{token=opaque();}while(sessions_.contains(token));sessions_.emplace(token,std::move(entry));return token;
}
std::uint64_t NativeSessions::attach(const std::string& token,const NativeView& view){
 if(std::this_thread::get_id()!=thread_||!valid(view))return 0;
 auto* e=find(token);if(!e||e->busy||e->attached||e->generation==std::numeric_limits<std::uint64_t>::max())return 0;
 if(!e->session){
  auto platform=factory_(view);if(!platform)return 0;
  auto session=std::make_unique<Session>(std::move(platform));
  auto open=e->options;open["command"]="open";
  if(!Json::parse(session->call(open.dump(),e->snapshot)).value("ok",false)){session->platform().release();return 0;}
  const auto permission=e->options.value("permission",std::string("unconfigured"));
  const int mode=permission=="unconfigured"?0:permission=="localSingleUser"?1:permission=="contextUnavailable"?2:-1;
  if(mode<0||!Json::parse(session->call(Json{{"command","multi"},{"action","permission"},{"mode",mode}}.dump())).value("ok",false)){session->platform().release();return 0;}
  e->session=std::move(session);e->snapshot.clear();
 }else if(!e->platform().attach(view))return 0;
 e->attached=true;return ++e->generation;
}
bool NativeSessions::detach(const std::string& token,std::uint64_t generation){
 if(std::this_thread::get_id()!=thread_)return false;
 auto* e=find(token);if(!e||e->busy||!e->attached||e->generation!=generation||!e->cancel())return false;
 if(!e->platform().detach())return false;
 e->attached=false;return true;
}
bool NativeSessions::resize(const std::string& token,std::uint64_t generation,float w,float h,float dpr){
 if(std::this_thread::get_id()!=thread_||!validDimensions(w,h,dpr))return false;
 auto* e=find(token);if(!e||!e->attached||e->generation!=generation||!e->cancel())return false;
 return Json::parse(e->session->call(Json{{"command","resize"},{"width",w},{"height",h},{"dpr",dpr}}.dump())).value("ok",false);
}
bool NativeSessions::frame(const std::string& token,std::uint64_t generation){
 if(std::this_thread::get_id()!=thread_)return false;
 auto* e=find(token);return e&&e->attached&&e->generation==generation&&Json::parse(e->session->call(R"({"command":"frame"})")).value("ok",false);
}
bool NativeSessions::input(const std::string& token,std::uint64_t generation,const input::PlatformPointerBatch& batch){
 if(std::this_thread::get_id()!=thread_||batch.samples.empty()||batch.samples.size()>4096)return false;
 auto* e=find(token);if(!e||!e->attached||e->busy||e->generation!=generation)return false;
 const auto& identity=batch.samples.front();
 if(identity.source<1||identity.source>3||batch.sourceLost||batch.terminalCancel)return false;
 for(const auto& sample:batch.samples)if(sample.source!=identity.source||sample.pointer!=identity.pointer)return false;
 if(batch.samples.size()>UINT64_MAX-e->sequence)return false;
 auto copy=batch;for(auto& s:copy.samples)s.sequence=++e->sequence;
 const auto& first=copy.samples.front();const auto id=std::make_pair(first.source,first.pointer);
 if(first.phase==input::PointerPhase::kDown){
  if(e->contacts.contains(id))return false;
  const auto key=e->session->composition()->beginContact(copy);if(!key)return false;e->contacts.emplace(id,*key);return true;
 }
 const auto i=e->contacts.find(id);if(i==e->contacts.end())return false;
 if(!e->session->composition()->submitContact(i->second,copy))return false;
 const auto phase=copy.samples.back().phase;if(phase==input::PointerPhase::kUp||phase==input::PointerPhase::kCancel)e->contacts.erase(i);return true;
}
std::string NativeSessions::call(const std::string& token,std::string_view json,std::span<const std::uint8_t> bytes){
 if(std::this_thread::get_id()!=thread_)return error("WRONG_THREAD");
 auto* e=find(token);if(!e)return error("INVALID_SESSION");if(!e->attached)return error("NOT_ATTACHED");if(e->busy)return error("REENTRANT");
 try{
  if(json.size()>1048576||bytes.size()>134217728)return error("INVALID_ARGUMENT");
  const auto request=Json::parse(json);const auto command=request.at("command").get<std::string>();
  if(command=="open"||command=="pointer"||command=="hover"||command=="frame"||command=="resize"||command=="lose"||command=="close")return error("HOST_ONLY");
  e->busy=true;struct Reset{bool& b;~Reset(){b=false;}}reset{e->busy};
  auto result=Json::parse(e->session->call(json,bytes));
  if(command=="state"&&result.value("ok",false))result["value"]["activeContacts"]=e->contacts.size();
  if(result.value("ok",false)&&!e->session->bytes().empty())result["bytes"]=e->session->bytes();
  return result.dump();
 }catch(const std::exception& ex){return Json{{"ok",false},{"code","INVALID_ARGUMENT"},{"message",ex.what()}}.dump();}
}
bool NativeSessions::close(const std::string& token){
 if(std::this_thread::get_id()!=thread_)return false;
 auto* e=find(token);if(!e||e->busy)return false;
 if(e->session){if(!e->cancel()||!Json::parse(e->session->call(R"({"command":"close"})")).value("ok",false))return false;}
 sessions_.erase(token);return true;
}
}
