#pragma once
using GLsync = void*;
inline constexpr unsigned GL_SYNC_GPU_COMMANDS_COMPLETE=1, GL_ALREADY_SIGNALED=2,
    GL_CONDITION_SATISFIED=3, GL_TIMEOUT_EXPIRED=4, GL_WAIT_FAILED=5;
extern unsigned ur3FenceResult;
inline GLsync glFenceSync(unsigned,unsigned){return reinterpret_cast<GLsync>(1);}
inline void glFlush(){}
inline void glDeleteSync(GLsync){}
inline unsigned glClientWaitSync(GLsync,unsigned,unsigned long long){return ur3FenceResult;}
