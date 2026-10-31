#include "session.hpp"
#include "canvas/semantic/codec.hpp"
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
int main(){
    std::vector<std::uint8_t> lastOperation;
    sdk::SessionCallbacks callbacks;callbacks.context=&lastOperation;
    callbacks.operation=[](void* context,const std::uint8_t* data,std::size_t size)noexcept{
        *static_cast<std::vector<std::uint8_t>*>(context)={data,data+size};
    };
    sdk::Session session(std::make_unique<RasterHost>(),callbacks);
    const auto call=[&](sdk::Json j){const auto reply=sdk::Json::parse(session.call(j.dump()));require(reply.value("ok",false),reply.dump().c_str());return reply;};
    call({{"command","open"},{"documentId",std::string(32,'9')},{"builtinFonts",false}});
    std::uint64_t timestamp=1'000'000;
    unsigned pointer=1;
    for(const auto profile:{"round-regular","round-handdrawn","marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1","laser-transient"}){
        call({{"command","control"},{"kind","preset"},{"profileId",profile},{"revision",1}});
        const auto c=call({{"command","readInkBrush"}}).at("value");require(c.at("pressureSource")==1,"native default pressure source");
        call({{"command","control"},{"kind","inkBrush"},{"value",c}});
        const auto count=call({{"command","state"}}).at("value").at("objects");
        for(unsigned phase:{0U,1U,2U}){timestamp+=10'000'000;call({{"command","pointer"},{"source",1},{"pointer",pointer},{"timestampNs",timestamp},{"phase",phase},{"x",30+40*phase},{"y",90},{"pressure",.99},{"pressureAvailable",false}});}
        const auto state=call({{"command","state"}}).at("value");
        require(state.at("objects").get<unsigned>()==count.get<unsigned>()+(std::string_view(profile)=="laser-transient"?0:1),"native family canonical mutation");
        const auto snapshot=call({{"command","snapshot"},{"generation",state.at("generation")},{"documentRevision","1"},{"recoveryFrontier",{8,1,16,1,24,1}}});
        require(!session.bytes().empty()&&snapshot.contains("checkpointDigest"),"native snapshot bytes");++pointer;
    }
    // Editing public controls after selecting visual revision 2 must preserve
    // its private evaluator/resource binding through real operation encoding.
    call({{"command","control"},{"kind","preset"},{"profileId","chalk-compact-v1"},{"revision",2}});
    auto config=call({{"command","readInkBrush"}}).at("value");config["size"]=30;
    call({{"command","control"},{"kind","inkBrush"},{"value",config}});
    require(call({{"command","readInkBrush"}}).at("value").at("size")==30,"visual preset config edit rejected");
    for(unsigned phase:{0U,1U,2U}){timestamp+=10'000'000;call({{"command","pointer"},{"source",1},{"pointer",pointer},{"timestampNs",timestamp},{"phase",phase},{"x",30+40*phase},{"y",130},{"pressure",.99},{"pressureAvailable",false}});}
    const auto visual=semantic::SemanticCodec::decodeProtobufOperation(lastOperation);
    require(visual.ok(),"visual operation encoding");
    const auto& bound=std::get<semantic::CompactStrokeContent>(std::get<semantic::AddStrokeOp>(visual.operation.payload).object.content).stroke.config;
    require(bound.algorithmVersion==2&&bound.materialVersion==2&&bound.materialResourceId==foundation::kInkMaterialV2ResourceId,"public config edit reverted visual preset binding");
    const auto before=call({{"command","state"}}).at("value").at("generation");
    const auto rejected=sdk::Json::parse(session.call(R"({"command":"control","kind":"inkBrush","value":{"family":7}})"));
    require(!rejected.value("ok",true)&&call({{"command","state"}}).at("value").at("generation")==before,"invalid native config mutated document");
    call({{"command","close"}});
    std::cout<<"InkV2NativeSession: seven profiles, default simulated, real ingress, snapshot, invalid rejection PASS\n";
}
