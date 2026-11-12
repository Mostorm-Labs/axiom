#include "../src/cpu_worker_protocol.hpp"
#include "canvas/render/skia_renderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;using namespace canvas::render::internal;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 CpuRequest r;r.identity.words={1,1,1,0,1,1,1,1};semantic::SweptCircleMask s;s.segments.push_back({{{10,20},4},{{11,21},4},{10.3,20.3},{10.6,20.6}});r.geometry=s;auto bytes=cpuEncodeRequest(r);auto decoded=cpuDecodeRequest(bytes);need(decoded&&decoded->identity==r.identity&&decoded->geometry==r.geometry,"losslessidentity/geometry roundtrip includingzeroD1");
 for(std::size_t i=0;i<bytes.size();++i){auto truncated=bytes;truncated.resize(i);need(!cpuDecodeRequest(truncated),"truncatedrequestaccepted");}
 for(auto offset:{0U,4U,16U,96U,104U}){auto wrong=bytes;wrong[offset]^=255;need(!cpuDecodeRequest(wrong),"foreignheaderaccepted");}
 auto invalid=bytes;for(unsigned i=0;i<8;++i)invalid[128+i]=255;need(!cpuDecodeRequest(invalid),"NaNaccepted");
 auto huge=r;std::get<semantic::SweptCircleMask>(huge.geometry).segments.resize(20000);need(cpuEncodeRequest(huge).empty(),"encodeallocatedunboundedpayload");
 auto path=cpuExactPath(r.geometry);need(bool(path),"build");std::vector<std::uint8_t>raw(path->writeToMemory(nullptr));path->writeToMemory(raw.data());need(cpuDecodePath(raw)==path,"pathstrictimport");raw.push_back(0);need(!cpuDecodePath(raw),"trailingpathbytesaccepted");
 CpuExactPathJob cancelled(r.geometry);need(cancelled.advance(1,1),"step");cancelled.cancel();need(!cancelled.advance(8,2000)&&cancelled.cancelled(),"cancelledjobadvanced");
 semantic::FilledPathMask filled;filled.path.fill_rule=semantic::FillRule::kEvenOdd;for(unsigned i=0;i<500;++i){filled.path.commands.emplace_back(semantic::MoveTo{{double(i),1}});filled.path.commands.emplace_back(semantic::LineTo{{double(i),2}});filled.path.commands.emplace_back(semantic::ClosePath{});}CpuExactPathJob job(filled);need(job.advance(8,2000)&&job.cursor()<=8&&!job.ready(),"Filledignoredstepbound");while(!job.ready())need(job.advance(8,2000),"Filledprogress");need(job.path()==cpuExactFilledPath(filled.path),"Filledfillruleorderchanged");
 render::SkiaRenderer renderer;const auto floor=renderer.reserveCpuWorkerGrant(1,64U*1024U*1024U);need(floor&&renderer.cpuWorkerGrantBytes()==64U*1024U*1024U,"originalownergrant");need(!renderer.releaseCpuWorkerGrant(2,floor),"foreignrelease");renderer.setRenderCacheOptions({});need(renderer.cpuWorkerGrantBytes()==64U*1024U*1024U,"replacementforgotgrant");need(renderer.releaseCpuWorkerGrant(1,floor)&&!renderer.releaseCpuWorkerGrant(1,floor)&&renderer.cpuWorkerGrantBytes()==0,"grantlastrelease");
 std::cout<<"{\"protocol\":true,\"finite\":true,\"Filledstep\":true,\"ownergrant\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
