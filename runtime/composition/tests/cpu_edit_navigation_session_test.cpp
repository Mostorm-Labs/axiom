#include "session.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/snapshot.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace canvas;
namespace {
class RasterHost final : public sdk::SessionPlatform {
    runtime::RuntimeComposition* owner_=nullptr;
public:
    sdk::Json open(runtime::RuntimeComposition& owner,const sdk::Json&) override {
        owner_=&owner;return {{"ok",owner.bindSurface(512,512)}};
    }
    bool loadFonts() override{return false;}
    bool resize(float w,float h,float) override{return owner_&&owner_->bindSurface(unsigned(w),unsigned(h));}
    bool lose() override{return false;}
    sdk::Json frame(bool) override{return {{"ok",owner_&&owner_->ownerPump()}};}
    bool shutdown() override{return true;}
    void release() noexcept override{owner_=nullptr;}
    std::uint64_t nowNs()const noexcept override{return 1'000'000'000;}
};
void need(bool v,const char* m){if(!v)throw std::runtime_error(m);}
}
int main(int argc,char** argv)try{
    need(argc==2,"image path");
    sdk::Session s(std::make_unique<RasterHost>());
    const auto call=[&](sdk::Json j,std::span<const std::uint8_t> bytes={}){
        auto r=sdk::Json::parse(s.call(j.dump(),bytes));need(r.value("ok",false),r.dump().c_str());return r;
    };
    call({{"command","open"},{"documentId",std::string(32,'9')},{"builtinFonts",false}});
    const auto presets=call({{"command","presets"}}).at("value");
    call({{"command","control"},{"kind","zoom"},{"x",0},{"y",0},{"scale",1.5}});
    auto zoomed=call({{"command","state"}}).at("value").at("camera");
    call({{"command","control"},{"kind","pan"},{"x",zoomed.at("x").get<double>()+200},{"y",zoomed.at("y").get<double>()+120}});
    const auto positioned=call({{"command","state"}}).at("value").at("camera");
    need(positioned.at("scale")==1.5&&positioned.at("x")==-200&&positioned.at("y")==-120,"public camera placement");
    call({{"command","control"},{"kind","zoom"},{"x",0},{"y",0},{"scale",1}});
    zoomed=call({{"command","state"}}).at("value").at("camera");
    call({{"command","control"},{"kind","pan"},{"x",zoomed.at("x")},{"y",zoomed.at("y")}});
    std::uint64_t timestamp=90'000'000'000;
    for(const auto* profile:{"chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1","round-handdrawn"}){
        const unsigned revision=std::string(profile)=="round-handdrawn"?4:3;
        need(std::any_of(presets.begin(),presets.end(),[&](const auto& p){return p.at("profileId")==profile&&p.at("revision")==revision;}),"missing actual preset");
        call({{"command","control"},{"kind","preset"},{"profileId",profile},{"revision",revision}});
        const auto config=call({{"command","readInkBrush"}}).at("value");
        need(config.at("version")==2,"material public version");
        for(unsigned i=0;i<10;++i){timestamp+=8'000'000;call({{"command","pointer"},{"source",2},{"pointer",1},{"timestampNs",timestamp},{"phase",i==0?0:i==9?2:1},{"x",50+i*10},{"y",70+i*6},{"pressure",.5},{"pressureAvailable",true}});}
    }
    std::ifstream image(argv[1],std::ios::binary);std::vector<std::uint8_t> png{std::istreambuf_iterator<char>(image),{}};
    call({{"command","image"},{"action","import"},{"mime","image/png"}},png);
    const auto resources=call({{"command","resource"},{"action","list"}}).at("value");need(resources.at("entries").size()==1,"one image resource");
    const auto state=call({{"command","state"}}).at("value");
    auto saved=call({{"command","snapshot"},{"generation",state.at("generation")},{"documentRevision","100"},{"recoveryFrontier",sdk::Json::array({8,1,16,1,24,1})}});
    std::vector<std::uint8_t> bytes(s.bytes().begin(),s.bytes().end());
    auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(decoded.ok(),"actual Snapshot decode");
    std::vector<unsigned> algorithms,families;
    for(const auto& o:decoded.snapshot->objects)if(const auto* c=std::get_if<semantic::CompactStrokeContent>(&o.content)){
        algorithms.push_back(c->stroke.config.algorithmVersion);families.push_back(unsigned(c->stroke.config.family));
    }
    std::map<unsigned,unsigned> actual;
    for(std::size_t i=0;i<families.size();++i)actual.emplace(families[i],algorithms[i]);
    std::cerr<<sdk::Json{{"actualFamilyAlgorithm",actual}}.dump()<<'\n';
    need(actual==std::map<unsigned,unsigned>{{1,4},{3,6},{4,5},{5,6}},"material algorithms must remain native round4/chalk6/crayon5/watercolor6");
    call({{"command","close"}});
    sdk::Session restored(std::make_unique<RasterHost>());
    auto opened=sdk::Json::parse(restored.call(sdk::Json{{"command","open"},{"documentId",std::string(32,'9')},{"builtinFonts",false},{"restore",{{"documentRevision","100"},{"recoveryFrontier",{8,1,16,1,24,1}},{"checkpointDigest",saved.at("checkpointDigest")},{"resourceBinding","1"}}}}.dump(),bytes));need(opened.value("ok",false),opened.dump().c_str());
    auto catalog=sdk::Json::parse(restored.call("{\"command\":\"resource\",\"action\":\"list\"}"));
    const auto token=catalog.at("value").at("entries").at(0).at("token");
    auto published=sdk::Json::parse(restored.call(sdk::Json{{"command","resource"},{"action","publish"},{"token",token}}.dump(),png));need(published.value("ok",false),"restored current resource token publish");
    auto listed=sdk::Json::parse(restored.call("{\"command\":\"resource\",\"action\":\"list\"}"));need(listed.at("value").at("entries").at(0).at("sha256")==resources.at("entries").at(0).at("sha256"),"restored image identity");
    std::cout<<sdk::Json{{"status","QUALIFIED"},{"algorithms",algorithms},{"families",families},{"snapshotBytes",bytes.size()},{"resourceRestore","current root token, actual Session publish"},{"resource",listed.at("value")}}.dump()<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
