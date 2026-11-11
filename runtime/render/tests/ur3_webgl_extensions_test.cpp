#include "../src/ur3_webgl_extensions.hpp"
#include <cassert>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
// The browser drops extension activation on restore. Its application handle
// and Emscripten's once-initialized registry remain the same.
struct Context {
 int handle=7;
 unsigned generation=1;
 bool initExtensionsDone=true;
 std::unordered_set<std::string> active;
 std::vector<std::pair<int,std::string>> requests;
 bool enable(int value,std::string_view name){assert(value==handle);requests.emplace_back(value,name);if(name=="not_supported")return false;active.emplace(name);return true;}
 void restore(){++generation;active.clear();}
 bool skiaProbeAndSampler()const{return active.contains("WEBGL_debug_renderer_info")&&active.contains("EXT_texture_filter_anisotropic");}
};
int main(){
 using canvas::render::internal::renewWebGlExtensions;
 Context context;
 auto renew=[&]{return renewWebGlExtensions(context.handle,"EXT_texture_filter_anisotropic WEBGL_debug_renderer_info not_supported WEBGL_lose_context",[&](int h,std::string_view name){return context.enable(h,name);});};
 renew();assert(context.skiaProbeAndSampler());context.restore();assert(!context.skiaProbeAndSampler());
 renew();assert(context.skiaProbeAndSampler());assert(context.generation==2&&context.handle==7&&context.initExtensionsDone);
 assert(context.requests.size()==6);assert(!context.active.contains("not_supported"));assert(!context.active.contains("WEBGL_lose_context"));
 auto count=context.requests.size();assert(!renewWebGlExtensions(0,"EXT_texture_filter_anisotropic",[&](int h,std::string_view s){return context.enable(h,s);}));assert(context.requests.size()==count);
 assert(renewWebGlExtensions(7,"",[](int,std::string_view){assert(false);return false;}));
}
