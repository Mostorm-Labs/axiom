#include "session.hpp"
#include "host_font_resources.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include <iostream>
#include <string>

namespace {
// A real Runtime oracle for portable control transport; never platform UI evidence.
class Host final : public canvas::sdk::SessionPlatform {
    canvas::runtime::RuntimeComposition* owner_=nullptr;
public:
    canvas::sdk::Json open(canvas::runtime::RuntimeComposition& owner,const canvas::sdk::Json&) override {owner_=&owner;return {{"ok",owner.bindSurface(512,512)}};}
    bool loadFonts() override{return canvas::minimal_runtime_host::bindHostTextFonts(canvas::runtime::RuntimePlatformAccess::core(*owner_),NATIVE_TEST_CJK,NATIVE_TEST_FONTS);}
    bool resize(float w,float h,float)override{return owner_&&owner_->bindSurface(unsigned(w),unsigned(h));}
    bool lose()override{return false;}
    canvas::sdk::Json frame(bool)override{return {{"ok",owner_&&owner_->ownerPump()}};}
    bool shutdown()override{return true;}
    void release()noexcept override{owner_=nullptr;}
    std::uint64_t nowNs()const noexcept override{return 1'000'000'000;}
};
}
int main(){
    canvas::sdk::Session session(std::make_unique<Host>());
    for(std::string line;std::getline(std::cin,line);){
        try{
            const auto input=canvas::sdk::Json::parse(line);
            const auto bytes=input.value("bytes",std::vector<std::uint8_t>{});
            auto reply=canvas::sdk::Json::parse(session.call(input.at("request").dump(),bytes));
            if(!session.bytes().empty())reply["bytes"]=session.bytes();
            std::cout<<reply.dump()<<std::endl;
        }catch(const std::exception& e){std::cout<<canvas::sdk::Json{{"ok",false},{"code","HOST_PROTOCOL"},{"message",e.what()}}.dump()<<std::endl;}
    }
}
