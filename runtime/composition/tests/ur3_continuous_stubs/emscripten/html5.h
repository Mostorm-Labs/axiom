#pragma once
using EMSCRIPTEN_WEBGL_CONTEXT_HANDLE=int;
inline constexpr int EMSCRIPTEN_RESULT_SUCCESS=0;
extern bool ur3ContextLost;
inline int emscripten_webgl_make_context_current(int){return 0;}
inline bool emscripten_is_webgl_context_lost(int){return ur3ContextLost;}
inline int emscripten_webgl_get_drawing_buffer_size(int,int*w,int*h){*w=256;*h=256;return 0;}
