#ifndef AXIOM_API_H
#define AXIOM_API_H
#include <stddef.h>
#include <stdint.h>
#if defined(AXIOM_DLL_BUILD)
#define AXIOM_API __declspec(dllexport)
#else
#define AXIOM_API __declspec(dllimport)
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define AXIOM_ABI_VERSION 1u
typedef uint64_t axiom_runtime;
typedef uint64_t axiom_buffer;
enum axiom_status {
    AXIOM_OK = 0,
    AXIOM_INVALID_ARGUMENT = 1,
    AXIOM_INVALID_HANDLE = 2,
    AXIOM_WRONG_THREAD = 3,
    AXIOM_REENTRANT = 4,
    AXIOM_OUT_OF_MEMORY = 5,
    AXIOM_REJECTED = 6,
    AXIOM_BUSY = 7,
    AXIOM_BUFFER_TOO_SMALL = 8,
    AXIOM_INTERNAL_ERROR = 9
};
typedef void (*axiom_operation_callback)(void*, const uint8_t*, size_t);
typedef void (*axiom_error_callback)(void*, uint32_t);
typedef struct axiom_options {
    uint32_t size, abi_version;
    void* parent_window;
    const wchar_t* asset_root; /* absolute; null = DLL/share/axiom */
    const char* open_json;     /* documentId, restore/resources etc. Same v1 vocabulary as JS. */
    const uint8_t* snapshot;
    size_t snapshot_size;
    void* callback_context;
    axiom_operation_callback on_operation;
    axiom_error_callback on_error;
} axiom_options;
/* All runtime calls are owner-thread, non-reentrant. No STL/CRT allocation crosses this ABI.
   JSON buffers are UTF-8 without NUL. Byte responses are separate immutable handles.
   A rejected JSON command still returns AXIOM_OK: inspect response.ok/code.
   Buffer copy/size/release are any-thread and remain valid after runtime close. */
AXIOM_API uint32_t axiom_abi_version(void);
AXIOM_API int axiom_create(const axiom_options*, axiom_runtime*, axiom_buffer* error);
AXIOM_API int axiom_call(
    axiom_runtime, const char*, size_t, const uint8_t*, size_t, axiom_buffer*, axiom_buffer* bytes);
AXIOM_API int axiom_window(axiom_runtime, void**);
AXIOM_API int
    axiom_close(axiom_runtime); /* BUSY: pump messages/frames and retry, no false IME commit */
AXIOM_API int axiom_abandon(axiom_runtime); /* forced teardown, no IME/persistence promise */
AXIOM_API int axiom_buffer_size(axiom_buffer, size_t*);
AXIOM_API int axiom_buffer_copy(axiom_buffer, void*, size_t, size_t* written);
AXIOM_API int axiom_buffer_release(axiom_buffer);
/* Native Windows collection is automatic. Custom input uses full contact generations.
   source >= 0x100000000, registered explicitly; logical coordinates, one source per batch.
   flags bits: pressure,barrel,hardwareEraser,history,geometry. Do not invent capabilities. */
typedef struct axiom_sample {
    uint64_t source, pointer, sequence, timestamp_ns;
    float x, y, pressure, tilt_x, tilt_y, width, height, orientation;
    uint32_t phase, tool, flags, provenance, modifiers;
} axiom_sample;
typedef struct axiom_contact {
    uint64_t runtime, source, pointer, generation;
} axiom_contact;
AXIOM_API int axiom_input_source(axiom_runtime, uint64_t source, int add);
AXIOM_API int axiom_input_begin(axiom_runtime, const axiom_sample*, size_t, axiom_contact*);
AXIOM_API int axiom_input_submit(axiom_runtime, const axiom_contact*, const axiom_sample*, size_t);
AXIOM_API int
axiom_input_cancel(axiom_runtime, const axiom_contact*, uint64_t sequence, uint64_t timestamp_ns);
#ifdef __cplusplus
}
#endif
#endif
