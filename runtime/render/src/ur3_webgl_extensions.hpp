#pragma once
#include <string_view>
namespace canvas::render::internal {
// A browser restore invalidates activation even when the host/glue handle is
// retained. Request the current context's supported optional capabilities on
// every new Skia context, without relying on a once-initialized registry bit.
template<class Enable>
bool renewWebGlExtensions(int handle,std::string_view supported,Enable enable) {
 if(handle<=0)return false;
 while(!supported.empty()){
  const auto first=supported.find_first_not_of(' ');
  if(first==std::string_view::npos)break;
  supported.remove_prefix(first);
  const auto end=supported.find(' ');
  const auto name=supported.substr(0,end);
  // These are diagnostic controls, rather than renderer capabilities.
  if(name!="WEBGL_lose_context"&&name!="WEBGL_debug_shaders")
   (void)enable(handle,name); // Optional absence retains the ordinary fallback.
  if(end==std::string_view::npos)break;
  supported.remove_prefix(end+1);
 }
 return true;
}
}
