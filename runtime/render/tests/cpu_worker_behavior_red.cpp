// Compares the old synchronous facade with the new resumable contract; old
// geometry is correct, but it cannot yield an incomplete bounded step.
#include "../src/cpu_exact_path_kernel.hpp"
#include <iostream>
using namespace canvas;using namespace canvas::render::internal;
int main(int argc,char**){semantic::SweptCircleMask m;for(unsigned i=0;i<1600;++i)m.segments.push_back({{{double(i),100},5},{{double(i+1),100},5},{i+.3,100},{i+.6,100}});
 if(argc==1){const auto originalSynchronous=cpuExactPath(m);std::cout<<"{\"numeric_correct\":"<<bool(originalSynchronous)<<",\"yielded_before_complete\":false}\n";return 1;}
 CpuExactPathJob job(m);job.advance(8,2000);const bool yielded=!job.ready()&&job.cursor()<=8;std::cout<<"{\"yielded_before_complete\":"<<yielded<<",\"cursor\":"<<job.cursor()<<"}\n";return yielded?0:1;}
