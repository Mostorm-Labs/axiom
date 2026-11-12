// Private same-build CPU ABI. One task owns its geometry/path/bytes. No Runtime.
#include "cpu_worker_protocol.hpp"
#include <memory>
#include <cstring>
#include <string>
using namespace canvas::render::internal;
namespace {
std::unique_ptr<CpuExactPathJob> job;std::vector<std::uint8_t> result;
std::array<std::uint8_t,cpuHeaderSize> header{};std::uint32_t serial=0,handle=0;
void set32(std::vector<std::uint8_t>& b,unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=v>>(i*8);}
}
extern "C" {
const char* axiom_cpu_build(){return AXIOM_CPU_KERNEL_ID;}
std::uint32_t axiom_cpu_create(const std::uint8_t* bytes,std::uint32_t size){
 try{
  if(job||!bytes||size>cpuInputLimit||serial==UINT32_MAX)return 0;
  auto r=cpuDecodeRequest({bytes,size});if(!r)return 0;
  const auto count=std::visit([](const auto& v){if constexpr(requires{v.segments;})return v.segments.size();else return v.path.commands.size();},r->geometry);
  // Task construction is within the existing charged fixed64MiB Worker heap.
  // It does not request a larger resident Render cache.
  if(!cpuConstructionEstimate(count,size,std::holds_alternative<canvas::semantic::SweptCircleMask>(r->geometry)))return 0;
  std::copy_n(bytes,cpuHeaderSize,header.begin());job=std::make_unique<CpuExactPathJob>(std::move(r->geometry),r->prefixCount,r->prefixPath);handle=++serial;return handle;
 }catch(...){return 0;}
}
int axiom_cpu_step(std::uint32_t token,std::uint32_t steps,std::uint32_t micros){
 if(!job||token!=handle||steps>32||micros>2000)return -1;
 if(job->cancelled())return 3;
 try{
  if(!job->advance(steps,micros))return 4;
  if(!job->ready())return 1;
  if(result.empty()){
   const auto size=job->path().writeToMemory(nullptr);if(size>cpuOutputLimit-cpuHeaderSize)return 4;
   const bool piece=header[4]==3;
   result.resize(cpuHeaderSize+size);std::copy(header.begin(),header.end(),result.begin());set32(result,0,0x31525043);set32(result,4,1);set32(result,96,size);set32(result,100,piece?3:2);if(!piece)for(unsigned i=104;i<128;++i)result[i]=0;
   job->path().writeToMemory(result.data()+cpuHeaderSize);
   if(!cpuDecodePath({result.data()+cpuHeaderSize,size}))return 4;
  }
  return 2;
 }catch(...){return 4;}
}
std::uint32_t axiom_cpu_cursor(std::uint32_t token){return job&&token==handle?job->cursor():0;}
double axiom_cpu_max_union_ns(std::uint32_t token){return job&&token==handle?double(job->maxUnionNanoseconds()):0;}
const std::uint8_t* axiom_cpu_result(std::uint32_t token){return job&&token==handle&&!result.empty()?result.data():nullptr;}
std::uint32_t axiom_cpu_result_size(std::uint32_t token){return job&&token==handle?result.size():0;}
int axiom_cpu_cancel(std::uint32_t token){if(!job||token!=handle)return 0;job->cancel();return 1;}
int axiom_cpu_release(std::uint32_t token){if(!job||token!=handle)return 0;job.reset();std::vector<std::uint8_t>{}.swap(result);handle=0;return 1;}
int axiom_cpu_validate(const std::uint8_t* bytes,std::uint32_t size){return bytes&&cpuDecodePath({bytes,size})?1:0;}
}
