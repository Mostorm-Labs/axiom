#define main oldKernelTestMain
#include "cpu_exact_kernel_test.cpp"
#undef main
int main(){try{
 using namespace render::internal;
 std::vector<foundation::WorldPoint> trace;for(unsigned i=0;i<=7200;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/7200)});
 const semantic::Transform2D pose{1,0,0,1,.125,.25};
 auto full=interaction::partialEraseGeometry(trace,pose,16);require(bool(full),"full geometry");
 const auto expected=original(*full);SkPath path;semantic::EraseMaskGeometry prior;unsigned last=0,total=0;std::size_t highwater=0;
 for(unsigned count:{800U,3600U,7100U,7200U}){
  auto input=*full;std::get<semantic::SweptCircleMask>(input).segments.resize(count);
  CpuRequest request;request.identity.words={1,2,3,4,5,count,1,9};request.geometry=input;
  if(last){request.prefixCount=last;request.prefixPath=path;request.prefixWitness=prior;}
  const auto bytes=cpuEncodeRequest(request);require(!bytes.empty()&&bytes.size()<=1048576,"7200 checkpoint exceeds1MiB");highwater=std::max(highwater,bytes.size());
  auto decoded=cpuDecodeRequest(bytes);require(decoded&&decoded->geometry==input&&decoded->prefixCount==last,"full witness transport");
  CpuExactPathJob job(std::move(decoded->geometry),decoded->prefixCount,decoded->prefixPath);require(job.cursor()==last,"prefix rerun");
  while(!job.ready()){const auto before=job.cursor();require(job.advance(8,2000)&&job.cursor()-before<=8,"shared step bound");total+=job.cursor()-before;}
  path=job.path();last=count;prior=std::move(input);
 }
 require(total==7200&&path==expected,"original leftfold changed or repeated prefix");
 for(std::size_t i=0;i<trace.size()-1;++i){const auto s=interaction::partialEraseSimilaritySegment(trace[i],trace[i+1],pose,16);require(s&&*s==std::get<semantic::SweptCircleMask>(*full).segments[i],"incremental local f32 differs");}
 std::cout<<"{\"segments\":7200,\"actual_union_steps\":"<<total<<",\"max_request_bytes\":"<<highwater<<",\"original_numeric_equal\":true}"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
