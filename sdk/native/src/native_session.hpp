#pragma once
#include "session.hpp"
#include <functional>
#include <map>
#include <thread>

namespace canvas::sdk {
// Private Host integration only. Native object references are never exposed to JS.
struct NativeView final {void* canonical=nullptr;void* preview=nullptr;float width=0,height=0,dpr=1;};
class NativeSessionPlatform:public SessionPlatform {
public:
 virtual bool attach(const NativeView&)=0;
 virtual bool detach()=0;
};
class NativeSessions final {
public:
 using PlatformFactory=std::function<std::unique_ptr<NativeSessionPlatform>(const NativeView&)>;
 explicit NativeSessions(PlatformFactory);
 ~NativeSessions();
 std::string create(std::string_view,std::span<const std::uint8_t>);
 std::uint64_t attach(const std::string&,const NativeView&);
 bool detach(const std::string&,std::uint64_t);
 bool resize(const std::string&,std::uint64_t,float,float,float);
 bool frame(const std::string&,std::uint64_t);
 bool input(const std::string&,std::uint64_t,const input::PlatformPointerBatch&);
 std::string call(const std::string&,std::string_view,std::span<const std::uint8_t>);
 bool close(const std::string&);
private:
 class Entry;
 std::map<std::string,std::unique_ptr<Entry>> sessions_;
 PlatformFactory factory_;
 const std::thread::id thread_=std::this_thread::get_id();
 Entry* find(const std::string&);
};
}
