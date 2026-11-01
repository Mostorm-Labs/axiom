#include "canvas/foundation/internal/frame_timing.hpp"
#include <stdexcept>
using namespace canvas::foundation::internal::frame_timing;
int main(){
 Context context;active=&context;
 {Scope root(Stage::attempt);{Scope parent(Stage::render);{Scope child(Stage::cpu_coverage);} {Scope child(Stage::cpu_coverage);} } {Scope second(Stage::visibility);} }
 active=nullptr;
 std::uint64_t exclusive=0;for(auto s:context.stages){exclusive+=s.exclusive;if(s.inclusive<s.exclusive)throw std::runtime_error("negative exclusive");}
 if(exclusive!=context.stages[0].inclusive)throw std::runtime_error("nested time double counted");
 if(context.stages[std::size_t(Stage::cpu_coverage)].calls!=2)throw std::runtime_error("repeated stage lost");
 if(context.top)throw std::runtime_error("scope stack leaked");
 {Attempt record(true,1,1,1,1,1,1,1);Scope a(Stage::render);record.outcome("retry");}
 try{Attempt record(false,1,2,1,1,1,1,1);Scope parent(Stage::render);throw 1;}catch(int){}
 if(active)throw std::runtime_error("exception left active collector");
 {Attempt record(true,1,3,1,1,1,1,1);Scope a(Stage::render);a.finish();a.finish();record.outcome("accepted");}
 if(active)throw std::runtime_error("attempt left active collector");
}
