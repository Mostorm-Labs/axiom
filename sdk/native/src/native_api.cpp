#include <axiom/native.h>
#include "native_session.hpp"
#include "skia_native_platform.hpp"
#if defined(__APPLE__)
#include "metal_surface.hpp"
#elif defined(__ANDROID__)
#include "android_surface.hpp"
#elif defined(_WIN32)
#include "windows_surface.hpp"
#endif
#include <filesystem>
#include <mutex>
#include <cstring>
#include <atomic>
#include <cmath>

namespace {
std::unique_ptr<canvas::sdk::NativeSessions> registry;
std::mutex initializationMutex;
std::thread::id owner;
std::mutex bufferMutex;
std::map<std::uint64_t,std::string> buffers;
std::uint64_t nextBuffer=1;
int check(){std::lock_guard lock(initializationMutex);return !registry?AXIOM_NATIVE_REJECTED:std::this_thread::get_id()!=owner?AXIOM_NATIVE_WRONG_THREAD:AXIOM_NATIVE_OK;}
bool token(const char* s){if(!s)return false;for(unsigned i=0;i<32;++i)if(!s[i]||!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return s[32]==0;}
template<class F>int safe(F&& f)noexcept{try{return f();}catch(const std::invalid_argument&){return AXIOM_NATIVE_INVALID_ARGUMENT;}catch(...){return AXIOM_NATIVE_INTERNAL;}}
}
extern "C" {
int axiom_native_initialize(const char* assets,size_t length){return safe([&]() -> int {
 if(!assets||!length||length>32768)return AXIOM_NATIVE_INVALID_ARGUMENT;
 const std::filesystem::path path(std::string(assets,length));if(!path.is_absolute())return AXIOM_NATIVE_INVALID_ARGUMENT;
 std::lock_guard lock(initializationMutex);
 if(registry)return std::this_thread::get_id()==owner?AXIOM_NATIVE_OK:AXIOM_NATIVE_WRONG_THREAD;
 owner=std::this_thread::get_id();
 registry=std::make_unique<canvas::sdk::NativeSessions>([path](const canvas::sdk::NativeView& view){
#if defined(__APPLE__)
 return canvas::sdk::makeSkiaNativePlatform(view,canvas::sdk::makeMetalSurfaces,path);
#elif defined(__ANDROID__)
 return canvas::sdk::makeSkiaNativePlatform(view,canvas::sdk::makeAndroidSurfaces,path);
#elif defined(_WIN32)
 return canvas::sdk::makeWindowsNativePlatform(view,path);
#else
 (void)view;(void)path;return std::unique_ptr<canvas::sdk::NativeSessionPlatform>{};
#endif
 });return AXIOM_NATIVE_OK;
});}
int axiom_native_create(const char* json,size_t length,const uint8_t* bytes,size_t size,char out[33]){return safe([&]() -> int {
 if(const int status=check())return status;if(!json||!out||(!bytes&&size))return AXIOM_NATIVE_INVALID_ARGUMENT;
 const auto s=registry->create({json,length},{bytes,size});std::memcpy(out,s.c_str(),33);return AXIOM_NATIVE_OK;
});}
int axiom_native_attach(const char* s,const axiom_native_view* view,uint64_t* generation){return safe([&]() -> int {
 if(const int status=check())return status;if(!token(s)||!view||view->size!=sizeof(*view)||view->abi_version!=1||!generation)return AXIOM_NATIVE_INVALID_ARGUMENT;
 *generation=registry->attach(s,{view->canonical,view->preview,view->width,view->height,view->dpr});return *generation?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;
});}
int axiom_native_detach(const char* s,uint64_t g){return safe([&]() -> int {if(const int status=check())return status;if(!token(s))return AXIOM_NATIVE_INVALID_ARGUMENT;return registry->detach(s,g)?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;});}
int axiom_native_resize(const char* s,uint64_t g,float w,float h,float d){return safe([&]() -> int {if(const int status=check())return status;if(!token(s))return AXIOM_NATIVE_INVALID_ARGUMENT;return registry->resize(s,g,w,h,d)?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;});}
int axiom_native_frame(const char* s,uint64_t g){return safe([&]() -> int {if(const int status=check())return status;if(!token(s))return AXIOM_NATIVE_INVALID_ARGUMENT;return registry->frame(s,g)?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;});}
int axiom_native_navigation(const char* s,uint64_t g,const axiom_native_navigation_sample* sample){return safe([&]() -> int {
 if(const int status=check())return status;
 if(!token(s)||!sample||sample->size!=sizeof(*sample)||sample->abi_version!=AXIOM_NATIVE_ABI_VERSION||
    sample->kind>AXIOM_NATIVE_ZOOM||!std::isfinite(sample->delta_x)||!std::isfinite(sample->delta_y)||
    !std::isfinite(sample->anchor_x)||!std::isfinite(sample->anchor_y)||
    !std::isfinite(sample->scale_delta)||sample->scale_delta<=0)return AXIOM_NATIVE_INVALID_ARGUMENT;
 const canvas::interaction::ViewportNavigationSample input{
  sample->kind==AXIOM_NATIVE_PAN?canvas::interaction::ViewportNavigationKind::kWheelPan:
    canvas::interaction::ViewportNavigationKind::kBrowserGesture,
  sample->delta_x,sample->delta_y,sample->anchor_x,sample->anchor_y,sample->scale_delta};
 return registry->navigate(s,g,input)?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;
});}
int axiom_native_input(const char* s,uint64_t g,const axiom_native_sample* samples,size_t n){return safe([&]() -> int {
 if(const int status=check())return status;if(!token(s)||!samples||!n||n>4096)return AXIOM_NATIVE_INVALID_ARGUMENT;
 canvas::input::PlatformPointerBatch batch;batch.samples.reserve(n);
 for(size_t i=0;i<n;++i){const auto& v=samples[i];if(v.source<1||v.source>3||v.phase>3||v.tool>3||v.flags>15||v.provenance>2)return AXIOM_NATIVE_INVALID_ARGUMENT;
  canvas::input::PlatformPointerSample p;p.source=v.source;p.pointer=v.pointer;p.timestampNs=v.timestamp_ns;p.x=v.x;p.y=v.y;p.pressure=v.pressure;p.tiltX=v.tilt_x;p.tiltY=v.tilt_y;p.phase=static_cast<canvas::input::PointerPhase>(v.phase);p.capabilities.tool=static_cast<canvas::input::PointerTool>(v.tool);p.capabilities.pressure=(v.flags&1)!=0;p.capabilities.barrelButton=(v.flags&2)!=0;p.capabilities.hardwareEraser=(v.flags&4)!=0;p.capabilities.coalescedHistory=(v.flags&8)!=0;p.provenance=static_cast<canvas::input::SampleProvenance>(v.provenance);batch.samples.push_back(p);
 }return registry->input(s,g,batch)?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;
});}
int axiom_native_call(const char* s,const char* json,size_t length,const uint8_t* bytes,size_t size,axiom_native_buffer* out){return safe([&]() -> int {
 if(const int status=check())return status;if(!token(s)||!json||!out||(!bytes&&size))return AXIOM_NATIVE_INVALID_ARGUMENT;
 *out=0;
 std::lock_guard lock(bufferMutex);
 if(buffers.size()>=128||nextBuffer==UINT64_MAX)return AXIOM_NATIVE_REJECTED;
 const auto handle=nextBuffer++;
 // Reserve the output slot before entering the semantic command.
 auto [slot,inserted]=buffers.emplace(handle,std::string{});(void)inserted;
 try{slot->second=registry->call(s,{json,length},{bytes,size});}catch(...){buffers.erase(slot);throw;}
 *out=handle;return AXIOM_NATIVE_OK;
});}
int axiom_native_close(const char* s){return safe([&]() -> int {if(const int status=check())return status;if(!token(s))return AXIOM_NATIVE_INVALID_ARGUMENT;return registry->close(s)?AXIOM_NATIVE_OK:AXIOM_NATIVE_REJECTED;});}
int axiom_native_buffer_copy(axiom_native_buffer id,void* dest,size_t capacity,size_t* required){return safe([&]() -> int {std::lock_guard lock(bufferMutex);const auto it=buffers.find(id);if(it==buffers.end()||!required)return AXIOM_NATIVE_INVALID_ARGUMENT;*required=it->second.size();if(!dest||capacity<*required)return AXIOM_NATIVE_BUFFER_TOO_SMALL;std::memcpy(dest,it->second.data(),*required);return AXIOM_NATIVE_OK;});}
int axiom_native_buffer_release(axiom_native_buffer id){return safe([&]() -> int {std::lock_guard lock(bufferMutex);return buffers.erase(id)?AXIOM_NATIVE_OK:AXIOM_NATIVE_INVALID_ARGUMENT;});}
}
