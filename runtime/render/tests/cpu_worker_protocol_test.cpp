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
 // A serialized known prefix must resume in the original sequential order.
 auto continuation=r;auto& full=std::get<semantic::SweptCircleMask>(continuation.geometry);for(unsigned i=0;i<25;++i)full.segments.push_back({{{11.0+i,21},4},{{12.0+i,21},4},{11.3+i,21},{11.6+i,21}});
 CpuExactPathJob prefix(r.geometry);while(!prefix.ready())need(prefix.advance(8,2000),"prefix advance");
 continuation.prefixCount=1;continuation.prefixPath=prefix.path();continuation.prefixWitness=r.geometry;auto carried=cpuDecodeRequest(cpuEncodeRequest(continuation));need(carried&&carried->prefixCount==1,"checkpoint roundtrip");
 CpuExactPathJob resumed(carried->geometry,carried->prefixCount,carried->prefixPath);need(resumed.cursor()==1,"prefix was recomputed");while(!resumed.ready())need(resumed.advance(8,2000),"suffix");need(resumed.path()==cpuExactPath(continuation.geometry),"serialized prefix altered leftfold");
 auto wrongProof=continuation;std::get<semantic::SweptCircleMask>(*wrongProof.prefixWitness).segments[0].control1.x+=.1;need(cpuEncodeRequest(wrongProof).empty(),"changed prefix accepted");
 auto tap=continuation;tap.prefixWitness=semantic::SweptCircleMask{{{{{10,20},4},{{10,20},4},{10,20},{10,20}}}};need(cpuEncodeRequest(tap).empty(),"tap promoted to real segment without fork");
 auto invalidPrefix=continuation;invalidPrefix.prefixCount=100;need(cpuEncodeRequest(invalidPrefix).empty(),"prefix beyond input accepted");
 render::SkiaRenderer renderer;const auto floor=renderer.reserveCpuWorkerGrant(1,64U*1024U*1024U);need(floor&&renderer.cpuWorkerGrantBytes()==64U*1024U*1024U,"originalownergrant");need(!renderer.releaseCpuWorkerGrant(2,floor),"foreignrelease");renderer.setRenderCacheOptions({});need(renderer.cpuWorkerGrantBytes()==64U*1024U*1024U,"replacementforgotgrant");need(renderer.releaseCpuWorkerGrant(1,floor)&&!renderer.releaseCpuWorkerGrant(1,floor)&&renderer.cpuWorkerGrantBytes()==0,"grantlastrelease");
 std::cout<<"{\"protocol\":true,\"finite\":true,\"Filledstep\":true,\"ownergrant\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
