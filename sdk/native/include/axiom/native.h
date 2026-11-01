#ifndef AXIOM_NATIVE_H
#define AXIOM_NATIVE_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#if defined(AXIOM_NATIVE_BUILD)
#define AXIOM_NATIVE_API __declspec(dllexport)
#else
#define AXIOM_NATIVE_API __declspec(dllimport)
#endif
#else
#define AXIOM_NATIVE_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define AXIOM_NATIVE_ABI_VERSION 1u
/* All Session/View operations run on the UI owner thread, non-reentrant.
   Session strings are 32 ASCII hex digits; revision/generation values are uint64.
   JSON and byte inputs are borrowed for the call and copied before returning.
   Opaque output buffers are immutable and survive session close. Copy/release
   use these functions; no CRT allocation is transferred across this ABI. */
typedef uint64_t axiom_native_buffer;
enum axiom_native_status { AXIOM_NATIVE_OK=0, AXIOM_NATIVE_INVALID_ARGUMENT=1,
 AXIOM_NATIVE_WRONG_THREAD=2, AXIOM_NATIVE_REJECTED=3, AXIOM_NATIVE_INTERNAL=4,
 AXIOM_NATIVE_BUFFER_TOO_SMALL=5 };
/* Apple: two CAMetalLayer*. Android: two ANativeWindow*. Windows: parent HWND
   and a distinct reserved view identity. Handles are borrowed until detach;
   Apple/Android providers retain their native references. Logical units + DPR. */
typedef struct axiom_native_view {uint32_t size,abi_version;void* canonical;void* preview;float width,height,dpr;} axiom_native_view;
typedef struct axiom_native_sample {
 uint64_t source,pointer,timestamp_ns;
 float x,y,pressure,tilt_x,tilt_y;
 uint32_t phase,tool,flags,provenance;
} axiom_native_sample;
/* asset_root UTF-8 absolute path, immutable resources/licenses from the same
   release. One library initializes once on the platform UI thread. */
AXIOM_NATIVE_API int axiom_native_initialize(const char* asset_root,size_t length);
AXIOM_NATIVE_API int axiom_native_create(const char* json,size_t length,const uint8_t* snapshot,size_t size,char session[33]);
AXIOM_NATIVE_API int axiom_native_attach(const char* session,const axiom_native_view*,uint64_t* generation);
AXIOM_NATIVE_API int axiom_native_detach(const char* session,uint64_t generation);
AXIOM_NATIVE_API int axiom_native_resize(const char* session,uint64_t generation,float width,float height,float dpr);
AXIOM_NATIVE_API int axiom_native_frame(const char* session,uint64_t generation);
AXIOM_NATIVE_API int axiom_native_input(const char* session,uint64_t generation,const axiom_native_sample*,size_t count);
/* Host-only input path. JS pointer/surface/close JSON is rejected. Return OK
   indicates transport succeeded; inspect JSON ok/code for semantic rejection. */
AXIOM_NATIVE_API int axiom_native_call(const char* session,const char* json,size_t length,const uint8_t*,size_t size,axiom_native_buffer*);
AXIOM_NATIVE_API int axiom_native_close(const char* session);
AXIOM_NATIVE_API int axiom_native_buffer_copy(axiom_native_buffer,void*,size_t capacity,size_t* required);
AXIOM_NATIVE_API int axiom_native_buffer_release(axiom_native_buffer);
#ifdef __cplusplus
}
#endif
#endif
