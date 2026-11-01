#include "canvas/ink/compact_geometry.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace canvas;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<foundation::InkKnot> flatten(const ink::BrushPreviewDelta& delta){
    std::vector<const ink::BrushPreviewDelta::Chunk*> chunks;
    for(auto p=delta.compactChunks.get();p;p=p->previous.get())chunks.push_back(p);
    std::vector<foundation::InkKnot> knots;
    for(auto c=chunks.rbegin();c!=chunks.rend();++c)for(auto k:(*c)->knots)
        if(knots.empty()||knots.back()!=k)knots.push_back(k);
    return knots;
}
std::vector<foundation::InkKnot> stroke(foundation::InkBrushConfig config,unsigned batch=1){
    auto package=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",3).package;
    package.compactConfig=config;
    ink::BrushSession session(1,ink::resolveBrushState(package,42));require(session.begin(),"begin");
    std::vector<ink::BrushSample> input;
    for(unsigned i=0;i<150;++i)input.push_back({double(i)*4,40+25*std::sin(i*.25),.5,true,i+1,std::uint64_t(i)*8'000'000,true});
    ink::BrushPreviewDelta preview;
    for(std::size_t i=0;i<input.size();i+=batch)require(session.append({input.data()+i,std::min<std::size_t>(batch,input.size()-i)},{},preview)==ink::BrushSessionError::kNone,"append");
    const auto final=flatten(preview);
    const ink::BrushSample predicted{900,900,.9,true,999,2'000'000'000,true};
    require(session.append({},{&predicted,1},preview)==ink::BrushSessionError::kNone,"prediction");
    ink::BrushCommitIntent sealed;require(session.seal(sealed)==ink::BrushSessionError::kNone,"seal");
    require(sealed.knots==final,"prediction or seal changed confirmed geometry");
    return sealed.knots;
}
}
int main(){
    const auto loaded=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",3);
    require(bool(loaded),"revision 3 missing");auto c=*loaded.package.compactConfig;
    c.thinning=.99;
    require(std::abs(ink::compactRadius(c,0)-.08)<1e-9,"thinning 0.99 minimum radius");
    require(std::abs(ink::compactRadius(c,65535)-15.92)<1e-9,"thinning 0.99 maximum radius");
    c.thinning=0;require(ink::compactRadius(c,0)==8&&ink::compactRadius(c,65535)==8,"thinning zero not constant");
    c.thinning=-.99;require(ink::compactRadius(c,0)>ink::compactRadius(c,65535),"negative thinning ignored");
    c.thinning=1;c.easing=foundation::InkEasing::kEaseIn;
    require(ink::compactRadius(c,0)==.0016&&ink::compactRadius(c,65535)==16,"easing endpoint response");
    c=*loaded.package.compactConfig;c.pressureSource=foundation::InkPressureSource::kDevice;
    // At smoothing zero the raw centerline and its effective pressure stay paired.
    auto rawConfig=ink::CompactBrushConfig{};rawConfig.algorithmVersion=3;rawConfig.smoothing=0;rawConfig.streamline=0;
    rawConfig.pressureSource=ink::BrushPressureSource::kDevice;
    ink::CompactBrushSession rawPressure(rawConfig);
    const std::array<ink::TimedBrushSample,3> varying{{{0,0,0,true,1,1,true},{20,0,1,true,2,2,true},{40,0,0,true,3,3,true}}};
    require(rawPressure.append(varying,{}),"raw pressure fixture");
    bool peak=false;for(const auto& k:rawPressure.knots())if(k.x==20&&k.pressure==65535)peak=true;
    require(peak,"smoothing zero detaches pressure from its centerline control");
    const auto smooth=stroke(c);require(stroke(c,19)==smooth,"input batching changes tuned geometry");
    c.smoothing=0;const auto raw=stroke(c);require(raw!=smooth,"smoothing control ignored");
    c.streamline=1;require(stroke(c)!=raw,"streamline control ignored");
    c=*loaded.package.compactConfig;c.size=16;c.thinning=0;c.startTaper=0;c.endTaper=16;
    const std::vector<foundation::InkKnot> line{{0,0,32768},{64,0,32768}};
    const auto cover=ink::compactCoverage(c,line);
    require(cover.front().radius==8&&cover.back().radius==0,"taper endpoint geometry");
    bool middle=false;for(auto p:cover)if(p.x==56){middle=true;require(std::abs(p.radius-4)<1e-9,"taper midpoint envelope");}
    require(middle,"long segment lacks taper transition samples");
    c=*loaded.package.compactConfig;auto package=loaded.package;const auto digest=ink::brushPackageCanonicalDigest(package);
    package.compactConfig->endTaper+=1;require(ink::brushPackageCanonicalDigest(package)!=digest,"digest omits taper");
    c.thinning=2;require(!foundation::validInkBrushConfig(c),"out of range thinning accepted");
    c=*loaded.package.compactConfig;c.smoothing=std::nan("");require(!foundation::validInkBrushConfig(c),"NaN smoothing accepted");
    for(const char* profile:{"round-regular","round-handdrawn","marker-compact-v1"})require(bool(ink::BrushPackageCatalog{}.loadDefault(profile,3)),"shared profile revision missing");
    // The distance model follows a pinned recurrence, not wall time.
    auto d=ink::CompactBrushConfig{};d.algorithmVersion=3;d.pressureModel=foundation::InkPressureModel::kDistance;d.size=16;d.streamline=0;
    ink::CompactBrushSession distance(d);
    const ink::TimedBrushSample a{0,0,0,false,1,1},b{16,0,0,false,2,10'000'001};
    require(distance.append({&a,1},{})&&distance.append({&b,1},{}),"distance fixture");
    require(std::abs(distance.latestPressure()-.3625)<1./65535,"distance-pressure recurrence differs");
    const ink::TimedBrushSample end{16.001,0,0,false,3,210'000'001,false,true};
    require(distance.append({&end,1},{}),"terminal movement");
    require(distance.latestPressure()<=.3625+1./65535,"terminal observation increased pressure");
    std::cout<<"VectorTuning: controls, pressure envelope, batches, prediction, seal, identity\n";
}
