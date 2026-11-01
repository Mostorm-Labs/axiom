#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace canvas;
void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){
 auto package=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",3).package;
 auto& cfg=*package.compactConfig;cfg.algorithmVersion=4;cfg.signalVersion=3;cfg.endTaper=0;
 ink::BrushSession session(1,ink::resolveBrushState(package,42));need(session.begin(),"begin");
 ink::BrushPreviewDelta preview;double x=0;
 for(unsigned i=0;i<22;++i){if(i)x+=i<12?20:2;ink::BrushSample s{x,50,.5,true,i+1,std::uint64_t(i+1)*10'000'000,true,i==21};
  need(session.append({&s,1},{},preview)==ink::BrushSessionError::kNone,"append");}
 ink::BrushCommitIntent intent;need(session.seal(intent)==ink::BrushSessionError::kNone,"seal");
 // Independently frozen output of pinned PF TS on this raw trace; p quantization
 // is the only accepted error, never a call to the implementation under test.
 need(std::abs(double(intent.knots.front().pressure)/65535-.1320693188740918)<2./65535,"fat start: PF initial pressure missing");
 need(std::abs(double(intent.knots.back().pressure)/65535-.259588884841479)<.012,"terminal pressure recovery missing");
 need(ink::compactRadius(cfg,intent.knots.front().pressure)<3,"start cap too large");
 const auto preset=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);
 need(bool(preset)&&preset.package.compactConfig->endTaper==0&&preset.package.compactConfig->endCap,"default tip forced to zero");

 auto base=ink::CompactBrushConfig{};base.algorithmVersion=4;base.streamline=.13;base.smoothing=.61;
 base.pressureModel=foundation::InkPressureModel::kDistance;base.geometryConfig=cfg;
 std::vector<ink::TimedBrushSample> trace;
 for(unsigned i=0;i<300;++i)trace.push_back({i*2.,std::sin(i*.09)*15,.2+i%5*.1,true,i+1,std::uint64_t(i)*8'000'000,true,i==299});
 ink::CompactBrushSession whole(base),single(base),predicted(base);
 need(whole.append(trace,{}),"batch fixture");
 for(auto sample:trace){
   auto prediction=sample;prediction.sequence+=1000;prediction.x+=40;prediction.timestampNs+=30'000'000;
   need(single.append({&sample,1},{}),"single input");
   need(predicted.append({&sample,1},{&prediction,1}),"predicted input");
 }
 need(whole.knots()==single.knots(),"batching changes persisted signal");
 need(whole.knots()==predicted.knots(),"predicted tail persisted");
 const auto before=single.knots();auto up=trace.back();up.sequence++;up.timestampNs+=500'000'000;
 need(single.append({&up,1},{}),"stationary up");need(before==single.knots(),"stationary up changes cap");
 auto deviceConfig=base;deviceConfig.pressureSource=ink::BrushPressureSource::kDevice;
 ink::CompactBrushSession device(deviceConfig);need(device.append(trace,{}),"device fixture");
 need(device.knots().back().pressure==std::lround(trace.back().pressure*65535),"device pressure replaced");
 auto invalid=trace.front();invalid.pressureCapability=false;
 ink::CompactBrushSession rejected(deviceConfig);need(!rejected.append({&invalid,1},{})&&rejected.confirmedCount()==0,"missing capability accepted");
 // The explicit timed model must carry the pressure resolved at each raw
 // observation through startup normalization, rather than reset it to .5.
 auto timedConfig=base;timedConfig.pressureModel=foundation::InkPressureModel::kTimedSpeed;
 ink::CompactBrushSession timed(timedConfig);
 auto shortTrace=trace;shortTrace.resize(10);for(auto& q:shortTrace)q.x=q.sequence*20.;shortTrace.back().terminal=true;
 need(timed.append(shortTrace,{}),"timed fixture");
 need(timed.knots().back().pressure<.4*65535,"timed startup resets simulated pressure");
 need(std::abs(timed.latestPressure()-double(timed.knots().back().pressure)/65535)<1e-9,"latest pressure is stale");

 const std::vector<ink::TimedBrushSample> deviceShort{{10,10,.2,true,1,1,true},{80,10,.9,true,2,10'000'001,true,true}};
 ink::CompactBrushSession deviceTwo(deviceConfig);need(deviceTwo.append(deviceShort,{}),"two device points");
 need(deviceTwo.knots().back().pressure==std::lround(.9*65535),"two-point device pressure lost in normalization");
 auto tap=deviceShort.front();tap.terminal=true;
 ink::CompactBrushSession dot(deviceConfig);need(dot.append({&tap,1},{}),"tap");
 need(dot.knots().size()==1&&dot.knots().front().x==tap.x&&dot.knots().front().y==tap.y,"tap gains artificial diagonal");
 // Dense input must retain bounded preview work even before the cursor has
 // crossed the replaceable tail's distance horizon.
 ink::CompactBrushSession dense(base);
 for(unsigned i=0;i<4000;++i){ink::TimedBrushSample q{i*.01,50,0,false,i+1,std::uint64_t(i)*1'000'000};need(dense.append({&q,1},{}),"dense fixture");}
 need(dense.metrics().copiedKnots<4000ULL*512,"dense input causes unbounded tail copies");
 std::cout<<"VectorQuality: PF endpoints, batches, prediction, release, device/timed pressure, tap, bounded dense input passed\n";
}
