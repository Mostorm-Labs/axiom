#include "session.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
namespace {
class RasterHost final : public sdk::SessionPlatform {
    runtime::RuntimeComposition* owner_=nullptr;
public:
    sdk::Json open(runtime::RuntimeComposition& owner,const sdk::Json&) override {owner_=&owner;return {{"ok",owner.bindSurface(512,512)}};}
    bool loadFonts() override{return false;}
    bool resize(float w,float h,float) override{return owner_&&owner_->bindSurface(unsigned(w),unsigned(h));}
    bool lose() override{return false;}
    sdk::Json frame(bool) override{return {{"ok",owner_&&owner_->ownerPump()}};}
    bool shutdown() override{return true;}
    void release() noexcept override{owner_=nullptr;}
    std::uint64_t nowNs()const noexcept override{return 1'000'000'000;}
};
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(){try{
    sdk::Session session(std::make_unique<RasterHost>());
    const auto raw=[&](sdk::Json j){return sdk::Json::parse(session.call(j.dump()));};
    const auto call=[&](sdk::Json j){auto r=raw(std::move(j));if(!r.value("ok",false))throw std::runtime_error(r.dump());return r;};
    call({{"command","open"},{"documentId",std::string(32,'9')},{"builtinFonts",false}});
    call({{"command","data"},{"action","attach"},{"capacity",1}});
    call({{"command","control"},{"kind","preset"},{"profileId","round-handdrawn"},{"revision",4}});
    std::uint64_t time=10'000'000;
    const auto pointer=[&](unsigned phase,double x,double y){time+=10'000'000;return raw({{"command","pointer"},{"source",1},{"pointer",1},{"phase",phase},{"timestampNs",time},{"x",x},{"y",y}});};
    const auto state=[&]{return call({{"command","state"}}).at("value");};
    const auto release=[&]{const auto p=call({{"command","data"},{"action","next"}}).at("value");require(!p.is_null(),"publication missing");call({{"command","data"},{"action","release"},{"packet",p}});};
    require(pointer(0,40,100).at("ok")&&pointer(1,400,100).at("ok")&&pointer(2,400,100).at("ok"),"source stroke");
    call({{"command","control"},{"kind","tool"},{"value",2}});
    call({{"command","control"},{"kind","eraser"},{"mode",2},{"diameter",30}});
    require(pointer(0,200,60).at("ok")&&pointer(1,200,140).at("ok"),"erase begin");
    const auto before=state();
    const auto rejected=pointer(2,200,140);
    require(!rejected.at("ok").get<bool>()&&rejected.at("code")=="INPUT_REJECTED","capacity must reject erase commit");
    const auto after=state();
    require(after.at("generation")==before.at("generation")&&after.at("lastCommit")==before.at("lastCommit"),"rejected erase mutated canonical state");
    require(after.at("activeContacts")==0,"rejected Up stranded transport contact");
    release();
    require(pointer(0,220,60).at("ok"),"same pointer cannot restart after rejected Up");
    const auto born=time;
    const auto stale=raw({{"command","pointer"},{"source",1},{"pointer",1},{"phase",2},{"timestampNs",born-1},{"x",220},{"y",60}});
    require(!stale.at("ok").get<bool>()&&state().at("activeContacts")==1,"stale terminal cancelled reused contact");
    require(pointer(1,220,140).at("ok")&&pointer(2,220,140).at("ok"),"recovered erase commit");
    require(state().at("activeContacts")==0&&state().at("generation")!=before.at("generation"),"recovered contact did not commit");
    release();
    // A no-op erase is accepted terminal control, with no document mutation.
    const auto generation=state().at("generation");
    require(pointer(0,500,450).at("ok")&&pointer(2,500,450).at("ok"),"empty erase terminal");
    require(state().at("generation")==generation&&state().at("activeContacts")==0,"no-op erase changed state or stranded contact");
    call({{"command","close"}});
    std::cout<<"rejected erase Up recovery,stale terminal isolation,no-op and canonical rejection PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
